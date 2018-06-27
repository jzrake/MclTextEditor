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




#define GUTTER_WIDTH 48.f
#define CURSOR_WIDTH 3.f
#define TEST_MULTI_CARET_EDITING true
#define TEST_SYNTAX_SUPPORT true
#define ENABLE_CARET_BLINK false
#define PROFILE_PAINTS false




//==============================================================================
mcl::CaretComponent::CaretComponent (const TextDocument& document) : document (document)
{
    setInterceptsMouseClicks (false, false);
#if ENABLE_CARET_BLINK
    startTimerHz (20);
#endif
}

void mcl::CaretComponent::setViewTransform (const AffineTransform& transformToUse)
{
    transform = transformToUse;
    repaint();
}

void mcl::CaretComponent::updateSelections()
{
    phase = 0.f;
    repaint();
}

void mcl::CaretComponent::paint (Graphics& g)
{
#if PROFILE_PAINTS
    auto start = Time::getMillisecondCounterHiRes();
#endif

    g.setColour (getParentComponent()->findColour (juce::CaretComponent::caretColourId).withAlpha (squareWave (phase)));

    for (const auto &r : getCaretRectangles())
        g.fillRect (r);

#if PROFILE_PAINTS
    std::cout << "[CaretComponent::paint] " << Time::getMillisecondCounterHiRes() - start << std::endl;
#endif
}

float mcl::CaretComponent::squareWave (float wt) const
{
    const float delta = 0.222f;
    const float A = 1.0;
    return 0.5f + A / 3.14159f * std::atanf (std::cosf (wt) / delta);
}

void mcl::CaretComponent::timerCallback()
{
    phase += 3.2e-1;

    for (const auto &r : getCaretRectangles())
        repaint (r.getSmallestIntegerContainer());
}

Array<Rectangle<float>> mcl::CaretComponent::getCaretRectangles() const
{
    Array<Rectangle<float>> rectangles;

    for (const auto& selection : document.getSelections())
    {
        rectangles.add (document
                        .getGlyphBounds (selection.head)
                        .removeFromLeft (CURSOR_WIDTH)
                        .translated (selection.head.y == 0 ? 0 : -0.5f * CURSOR_WIDTH, 0.f)
                        .transformedBy (transform)
                        .expanded (0.f, 1.f));
    }
    return rectangles;
}




//==========================================================================
mcl::GutterComponent::GutterComponent (const TextDocument& document) : document (document)
{
    setInterceptsMouseClicks (false, false);
}

void mcl::GutterComponent::setViewTransform (const AffineTransform& transformToUse)
{
    transform = transformToUse;
    repaint();
}

void mcl::GutterComponent::updateSelections()
{
    repaint();
}

void mcl::GutterComponent::paint (Graphics& g)
{
#if PROFILE_PAINTS
    auto start = Time::getMillisecondCounterHiRes();
#endif

    /*
     Draw the gutter background, shadow, and outline
     ------------------------------------------------------------------
     */
    auto bg = getParentComponent()->findColour (CodeEditorComponent::backgroundColourId);
    auto ln = bg.overlaidWith (getParentComponent()->findColour (CodeEditorComponent::lineNumberBackgroundId));

    g.setColour (ln);
    g.fillRect (getLocalBounds().removeFromLeft (GUTTER_WIDTH));

    if (transform.getTranslationX() < GUTTER_WIDTH)
    {
        auto shadowRect = getLocalBounds().withLeft (GUTTER_WIDTH).withWidth (12);
        auto gradient = ColourGradient::horizontal (ln.contrasting().withAlpha (0.3f),
                                                    Colours::transparentBlack, shadowRect);
        g.setFillType (gradient);
        g.fillRect (shadowRect);
    }
    else
    {
        g.setColour (ln.darker (0.2f));
        g.drawVerticalLine (GUTTER_WIDTH - 1.f, 0.f, getHeight());
    }

    /*
     Draw the line numbers and selected rows
     ------------------------------------------------------------------
     */
    auto area = g.getClipBounds().toFloat().transformedBy (transform.inverted());
    auto rowData = document.findRowsIntersecting (area);
    auto verticalTransform = transform.withAbsoluteTranslation (0.f, transform.getTranslationY());

    for (const auto& r : rowData)
    {
        auto A = r.bounds
            .transformedBy (transform)
            .withX (0)
            .withWidth (GUTTER_WIDTH);

        if (r.isRowSelected)
        {
            g.setColour (ln.contrasting (0.1f));
            g.fillRect (A);
        }

        g.setColour (getParentComponent()->findColour (CodeEditorComponent::lineNumberTextId));
        getLineNumberGlyphs (r.rowNumber, true).draw (g, verticalTransform);
    }

#if PROFILE_PAINTS
    std::cout << "[GutterComponent::paint] " << Time::getMillisecondCounterHiRes() - start << std::endl;
#endif
}

void mcl::GutterComponent::cacheLineNumberGlyphs (int cacheSize)
{
    /*
     Larger cache sizes than ~1000 slows component loading. The proper way to
     do this is to write a GlyphArrangementMemoizer class. Soon enough.
     */
    lineNumberGlyphsCache.clear();

    for (int n = 0; n < cacheSize; ++n)
    {
        lineNumberGlyphsCache.add (getLineNumberGlyphs (n, false));
    }
}

