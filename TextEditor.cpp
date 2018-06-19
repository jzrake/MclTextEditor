#include "TextEditor.hpp"
using namespace juce;




//==============================================================================
void mcl::CaretComponent::showAndResetPhase()
{
    phase = 0.f;
    startTimerHz (40);
    setVisible (true);
    setInterceptsMouseClicks (false, false);
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
mcl::HighlightComponent::HighlightComponent()
{
    setInterceptsMouseClicks (false, false);
    setPaintingIsUnclipped (true);
}

void mcl::HighlightComponent::setSelectedRegion (Array<Rectangle<float>> regionToFill)
{
    if (regionToFill != region)
    {
        region = regionToFill;
        repaint();
    }
}

void mcl::HighlightComponent::setViewTransform (const juce::AffineTransform& viewTransformToUse)
{
    transform = viewTransformToUse;
    repaint();
}

void mcl::HighlightComponent::clear()
{
    if (! region.isEmpty())
    {
        region.clear();
        repaint();
    }
}

void mcl::HighlightComponent::paint (juce::Graphics& g)
{
    g.saveState();
    g.addTransform (transform);
    g.setColour (findColour (juce::TextEditor::highlightColourId));

    for (const auto& rect : region)
    {
        g.fillRect (rect);
    }
    g.restoreState();
}




//==============================================================================
mcl::ContentSelection::ContentSelection (TextLayout& layout) : layout (layout)
{
    setInterceptsMouseClicks (false, false);
    addAndMakeVisible (highlight);
    addAndMakeVisible (caret);
}

void mcl::ContentSelection::setViewTransform (const AffineTransform& transform)
{
    caret.setTransform (transform);
    highlight.setViewTransform (transform);
}

void mcl::ContentSelection::setCaretPosition (int row, int col, int startRow, int startCol)
{
    caretRow = row;
    caretCol = col;
    selectionStartRow = startRow == -1 ? row : startRow;
    selectionStartCol = startCol == -1 ? col : startCol;

    caret.setBounds (layout.getGlyphBounds (row, col)
                      .getSmallestIntegerContainer()
                      .removeFromLeft (2)
                      .expanded (0, 3));
    caret.showAndResetPhase();

    if (selectionStartRow == caretRow && selectionStartCol == caretCol)
    {
        highlight.clear();
    }
    else
    {
        auto rectList = Array<Rectangle<float>>();

        if (caretRow == selectionStartRow)
        {
            int c0 = jmin (selectionStartCol, caretCol);
            int c1 = jmax (selectionStartCol, caretCol);
            rectList.add (layout.getGlyphBounds (caretRow, Range<int> (c0, c1)));
        }
        else
        {
            if (caretRow > selectionStartRow) // for a forward selection
            {
                int r0 = selectionStartRow;
                int c0 = selectionStartCol;
                int r1 = caretRow;
                int c1 = caretCol;

                rectList.add (layout.getGlyphBounds (r0, Range<int> (c0, layout.getNumColumns (r0))));
                rectList.add (layout.getGlyphBounds (r1, Range<int> (0, c1)));

                for (int n = r0 + 1; n < r1; ++n)
                {
                    rectList.add (layout.getGlyphBounds (n, Range<int> (0, layout.getNumColumns (n))));
                }
            }
            else // for a backward selection
            {
                int r0 = caretRow;
                int c0 = caretCol;
                int r1 = selectionStartRow;
                int c1 = selectionStartCol;

                rectList.add (layout.getGlyphBounds (r0, Range<int> (c0, layout.getNumColumns (r0))));
                rectList.add (layout.getGlyphBounds (r1, Range<int> (0, c1)));
                
                for (int n = r0 + 1; n < r1; ++n)
                {
                    rectList.add (layout.getGlyphBounds (n, Range<int> (0, layout.getNumColumns (n))));
                }
            }
        }
        highlight.setSelectedRegion (rectList);
    }
}

void mcl::ContentSelection::extendSelectionTo (int row, int col)
{
    setCaretPosition (row, col, selectionStartRow, selectionStartCol);
}

bool mcl::ContentSelection::moveCaretForward()
{
    if (caretRow != selectionStartRow || caretCol != selectionStartCol)
    {
        setCaretPosition (caretRow, caretCol);
    }
    else if (caretCol < layout.getNumColumns (caretRow))
    {
        setCaretPosition (caretRow, caretCol + 1);
    }
    else if (caretRow < layout.getNumRows() - 1)
    {
        setCaretPosition (jmin (caretRow + 1, layout.getNumRows() - 1), 0);
    }
    return true;
}

bool mcl::ContentSelection::moveCaretBackward()
{
    if (caretRow != selectionStartRow || caretCol != selectionStartCol)
    {
        setCaretPosition (caretRow, caretCol);
    }
    else if (caretCol > 0)
    {
        setCaretPosition (caretRow, caretCol - 1);
    }
    else if (caretRow > 0)
    {
        setCaretPosition (jmax (caretRow - 1, 0), layout.getNumColumns (jmax (caretRow - 1, 0)));
    }
    return true;
}

bool mcl::ContentSelection::moveCaretUp()
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

bool mcl::ContentSelection::moveCaretDown()
{
    if (caretRow < layout.getNumRows() - 1)
    {
        setCaretPosition (caretRow + 1, caretCol);
    }
    return true;
}

bool mcl::ContentSelection::moveCaretToLineEnd()
{
    setCaretPosition (caretRow, layout.getNumColumns (caretRow));
    return true;
}

bool mcl::ContentSelection::moveCaretToLineStart()
{
    setCaretPosition (caretRow, 0);
    return true;
}

bool mcl::ContentSelection::extendSelectionBackward()
{
    if (caretCol > 0)
    {
        extendSelectionTo (caretRow, caretCol - 1);
    }
    return true;
}

bool mcl::ContentSelection::extendSelectionForward()
{
    if (caretCol < layout.getNumColumns (caretRow))
    {
        extendSelectionTo (caretRow, caretCol + 1);
    }
    return true;
}

bool mcl::ContentSelection::extendSelectionUp()
{
    if (caretRow > 0)
    {
        extendSelectionTo (caretRow - 1, caretCol);
    }
    return true;
}

bool mcl::ContentSelection::extendSelectionDown()
{
    if (caretRow < layout.getNumRows() - 1)
    {
        extendSelectionTo (caretRow + 1, caretCol);
    }
    return true;
}

bool mcl::ContentSelection::insertLineBreakAtCaret()
{
    layout.breakRowAtColumn (caretRow, caretCol);
    setCaretPosition (caretRow + 1, 0);
    return true;
}

bool mcl::ContentSelection::insertCharacterAtCaret (juce::juce_wchar character)
{
    layout.insertCharacter (caretRow, caretCol, character);
    moveCaretForward();
    return true;
}

bool mcl::ContentSelection::insertTextAtCaret (const juce::String& text)
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
    return true;
}

