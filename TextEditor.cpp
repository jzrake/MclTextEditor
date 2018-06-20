/** ============================================================================
 *
 * TextEditor.cpp
 *
 * Copyright (C) Jonathan Zrake
 *
 * You may use, distribute and modify this code under the terms of the GPL3
 * license.
 * =============================================================================
 */

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
}

void mcl::HighlightComponent::setSelectedRegion (Array<Rectangle<float>> regionToFill)
{
    if (regionToFill != region)
    {
        region = regionToFill;

        if (! region.isEmpty())
        {
            regionBoundary = RectangularPatchList (region).getOutlinePath (3.f);
            setVisible (true);
            repaint();
        }
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
        setVisible (false);
    }
}

void mcl::HighlightComponent::paint (juce::Graphics& g)
{
    g.saveState();
    g.addTransform (transform);

#if true
    g.setColour (findColour (juce::TextEditor::highlightColourId));

    for (const auto& rect : region)
    {
        g.fillRect (rect);
    }

#else
    // Region boundary calculation currently can't handle disjoint patches,
    // so we're rendering the highlight in the simple way for now.

    g.setColour (findColour (juce::TextEditor::highlightColourId));
    g.fillPath (regionBoundary);

    g.setColour (findColour (juce::TextEditor::highlightColourId).brighter (0.4f));
    g.strokePath (regionBoundary, PathStrokeType (1.f));
#endif

    g.restoreState();
}




//==============================================================================
mcl::ContentSelection::ContentSelection (TextLayout& layout) : layout (layout)
{
    setInterceptsMouseClicks (false, false);
    addChildComponent (highlight);
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

    if (isSelectionEmpty())
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
            auto range = getProperSelectionRange();
            int r0 = range.getStartX();
            int c0 = range.getStartY();
            int r1 = range.getEndX();
            int c1 = range.getEndY();

            rectList.add (layout.getGlyphBounds (r0, Range<int> (c0, layout.getNumColumns (r0))));
            rectList.add (layout.getGlyphBounds (r1, Range<int> (0, c1)));

            for (int n = r0 + 1; n < r1; ++n)
            {
                rectList.add (layout.getGlyphBounds (n, Range<int> (0, layout.getNumColumns (n) + 1)));
            }
        }
        highlight.setSelectedRegion (rectList);
    }
}

void mcl::ContentSelection::setCaretPosition (juce::Point<int> position)
{
    setCaretPosition (position.x, position.y);
}

void mcl::ContentSelection::extendSelectionTo (int row, int col)
{
    setCaretPosition (row, col, selectionStartRow, selectionStartCol);
}

bool mcl::ContentSelection::isSelectionEmpty()
{
    return selectionStartRow == caretRow && selectionStartCol == caretCol;
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
    if (isSelectionEmpty())
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
    }
    else
    {
        auto range = getProperSelectionRange();
        layout.removeTextInRange (range);
        setCaretPosition (range.getStart());
    }
    return true;
}