GlyphArrangement mcl::GutterComponent::getLineNumberGlyphs (int row, bool useCached) const
{
    if (useCached && row < lineNumberGlyphsCache.size())
    {
        return lineNumberGlyphsCache.getReference (row);
    }

    GlyphArrangement glyphs;
    glyphs.addLineOfText (document.getFont().withHeight (12.f),
                          String (row + 1),
                          8.f, document.getVerticalPosition (row, TextDocument::Metric::baseline));
    return glyphs;
}




//==========================================================================
mcl::HighlightComponent::HighlightComponent (const TextDocument& document) : document (document)
{
    setInterceptsMouseClicks (false, false);
}

void mcl::HighlightComponent::setViewTransform (const AffineTransform& transformToUse)
{
    transform = transformToUse;

    outlinePath.clear();
    auto clip = getLocalBounds().toFloat().transformedBy (transform.inverted());
    
    for (const auto& s : document.getSelections())
    {
        outlinePath.addPath (getOutlinePath (document.getSelectionRegion (s, clip)));
    }
    repaint (outlinePath.getBounds().getSmallestIntegerContainer());
}

void mcl::HighlightComponent::updateSelections()
{
    outlinePath.clear();
    auto clip = getLocalBounds().toFloat().transformedBy (transform.inverted());

    for (const auto& s : document.getSelections())
    {
        outlinePath.addPath (getOutlinePath (document.getSelectionRegion (s, clip)));
    }
    repaint (outlinePath.getBounds().getSmallestIntegerContainer());
}

void mcl::HighlightComponent::paint (Graphics& g)
{
#if PROFILE_PAINTS
    auto start = Time::getMillisecondCounterHiRes();
#endif

    g.addTransform (transform);
    auto highlight = getParentComponent()->findColour (CodeEditorComponent::highlightColourId);

    g.setColour (highlight);
    g.fillPath (outlinePath);

    g.setColour (highlight.darker());
    g.strokePath (outlinePath, PathStrokeType (1.f));

#if PROFILE_PAINTS
    std::cout << "[HighlightComponent::paint] " << Time::getMillisecondCounterHiRes() - start << std::endl;
#endif
}

Path mcl::HighlightComponent::getOutlinePath (const Array<Rectangle<float>>& rectangles)
{
    auto p = Path();
    auto rect = rectangles.begin();

    if (rect == rectangles.end())
        return p;

    p.startNewSubPath (rect->getTopLeft());
    p.lineTo (rect->getBottomLeft());

    while (++rect != rectangles.end())
    {
        p.lineTo (rect->getTopLeft());
        p.lineTo (rect->getBottomLeft());
    }

    while (rect-- != rectangles.begin())
    {
        p.lineTo (rect->getBottomRight());
        p.lineTo (rect->getTopRight());
    }

    p.closeSubPath();
    return p.createPathWithRoundedCorners (4.f);
}




//==============================================================================
mcl::Selection::Selection (const String& content)
{
    int rowSpan = 0;
    int n = 0, lastLineStart = 0;
    auto c = content.getCharPointer();

    while (*c != '\0')
    {
        if (*c == '\n')
        {
            ++rowSpan;
            lastLineStart = n + 1;
        }
        ++c; ++n;
    }

    head = { 0, 0 };
    tail = { rowSpan, content.length() - lastLineStart };
}

bool mcl::Selection::isOriented() const
{
    return ! (head.x > tail.x || (head.x == tail.x && head.y > tail.y));
}

mcl::Selection mcl::Selection::oriented() const
{
    if (! isOriented())
        return swapped();

    return *this;
}

mcl::Selection mcl::Selection::swapped() const
{
    Selection s = *this;
    std::swap (s.head, s.tail);
    return s;
}

mcl::Selection mcl::Selection::horizontallyMaximized (const TextDocument& document) const
{
    Selection s = *this;

    if (isOriented())
    {
        s.head.y = 0;
        s.tail.y = document.getNumColumns (s.tail.x);
    }
    else
    {
        s.head.y = document.getNumColumns (s.head.x);
        s.tail.y = 0;
    }
    return s;
}

mcl::Selection mcl::Selection::measuring (const String& content) const
{
    Selection s (content);

    if (isOriented())
    {
        return Selection (content).startingFrom (head);
    }
    else
    {
        return Selection (content).startingFrom (tail).swapped();
    }
}

mcl::Selection mcl::Selection::startingFrom (Point<int> index) const
{
    Selection s = *this;

    /*
     Pull the whole selection back to the origin.
     */
    s.pullBy (Selection ({}, isOriented() ? head : tail));

    /*
     Then push it forward to the given index.
     */
    s.pushBy (Selection ({}, index));

    return s;
}

void mcl::Selection::pullBy (Selection disappearingSelection)
{
    disappearingSelection.pull (head);
    disappearingSelection.pull (tail);
}

void mcl::Selection::pushBy (Selection appearingSelection)
{
    appearingSelection.push (head);
    appearingSelection.push (tail);
}