bool mcl::ContentSelection::removeLineAtCaret()
{
    jassertfalse;
    return true;
}

bool mcl::ContentSelection::deleteBackward()
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
    return true;
}

bool mcl::ContentSelection::deleteForward()
{
    layout.removeCharacter (caretRow, caretCol);
    return true;
}

void mcl::ContentSelection::setSelectionToColumnRange (int row, juce::Range<int> columnRange)
{
    setCaretPosition (row, columnRange.getEnd(), row, columnRange.getStart());
}

void mcl::ContentSelection::resized()
{
    highlight.setBounds (getLocalBounds());
}




//==============================================================================
mcl::TextLayout::TextLayout()
{
    changeCallback = [] (auto) {};
}

void mcl::TextLayout::setChangeCallback (std::function<void (juce::Rectangle<float>)> changeCallbackToUse)
{
    if (! (changeCallback = changeCallbackToUse))
    {
        changeCallbackToUse = [] (auto) {};
    }
}

void mcl::TextLayout::setFont (juce::Font newFont)
{
    font = newFont;
    changeCallback (Rectangle<float>());
}

void mcl::TextLayout::appendRow (const juce::String& text)
{
    lines.add (text);
    changeCallback (Rectangle<float>());
}

void mcl::TextLayout::insertRow (int index, const juce::String& text)
{
    lines.insert (index, text);
    changeCallback (Rectangle<float>());
}

