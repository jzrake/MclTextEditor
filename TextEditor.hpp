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
    class CaretComponent;     // draws the caret symbol(s)
    class GutterComponent;    // draws the gutter
    class HighlightComponent; // draws the highlight region(s)
    class Selection;          // stores leading and trailing edges of an editing region
    class TextLayout;         // stores text data and caret ranges, supplies metrics, accepts actions
    class TextEditor;         // is a component, issues actions, computes view transform
    class Transaction;        // a text replacement, the layout computes the inverse on fulfilling it
}




//==============================================================================
/**
    A data structure encapsulating a contiguous range within a TextLayout.
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
        head, tail,
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
    Selection horizontallyMaximized (const TextLayout& layout) const;

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

    /** Modify this selection (if necessary) to account for the disapearance of a
        selection someplace else.
     */
    void pulledBy (Selection disappearingSelection);

    /** Modify this selection (if necessary) to account for the appearance of a
        selection someplace else.
     */
    void pushedBy (Selection appearingSelection);

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
};




//==============================================================================
struct mcl::Transaction
{
    using Callback = std::function<void(const Transaction&)>;

    /** Return a copy of this transaction, corrected for delete and backspace
        characters. For example, if content == "\b" then the selection head is
        decremented and the content is erased.
     */
    Transaction accountingForSpecialCharacters (const TextLayout& layout) const;

    /** Return an undoable action, whose perform method thill fulfill this
        transaction, and which caches the reciprocal transaction to be
        issued in the undo method.
     */
    juce::UndoableAction* on (TextLayout& layout, Callback callback);

    mcl::Selection selection;
    juce::String content;
    juce::Rectangle<float> affectedArea;

private:
    class Undoable;
};




//==============================================================================
class mcl::CaretComponent : public juce::Component, private juce::Timer
{
public:
    CaretComponent (const TextLayout& layout);
    void setViewTransform (const juce::AffineTransform& transformToUse);
    void updateSelections();

    //==========================================================================
    void paint (juce::Graphics& g) override;

private:
    //==========================================================================
    float squareWave (float wt) const;
    void timerCallback() override;
    //==========================================================================
    float phase = 0.f;
    const TextLayout& layout;
    juce::AffineTransform transform;
};




//==============================================================================
class mcl::GutterComponent : public juce::Component
{
public:
    GutterComponent (const TextLayout& layout);
    void setViewTransform (const juce::AffineTransform& transformToUse);
    void updateSelections();
    void cacheLineNumberGlyphs (int cacheSize=1000);

    //==========================================================================
    void paint (juce::Graphics& g) override;

private:
    juce::GlyphArrangement getLineNumberGlyphs (int row, bool useCached) const;
    //==========================================================================
    const TextLayout& layout;
    juce::AffineTransform transform;
    juce::Array<juce::GlyphArrangement> lineNumberGlyphsCache;
};




//==============================================================================
class mcl::HighlightComponent : public juce::Component
{
public:
    HighlightComponent (const TextLayout& layout);
    void setViewTransform (const juce::AffineTransform& transformToUse);
    void updateSelections();
    
    //==========================================================================
    void paint (juce::Graphics& g) override;

private:
    static juce::Path getOutlinePath (const juce::Array<juce::Rectangle<float>>& rectangles);

    //==========================================================================
    bool useRoundedHighlight = true;
    const TextLayout& layout;
    juce::AffineTransform transform;
};




//==============================================================================
class mcl::TextLayout
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

    enum class Navigation
    {
        identity,
        wholeDocument,
        wholeLine,
        wholeWord,
        forwardByChar, backwardByChar,
        forwardByWord, backwardByWord,
        forwardByLine, backwardByLine,
        toLineStart, toLineEnd,
    };

    struct RowData
    {
        int rowNumber = 0;
        bool isRowSelected = false;
        juce::Rectangle<float> bounds;
    };

    juce::Font getFont() const { return font; }

    void setFont (juce::Font fontToUse) { font = fontToUse; }

    void replaceAll (const juce::String& content);

    void setSelections (const juce::Array<Selection>& newSelections) { selections = newSelections; }

    /** Get the number of rows in the layout. */
    int getNumRows() const;

    /** Get the height of the text layout. */
    float getHeight() const { return font.getHeight() * lineSpacing * getNumRows(); }

    /** Get the number of columns in the given row. */
    int getNumColumns (int row) const;

    /** Return the vertical position of a metric on a row. */
    float getVerticalPosition (int row, Metric metric) const;

    /** Return the position in the layout at the given index, using the given
        metric for the vertical position. */
    juce::Point<float> getPosition (juce::Point<int> index, Metric metric) const;

    /** Return an array of rectangles covering the given selection. If
        the clip rectangle is empty, the whole selection is returned.
        Otherwise it gets only the overlapping parts.
     */
    juce::Array<juce::Rectangle<float>> getSelectionRegion (Selection selection,
                                                            juce::Rectangle<float> clip={}) const;

    /** Return the bounds of the entire layout. */
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

    /** Return a glyph arrangement for the given row. */
    juce::GlyphArrangement getGlyphsForRow (int row, bool withTrailingSpace=false, bool useCached=true) const;
    
    /** Return glyphs whose bounding boxes intersect the given area. */
    juce::GlyphArrangement findGlyphsIntersecting (juce::Rectangle<float> area) const;

    /** Return data on the rows intersecting the given area. This is sort
        of a convenience method for calling getBoundsOnRow() over a range,
        but could be faster if horizontal extents are not computed.
     */
    juce::Array<RowData> findRowsIntersecting (juce::Rectangle<float> area,
                                               bool computeHorizontalExtent=false) const;

    /** Find the row and column index nearest to the given position. */
    juce::Point<int> findIndexNearestPosition (juce::Point<float> position) const;

    juce::Point<int> getLast() const;

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

    /** Move the given index to the next word if possible. */
    bool nextWord (juce::Point<int>& index) const;

    /** Move the given index to the previous word if possible. */
    bool prevWord (juce::Point<int>& index) const;

    /** Return the character at the given index. */
    juce::juce_wchar getCharacter (juce::Point<int> index) const;

    /** Return the current selection state, possibly operated on. */
    juce::Array<Selection> getSelections (Navigation navigation=Navigation::identity, bool fixingTail=false) const;

    /** Return the content within the given selection, with newlines if the
        selection spans muliple lines.
     */
    juce::String getSelectionContent (Selection selection) const;

    /** Apply a transaction to the layout, and return its reciprocal. */
    Transaction fulfill (const Transaction& transaction);

private:
    float lineSpacing = 1.25f;
    mutable juce::Rectangle<float> cachedBounds;
    juce::Font font;
    juce::StringArray lines;
    juce::Array<Selection> selections;
};




//==============================================================================
class mcl::TextEditor : public juce::Component
{
public:
    TextEditor();
    void setFont (juce::Font font);
    void setText (const juce::String& text);
    void translateView (float dx, float dy);
    void scaleView (float scaleFactor);

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
    void updateViewTransform();
    void updateSelections();
    void translateToEnsureCaretIsVisible();

    double lastTransactionTime;
    bool tabKeyUsed = true;
    TextLayout layout;
    CaretComponent caret;
    GutterComponent gutter;
    HighlightComponent highlight;

    float viewScaleFactor = 1.f;
    juce::Point<float> translation;
    juce::AffineTransform transform;
    juce::UndoManager undo;
};