void mcl::Selection::pull (Point<int>& index) const
{
    const auto S = oriented();

    /*
     If the selection tail is on index's row, then shift its column back,
     either by the difference between our head and tail column indexes if
     our head and tail are on the same row, or otherwise by our tail's
     column index.
     */
    if (S.tail.x == index.x && S.head.y <= index.y)
    {
        if (S.head.x == S.tail.x)
        {
            index.y -= S.tail.y - S.head.y;
        }
        else
        {
            index.y -= S.tail.y;
        }
    }

    /*
     If this selection starts on the same row or an earlier one,
     then shift the row index back by our row span.
     */
    if (S.head.x <= index.x)
    {
        index.x -= S.tail.x - S.head.x;
    }
}

void mcl::Selection::push (Point<int>& index) const
{
    const auto S = oriented();

    /*
     If our head is on index's row, then shift its column forward, either
     by our head to tail distance if our head and tail are on the
     same row, or otherwise by our tail's column index.
     */
    if (S.head.x == index.x && S.head.y <= index.y)
    {
        if (S.head.x == S.tail.x)
        {
            index.y += S.tail.y - S.head.y;
        }
        else
        {
            index.y += S.tail.y;
        }
    }

    /*
     If this selection starts on the same row or an earlier one,
     then shift the row index forward by our row span.
     */
    if (S.head.x <= index.x)
    {
        index.x += S.tail.x - S.head.x;
    }
}

SparseSet<int> mcl::Selection::createSparseSetOnRow (int row, int numColumns, const Array<Selection>& selections)
{
    SparseSet<int> set;

    for (const auto& selection : selections)
    {
        set.addRange (selection.getColumnRangeOnRow (row, numColumns));
    }
    return set;
}




//==============================================================================
const String& mcl::GlyphArrangementArray::operator[] (int index) const
{
    if (isPositiveAndBelow (index, lines.size()))
    {
        return lines.getReference (index).string;
    }

    static String empty;
    return empty;
}

GlyphArrangement mcl::GlyphArrangementArray::getGlyphs (int index,
                                                        Font font,
                                                        float baseline,
                                                        SparseSet<int> columns,
                                                        bool withTrailingSpace) const
{
    static Entry empty;
    auto& entry = isPositiveAndBelow (index, lines.size()) ? lines.getReference (index) : empty;

    if (entry.dirty)
    {
        entry.glyphs.clear();
        entry.glyphsWithTrailingSpace.clear();
        entry.glyphs.addLineOfText (font, entry.string, 0.f, 0.f);
        entry.glyphsWithTrailingSpace.addLineOfText (font, entry.string + " ", 0.f, 0.f);
        entry.dirty = false;
    }

    auto& glyphSource = withTrailingSpace ? entry.glyphsWithTrailingSpace : entry.glyphs;
    auto glyphs = GlyphArrangement();

    for (int n = 0; n < glyphSource.getNumGlyphs(); ++n)
    {
        if (columns.contains (n))
        {
            auto glyph = glyphSource.getGlyph (n);
            glyph.moveBy (0.f, baseline);
            glyphs.addGlyph (glyph);
        }
    }
    return glyphs;
}

void mcl::GlyphArrangementArray::invalidateAll()
{
    for (auto& entry : lines)
    {
        entry.dirty = true;
    }
}




//==============================================================================
void mcl::TextDocument::replaceAll (const String& content)
{
    lines.clear();

    for (const auto& line : StringArray::fromLines (content))
    {
        lines.add (line);
    }
}

int mcl::TextDocument::getNumRows() const
{
    return lines.size();
}

int mcl::TextDocument::getNumColumns (int row) const
{
    return lines[row].length();
}

float mcl::TextDocument::getVerticalPosition (int row, Metric metric) const
{
    float lineHeight = font.getHeight() * lineSpacing;
    float gap = font.getHeight() * (lineSpacing - 1.f) * 0.5f;

    switch (metric)
    {
        case Metric::top     : return lineHeight * row;
        case Metric::ascent  : return lineHeight * row + gap;
        case Metric::baseline: return lineHeight * row + gap + font.getAscent();
        case Metric::descent : return lineHeight * row + gap + font.getAscent() + font.getDescent();
        case Metric::bottom  : return lineHeight * row + lineHeight;
    }
}

Point<float> mcl::TextDocument::getPosition (Point<int> index, Metric metric) const
{
    return Point<float> (getGlyphBounds (index).getX(), getVerticalPosition (index.x, metric));
}

Array<Rectangle<float>> mcl::TextDocument::getSelectionRegion (Selection selection, Rectangle<float> clip) const
{
    Array<Rectangle<float>> patches;
    Selection s = selection.oriented();

    if (s.head.x == s.tail.x)
    {
        int c0 = s.head.y;
        int c1 = s.tail.y;
        patches.add (getBoundsOnRow (s.head.x, Range<int> (c0, c1)));
    }
    else
    {
        int r0 = s.head.x;
        int c0 = s.head.y;
        int r1 = s.tail.x;
        int c1 = s.tail.y;

        for (int n = r0; n <= r1; ++n)
        {
            if (! clip.isEmpty() &&
                ! clip.getVerticalRange().intersects (
            {
                getVerticalPosition (n, Metric::top),
                getVerticalPosition (n, Metric::bottom)
            })) continue;

            if      (n == r0) patches.add (getBoundsOnRow (r0, Range<int> (c0, getNumColumns (r0) + 1)));
            else if (n == r1) patches.add (getBoundsOnRow (r1, Range<int> (0, c1)));
            else              patches.add (getBoundsOnRow (n,  Range<int> (0, getNumColumns (n) + 1)));
        }
    }
    return patches;
}

