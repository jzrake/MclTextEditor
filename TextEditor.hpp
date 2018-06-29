/** ============================================================================
 *
 * TextEditor.hpp
 *
 * Copyright (C) Jonathan Zrake
 *
 * You may use, distribute and modify this code under the terms of the GPL3
 * license.
 * =============================================================================
 */

#pragma once
#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "JuceHeader.h"




namespace mcl {
    /**
        Factoring of responsibilities in the text editor classes:
     */
    class CaretComponent;         // draws the caret symbol(s)
    class GutterComponent;        // draws the gutter
    class GlyphArrangementArray;  // like StringArray but caches glyph positions
    class HighlightComponent;     // draws the highlight region(s)
    class Selection;              // stores leading and trailing edges of an editing region
    class TextDocument;           // stores text data and caret ranges, supplies metrics, accepts actions
    class TextEditor;             // is a component, issues actions, computes view transform
    class Transaction;            // a text replacement, the document computes the inverse on fulfilling it


    //==============================================================================
    template <typename ArgType, typename DataType>
    class Memoizer
    {
    public:
        using FunctionType = std::function<DataType(ArgType)>;

        Memoizer (FunctionType f) : f (f) {}
        DataType operator() (ArgType argument) const
        {
            if (map.contains (argument))
            {
                return map.getReference (argument);
            }
            map.set (argument, f (argument));
            return this->operator() (argument);
        }
        FunctionType f;
        mutable juce::HashMap<ArgType, DataType> map;
    };
}




//==============================================================================
/**
    A data structure encapsulating a contiguous range within a TextDocument.
    The head and tail refer to the leading and trailing edges of a selected
    region (the head is where the caret would be rendered). The selection is
    exclusive with respect to the range of columns (y), but inclusive with
    respect to the range of rows (x). It is said to be oriented when
    head <= tail, and singular when head == tail, in which case it would be
    rendered without any highlighting.
 */
struct mcl::Selection
{
    enum class Part
    {
        head, tail, both,
    };

    Selection() {}
    Selection (juce::Point<int> head) : head (head), tail (head) {}
    Selection (juce::Point<int> head, juce::Point<int> tail) : head (head), tail (tail) {}
    Selection (int r0, int c0, int r1, int c1) : head (r0, c0), tail (r1, c1) {}

    /** Construct a selection whose head is at (0, 0), and whose tail is at the end of
        the given content string, which may span multiple lines.
     */
    Selection (const juce::String& content);

    bool operator== (const Selection& other) const
    {
        return head == other.head && tail == other.tail;
    }

    bool operator< (const Selection& other) const
    {
        const auto A = this->oriented();
        const auto B = other.oriented();
        if (A.head.x == B.head.x) return A.head.y < B.head.y;
        return A.head.x < B.head.x;
    }

    juce::String toString() const
    {
        return "(" + head.toString() + ") - (" + tail.toString() + ")";
    }

    /** Whether or not this selection covers any extent. */
    bool isSingular() const { return head == tail; }

    /** Whether or not this selection is only a single line. */
    bool isSingleLine() const { return head.x == tail.x; }

    /** Whether the given row is within the selection. */
    bool intersectsRow (int row) const
    {
        return isOriented()
            ? head.x <= row && row <= tail.x
            : head.x >= row && row >= tail.x;
    }

    /** Return the range of columns this selection covers on the given row.
     */
    juce::Range<int> getColumnRangeOnRow (int row, int numColumns) const
    {
        const auto A = oriented();

        if (row < A.head.x || row > A.tail.x)
            return { 0, 0 };
        if (row == A.head.x && row == A.tail.x)
            return { A.head.y, A.tail.y };
        if (row == A.head.x)
            return { A.head.y, numColumns };
        if (row == A.tail.x)
            return { 0, A.tail.y };
        return { 0, numColumns };
    }

    /** Whether the head precedes the tail. */
    bool isOriented() const;

    /** Return a copy of this selection, oriented so that head <= tail. */
    Selection oriented() const;

    /** Return a copy of this selection, with its head and tail swapped. */
    Selection swapped() const;

    /** Return a copy of this selection, with head and tail at the beginning and end
        of their respective lines if the selection is oriented, or otherwise with
        the head and tail at the end and beginning of their respective lines.
     */
    Selection horizontallyMaximized (const TextDocument& document) const;

    /** Return a copy of this selection, with its tail (if oriented) moved to
        account for the shape of the given content, which may span multiple
        lines. If instead head > tail, then the head is bumped forward.
     */
    Selection measuring (const juce::String& content) const;

    /** Return a copy of this selection, with its head (if oriented) placed
        at the given index, and tail moved as to leave the measure the same.
        If instead head > tail, then the tail is moved.
     */
    Selection startingFrom (juce::Point<int> index) const;

