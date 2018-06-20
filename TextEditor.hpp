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
    class Selection;
    class TextLayout;
    class TextEditor;
}




/**
 Factoring of responsibilities in the text editor classes:
 
 - Selection: stores leading and trailing edges of an editing region
 - TextAction: visits a layout, operates on text and caret ranges, is undoable
 - TextLayout: stores text data and caret ranges, supplies metrics, accepts actions
 - TextEditor: is a component, issues actions, computes view transform
 - CaretComponent: draws the caret symbol(s) and highlighted region(s)
 */




struct mcl::Selection
{
    juce::Point<int> head; // (row, col) of the selection head (where the caret is drawn)
    juce::Point<int> tail; // (row, col) of the tail
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

    /** Return a glyph arrangement for the given row. */
    juce::GlyphArrangement getGlyphsForRow (int row, bool withTrailingSpace=false) const;
    
    /** Return glyphs whose bounding boxes intersect the given area. */
    juce::GlyphArrangement findGlyphsIntersecting (juce::Rectangle<float> area) const;

    /** Find the row and column index nearest to the given position. */
    juce::Point<int> findRowAndColumnNearestPosition (juce::Point<float> position) const;

private:
    float lineSpacing = 1.25f;
    juce::Font font;
    juce::StringArray lines;
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
    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;
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

    float viewScaleFactor = 1.f;
    juce::Point<float> translation;
    juce::AffineTransform transform;
};