Rectangle<float> mcl::TextDocument::getBounds() const
{
    if (cachedBounds.isEmpty())
    {
        auto bounds = Rectangle<float>();

        for (int n = 0; n < getNumRows(); ++n)
        {
            bounds = bounds.getUnion (getBoundsOnRow (n, Range<int> (0, getNumColumns (n))));
        }
        return cachedBounds = bounds;
    }
    return cachedBounds;
}

Rectangle<float> mcl::TextDocument::getBoundsOnRow (int row, Range<int> columns) const
{
    return getGlyphsForRow (row, -1, true)
        .getBoundingBox    (columns.getStart(), columns.getLength(), true)
        .withTop           (getVerticalPosition (row, Metric::top))
        .withBottom        (getVerticalPosition (row, Metric::bottom));
}

Rectangle<float> mcl::TextDocument::getGlyphBounds (Point<int> index) const
{
    index.y = jlimit (0, getNumColumns (index.x), index.y);
    return getBoundsOnRow (index.x, Range<int> (index.y, index.y + 1));
}

GlyphArrangement mcl::TextDocument::getGlyphsForRow (int row, int style, bool withTrailingSpace) const
{
    auto columns = SparseSet<int>();
    auto ncol = getNumColumns (row) + withTrailingSpace;

    if (style == -1) // don't filter on style
    {
        columns.addRange ({ 0, ncol });
    }
    else if (style == 0) // get glyphs not in a style zone
    {
        columns = Selection::createSparseSetOnRow (row, ncol, findStyleZonesIntersecting (row, -1));
        columns.invertRange ({0, ncol});
    }
    else // get glyphs in the given style zone
    {
        columns = Selection::createSparseSetOnRow (row, ncol, findStyleZonesIntersecting (row, style));
    }

    return lines.getGlyphs (row,
                            font,
                            getVerticalPosition (row, Metric::baseline),
                            columns, withTrailingSpace);
}

GlyphArrangement mcl::TextDocument::findGlyphsIntersecting (Rectangle<float> area, int style) const
{
    auto lineHeight = font.getHeight() * lineSpacing;
    auto row0 = jlimit (0, jmax (getNumRows() - 1, 0), int (area.getY() / lineHeight));
    auto row1 = jlimit (0, jmax (getNumRows() - 1, 0), int (area.getBottom() / lineHeight));
    auto glyphs = GlyphArrangement();

    for (int n = row0; n <= row1; ++n)
    {
        glyphs.addGlyphArrangement (getGlyphsForRow (n, style));
    }
    return glyphs;
}

Array<mcl::TextDocument::RowData> mcl::TextDocument::findRowsIntersecting (Rectangle<float> area,
                                                                           bool computeHorizontalExtent) const
{
    auto lineHeight = font.getHeight() * lineSpacing;
    auto row0 = jlimit (0, jmax (getNumRows() - 1, 0), int (area.getY() / lineHeight));
    auto row1 = jlimit (0, jmax (getNumRows() - 1, 0), int (area.getBottom() / lineHeight));
    auto rows = Array<RowData>();

    for (int n = row0; n <= row1; ++n)
    {
        RowData data;
        data.rowNumber = n;

        if (computeHorizontalExtent) // slower
        {
            data.bounds = getBoundsOnRow (n, Range<int> (0, getNumColumns (n)));
        }
        else // faster
        {
            data.bounds.setY      (getVerticalPosition (n, Metric::top));
            data.bounds.setBottom (getVerticalPosition (n, Metric::bottom));
        }

        for (const auto& s : selections)
        {
            if (s.intersectsRow (n))
            {
                data.isRowSelected = true;
                break;
            }
        }
        rows.add (data);
    }
    return rows;
}

juce::Array<mcl::Selection> mcl::TextDocument::findStyleZonesIntersecting (int row, int style) const
{
    juce::Array<mcl::Selection> zones;

    for (const auto& s : styleZones)
    {
        if ((style == -1 || s.style == style) && s.intersectsRow (row))
        {
            zones.add (s);
        }
    }
    return zones;
}

Point<int> mcl::TextDocument::findIndexNearestPosition (Point<float> position) const
{
    auto lineHeight = font.getHeight() * lineSpacing;
    auto row = jlimit (0, jmax (getNumRows() - 1, 0), int (position.y / lineHeight));
    auto col = 0;
    auto glyphs = getGlyphsForRow (row);

    if (position.x > 0.f)
    {
        col = glyphs.getNumGlyphs();

        for (int n = 0; n < glyphs.getNumGlyphs(); ++n)
        {
            if (glyphs.getBoundingBox (n, 1, true).getHorizontalRange().contains (position.x))
            {
                col = n;
                break;
            }
        }
    }
    return { row, col };
}

Point<int> mcl::TextDocument::getEnd() const
{
    return { getNumRows(), 0 };
}

bool mcl::TextDocument::next (Point<int>& index) const
{
    if (index.y < getNumColumns (index.x))
    {
        index.y += 1;
        return true;
    }
    else if (index.x < getNumRows())
    {
        index.x += 1;
        index.y = 0;
        return true;
    }
    return false;
}