    Selection withStyle (int token) const { auto s = *this; s.token = token; return s; }

    /** Modify this selection (if necessary) to account for the disapearance of a
        selection someplace else.
     */
    void pullBy (Selection disappearingSelection);

    /** Modify this selection (if necessary) to account for the appearance of a
        selection someplace else.
     */
    void pushBy (Selection appearingSelection);

    /** Modify an index (if necessary) to account for the disapearance of
        this selection.
     */
    void pull (juce::Point<int>& index) const;

    /** Modify an index (if necessary) to account for the appearance of
        this selection.
     */
    void push (juce::Point<int>& index) const;

    juce::Point<int> head; // (row, col) of the selection head (where the caret is drawn)
    juce::Point<int> tail; // (row, col) of the tail
    int token = 0;
};




//==============================================================================
struct mcl::Transaction
{
    using Callback = std::function<void(const Transaction&)>;
    enum class Direction { forward, reverse };

    /** Return a copy of this transaction, corrected for delete and backspace
        characters. For example, if content == "\b" then the selection head is
        decremented and the content is erased.
     */
    Transaction accountingForSpecialCharacters (const TextDocument& document) const;

    /** Return an undoable action, whose perform method thill fulfill this
        transaction, and which caches the reciprocal transaction to be
        issued in the undo method.
     */
    juce::UndoableAction* on (TextDocument& document, Callback callback);

    mcl::Selection selection;
    juce::String content;
    juce::Rectangle<float> affectedArea;
    Direction direction = Direction::forward;

private:
    class Undoable;
};




//==============================================================================
/**
   This class wraps a StringArray and memoizes the evaluation of glyph
   arrangements derived from the associated strings.
*/
class mcl::GlyphArrangementArray
{
public:
    int size() const { return lines.size(); }
    void clear() { lines.clear(); }
    void add (const juce::String& string) { lines.add (string); }
    void insert (int index, const juce::String& string) { lines.insert (index, string); }
    void removeRange (int startIndex, int numberToRemove) { lines.removeRange (startIndex, numberToRemove); }
    const juce::String& operator[] (int index) const;

    int getToken (int row, int col) const;
    void clearTokens (int index);
    void applyTokens (int index, Selection zone);
    juce::GlyphArrangement getGlyphs (int index,
                                      float baseline,
                                      int token,
                                      bool withTrailingSpace=false) const;

private:
    friend class TextDocument;
    friend class TextEditor;
    juce::Font font;
    bool cacheGlyphArrangement = true;

    void ensureValid (int index) const;
    void invalidateAll();

    struct Entry
    {
        Entry() {}
        Entry (const juce::String& string) : string (string) {}
        juce::String string;
        juce::GlyphArrangement glyphsWithTrailingSpace;
        juce::GlyphArrangement glyphs;
        juce::Array<int> tokens;
        bool glyphsAreDirty = true;
        bool tokensAreDirty = true;
    };
    mutable juce::Array<Entry> lines;
};




//==============================================================================
class mcl::TextDocument
{
public:
    enum class Metric
    {
        top,
        ascent,
        baseline,
        descent,
        bottom,
    };

    /**
     Text categories the caret may be targeted to. For forward jumps,
     the caret is moved to be immediately in front of the first character
     in the given catagory. For backward jumps, it goes just after the
     first character of that category.
     */
    enum class Target
    {
        whitespace,
        punctuation,
        character,
        subword,
        word,
        token,
        line,
        paragraph,
        scope,
        document,
    };
    enum class Direction { forwardRow, backwardRow, forwardCol, backwardCol, };

    struct RowData
    {
        int rowNumber = 0;
        bool isRowSelected = false;
        juce::Rectangle<float> bounds;
    };

    class Iterator
    {
    public:
        Iterator (const TextDocument& document, juce::Point<int> index) noexcept : document (&document), index (index) { t = get(); }
        juce::juce_wchar nextChar() noexcept      { if (isEOF()) return 0; auto s = t; document->next (index); t = get(); return s; }
        juce::juce_wchar peekNextChar() noexcept  { return t; }
        void skip() noexcept                      { if (! isEOF()) { document->next (index); t = get(); } }
        void skipWhitespace() noexcept            { while (! isEOF() && juce::CharacterFunctions::isWhitespace (t)) skip(); }
        void skipToEndOfLine() noexcept           { while (t != '\r' && t != '\n' && t != 0) skip(); }
        bool isEOF() const noexcept               { return index == document->getEnd(); }
        const juce::Point<int>& getIndex() const noexcept { return index; }
    private:
        juce::juce_wchar get() { return document->getCharacter (index); }
        juce::juce_wchar t;
        const TextDocument* document;
        juce::Point<int> index;
    };

