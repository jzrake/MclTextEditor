#include "TextEditor.hpp"
using namespace juce;




//==============================================================================
void mcl::CaretComponent::showAndResetPhase()
{
    phase = 0.f;
    startTimerHz (40);
    setVisible (true);
}

void mcl::CaretComponent::hideAndPause()
{
    stopTimer();
    setVisible (false);
}

void mcl::CaretComponent::paint (Graphics &g)
{
    g.setColour (colour);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 2.f);
}

float mcl::CaretComponent::squareWave (float wt) const
{
    const float delta = 0.222f;
    const float A = 1.0;
    return 0.5f + A / 3.14159f * std::atanf (std::cosf (wt) / delta);
}

void mcl::CaretComponent::timerCallback()
{
    phase += 1.6e-1;
    colour = colour.withAlpha (squareWave (phase));
    repaint();
}




//==============================================================================
void mcl::TextLayout::setFont (juce::Font newFont)
{
    font = newFont;
}

void mcl::TextLayout::appendRow (const juce::String& text)
{
    lines.add (text);
}

void mcl::TextLayout::insertRow (int index, const juce::String& text)
{
    lines.insert (index, text);
}

void mcl::TextLayout::breakRowAtColumn (int row, int col)
{
    auto lineA = lines[row].substring (0, col);
    auto lineB = lines[row].substring (col);
    lines.set (row, lineA);
    lines.insert (row + 1, lineB);
}

void mcl::TextLayout::joinRowWithPrevious (int row)
{
    jassert (row != 0);
    lines.set (row - 1, lines[row - 1] + lines[row]);
    lines.remove (row);
}

void mcl::TextLayout::removeRow (int index)
{
    lines.remove (index);
}

void mcl::TextLayout::insertCharacter (int row, int col, juce::juce_wchar character)
{
    const auto& line = lines[row];
    lines.set (row, line.substring (0, col) + character + line.substring (col));
}

void mcl::TextLayout::removeCharacter (int row, int col)
{
    const auto& line = lines[row];
    lines.set (row, line.substring (0, col) + line.substring (col + 1));
}

void mcl::TextLayout::insertText (int row, int col, const juce::String& text)
{
    const auto& line = lines[row];
    lines.set (row, line.substring (0, col) + text + line.substring (col));
}

void mcl::TextLayout::clear()
{
    lines.clear();
}

int mcl::TextLayout::getNumRows() const
{
    return lines.size();
}

int mcl::TextLayout::getNumColumns (int row) const
{
    return lines[row].length();
}

int mcl::TextLayout::findRowContainingVerticalPosition (float y) const
{
    if (y < getVerticalRangeForRow (0).getStart())
        return 0;

    else if (y >= getVerticalRangeForRow (getNumRows() - 1).getEnd())
        return getNumRows();

    for (int n = 0; n < getNumRows(); ++n)
    {
        if (getVerticalRangeForRow (n).contains (y))
        {
            return n;
        }
    }
    jassertfalse;
    return -1;
}

Point<int> mcl::TextLayout::findRowAndColumnNearestPosition (Point<float> position) const
{
    auto row = findRowContainingVerticalPosition (position.y);
    auto glyphs = getGlyphsForRow (row);
    auto col = getNumColumns (row);

    for (int n = 0; n < glyphs.getNumGlyphs(); ++n)
    {
        if (glyphs.getBoundingBox (n, 1, true).contains (position.x, position.y))
        {
            col = n;
            break;
        }
    }
    return Point<int> (row, col);
}

Rectangle<float> mcl::TextLayout::getGlyphBounds (int row, int col) const
{
    auto line = lines[row] + " ";

    GlyphArrangement glyphs;
    glyphs.addLineOfText (font, line, 0.f, getVerticalRangeForRow (row).getEnd());

    return glyphs.getBoundingBox (jmin (col, line.length() - 1), 1, true);
}

juce::GlyphArrangement mcl::TextLayout::getGlyphsForRow (int row) const
{
    GlyphArrangement glyphs;
    glyphs.addLineOfText (font, lines[row], 0.f, getVerticalRangeForRow (row).getEnd());
    return glyphs;
}