bool mcl::TextDocument::prev (Point<int>& index) const
{
    if (index.y > 0)
    {
        index.y -= 1;
        return true;
    }
    else if (index.x > 0)
    {
        index.x -= 1;
        index.y = getNumColumns (index.x);
        return true;
    }
    return false;
}

bool mcl::TextDocument::nextRow (Point<int>& index) const
{
    if (index.x < getNumRows())
    {
        index.x += 1;
        index.y = jmin (index.y, getNumColumns (index.x));
        return true;
    }
    return false;
}

bool mcl::TextDocument::prevRow (Point<int>& index) const
{
    if (index.x > 0)
    {
        index.x -= 1;
        index.y = jmin (index.y, getNumColumns (index.x));
        return true;
    }
    return false;
}

bool mcl::TextDocument::nextWord (Point<int>& index) const
{
    if (index == getEnd())
    {
        return false;
    }

    if (CharacterFunctions::isWhitespace (getCharacter (index)))
        while (next (index) && CharacterFunctions::isWhitespace (getCharacter (index))) {}

    while (next (index))
        if (CharacterFunctions::isWhitespace (getCharacter (index)))
            return true;

    return false;
}

bool mcl::TextDocument::prevWord (Point<int>& index) const
{
    if (! prev (index))
        return false;

    if (CharacterFunctions::isWhitespace (getCharacter (index)))
        while (prev (index) && CharacterFunctions::isWhitespace (getCharacter (index))) {}

    while (prev (index))
    {
        if (CharacterFunctions::isWhitespace (getCharacter (index)))
        {
            next (index);
            return true;
        }
    }
    return false;
}

juce_wchar mcl::TextDocument::getCharacter (Point<int> index) const
{
    jassert (0 <= index.x && index.x <= lines.size());
    jassert (0 <= index.y && index.y <= lines[index.x].length());

    if (index == getEnd() || index.y == lines[index.x].length())
    {
        return '\n';
    }
    return  lines[index.x].getCharPointer()[index.y];
}

mcl::Selection mcl::TextDocument::getSelection (int index, Navigation navigation, bool fixingTail) const
{
    auto s = selections[index];

    auto post = [this, fixingTail] (auto& T)
    {
        if (T.head == getEnd())
        {
            prev (T.head);
        }
        if (! fixingTail)
        {
            T.tail = T.head;
        }
        return T;
    };

    switch (navigation)
    {
        case Navigation::identity: return s;
        case Navigation::wholeDocument : { s.head = { 0, 0 }; s.tail = getEnd(); } return post (s);
        case Navigation::wholeLine     : { s.head.y = 0; s.tail.y = getNumColumns (s.tail.x); } return post (s);
        case Navigation::wholeWord     : { prevWord (s.head); nextWord (s.tail); } return post (s);
        case Navigation::forwardByChar : next (s.head); return post (s);
        case Navigation::backwardByChar: prev (s.head); return post (s);
        case Navigation::forwardByWord : nextWord (s.head); return post (s);
        case Navigation::backwardByWord: prevWord (s.head); return post (s);
        case Navigation::forwardByLine : nextRow (s.head); return post (s);
        case Navigation::backwardByLine: prevRow (s.head); return post (s);
        case Navigation::toLineStart   : s.head.y = 0; return post (s);
        case Navigation::toLineEnd     : s.head.y = getNumColumns (s.head.x); return post (s);
    }
}

Array<mcl::Selection> mcl::TextDocument::getSelections (Navigation navigation, bool fixingTail) const
{
    auto resultingSelections = Array<Selection>();
    resultingSelections.ensureStorageAllocated (getNumSelections());

    for (int n = 0; n < getNumSelections(); ++n)
    {
        resultingSelections.add (getSelection (n, navigation, fixingTail));
    }
    return resultingSelections;
}

String mcl::TextDocument::getSelectionContent (Selection s) const
{
    s = s.oriented();

    if (s.isSingleLine())
    {
        return lines[s.head.x].substring (s.head.y, s.tail.y);
    }
    else
    {
        String content = lines[s.head.x].substring (s.head.y) + "\n";

        for (int row = s.head.x + 1; row < s.tail.x; ++row)
        {
            content += lines[row] + "\n";
        }
        content += lines[s.tail.x].substring (0, s.tail.y);
        return content;
    }
}

mcl::Transaction mcl::TextDocument::fulfill (const Transaction& transaction)
{
    cachedBounds = {}; // invalidate the bounds

    const auto t = transaction.accountingForSpecialCharacters (*this);
    const auto s = t.selection.oriented();
    const auto L = getSelectionContent (s.horizontallyMaximized (*this));
    const auto i = s.head.y;
    const auto j = L.lastIndexOf ("\n") + s.tail.y + 1;
    const auto M = L.substring (0, i) + t.content + L.substring (j);

    for (auto& existingSelection : selections)
    {
        existingSelection.pullBy (s);
        existingSelection.pushBy (Selection (t.content).startingFrom (s.head));
    }

    lines.removeRange (s.head.x, s.tail.x - s.head.x + 1);
    int row = s.head.x;

    if (M.isEmpty())
    {
        lines.insert (row++, String());
    }
    for (const auto& line : StringArray::fromLines (M))
    {
        lines.insert (row++, line);
    }

    using D = Transaction::Direction;
    auto inf = std::numeric_limits<float>::max();

    Transaction r;
    r.selection = Selection (t.content).startingFrom (s.head);
    r.content = L.substring (i, j);
    r.affectedArea = Rectangle<float> (0, 0, inf, inf);
    r.direction = t.direction == D::forward ? D::reverse : D::forward;

    return r;
}