void mcl::TextLayout::breakRowAtColumn (int row, int col)
{
    auto lineA = lines[row].substring (0, col);
    auto lineB = lines[row].substring (col);
    lines.set (row, lineA);
    lines.insert (row + 1, lineB);
    changeCallback (Rectangle<float>());
}

void mcl::TextLayout::joinRowWithPrevious (int row)
{
    jassert (row != 0);
    lines.set (row - 1, lines[row - 1] + lines[row]);
    lines.remove (row);
    changeCallback (Rectangle<float>());
}

void mcl::TextLayout::removeRow (int index)
{
    lines.remove (index);
    changeCallback (Rectangle<float>());
}

void mcl::TextLayout::insertCharacter (int row, int col, juce::juce_wchar character)
{
    const auto& line = lines[row];
    lines.set (row, line.substring (0, col) + character + line.substring (col));
    changeCallback (getGlyphBounds (row, Range<int> (col, getNumColumns (row))));
}

void mcl::TextLayout::removeCharacter (int row, int col)
{
    const auto& line = lines[row];
    lines.set (row, line.substring (0, col) + line.substring (col + 1));
    changeCallback (getGlyphBounds (row, Range<int> (col, getNumColumns (row))));
}

void mcl::TextLayout::insertText (int row, int col, const juce::String& text)
{
    const auto& line = lines[row];
    lines.set (row, line.substring (0, col) + text + line.substring (col));
    changeCallback (getGlyphBounds (row, Range<int> (col, getNumColumns (row))));
}

void mcl::TextLayout::clear()
{
    lines.clear();
    changeCallback (Rectangle<float>());
}

int mcl::TextLayout::getNumRows() const
{
    return lines.size();
}

int mcl::TextLayout::getNumColumns (int row) const
{
    return lines[row].length();
}

Range<int> mcl::TextLayout::findRangeOfColumns (int row, int col, ColumnRangeType type)
{
    switch (type)
    {
        case ColumnRangeType::line:
        {
            return Range<int> (0, getNumColumns (row));
        }
        case ColumnRangeType::word:
        {
            int i0 = col;
            int i1 = col + 1;
            const auto& line = lines[row];
            const auto& p = line.getCharPointer();

            while (i0 > 0 && p[i0 - 1] != ' ')
            {
                i0 -= 1;
            }
            while (i1 < line.length() && p[i1] != ' ')
            {
                i1 += 1;
            }
            return Range<int> (i0, i1);
        }
    }
}

int mcl::TextLayout::findRowContainingVerticalPosition (float y) const
{
    if (y < getVerticalRangeForRow (0).getStart())
    {
        return 0;
    }

    else if (y >= getVerticalRangeForRow (getNumRows() - 1).getEnd())
    {
        return getNumRows();
    }

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
        if (glyphs.getBoundingBox (n, 1, true).getHorizontalRange().contains (position.x))
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
    return glyphs.getBoundingBox (jlimit (0, line.length() - 1, col), 1, true);
}