GlyphArrangement mcl::TextLayout::getGlyphsInside (Rectangle<float> area) const
{
    auto range = getRowIndexRangeIntersecting (area.getVerticalRange());
    GlyphArrangement glyphs;

    for (int n = range.getStart(); n < range.getEnd(); ++n)
    {
        glyphs.addGlyphArrangement (getGlyphsForRow(n));
    }
    return glyphs;
}

Range<int> mcl::TextLayout::getRowIndexRangeIntersecting (Range<float> verticalRange) const
{
    auto i0 = findRowContainingVerticalPosition (verticalRange.getStart());
    auto i1 = findRowContainingVerticalPosition (verticalRange.getEnd());
    return juce::Range<int> (i0, i1);
}

Range<float> mcl::TextLayout::getVerticalRangeForRow (int row) const
{
    auto lineHeight = font.getHeight() * lineSpacing;
    return Range<float> (row * lineHeight, (row + 1) * lineHeight);
}

Range<float> mcl::TextLayout::getHorizontalRangeForRow (int row) const
{
    if (row < 0 || row >= lines.size())
    {
        return Range<float>();
    }
    return getGlyphsForRow (row).getBoundingBox (0, -1, true).getHorizontalRange();
}

Rectangle<float> mcl::TextLayout::getBounds() const
{
    int numRows = getNumRows();
    Range<float> verticalRange;
    Range<float> horizontalRange;

    verticalRange.setStart (getVerticalRangeForRow (0).getStart());
    verticalRange.setEnd (getVerticalRangeForRow (numRows - 1).getEnd());

    for (int n = 0; n < numRows; ++n)
    {
        horizontalRange = horizontalRange.getUnionWith (getHorizontalRangeForRow (n));
    }
    Rectangle<float> bounds;
    bounds.setHorizontalRange (horizontalRange);
    bounds.setVerticalRange (verticalRange);
    return bounds;
}

float mcl::TextLayout::getHeight() const
{
    return getVerticalRangeForRow (getNumRows() - 1).getEnd();
}




//==============================================================================
mcl::TextEditor::TextEditor()
{
    layout.setFont (Font ("Monaco", 12, 0));

    setCaretPosition (0, 0);
    addAndMakeVisible (caret);
    setWantsKeyboardFocus (true);
}

void mcl::TextEditor::setText (const String& text)
{
    layout.clear();

    for (const auto& line : StringArray::fromLines (text))
    {
        layout.appendRow (line);
    }
    repaint();
}

void mcl::TextEditor::setCaretPosition (int row, int col)
{
    caretRow = row;
    caretCol = col;

    caret.setBounds (layout.getGlyphBounds (row, col)
                      .getSmallestIntegerContainer()
                      .removeFromLeft (2)
                      .expanded (0, 3));

    caret.showAndResetPhase();
}

bool mcl::TextEditor::moveCaretForward()
{
    if (caretCol < layout.getNumColumns (caretRow))
    {
        setCaretPosition (caretRow, caretCol + 1);
    }
    else if (caretRow < layout.getNumRows() - 1)
    {
        setCaretPosition (jmin (caretRow + 1, layout.getNumRows() - 1), 0);
    }
    return true;
}

bool mcl::TextEditor::moveCaretBackward()
{
    if (caretCol > 0)
    {
        setCaretPosition (caretRow, caretCol - 1);
    }
    else if (caretRow > 0)
    {
        setCaretPosition (jmax (caretRow - 1, 0), layout.getNumColumns (jmax (caretRow - 1, 0)));
    }
    return true;
}

bool mcl::TextEditor::moveCaretUp()
{
    if (caretRow > 0)
    {
        setCaretPosition (caretRow - 1, caretCol);
    }
    else
    {
        setCaretPosition (0, 0);
    }
    return true;
}

bool mcl::TextEditor::moveCaretDown()
{
    if (caretRow < layout.getNumRows() - 1)
    {
        setCaretPosition (caretRow + 1, caretCol);
    }
    return true;
}

bool mcl::TextEditor::moveCaretToLineEnd()
{
    setCaretPosition (caretRow, layout.getNumColumns (caretRow));
    return true;
}

