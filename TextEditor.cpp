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
#define TEST_MULTI_CARET_EDITING 0




//==============================================================================
mcl::CaretComponent::CaretComponent (const TextLayout& layout) : layout (layout)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (20);
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
    g.addTransform (transform);
    g.setColour (Colours::blue.withAlpha (squareWave (phase)));

    for (const auto& selection : layout.getSelections())
    {
        auto b = layout.getGlyphBounds (selection.head)
            .removeFromLeft (CURSOR_WIDTH)
            .translated (selection.head.y == 0 ? 0 : -0.5f * CURSOR_WIDTH, 0.f)
            .expanded (0.f, 1.f);
        g.fillRect (b);
    }
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
    repaint();
}




//==========================================================================
mcl::GutterComponent::GutterComponent (const TextLayout& layout) : layout (layout)
{
    setInterceptsMouseClicks (false, false);
}

void mcl::GutterComponent::setViewTransform (const juce::AffineTransform& transformToUse)
{
    transform = transformToUse;
    repaint();
}

void mcl::GutterComponent::updateSelections()
{
    repaint();
}

void mcl::GutterComponent::paint (juce::Graphics& g)
{
    // double start = Time::getMillisecondCounterHiRes();

    // DBG("A: " << Time::getMillisecondCounterHiRes() - start);

    /*
     Draw the gutter background, shadow, and outline
     ------------------------------------------------------------------
     */
    g.setColour (Colours::whitesmoke);
    g.fillRect (getLocalBounds().removeFromLeft (GUTTER_WIDTH));

    if (Point<float>().transformedBy (transform).getX() < GUTTER_WIDTH)
    {
        auto shadowRect = getLocalBounds().withLeft (GUTTER_WIDTH).withWidth (10);
        auto gradient = ColourGradient::horizontal (Colours::black.withAlpha (0.2f),
                                                    Colours::transparentBlack, shadowRect);
        g.setFillType (gradient);
        g.fillRect (shadowRect);
    }
    else
    {
        g.setColour (Colours::whitesmoke.darker (0.1f));
        g.drawVerticalLine (GUTTER_WIDTH - 1.f, 0.f, getHeight());
    }

    // DBG("B: " << Time::getMillisecondCounterHiRes() - start);


    /*
     Draw the line numbers and selected rows
     ------------------------------------------------------------------
     */
    auto area = g.getClipBounds().toFloat().transformedBy (transform.inverted());
    auto rowData = layout.findRowsIntersecting (area);
    auto verticalTransform = transform.withAbsoluteTranslation (0.f, transform.getTranslationY());

    for (const auto& r : rowData)
    {
        auto A = r.bounds
            .transformedBy (transform)
            .withX (0)
            .withWidth (GUTTER_WIDTH);

        if (r.isRowSelected)
        {
            g.setColour (Colours::whitesmoke.darker (0.1f));
            g.fillRect (A);
        }

        g.setColour (Colours::grey);
        getLineNumberGlyphs (r.rowNumber, true).draw (g, verticalTransform);
    }

    // DBG("C: " << Time::getMillisecondCounterHiRes() - start);
}

void mcl::GutterComponent::cacheLineNumberGlyphs (int cacheSize)
{
    /*
     Larger cache sizes than ~1000 slows component loading. The proper way to
     do this is to write a GlyphArrangementMemoizer class. Soon enough.
     */
    lineNumberGlyphsCache.clearQuick();

    for (int n = 0; n < cacheSize; ++n)
    {
        lineNumberGlyphsCache.add (getLineNumberGlyphs (n, false));
    }
}

juce::GlyphArrangement mcl::GutterComponent::getLineNumberGlyphs (int row, bool useCached) const
{
    if (useCached && row < lineNumberGlyphsCache.size())
    {
        return lineNumberGlyphsCache.getReference (row);
    }

    GlyphArrangement glyphs;
    glyphs.addLineOfText (layout.getFont().withHeight (12.f),
                          String (row + 1),
                          8.f, layout.getVerticalPosition (row, TextLayout::Metric::baseline));
    return glyphs;
}