juce::Rectangle<float> mcl::TextLayout::getGlyphBounds (int row, juce::Range<int> columnRange) const
{
    auto line = lines[row] + " ";
    GlyphArrangement glyphs;
    glyphs.addLineOfText (font, line, 0.f, getVerticalRangeForRow (row).getEnd());
    return glyphs.getBoundingBox (jlimit (0, line.length() - 1, columnRange.getStart()),
                                  columnRange.getLength(), true);
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
mcl::TextEditor::TextEditor() : selection (layout)
{
    layout.setFont (Font ("Monaco", 32, 0));
    layout.setChangeCallback ([this] (Rectangle<float> area)
    {
        if (area.isEmpty())
            repaint();
        else
            repaint (area.transformedBy (transform).getSmallestIntegerContainer());
    });

    selection.setCaretPosition (0, 0);
    addAndMakeVisible (selection);
    setWantsKeyboardFocus (true);
}

void mcl::TextEditor::setText (const String& text)
{
    layout.clear();

    for (const auto& line : StringArray::fromLines (text))
    {
        layout.appendRow (line);
    }
    selection.setCaretPosition (0, 0);
    repaint();
}

void mcl::TextEditor::translateView (float dx, float dy)
{
    translation.y = jlimit (jmin (-0.f, -layout.getHeight() + getHeight()), 0.f, translation.y + dy);
    setViewTransform (AffineTransform::translation (translation.x, translation.y));
}

void mcl::TextEditor::setViewTransform (const juce::AffineTransform& newTransform)
{
    transform = newTransform;
    selection.setViewTransform (transform);
    repaint();
}

void mcl::TextEditor::paint (Graphics& g)
{
    g.fillAll (findColour (juce::TextEditor::backgroundColourId));
}

void mcl::TextEditor::paintOverChildren (Graphics& g)
{
    g.setColour (findColour (juce::TextEditor::textColourId));
    layout.getGlyphsInside (g.getClipBounds().toFloat().transformedBy (transform.inverted())).draw (g, transform);
}

void mcl::TextEditor::resized()
{
    selection.setBounds (getLocalBounds());
}

void mcl::TextEditor::mouseDown (const juce::MouseEvent& e)
{
    auto rc = layout.findRowAndColumnNearestPosition (e.position.transformedBy (transform.inverted()));
    selection.setCaretPosition (rc.x, rc.y);
}

void mcl::TextEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mouseWasDraggedSinceMouseDown())
    {
        auto rc = layout.findRowAndColumnNearestPosition (e.position.transformedBy (transform.inverted()));
        selection.extendSelectionTo (rc.x, rc.y);
    }
}

void mcl::TextEditor::mouseDoubleClick (const MouseEvent& e)
{
    auto rc = layout.findRowAndColumnNearestPosition (e.position.transformedBy (transform.inverted()));
    auto colRange = layout.findRangeOfColumns (rc.x, rc.y, TextLayout::ColumnRangeType::word);
    selection.setSelectionToColumnRange (rc.x, colRange);
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
    if (key.getModifiers().isShiftDown())
    {
        if (key.isKeyCode (KeyPress::leftKey )) return selection.extendSelectionBackward();
        if (key.isKeyCode (KeyPress::rightKey)) return selection.extendSelectionForward();
        if (key.isKeyCode (KeyPress::upKey   )) return selection.extendSelectionUp();
        if (key.isKeyCode (KeyPress::downKey )) return selection.extendSelectionDown();
    }
    else
    {
        if (key.isKeyCode (KeyPress::backspaceKey)) return selection.deleteBackward();
        if (key.isKeyCode (KeyPress::leftKey     )) return selection.moveCaretBackward();
        if (key.isKeyCode (KeyPress::rightKey    )) return selection.moveCaretForward();
        if (key.isKeyCode (KeyPress::upKey       )) return selection.moveCaretUp();
        if (key.isKeyCode (KeyPress::downKey     )) return selection.moveCaretDown();
        if (key.isKeyCode (KeyPress::returnKey   )) return selection.insertLineBreakAtCaret();
    }

    if (key == KeyPress ('a', ModifierKeys::ctrlModifier, 0)) return selection.moveCaretToLineStart();
    if (key == KeyPress ('e', ModifierKeys::ctrlModifier, 0)) return selection.moveCaretToLineEnd();
    if (key == KeyPress ('d', ModifierKeys::ctrlModifier, 0)) return selection.deleteForward();

    if (key == KeyPress ('v', ModifierKeys::commandModifier, 0)) return selection.insertTextAtCaret (SystemClipboard::getTextFromClipboard());

    if (key.getTextCharacter() >= ' ' || (tabKeyUsed && (key.getTextCharacter() == '\t')))
    {
        return selection.insertCharacterAtCaret (key.getTextCharacter());
    }
    return false;
}

MouseCursor mcl::TextEditor::getMouseCursor()
{
    return MouseCursor::IBeamCursor;
}
