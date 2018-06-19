
#pragma once
#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "JuceHeader.h"




namespace mcl {
    class CaretComponent;
    class TextLayout;
    class TextEditor;
}




//==========================================================================
class mcl::CaretComponent : public juce::Component, private juce::Timer
{
public:
    void showAndResetPhase();
    void hideAndPause();
    void paint (juce::Graphics& g) override;
private:
    float squareWave (float wt) const;
    void timerCallback() override;
    //======================================================================
    juce::Colour colour = juce::Colours::lightblue;
    float phase = 0.f;
};




//==============================================================================
class mcl::TextLayout
{
public:
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

    /** Find the row index at the given position. Returns 0 if the position
     is before the beginning, and getNumRows() if it's past the end.
     */
    int findRowContainingVerticalPosition (float y) const;

    /** Find the row and column index nearest to the given position.
     */
    juce::Point<int> findRowAndColumnNearestPosition (juce::Point<float> position) const;

    /** Return the bounding box for the glyph at the given index.
     If the index is out of bounds, the bounds will be that of
     a space character sitting at the end of given row.
     */
    juce::Rectangle<float> getGlyphBounds (int row, int col) const;

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
    void setCaretPosition (int row, int col);
    bool moveCaretForward();
    bool moveCaretBackward();
    bool moveCaretUp();
    bool moveCaretDown();
    bool moveCaretToLineEnd();
    bool moveCaretToLineStart();
    bool insertLineBreakAtCaret();
    bool insertCharacterAtCaret (juce::juce_wchar);
    bool insertTextAtCaret (const juce::String&);
    bool removeLineAtCaret();
    bool deleteBackward();
    bool deleteForward();

    void translateView (float dx, float dy);

    //==========================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& d) override;
    void mouseMagnify (const juce::MouseEvent& e, float scaleFactor) override;
    bool keyPressed (const juce::KeyPress& key) override;
    juce::MouseCursor getMouseCursor() override;

private:
    //==========================================================================
    void setTransform (const juce::AffineTransform& newTransform);

    bool tabKeyUsed = true;
    int caretRow = 0;
    int caretCol = 0;
    TextLayout layout;
    CaretComponent caret;

    juce::Point<float> translation;
    juce::AffineTransform transform;
};