//==========================================================================
mcl::HighlightComponent::HighlightComponent (const TextLayout& layout) : layout (layout)
{
    setInterceptsMouseClicks (false, false);
}

void mcl::HighlightComponent::setViewTransform (const juce::AffineTransform& transformToUse)
{
    transform = transformToUse;
    repaint();
}

void mcl::HighlightComponent::updateSelections()
{
    repaint();
}

void mcl::HighlightComponent::paint (juce::Graphics& g)
{
    g.addTransform (transform);
    auto clip = g.getClipBounds().toFloat();

    if (useRoundedHighlight)
    {
        auto region = layout.getSelectionRegion (layout.getSelections().getFirst(), clip);
        auto p = getOutlinePath (region);

        g.setColour (Colours::black.withAlpha (0.2f));
        g.fillPath (p);

        g.setColour (Colours::black.withAlpha (0.3f));
        g.strokePath (p, PathStrokeType (1.f));
    }
    else
    {
        g.setColour (Colours::black.withAlpha (0.2f));

        for (const auto& s : layout.getSelections())
        {
            for (const auto& patch : layout.getSelectionRegion (s, clip))
            {
                g.fillRect (patch);
            }
        }
    }
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
mcl::Selection::Selection (const juce::String& content)
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

mcl::Selection mcl::Selection::horizontallyMaximized (const TextLayout& layout) const
{
    Selection s = *this;

    if (isOriented())
    {
        s.head.y = 0;
        s.tail.y = layout.getNumColumns (s.tail.x);
    }
    else
    {
        s.head.y = layout.getNumColumns (s.head.x);
        s.tail.y = 0;
    }
    return s;
}

mcl::Selection mcl::Selection::measuring (const juce::String& content) const
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

mcl::Selection mcl::Selection::startingFrom (juce::Point<int> index) const
{
    Selection s = *this;

    /*
     Pull the whole selection back to the origin.
     */
    s.pulledBy (Selection ({}, isOriented() ? head : tail));

    /*
     Then push it forward to the given index.
     */
    s.pushedBy (Selection ({}, index));

    return s;
}

void mcl::Selection::pulledBy (Selection disappearingSelection)
{
    disappearingSelection.pull (head);
    disappearingSelection.pull (tail);
}

void mcl::Selection::pushedBy (Selection appearingSelection)
{
    appearingSelection.push (head);
    appearingSelection.push (tail);
}

void mcl::Selection::pull (juce::Point<int>& index) const
{
    const auto S = oriented();

    /*
     If the selection tail is on index's row, then shift its column back,
     either by the difference between our head and tail column indexes if
     our head and tail are on the same row, or otherwise by our tail's
     column index.
     */
    if (S.tail.x == index.x)
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

void mcl::Selection::push (juce::Point<int>& index) const
{
    const auto S = oriented();

    /*
     If our head is on index's row, then shift its column forward, either
     by our head to tail distance if our head and tail are on the
     same row, or otherwise by our tail's column index.
     */
    if (S.head.x == index.x)
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




//==============================================================================
void mcl::TextLayout::replaceAll (const String& content)
{
    lines.clear();

    for (const auto& line : StringArray::fromLines (content))
    {
        lines.add (line);
    }
}

int mcl::TextLayout::getNumRows() const
{
    return lines.size();
}

int mcl::TextLayout::getNumColumns (int row) const
{
    return lines[row].length();
}

float mcl::TextLayout::getVerticalPosition (int row, Metric metric) const
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

Point<float> mcl::TextLayout::getPosition (Point<int> index, Metric metric) const
{
    return Point<float> (getGlyphBounds (index).getX(), getVerticalPosition (index.x, metric));
}

Array<Rectangle<float>> mcl::TextLayout::getSelectionRegion (Selection selection, Rectangle<float> clip) const
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

juce::Rectangle<float> mcl::TextLayout::getBounds() const
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

Rectangle<float> mcl::TextLayout::getBoundsOnRow (int row, Range<int> columns) const
{
    return getGlyphsForRow (row, true)
        .getBoundingBox (columns.getStart(), columns.getLength(), true)
        .withTop    (getVerticalPosition (row, Metric::top))
        .withBottom (getVerticalPosition (row, Metric::bottom));
}

Rectangle<float> mcl::TextLayout::getGlyphBounds (Point<int> index) const
{
    index.y = jlimit (0, getNumColumns (index.x), index.y);
    return getBoundsOnRow (index.x, Range<int> (index.y, index.y + 1));
}

GlyphArrangement mcl::TextLayout::getGlyphsForRow (int row, bool withTrailingSpace, bool useCached) const
{
    GlyphArrangement glyphs;

    if (withTrailingSpace)
    {
        glyphs.addLineOfText (font, lines[row] + " ", 0.f, getVerticalPosition (row, Metric::baseline));
    }
    else
    {
        glyphs.addLineOfText (font, lines[row], 0.f, getVerticalPosition (row, Metric::baseline));
    }
    return glyphs;
}

GlyphArrangement mcl::TextLayout::findGlyphsIntersecting (Rectangle<float> area) const
{
    auto lineHeight = font.getHeight() * lineSpacing;
    auto row0 = jlimit (0, jmax (getNumRows() - 1, 0), int (area.getY() / lineHeight));
    auto row1 = jlimit (0, jmax (getNumRows() - 1, 0), int (area.getBottom() / lineHeight));
    auto glyphs = GlyphArrangement();

    for (int n = row0; n <= row1; ++n)
    {
        glyphs.addGlyphArrangement (getGlyphsForRow (n));
    }
    return glyphs;
}

juce::Array<mcl::TextLayout::RowData> mcl::TextLayout::findRowsIntersecting (juce::Rectangle<float> area,
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
            data.bounds.setY      (getVerticalPosition(n, Metric::top));
            data.bounds.setBottom (getVerticalPosition(n, Metric::bottom));
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

Point<int> mcl::TextLayout::findIndexNearestPosition (Point<float> position) const
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
    return Point<int> (row, col);
}

Point<int> mcl::TextLayout::getLast() const
{
    return Point<int> (getNumRows() - 1, getNumColumns (getNumRows() - 1));
}

bool mcl::TextLayout::next (juce::Point<int>& index) const
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

bool mcl::TextLayout::prev (juce::Point<int>& index) const
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

bool mcl::TextLayout::nextRow (juce::Point<int>& index) const
{
    if (index.x < getNumRows())
    {
        index.x += 1;
        index.y = jmin (index.y, getNumColumns (index.x));
        return true;
    }
    return false;
}

bool mcl::TextLayout::prevRow (juce::Point<int>& index) const
{
    if (index.x > 0)
    {
        index.x -= 1;
        index.y = jmin (index.y, getNumColumns (index.x));
        return true;
    }
    return false;
}

bool mcl::TextLayout::nextWord (juce::Point<int>& index) const
{
    if (CharacterFunctions::isWhitespace (getCharacter (index)))
        while (next (index) && CharacterFunctions::isWhitespace (getCharacter (index))) {}

    while (next (index))
        if (CharacterFunctions::isWhitespace (getCharacter (index)))
            return true;

    return false;
}

bool mcl::TextLayout::prevWord (juce::Point<int>& index) const
{
    prev (index);

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

juce::juce_wchar mcl::TextLayout::getCharacter (juce::Point<int> index) const
{
    jassert (0 <= index.x && index.x < lines.size());
    jassert (0 <= index.y && index.y <= lines[index.x].length());
    return index.y == lines[index.x].length() ? '\n' : lines[index.x].getCharPointer()[index.y];
}

Array<mcl::Selection> mcl::TextLayout::getSelections (Navigation navigation, bool fixingTail) const
{
    auto S = selections;

    auto post = [fixingTail] (auto& T)
    {
        if (! fixingTail)
            for (auto& s : T)
                s.tail = s.head;
        return T;
    };

    switch (navigation)
    {
        case Navigation::identity: return S;
        case Navigation::wholeDocument : for (auto& s : S) { s.head = { 0, 0 }; s.tail = getLast(); } return post (S);
        case Navigation::wholeLine     : for (auto& s : S) { s.head.y = 0; s.tail.y = getNumColumns (s.tail.x); } return post (S);
        case Navigation::wholeWord     : for (auto& s : S) { prevWord (s.head); nextWord (s.tail); } return post (S);
        case Navigation::forwardByChar : for (auto& s : S) next (s.head); return post (S);
        case Navigation::backwardByChar: for (auto& s : S) prev (s.head); return post (S);
        case Navigation::forwardByWord : for (auto& s : S) nextWord (s.head); return post (S);
        case Navigation::backwardByWord: for (auto& s : S) prevWord (s.head); return post (S);
        case Navigation::forwardByLine : for (auto& s : S) nextRow (s.head); return post (S);
        case Navigation::backwardByLine: for (auto& s : S) prevRow (s.head); return post (S);
        case Navigation::toLineStart   : for (auto& s : S) s.head.y = 0; return post (S);
        case Navigation::toLineEnd     : for (auto& s : S) s.head.y = getNumColumns (s.head.x); return post (S);
    }
}

String mcl::TextLayout::getSelectionContent (Selection s) const
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

mcl::Transaction mcl::TextLayout::fulfill (const Transaction& transaction)
{
    cachedBounds = {}; // invalidate the bounds

    const auto t = transaction.accountingForSpecialCharacters (*this);
    const auto s = t.selection.oriented();
    const auto L = getSelectionContent (s.horizontallyMaximized (*this));
    const auto i = s.head.y;
    const auto j = L.lastIndexOf ("\n") + s.tail.y + 1;
    const auto M = L.substring (0, i) + t.content + L.substring (j);

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
class mcl::Transaction::Undoable : public juce::UndoableAction
{
public:
    Undoable (TextLayout& layout, Callback callback, Transaction forward)
    : layout (layout)
    , callback (callback)
    , forward (forward) {}

    bool perform() override
    {
        callback (reverse = layout.fulfill (forward));
        return true;
    }

    bool undo() override
    {
        callback (forward = layout.fulfill (reverse));
        return true;
    }

    TextLayout& layout;
    Callback callback;
    Transaction forward;
    Transaction reverse;
};




//==============================================================================
mcl::Transaction mcl::Transaction::accountingForSpecialCharacters (const TextLayout& layout) const
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
            layout.prev (s.head);
        }
        t.content.clear();
    }
    else if (content.getLastCharacter() == KeyPress::deleteKey)
    {
        if (s.head.y == s.tail.y)
        {
            layout.next (s.head);
        }
        t.content.clear();
    }
    return t;
}

juce::UndoableAction* mcl::Transaction::on (TextLayout& layout, Callback callback)
{
    return new Undoable (layout, callback, *this);
}




//==============================================================================
mcl::TextEditor::TextEditor()
: caret (layout)
, gutter (layout)
, highlight (layout)
{
    lastTransactionTime = Time::getApproximateMillisecondCounter();
    layout.setSelections ({ Selection() });

    setFont (Font (Font::getDefaultMonospacedFontName(), 16, 0));

    translateView (GUTTER_WIDTH, 0);
    setWantsKeyboardFocus (true);

    addAndMakeVisible (highlight);
    addAndMakeVisible (caret);
    addAndMakeVisible (gutter);
}

void mcl::TextEditor::setFont (juce::Font font)
{
    layout.setFont (font);
    gutter.cacheLineNumberGlyphs();
    repaint();
}

void mcl::TextEditor::setText (const String& text)
{
    layout.replaceAll (text);
    repaint();
}

void mcl::TextEditor::translateView (float dx, float dy)
{
    auto W = viewScaleFactor * layout.getBounds().getWidth();
    auto H = viewScaleFactor * layout.getBounds().getHeight();

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
    auto i = layout.getSelections().getFirst().head;
    auto t = Point<float> (0.f, layout.getVerticalPosition (i.x, TextLayout::Metric::top))   .transformedBy (transform);
    auto b = Point<float> (0.f, layout.getVerticalPosition (i.x, TextLayout::Metric::bottom)).transformedBy (transform);

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
}

void mcl::TextEditor::paint (Graphics& g)
{
    g.fillAll (Colours::white);
    g.setColour (Colours::black);

    // auto start = Time::getMillisecondCounterHiRes();

    layout.findGlyphsIntersecting (g.getClipBounds()
                                   .toFloat()
                                   .transformedBy (transform.inverted())
                                   ).draw (g, transform);

    // DBG(Time::getMillisecondCounterHiRes() - start);
}

void mcl::TextEditor::paintOverChildren (Graphics& g)
{
}

void mcl::TextEditor::mouseDown (const MouseEvent& e)
{
    if (e.getNumberOfClicks() > 1)
        return;

    auto selections = layout.getSelections();
    auto index = layout.findIndexNearestPosition (e.position.transformedBy (transform.inverted()));

    if (selections.contains (index))
    {
        return;
    }
    if (! e.mods.isCommandDown() || ! TEST_MULTI_CARET_EDITING)
    {
        selections.clear();
    }

    selections.add (index);
    layout.setSelections (selections);
    updateSelections();
}

void mcl::TextEditor::mouseDrag (const MouseEvent& e)
{
    if (e.mouseWasDraggedSinceMouseDown())
    {
        auto selection = layout.getSelections().getFirst();
        selection.head = layout.findIndexNearestPosition (e.position.transformedBy (transform.inverted()));
        layout.setSelections ({ selection });
        translateToEnsureCaretIsVisible();
        updateSelections();
    }
}

void mcl::TextEditor::mouseDoubleClick (const MouseEvent& e)
{
    if (e.getNumberOfClicks() == 2)
        layout.setSelections (layout.getSelections (TextLayout::Navigation::wholeWord, true).getFirst());
    else if (e.getNumberOfClicks() == 3)
        layout.setSelections (layout.getSelections (TextLayout::Navigation::wholeLine, true).getFirst());
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
    using Navigation = TextLayout::Navigation;

    auto nav = [this] (TextLayout::Navigation navigation)
    {
        layout.setSelections (layout.getSelections (navigation));
        translateToEnsureCaretIsVisible();
        updateSelections();
        return true;
    };

    auto expand = [this] (TextLayout::Navigation navigation)
    {
        layout.setSelections (layout.getSelections (navigation, true));
        updateSelections();
        return true;
    };

    if (key.getModifiers().isShiftDown())
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
        Transaction t;
        t.content = insertion;
        t.selection = layout.getSelections().getFirst();

        auto callback = [this] (const Transaction& r)
        {
            switch (r.direction) // NB: switching on the direction of the reciprocal here
            {
                case Transaction::Direction::forward: layout.setSelections ({ r.selection }); break;
                case Transaction::Direction::reverse: layout.setSelections ({ r.selection.tail }); break;
            }
            updateSelections();

            if (! r.affectedArea.isEmpty())
            {
                repaint (r.affectedArea.transformedBy (transform).getSmallestIntegerContainer());
            }
        };

        double now = Time::getApproximateMillisecondCounter();

        if (now > lastTransactionTime + 400)
        {
            lastTransactionTime = Time::getApproximateMillisecondCounter();
            undo.beginNewTransaction();
        }
        return undo.perform (t.on (layout, callback));
    };

    bool isTab = tabKeyUsed && (key.getTextCharacter() == '\t');

    if (key == KeyPress ('x', ModifierKeys::commandModifier, 0))
    {
        SystemClipboard::copyTextToClipboard (layout.getSelectionContent (layout.getSelections().getFirst()));
        return insert ("");
    }
    if (key == KeyPress ('c', ModifierKeys::commandModifier, 0))
    {
        SystemClipboard::copyTextToClipboard (layout.getSelectionContent (layout.getSelections().getFirst()));
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