bool mcl::TextEditor::moveCaretToLineStart()
{
    setCaretPosition (caretRow, 0);
    return true;
}

bool mcl::TextEditor::insertLineBreakAtCaret()
{
    layout.breakRowAtColumn (caretRow, caretCol);
    setCaretPosition (caretRow + 1, 0);
    repaint();
    return true;
}

bool mcl::TextEditor::insertCharacterAtCaret (juce::juce_wchar character)
{
    layout.insertCharacter (caretRow, caretCol, character);
    moveCaretForward();
    repaint();
    return true;
}

bool mcl::TextEditor::insertTextAtCaret (const juce::String& text)
{
    const auto lines = StringArray::fromLines (text);

    layout.insertText (caretRow, caretCol, lines[0]);
    caretCol += text.length();

    for (int n = 1; n < lines.size(); ++n)
    {
        caretRow += 1;
        caretCol = lines[n].length();
        layout.insertRow (caretRow, lines[n]);
    }
    setCaretPosition (caretRow, caretCol);
    repaint();
    return true;
}

bool mcl::TextEditor::removeLineAtCaret()
{
    jassertfalse;
    return true;
}

bool mcl::TextEditor::deleteBackward()
{
    if (caretCol == 0 && caretRow > 0)
    {
        layout.joinRowWithPrevious (caretRow);
    }
    else
    {
        layout.removeCharacter (caretRow, caretCol - 1);
    }
    moveCaretBackward();
    repaint();
    return true;
}

bool mcl::TextEditor::deleteForward()
{
    layout.removeCharacter (caretRow, caretCol);
    repaint();
    return true;
}

void mcl::TextEditor::translateView (float dx, float dy)
{
    translation.y = jlimit (jmin (-0.f, -layout.getHeight() + getHeight()), 0.f, translation.y + dy);
    setTransform (AffineTransform::translation (translation.x, translation.y));
}

void mcl::TextEditor::setTransform (const juce::AffineTransform& newTransform)
{
    transform = newTransform;
    caret.setTransform (transform);
    repaint();
}

void mcl::TextEditor::paint (Graphics& g)
{
    g.fillAll (findColour (juce::TextEditor::backgroundColourId));
    g.setColour (findColour (juce::TextEditor::textColourId));
    layout.getGlyphsInside (g.getClipBounds().toFloat().transformedBy (transform.inverted())).draw (g, transform);
}

void mcl::TextEditor::resized()
{
}

void mcl::TextEditor::mouseDown (const juce::MouseEvent& e)
{
    auto rc = layout.findRowAndColumnNearestPosition (e.position.transformedBy (transform.inverted()));
    setCaretPosition (rc.x, rc.y);
}

void mcl::TextEditor::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& d)
{
    translateView (d.deltaX * 600, d.deltaY * 600);
}

void mcl::TextEditor::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
}

bool mcl::TextEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.isKeyCode (KeyPress::backspaceKey)) return deleteBackward();
    if (key.isKeyCode (KeyPress::leftKey     )) return moveCaretBackward();
    if (key.isKeyCode (KeyPress::rightKey    )) return moveCaretForward();
    if (key.isKeyCode (KeyPress::upKey       )) return moveCaretUp();
    if (key.isKeyCode (KeyPress::downKey     )) return moveCaretDown();
    if (key.isKeyCode (KeyPress::returnKey   )) return insertLineBreakAtCaret();

    if (key == KeyPress ('a', ModifierKeys::ctrlModifier, 0)) return moveCaretToLineStart();
    if (key == KeyPress ('e', ModifierKeys::ctrlModifier, 0)) return moveCaretToLineEnd();
    if (key == KeyPress ('d', ModifierKeys::ctrlModifier, 0)) return deleteForward();

    if (key == KeyPress ('v', ModifierKeys::commandModifier, 0)) return insertTextAtCaret (SystemClipboard::getTextFromClipboard());

    if (key.getTextCharacter() >= ' ' || (tabKeyUsed && (key.getTextCharacter() == '\t')))
    {
        return insertCharacterAtCaret (key.getTextCharacter());
    }
    return false;
}

MouseCursor mcl::TextEditor::getMouseCursor()
{
    return MouseCursor::IBeamCursor;
}