//==============================================================================
class mcl::Transaction::Undoable : public UndoableAction
{
public:
    Undoable (TextDocument& document, Callback callback, Transaction forward)
    : document (document)
    , callback (callback)
    , forward (forward) {}

    bool perform() override
    {
        callback (reverse = document.fulfill (forward));
        return true;
    }

    bool undo() override
    {
        callback (forward = document.fulfill (reverse));
        return true;
    }

    TextDocument& document;
    Callback callback;
    Transaction forward;
    Transaction reverse;
};




//==============================================================================
mcl::Transaction mcl::Transaction::accountingForSpecialCharacters (const TextDocument& document) const
{
    Transaction t = *this;
    auto& s = t.selection;

    if (content.getLastCharacter() == KeyPress::tabKey)
    {
        t.content = "    ";
    }
    if (content.getLastCharacter() == KeyPress::backspaceKey)
    {
        if (s.head.y == s.tail.y)
        {
            document.prev (s.head);
        }
        t.content.clear();
    }
    else if (content.getLastCharacter() == KeyPress::deleteKey)
    {
        if (s.head.y == s.tail.y)
        {
            document.next (s.head);
        }
        t.content.clear();
    }
    return t;
}

UndoableAction* mcl::Transaction::on (TextDocument& document, Callback callback)
{
    return new Undoable (document, callback, *this);
}




//==============================================================================
mcl::TextEditor::TextEditor()
: caret (document)
, gutter (document)
, highlight (document)
{
    lastTransactionTime = Time::getApproximateMillisecondCounter();
    document.setSelections ({ Selection() });

    setFont (Font (Font::getDefaultMonospacedFontName(), 16, 0));

    translateView (GUTTER_WIDTH, 0);
    setWantsKeyboardFocus (true);

    addAndMakeVisible (highlight);
    addAndMakeVisible (caret);
    addAndMakeVisible (gutter);
}

mcl::TextEditor::~TextEditor()
{
}

void mcl::TextEditor::setFont (Font font)
{
    document.setFont (font);
    gutter.cacheLineNumberGlyphs();
    repaint();
}

void mcl::TextEditor::setText (const String& text)
{
    document.replaceAll (text);
    repaint();
}

void mcl::TextEditor::translateView (float dx, float dy)
{
    auto W = viewScaleFactor * document.getBounds().getWidth();
    auto H = viewScaleFactor * document.getBounds().getHeight();

    translation.x = jlimit (jmin (GUTTER_WIDTH, -W + getWidth()), GUTTER_WIDTH, translation.x + dx);
    translation.y = jlimit (jmin (-0.f, -H + getHeight()), 0.0f, translation.y + dy);

    updateViewTransform();
}

void mcl::TextEditor::scaleView (float scaleFactor)
{
    viewScaleFactor *= scaleFactor;
    updateViewTransform();
}

void mcl::TextEditor::updateViewTransform()
{
    transform = AffineTransform::scale (viewScaleFactor).translated (translation.x, translation.y);
    highlight.setViewTransform (transform);
    caret.setViewTransform (transform);
    gutter.setViewTransform (transform);
    repaint();
}

void mcl::TextEditor::updateSelections()
{
    highlight.updateSelections();
    caret.updateSelections();
    gutter.updateSelections();
}

void mcl::TextEditor::translateToEnsureCaretIsVisible()
{
    auto i = document.getSelections().getFirst().head;
    auto t = Point<float> (0.f, document.getVerticalPosition (i.x, TextDocument::Metric::top))   .transformedBy (transform);
    auto b = Point<float> (0.f, document.getVerticalPosition (i.x, TextDocument::Metric::bottom)).transformedBy (transform);

    if (t.y < 0.f)
    {
        translateView (0.f, -t.y);
    }
    else if (b.y > getHeight())
    {
        translateView (0.f, -b.y + getHeight());
    }
}

void mcl::TextEditor::resized()
{
    highlight.setBounds (getLocalBounds());
    caret.setBounds (getLocalBounds());
    gutter.setBounds (getLocalBounds());
    resetProfilingData();
}

void mcl::TextEditor::paint (Graphics& g)
{
    auto start = Time::getMillisecondCounterHiRes();
    g.fillAll (findColour (CodeEditorComponent::backgroundColourId));

    String renderSchemeString;

    switch (renderScheme)
    {
        case RenderScheme::usingAttributedString:
            renderTextUsingAttributedString (g);
            renderSchemeString = "attr. str";
            break;
        case RenderScheme::usingGlyphArrangement:
            renderTextUsingGlyphArrangement (g);
            renderSchemeString = "glyph arr.";
            break;
    }

    lastTimeInPaint = Time::getMillisecondCounterHiRes() - start;
    accumulatedTimeInPaint += lastTimeInPaint;
    numPaintCalls += 1;

    if (drawProfilingInfo)
    {
        String info;
        info += "paint mode         : " + renderSchemeString + "\n";
        info += "syntax highlight   : " + String (enableSyntaxHighlighting ? "yes" : "no") + "\n";
        info += "mean render time   : " + String (accumulatedTimeInPaint / numPaintCalls) + " ms\n";
        info += "last render time   : " + String (lastTimeInPaint) + " ms\n";
        info += "tokeniser time     : " + String (lastTokeniserTime) + " ms\n";

        g.setColour (findColour (CodeEditorComponent::defaultTextColourId));
        g.setFont (Font ("Courier New", 10, 0));
        g.drawMultiLineText (info, getWidth() - 200, 10, 200);
    }

#if PROFILE_PAINTS
    std::cout << "[TextEditor::paint] " << lastTimeInPaint << std::endl;
#endif
}

