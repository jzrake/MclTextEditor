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
    class CaretComponent;
    class HighlightComponent;
    class ContentSelection;
    class RectangularPatchList;
    class TextLayout;
    class TextEditor;
}




//==============================================================================
class mcl::CaretComponent : public juce::Component, private juce::Timer
{
public:
    void showAndResetPhase();
    void hideAndPause();
    void paint (juce::Graphics& g) override;
private:
    float squareWave (float wt) const;
    void timerCallback() override;
    //==========================================================================
    juce::Colour colour = juce::Colours::lightblue;
    float phase = 0.f;
};




//==============================================================================
class mcl::HighlightComponent : public juce::Component
{
public:
    HighlightComponent();
    void setSelectedRegion (juce::Array<juce::Rectangle<float>> regionToFill);
    void setViewTransform (const juce::AffineTransform& viewTransformToUse);
    void clear();
    void paint (juce::Graphics& g) override;
private:
    //==========================================================================
    juce::Array<juce::Rectangle<float>> region;
    juce::Path regionBoundary;
    juce::AffineTransform transform;
};




//==============================================================================
class mcl::ContentSelection : public juce::Component
{
public:
    ContentSelection (TextLayout& layout);
    void setViewTransform (const juce::AffineTransform& transform);
    void setCaretPosition (int row, int col, int startRow=-1, int startCol=-1);
    void extendSelectionTo (int row, int col);
    bool moveCaretForward();
    bool moveCaretBackward();
    bool moveCaretUp();
    bool moveCaretDown();
    bool moveCaretToLineEnd();
    bool moveCaretToLineStart();
    bool extendSelectionBackward();
    bool extendSelectionForward();
    bool extendSelectionUp();
    bool extendSelectionDown();
    bool insertLineBreakAtCaret();
    bool insertCharacterAtCaret (juce::juce_wchar);
    bool insertTextAtCaret (const juce::String&);
    bool removeLineAtCaret();
    bool deleteBackward();
    bool deleteForward();
    void setSelectionToColumnRange (int row, juce::Range<int> columnRange);

    void resized() override;

private:
    TextLayout& layout;
    int caretRow = 0;
    int caretCol = 0;
    int selectionStartRow = 0;
    int selectionStartCol = 0;
    HighlightComponent highlight;
    CaretComponent caret;
};




//==============================================================================
class mcl::RectangularPatchList
{
public:
    RectangularPatchList (const juce::Array<juce::Rectangle<float>>& rectangles);
    bool checkIfRectangleFallsInBin (int rectangleIndex, int binIndexI, int binIndexJ) const;
    bool isBinOccupied (int binIndexI, int binIndexJ) const;
    juce::Array<bool> getOccupationMatrix() const;
    juce::Rectangle<float> getGridPatch (int binIndexI, int binIndexJ) const;
    juce::Array<juce::Line<float>> getListOfBoundaryLines() const;
    juce::Path getOutlinePath (float cornerSize=3.f) const;

private:
    static juce::Array<float> uniqueValuesOfSortedArray (const juce::Array<float>& X);
    static juce::Array<float> getUniqueCoordinatesX (const juce::Array<juce::Rectangle<float>>& rectangles);
    static juce::Array<float> getUniqueCoordinatesY (const juce::Array<juce::Rectangle<float>>& rectangles);

    juce::Array<juce::Rectangle<float>> rectangles;
    juce::Array<float> xedges;
    juce::Array<float> yedges;
};




//==============================================================================
class mcl::TextLayout
{
public:
    enum class ColumnRangeType
    {
        line,
        word,
    };
    TextLayout();

    /** Use this to suppply a callback to be issued on changes to the layout.
        The argument will contain the area of the layout that needs
        repainting. An empty rectangle indicates that the change algorithm
        has not computed the dirty rectangle, and so any part of the layout
        might have changed.
     */
    void setChangeCallback (std::function<void (juce::Rectangle<float>)> changeCallbackToUse);
    juce::Font getFont() const { return font; }
    void setFont (juce::Font font);
    void appendRow (const juce::String& text);
    void insertRow (int index, const juce::String& text="");
    void breakRowAtColumn (int row, int col);
    void joinRowWithPrevious (int row);
    void removeRow (int index);
    void insertCharacter (int row, int col, juce::juce_wchar character);
    void removeCharacter (int row, int col);
    void insertText (int row, int col, const juce::String& text);
    void clear();

    /** Get the number of rows in the layout. */
    int getNumRows() const;

    /** Get the number of columns in the given row. */
    int getNumColumns (int row) const;

    /** Find a range of columns on the given row, containing the given column. */
    juce::Range<int> findRangeOfColumns (int row, int col, ColumnRangeType type);

    /** Find the row index at the given position. Returns 0 if the position
        is before the beginning, and getNumRows() if it's past the end.
     */
    int findRowContainingVerticalPosition (float y) const;

    /** Find the row and column index nearest to the given position. */
    juce::Point<int> findRowAndColumnNearestPosition (juce::Point<float> position) const;

    /** Return the bounding box for the glyph at the given index.
        If the index is out of bounds, the bounds will be that of
        a space character sitting at the end of row.
     */
    juce::Rectangle<float> getGlyphBounds (int row, int col) const;

    /** Return the bounding box for the glyphs on the given row, and
        within the given range of columns. The lower index is clipped
        to zero, and if the upper index is out-of-bounds, the bounds
        will include a space character sitting at the end of row.
     */
    juce::Rectangle<float> getGlyphBounds (int row, juce::Range<int> columnRange) const;

    /** Return a glyph arrangement for the given row. */
    juce::GlyphArrangement getGlyphsForRow (int row) const;

    /** Return positioned glyphs inside the given area. */
    juce::GlyphArrangement getGlyphsInside (juce::Rectangle<float> area) const;

    /** Return the range indexes of rows intersecting the given range. */
    juce::Range<int> getRowIndexRangeIntersecting (juce::Range<float> verticalRange) const;

    /** Return the vertical range for the given row index. */
    juce::Range<float> getVerticalRangeForRow (int row) const;

    /** Return the horizontal range for the given row index. */
    juce::Range<float> getHorizontalRangeForRow (int row) const;

    /** Return the vertical size of the layout. */
    float getHeight() const;

    /** Return the bounding box of the whole layout. */
    juce::Rectangle<float> getBounds() const;

private:
    std::function<void (juce::Rectangle<float>)> changeCallback = nullptr;
    float lineSpacing = 1.f;
    juce::Font font;
    juce::StringArray lines; // display lines (not necessarily ending in newlines)
};




//==============================================================================
class mcl::TextEditor : public juce::Component
{
public:
    //==========================================================================
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

    bool tabKeyUsed = true;
    TextLayout layout;
    ContentSelection selection;

    float viewScaleFactor = 1.f;
    juce::Point<float> translation;
    juce::AffineTransform transform;
};