bool mcl::ContentSelection::deleteForward()
{
    if (isSelectionEmpty())
    {
        layout.removeCharacter (caretRow, caretCol);
    }
    else
    {
        auto range = getProperSelectionRange();
        layout.removeTextInRange (range);
        setCaretPosition (range.getStart());
    }
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

juce::Line<int> mcl::ContentSelection::getProperSelectionRange()
{
    if (caretRow == selectionStartRow)
    {
        return Line<int> (caretRow, jmin (selectionStartCol, caretCol),
                          caretRow, jmax (selectionStartCol, caretCol));
    }
    if (caretRow > selectionStartRow) // for a forward selection
    {
        int r0 = selectionStartRow;
        int c0 = selectionStartCol;
        int r1 = caretRow;
        int c1 = caretCol;
        return Line<int> (r0, c0, r1, c1);
    }
    else // for a backward selection
    {
        int r0 = caretRow;
        int c0 = caretCol;
        int r1 = selectionStartRow;
        int c1 = selectionStartCol;
        return Line<int> (r0, c0, r1, c1);
    }
}




//==============================================================================
mcl::RectangularPatchList::RectangularPatchList (const Array<Rectangle<float>>& rectangles)
: rectangles (rectangles)
{
    xedges = getUniqueCoordinatesX (rectangles);
    yedges = getUniqueCoordinatesY (rectangles);
}

bool mcl::RectangularPatchList::checkIfRectangleFallsInBin (int rectangleIndex, int binIndexI, int binIndexJ) const
{
    return rectangles.getReference (rectangleIndex).intersects (getGridPatch (binIndexI, binIndexJ));
}

bool mcl::RectangularPatchList::isBinOccupied (int binIndexI, int binIndexJ) const
{
    for (int n = 0; n < rectangles.size(); ++n)
    {
        if (checkIfRectangleFallsInBin (n, binIndexI, binIndexJ))
        {
            return true;
        }
    }
    return false;
}

Rectangle<float> mcl::RectangularPatchList::getGridPatch (int binIndexI, int binIndexJ) const
{
    auto gridPatch = Rectangle<float>();
    gridPatch.setHorizontalRange (Range<float> (xedges.getUnchecked (binIndexI), xedges.getUnchecked (binIndexI + 1)));
    gridPatch.setVerticalRange   (Range<float> (yedges.getUnchecked (binIndexJ), yedges.getUnchecked (binIndexJ + 1)));
    return gridPatch;
}

Array<juce::Line<float>> mcl::RectangularPatchList::getListOfBoundaryLines() const
{
    auto matrix = getOccupationMatrix();
    auto ni = xedges.size() - 1;
    auto nj = yedges.size() - 1;
    auto lines = Array<Line<float>>();

    for (int i = 0; i < ni; ++i)
    {
        for (int j = 0; j < nj; ++j)
        {
            if (matrix.getUnchecked (nj * i + j))
            {
                bool L = i == 0      || ! matrix.getUnchecked (nj * (i - 1) + j);
                bool R = i == ni - 1 || ! matrix.getUnchecked (nj * (i + 1) + j);
                bool T = j == 0      || ! matrix.getUnchecked (nj * i + (j - 1));
                bool B = j == nj - 1 || ! matrix.getUnchecked (nj * i + (j + 1));

                if (L)
                {
                    auto p0 = Point<float> (xedges.getUnchecked (i), yedges.getUnchecked (j + 0));
                    auto p1 = Point<float> (xedges.getUnchecked (i), yedges.getUnchecked (j + 1));
                    lines.add (Line<float> (p0, p1));
                }
                if (R)
                {
                    auto p0 = Point<float> (xedges.getUnchecked (i + 1), yedges.getUnchecked (j + 0));
                    auto p1 = Point<float> (xedges.getUnchecked (i + 1), yedges.getUnchecked (j + 1));
                    lines.add (Line<float> (p0, p1));
                }
                if (T)
                {
                    auto p0 = Point<float> (xedges.getUnchecked (i + 0), yedges.getUnchecked (j));
                    auto p1 = Point<float> (xedges.getUnchecked (i + 1), yedges.getUnchecked (j));
                    lines.add (Line<float> (p0, p1));
                }
                if (B)
                {
                    auto p0 = Point<float> (xedges.getUnchecked (i + 0), yedges.getUnchecked (j + 1));
                    auto p1 = Point<float> (xedges.getUnchecked (i + 1), yedges.getUnchecked (j + 1));
                    lines.add (Line<float> (p0, p1));
                }
            }
        }
    }
    return lines;
}

Path mcl::RectangularPatchList::getOutlinePath (float cornerSize) const
{
    auto p = Path();
    auto lines = getListOfBoundaryLines();

    if (lines.isEmpty())
    {
        return p;
    }

    auto findOtherLineWithEndpoint = [&lines] (Line<float> ab)
    {
        for (const auto& line : lines)
        {
            if (! (line == ab || line == ab.reversed()))
            {
                // If the line starts on b, then return it.
                if (line.getStart() == ab.getEnd())
                {
                    return line;
                }
                // If the line ends on b, then reverse and return it.
                if (line.getEnd() == ab.getEnd())
                {
                    return line.reversed();
                }
            }
        }
        return Line<float>();
    };

    auto currentLine = lines.getFirst();
    p.startNewSubPath (currentLine.withShortenedStart (cornerSize).getStart());

    do {
        auto nextLine = findOtherLineWithEndpoint (currentLine);

        p.lineTo (currentLine.withShortenedEnd (cornerSize).getEnd());
        p.quadraticTo (nextLine.getStart(), nextLine.withShortenedStart (cornerSize).getStart());

        currentLine = nextLine;
    } while (currentLine != lines.getFirst());

    p.closeSubPath();

    return p;
}

juce::Array<bool> mcl::RectangularPatchList::getOccupationMatrix() const
{
    auto matrix = Array<bool>();
    auto ni = xedges.size() - 1;
    auto nj = yedges.size() - 1;

    matrix.resize (ni * nj);

    for (int i = 0; i < ni; ++i)
    {
        for (int j = 0; j < nj; ++j)
        {
            matrix.setUnchecked (nj * i + j, isBinOccupied (i, j));
        }
    }
    return matrix;
}

Array<float> mcl::RectangularPatchList::getUniqueCoordinatesX (const Array<Rectangle<float>>& rectangles)
{
    Array<float> X;

    for (const auto& rect : rectangles)
    {
        X.addUsingDefaultSort (rect.getX());
        X.addUsingDefaultSort (rect.getRight());
    }
    return uniqueValuesOfSortedArray (X);
}

Array<float> mcl::RectangularPatchList::getUniqueCoordinatesY (const Array<Rectangle<float>>& rectangles)
{
    Array<float> Y;

    for (const auto& rect : rectangles)
    {
        Y.addUsingDefaultSort (rect.getY());
        Y.addUsingDefaultSort (rect.getBottom());
    }
    return uniqueValuesOfSortedArray (Y);
}

Array<float> mcl::RectangularPatchList::uniqueValuesOfSortedArray (const Array<float>& X)
{
    jassert (! X.isEmpty());
    Array<float> unique { X.getFirst() };

    for (const auto& x : X)
        if (x != unique.getLast())
            unique.add (x);

    return unique;
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
    changeCallback (Rectangle<float>());
}

void mcl::TextLayout::removeCharacter (int row, int col)
{
    const auto& line = lines[row];
    lines.set (row, line.substring (0, col) + line.substring (col + 1));
    changeCallback (Rectangle<float>());
}

void mcl::TextLayout::removeTextInRange (int row0, int col0, int row1, int col1)
{
    if (row0 == row1)
    {
        const auto& line = lines[row0];
        lines.set (row0, line.substring (0, col0) + line.substring (col1));
        changeCallback (Rectangle<float>());
    }
    else
    {
        const auto& line0 = lines[row0];
        const auto& line1 = lines[row1];

        lines.set (row0, line0.substring (0, col0));
        lines.set (row1, line1.substring (col1));
        lines.removeRange (row0 + 1, row1 - row0 - 1);
    }
}

void mcl::TextLayout::removeTextInRange (juce::Line<int> properRange)
{
    removeTextInRange (properRange.getStartX(),
                       properRange.getStartY(),
                       properRange.getEndX(),
                       properRange.getEndY());
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

    if (position.x < 0.f)
    {
        col = 0;
    }
    else
    {
        for (int n = 0; n < glyphs.getNumGlyphs(); ++n)
        {
            if (glyphs.getBoundingBox (n, 1, true).getHorizontalRange().contains (position.x))
            {
                col = n;
                break;
            }
        }
    }
    return Point<int> (row, col);
}

Rectangle<float> mcl::TextLayout::getGlyphBounds (int row, int col) const
{
    return getGlyphBounds (row, Range<int> (col, col + 1));
}

juce::Rectangle<float> mcl::TextLayout::getGlyphBounds (int row, juce::Range<int> columnRange) const
{
    auto line = lines[row] + " ";
    GlyphArrangement glyphs;
    glyphs.addLineOfText (font, line, 0.f, getVerticalRangeForRow (row).getEnd());

    auto start = jlimit (0, line.length() - 1, columnRange.getStart());
    auto box = glyphs.getBoundingBox (start, columnRange.getLength(), true);

    // box.setVerticalRange (getVerticalRangeForRow (row));
    return box;
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
        glyphs.addGlyphArrangement (getGlyphsForRow (n));
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

float mcl::TextLayout::getHeight() const
{
    return getVerticalRangeForRow (getNumRows() - 1).getEnd();
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




//==============================================================================
mcl::TextEditor::TextEditor() : selection (layout)
{
    layout.setFont (Font ("Monaco", 12, 0));
    layout.setChangeCallback ([this] (Rectangle<float> area)
    {
        if (area.isEmpty())
            repaint();
        else
            repaint (area.transformedBy (transform).getSmallestIntegerContainer());
    });

    translation.x = 10.f; // left indent
    updateViewTransform();

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
    updateViewTransform();
}

void mcl::TextEditor::scaleView (float scaleFactor)
{
    viewScaleFactor *= scaleFactor;
    updateViewTransform();
}

void mcl::TextEditor::updateViewTransform()
{
    transform = AffineTransform::translation (translation.x, translation.y).scaled (viewScaleFactor);
    selection.setViewTransform (transform);
    repaint();
}

void mcl::TextEditor::paintRowsInAlternatingColors (juce::Graphics& g)
{
    g.saveState();
    g.addTransform (transform);

    for (int n = 0; n < layout.getNumRows(); ++n)
    {
        auto rect = Rectangle<float>();

        rect.setHorizontalRange (Range<float> (0, getWidth()));
        rect.setVerticalRange (layout.getVerticalRangeForRow(n));

        g.setColour (n % 2 == 0 ? Colours::lightblue.withAlpha (0.2f) : Colours::transparentBlack);
        g.fillRect (rect);
    }
    g.restoreState();
}

void mcl::TextEditor::paint (Graphics& g)
{
    g.fillAll (findColour (juce::TextEditor::backgroundColourId));
    paintRowsInAlternatingColors (g);
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
    scaleView (scaleFactor);
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