void mcl::TextEditor::paintOverChildren (Graphics& g)
{
}

void mcl::TextEditor::mouseDown (const MouseEvent& e)
{
    if (e.getNumberOfClicks() > 1)
    {
        return;
    }
    else if (e.mods.isRightButtonDown())
    {
        PopupMenu menu;

        menu.addItem (1, "Render scheme: attr. str", true, renderScheme == RenderScheme::usingAttributedString, nullptr);
        menu.addItem (2, "Render scheme: glyph arr.", true, renderScheme == RenderScheme::usingGlyphArrangement, nullptr);
        menu.addItem (3, "Syntax highlighting", true, enableSyntaxHighlighting, nullptr);
        menu.addItem (4, "Draw profiling info", true, drawProfilingInfo, nullptr);

        switch (menu.show())
        {
            case 1: renderScheme = RenderScheme::usingAttributedString; break;
            case 2: renderScheme = RenderScheme::usingGlyphArrangement; break;
            case 3: enableSyntaxHighlighting = ! enableSyntaxHighlighting; break;
            case 4: drawProfilingInfo = ! drawProfilingInfo; break;
        }
        resetProfilingData();
        repaint();
        return;
    }

    auto selections = document.getSelections();
    auto index = document.findIndexNearestPosition (e.position.transformedBy (transform.inverted()));

    if (selections.contains (index))
    {
        return;
    }
    if (! e.mods.isCommandDown() || ! TEST_MULTI_CARET_EDITING)
    {
        selections.clear();
    }

    selections.add (index);
    document.setSelections (selections);
    updateSelections();
}

void mcl::TextEditor::mouseDrag (const MouseEvent& e)
{
    if (e.mouseWasDraggedSinceMouseDown())
    {
        auto selection = document.getSelections().getFirst();
        selection.head = document.findIndexNearestPosition (e.position.transformedBy (transform.inverted()));
        document.setSelections ({ selection });
        translateToEnsureCaretIsVisible();
        updateSelections();
    }
}

void mcl::TextEditor::mouseDoubleClick (const MouseEvent& e)
{
    if (e.getNumberOfClicks() == 2)
    {
        document.setSelections (document.getSelections (TextDocument::Navigation::wholeWord, true).getFirst());
    }
    else if (e.getNumberOfClicks() == 3)
    {
        document.setSelections (document.getSelections (TextDocument::Navigation::wholeLine, true).getFirst());
    }
    updateSelections();
}

void mcl::TextEditor::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& d)
{
    float dx = d.deltaX;
    /*
     make scrolling away from the gutter just a little "sticky"
     */
    if (translation.x == GUTTER_WIDTH && -0.01f < dx && dx < 0.f)
    {
        dx = 0.f;
    }
    translateView (dx * 600, d.deltaY * 600);
}

void mcl::TextEditor::mouseMagnify (const MouseEvent& e, float scaleFactor)
{
    scaleView (scaleFactor);
}

