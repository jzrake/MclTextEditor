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
    class HighlightComponent; // draws the highlight region(s)
    class TextAction;         // visits a layout, operates on text and caret ranges, is undoable
    class TextLayout;         // stores text data and caret ranges, supplies metrics, accepts actions
    class TextEditor;         // is a component, issues actions, computes view transform
    class Selection;          // stores leading and trailing edges of an editing region
}




//==============================================================================
struct mcl::Selection
{
    juce::Point<int> head; // (row, col) of the selection head (where the caret is drawn)
    juce::Point<int> tail; // (row, col) of the tail
};




//==============================================================================
class mcl::CaretComponent : public juce::Component, private juce::Timer
{
public:
    CaretComponent (const TextLayout& layout);
    void setViewTransform (const juce::AffineTransform& transformToUse);
    void refreshSelections();

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
/**
    All text layout actions can be expressed as two operations performed in serial:
 
    1. A (possible) modification to the selections
    2. A (possible) replacement of the text in those selections
    3. A (possible) modification to the selections
 
    The symmetry of the operation means that computing the inverse is trivial.
*/
class mcl::TextAction
{
public:
    enum class Navigation
    {
        identity,
        forwardByChar, backwardByChar,
        forwardByWord, backwardByWord,
        toLineStart, toLineStartInverse,
        toLineEnd, toLineEndInverse,
    };
    
    struct Report
    {
        bool navigationOcurred = false;
        juce::Rectangle<float> textAreaAffected;
    };
    using Callback = std::function<void(Report)>;

    TextAction();
    TextAction (Callback callback, Navigation pre);
    bool perform (TextLayout& layout);
    TextAction inverted();
    juce::UndoableAction* on (TextLayout& layout) const;

private:
    class Undoable;
    static Navigation inverseOf (Navigation);
    Callback callback = nullptr;
    Navigation navigationPre  = Navigation::identity;
    Navigation navigationPost = Navigation::identity;
    juce::StringArray replacementFwd;
    juce::StringArray replacementRev;
    juce::Array<Selection> priorSelection;
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

    void setFont (juce::Font fontToUse) { font = fontToUse; }

    void replaceAll (const juce::String& content);

    /** Get the number of rows in the layout. */
    int getNumRows() const;

    /** Get the height of the text layout. */
    float getHeight() const { return font.getHeight() * lineSpacing * getNumRows(); }

    /** Get the number of columns in the given row. */
    int getNumColumns (int row) const;

    /** Return the vertical position of a metric on a row. */
    float getVerticalPosition (int row, Metric metric) const;

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
    juce::GlyphArrangement getGlyphsForRow (int row, bool withTrailingSpace=false) const;
    
    /** Return glyphs whose bounding boxes intersect the given area. */
    juce::GlyphArrangement findGlyphsIntersecting (juce::Rectangle<float> area) const;

    /** Find the row and column index nearest to the given position. */
    juce::Point<int> findIndexNearestPosition (juce::Point<float> position) const;

    /** Return the current selection state. */
    const juce::Array<Selection>& getSelections() const { return selections; }
    juce::Array<Selection>& getSelections() { return selections; }

private:
    friend class TextEditor; // !!!
    float lineSpacing = 1.25f;
    juce::Font font;
    juce::StringArray lines;
    juce::Array<Selection> selections;
};




//==============================================================================
class mcl::TextEditor : public juce::Component
{
public:
    TextEditor();
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
    void paintRowsInAlternatingColors (juce::Graphics& g);

    bool tabKeyUsed = true;
    TextLayout layout;
    CaretComponent caret;

    float viewScaleFactor = 1.f;
    juce::Point<float> translation;
    juce::AffineTransform transform;
    juce::UndoManager undo;
};