    /** Get the current font. */
    juce::Font getFont() const { return font; }

    /** Get the line spacing. */
    float getLineSpacing() const { return lineSpacing; }

    /** Set the font to be applied to all text. */
    void setFont (juce::Font fontToUse) { font = fontToUse; lines.font = fontToUse; }

    /** Replace the whole document content. */
    void replaceAll (const juce::String& content);

    /** Replace the list of selections with a new one. */
    void setSelections (const juce::Array<Selection>& newSelections) { selections = newSelections; }

    /** Replace the selection at the given index. The index must be in range. */
    void setSelection (int index, Selection newSelection) { selections.setUnchecked (index, newSelection); }

    /** Get the number of rows in the document. */
    int getNumRows() const;

    /** Get the height of the text document. */
    float getHeight() const { return font.getHeight() * lineSpacing * getNumRows(); }

    /** Get the number of columns in the given row. */
    int getNumColumns (int row) const;

    /** Return the vertical position of a metric on a row. */
    float getVerticalPosition (int row, Metric metric) const;

    /** Return the position in the document at the given index, using the given
        metric for the vertical position. */
    juce::Point<float> getPosition (juce::Point<int> index, Metric metric) const;

    /** Return an array of rectangles covering the given selection. If
        the clip rectangle is empty, the whole selection is returned.
        Otherwise it gets only the overlapping parts.
     */
    juce::Array<juce::Rectangle<float>> getSelectionRegion (Selection selection,
                                                            juce::Rectangle<float> clip={}) const;

    /** Return the bounds of the entire document. */
    juce::Rectangle<float> getBounds() const;

    /** Return the bounding box for the glyphs on the given row, and within
        the given range of columns. The range start must not be negative, and
        must be smaller than ncols. The range end is exclusive, and may be as
        large as ncols + 1, in which case the bounds include an imaginary
        whitespace character at the end of the line. The vertical extent is
        that of the whole line, not the ascent-to-descent of the glyph.
     */
    juce::Rectangle<float> getBoundsOnRow (int row, juce::Range<int> columns) const;

    /** Return the position of the glyph at the given row and column. */
    juce::Rectangle<float> getGlyphBounds (juce::Point<int> index) const;

    /** Return a glyph arrangement for the given row. If token != -1, then
     only glyphs with that token are returned.
     */
    juce::GlyphArrangement getGlyphsForRow (int row, int token=-1, bool withTrailingSpace=false) const;

    /** Return all glyphs whose bounding boxes intersect the given area. This method
        may be generous (including glyphs that don't intersect). If token != -1, then
        only glyphs with that token mask are returned.
     */
    juce::GlyphArrangement findGlyphsIntersecting (juce::Rectangle<float> area, int token=-1) const;

    /** Return the range of rows intersecting the given rectangle. */
    juce::Range<int> getRangeOfRowsIntersecting (juce::Rectangle<float> area) const;

    /** Return data on the rows intersecting the given area. This is sort
        of a convenience method for calling getBoundsOnRow() over a range,
        but could be faster if horizontal extents are not computed.
     */
    juce::Array<RowData> findRowsIntersecting (juce::Rectangle<float> area,
                                               bool computeHorizontalExtent=false) const;

    /** Find the row and column index nearest to the given position. */
    juce::Point<int> findIndexNearestPosition (juce::Point<float> position) const;

    /** Return an index pointing to one-past-the-end. */
    juce::Point<int> getEnd() const;

    /** Advance the given index by a single character, moving to the next
        line if at the end. Return false if the index cannot be advanced
        further.
     */
    bool next (juce::Point<int>& index) const;

    /** Move the given index back by a single character, moving to the previous
        line if at the end. Return false if the index cannot be advanced
        further.
     */
    bool prev (juce::Point<int>& index) const;

    /** Move the given index to the next row if possible. */
    bool nextRow (juce::Point<int>& index) const;

    /** Move the given index to the previous row if possible. */
    bool prevRow (juce::Point<int>& index) const;

    /** Navigate an index to the first character of the given categaory.
     */
    void navigate (juce::Point<int>& index, Target target, Direction direction) const;

    /** Navigate all selections. */
    void navigateSelections (Target target, Direction direction, Selection::Part part);

    Selection search (juce::Point<int> start, const juce::String& target) const;

    /** Return the character at the given index. */
    juce::juce_wchar getCharacter (juce::Point<int> index) const;

    /** Add a selection to the list. */
    void addSelection (Selection selection) { selections.add (selection); }

    /** Return the number of active selections. */
    int getNumSelections() const { return selections.size(); }