bool mcl::TextEditor::keyPressed (const KeyPress& key)
{
    using Navigation = TextDocument::Navigation;

    auto nav = [this] (TextDocument::Navigation navigation)
    {
        document.setSelections (document.getSelections (navigation));
        translateToEnsureCaretIsVisible();
        updateSelections();
        return true;
    };

    auto expand = [this] (TextDocument::Navigation navigation)
    {
        document.setSelections (document.getSelections (navigation, true));
        updateSelections();
        return true;
    };

    auto addCaret = [this] (TextDocument::Navigation navigation)
    {
        document.addSelection (document.getSelection (document.getNumSelections() - 1, navigation));
        updateSelections();
        return true;
    };

    if (key.isKeyCode (KeyPress::escapeKey))
    {
        document.setSelections (document.getSelections().getLast());
        updateSelections();
        return true;
    }

    if (key.getModifiers().isShiftDown() && key.getModifiers().isCtrlDown())
    {
        if (key.isKeyCode (KeyPress::upKey  )) return addCaret (Navigation::backwardByLine);
        if (key.isKeyCode (KeyPress::downKey)) return addCaret (Navigation::forwardByLine);
    }
    else if (key.getModifiers().isShiftDown())
    {
        if (key.isKeyCode (KeyPress::rightKey )) return expand (Navigation::forwardByChar);
        if (key.isKeyCode (KeyPress::leftKey  )) return expand (Navigation::backwardByChar);
        if (key.isKeyCode (KeyPress::downKey  )) return expand (Navigation::forwardByLine);
        if (key.isKeyCode (KeyPress::upKey    )) return expand (Navigation::backwardByLine);
    }
    else if (key.getModifiers().isAltDown())
    {
        if (key.isKeyCode (KeyPress::rightKey)) return nav (Navigation::forwardByWord);
        if (key.isKeyCode (KeyPress::leftKey )) return nav (Navigation::backwardByWord);
    }
    else
    {
        if (key.isKeyCode (KeyPress::rightKey)) return nav (Navigation::forwardByChar);
        if (key.isKeyCode (KeyPress::leftKey )) return nav (Navigation::backwardByChar);
        if (key.isKeyCode (KeyPress::downKey )) return nav (Navigation::forwardByLine);
        if (key.isKeyCode (KeyPress::upKey   )) return nav (Navigation::backwardByLine);
    }

    if (key == KeyPress ('a', ModifierKeys::ctrlModifier, 0)) return nav (Navigation::toLineStart);
    if (key == KeyPress ('e', ModifierKeys::ctrlModifier, 0)) return nav (Navigation::toLineEnd);
    if (key == KeyPress ('a', ModifierKeys::commandModifier, 0)) return expand (Navigation::wholeDocument);
    if (key == KeyPress ('l', ModifierKeys::commandModifier, 0)) return expand (Navigation::wholeLine);
    if (key == KeyPress ('z', ModifierKeys::commandModifier, 0)) return undo.undo();
    if (key == KeyPress ('r', ModifierKeys::commandModifier, 0)) return undo.redo();

    auto insert = [this] (String insertion)
    {
        double now = Time::getApproximateMillisecondCounter();

        if (now > lastTransactionTime + 400)
        {
            lastTransactionTime = Time::getApproximateMillisecondCounter();
            undo.beginNewTransaction();
        }

        for (int n = 0; n < document.getNumSelections(); ++n)
        {
            Transaction t;
            t.content = insertion;
            t.selection = document.getSelection (n);

            auto callback = [this, n] (const Transaction& r)
            {
                switch (r.direction) // NB: switching on the direction of the reciprocal here
                {
                    case Transaction::Direction::forward: document.setSelection (n, r.selection); break;
                    case Transaction::Direction::reverse: document.setSelection (n, r.selection.tail); break;
                }

                if (! r.affectedArea.isEmpty())
                {
                    repaint (r.affectedArea.transformedBy (transform).getSmallestIntegerContainer());
                }
            };
            undo.perform (t.on (document, callback));
        }
        updateSelections();
        return true;
    };

    bool isTab = tabKeyUsed && (key.getTextCharacter() == '\t');

    if (key == KeyPress ('x', ModifierKeys::commandModifier, 0))
    {
        SystemClipboard::copyTextToClipboard (document.getSelectionContent (document.getSelections().getFirst()));
        return insert ("");
    }
    if (key == KeyPress ('c', ModifierKeys::commandModifier, 0))
    {
        SystemClipboard::copyTextToClipboard (document.getSelectionContent (document.getSelections().getFirst()));
        return true;
    }

    if (key == KeyPress ('v', ModifierKeys::commandModifier, 0))   return insert (SystemClipboard::getTextFromClipboard());
    if (key == KeyPress ('d', ModifierKeys::ctrlModifier, 0))      return insert (String::charToString (KeyPress::deleteKey));
    if (key.isKeyCode (KeyPress::returnKey))                       return insert ("\n");
    if (key.getTextCharacter() >= ' ' || isTab)                    return insert (String::charToString (key.getTextCharacter()));

    return false;
}

MouseCursor mcl::TextEditor::getMouseCursor()
{
    return getMouseXYRelative().x < GUTTER_WIDTH ? MouseCursor::NormalCursor : MouseCursor::IBeamCursor;
}




//==============================================================================
void mcl::TextEditor::renderTextUsingAttributedString (juce::Graphics& g)
{
    /*
     Credit to chrisboy2000 for this
     */
    auto colourScheme = CPlusPlusCodeTokeniser().getDefaultColourScheme();
    auto originalHeight = document.getFont().getHeight();
    auto font = document.getFont().withHeight (originalHeight * transform.getScaleFactor());
    auto rows = document.findRowsIntersecting (g.getClipBounds().toFloat().transformedBy (transform.inverted()));

    for (const auto& r: rows)
    {
        auto line = document.getLine (r.rowNumber);
        auto bounds = document.getBoundsOnRow (r.rowNumber, {0, 1000}).transformedBy (transform);

        AttributedString s;

        if (! enableSyntaxHighlighting)
        {
            s.append (line, font);
        }
        else
        {
            auto start = Time::getMillisecondCounterHiRes();

            CppTokeniserFunctions::StringIterator si (line);
            auto previous = si.t;

            while (! si.isEOF())
            {
                auto tokenType = CppTokeniserFunctions::readNextToken (si);
                auto colour = colourScheme.types[tokenType].colour;
                auto token = String (previous, si.t);

                previous = si.t;
                s.append (token, font, colour);
            }
            lastTokeniserTime = Time::getMillisecondCounterHiRes() - start;
        }
        s.draw (g, bounds);
    }
}

void mcl::TextEditor::renderTextUsingGlyphArrangement (juce::Graphics& g)
{
    document.findGlyphsIntersecting (g.getClipBounds()
                                   .toFloat()
                                   .transformedBy (transform.inverted())
                                   ).draw (g, transform);
}

void mcl::TextEditor::resetProfilingData()
{
    accumulatedTimeInPaint = 0.f;
    numPaintCalls = 0;
}