    /** Return a line in the document. */
    const juce::String& getLine (int lineIndex) const { return lines[lineIndex]; }

    /** Return one of the current selections. */
    const Selection& getSelection (int index) const;

    /** Return the current selection state. */
    const juce::Array<Selection>& getSelections() const;

    /** Return the content within the given selection, with newlines if the
        selection spans muliple lines.
     */
    juce::String getSelectionContent (Selection selection) const;

    /** Apply a transaction to the document, and return its reciprocal. The selection
        identified in the transaction does not need to exist in the document.
     */
    Transaction fulfill (const Transaction& transaction);

    /* Reset glyph token values on the given range of rows. */
    void clearTokens (juce::Range<int> rows);

    /** Apply tokens from a set of zones to a range of rows. */
    void applyTokens (juce::Range<int> rows, const juce::Array<Selection>& zones);

private:
    friend class TextEditor;

    float lineSpacing = 1.25f;
    mutable juce::Rectangle<float> cachedBounds;
    GlyphArrangementArray lines;
    juce::Font font;
    juce::Array<Selection> selections;
};




//==============================================================================
class mcl::CaretComponent : public juce::Component, private juce::Timer
{
public:
    CaretComponent (const TextDocument& document);
    void setViewTransform (const juce::AffineTransform& transformToUse);
    void updateSelections();

    //==========================================================================
    void paint (juce::Graphics& g) override;

private:
    //==========================================================================
    float squareWave (float wt) const;
    void timerCallback() override;
    juce::Array<juce::Rectangle<float>> getCaretRectangles() const;
    //==========================================================================
    float phase = 0.f;
    const TextDocument& document;
    juce::AffineTransform transform;
};




//==============================================================================
class mcl::GutterComponent : public juce::Component
{
public:
    GutterComponent (const TextDocument& document);
    void setViewTransform (const juce::AffineTransform& transformToUse);
    void updateSelections();

    //==========================================================================
    void paint (juce::Graphics& g) override;

private:
    juce::GlyphArrangement getLineNumberGlyphs (int row) const;
    //==========================================================================
    const TextDocument& document;
    juce::AffineTransform transform;
    Memoizer<int, juce::GlyphArrangement> memoizedGlyphArrangements;
};




//==============================================================================
class mcl::HighlightComponent : public juce::Component
{
public:
    HighlightComponent (const TextDocument& document);
    void setViewTransform (const juce::AffineTransform& transformToUse);
    void updateSelections();

    //==========================================================================
    void paint (juce::Graphics& g) override;

private:
    static juce::Path getOutlinePath (const juce::Array<juce::Rectangle<float>>& rectangles);

    //==========================================================================
    bool useRoundedHighlight = true;
    const TextDocument& document;
    juce::AffineTransform transform;
    juce::Path outlinePath;
};




//==============================================================================
class mcl::TextEditor : public juce::Component
{
public:
    enum class RenderScheme {
        usingAttributedStringSingle,
        usingAttributedString,
        usingGlyphArrangement,
    };

    TextEditor();
    ~TextEditor();
    void setFont (juce::Font font);
    void setText (const juce::String& text);
    void translateView (float dx, float dy);
    void scaleView (float scaleFactor, float verticalCenter);

    //==========================================================================
    void resized() override;
    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& d) override;
    void mouseMagnify (const juce::MouseEvent& e, float scaleFactor) override;
    bool keyPressed (const juce::KeyPress& key) override;
    juce::MouseCursor getMouseCursor() override;

private:
    //==========================================================================
    bool insert (const juce::String& content);
    void updateViewTransform();
    void updateSelections();
    void translateToEnsureCaretIsVisible();

    //==========================================================================
    void renderTextUsingAttributedStringSingle (juce::Graphics& g);
    void renderTextUsingAttributedString (juce::Graphics& g);
    void renderTextUsingGlyphArrangement (juce::Graphics& g);
    void resetProfilingData();
    bool enableSyntaxHighlighting = true;
    bool allowCoreGraphics = true;
    bool useOpenGLRendering = false;
    bool drawProfilingInfo = false;
    float accumulatedTimeInPaint = 0.f;
    float lastTimeInPaint = 0.f;
    float lastTokeniserTime = 0.f;
    int numPaintCalls = 0;
    RenderScheme renderScheme = RenderScheme::usingGlyphArrangement;

    //==========================================================================
    double lastTransactionTime;
    bool tabKeyUsed = true;
    TextDocument document;
    CaretComponent caret;
    GutterComponent gutter;
    HighlightComponent highlight;

    float viewScaleFactor = 1.f;
    juce::Point<float> translation;
    juce::AffineTransform transform;
    juce::UndoManager undo;
    juce::OpenGLContext context;
};

