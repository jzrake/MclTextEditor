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




//==============================================================================
mcl::CaretComponent::CaretComponent (const TextLayout& layout) : layout (layout)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (40);
}

void mcl::CaretComponent::setViewTransform (const AffineTransform& transformToUse)
{
    transform = transformToUse;
    repaint();
}

void mcl::CaretComponent::refreshSelections()
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
    phase += 1.6e-1;
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

void mcl::GutterComponent::refreshSelections()
{
    repaint();
}

void mcl::GutterComponent::paint (juce::Graphics& g)
{
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
        g.drawVerticalLine (GUTTER_WIDTH - 1., 0.f, getHeight());
    }

    /*
     Draw the highlighted regions
     ------------------------------------------------------------------
     */
    g.setColour (Colours::whitesmoke.darker (0.1f));

    for (const auto& s : layout.getSelections()) // caret rows only
    {
        auto patch = layout.getBoundsOnRow (s.head.x, juce::Range<int> (0, 1));
        g.fillRect (patch.transformedBy (transform).withLeft (0).withWidth (GUTTER_WIDTH));
    }

    for (const auto& s : layout.getSelections()) // highlighted regions
    {
        auto region = Rectangle<float>();

        for (const auto& patch : layout.getSelectionRegion (s))
        {
            region = region.getUnion (patch);
        }
        g.fillRect (region.transformedBy (transform).withLeft (0).withWidth (GUTTER_WIDTH));
    }

    /*
     Draw the line numbers
     ------------------------------------------------------------------
     */
    auto area = g.getClipBounds()
        .toFloat()
        .transformedBy (transform.inverted());

    g.setFont (Font ("Monaco", 12, 0));
    g.setColour (Colours::grey);

    for (const auto& r : layout.findRowsIntersecting (area))
    {
        auto area = r.bounds
            .transformedBy (transform)
            .withLeft (8)
            .withWidth (GUTTER_WIDTH);
        g.drawText (String (r.rowNumber), area, Justification::left);
    }
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

void mcl::HighlightComponent::refreshSelections()
{
    repaint();
}

void mcl::HighlightComponent::paint (juce::Graphics& g)
{
    g.setColour (Colours::grey);
    g.drawText (layout.getSelections().getFirst().toString(), getLocalBounds().toFloat(), Justification::bottomRight);

    g.addTransform (transform);
    g.setColour (Colours::black.withAlpha (0.2f));

    for (const auto& s : layout.getSelections())
    {
        for (const auto& patch : layout.getSelectionRegion (s))
        {
            g.fillRect (patch);
        }
    }
}




//==============================================================================
class mcl::TextAction::Undoable : public juce::UndoableAction
{
public:
    Undoable (TextAction operation, TextLayout& layout) : operation (operation), layout (layout)
    {
    }
    bool perform() override
    {
        return operation.perform (layout);
    }
    bool undo() override
    {
        return operation.inverted().perform (layout);
    }
private:
    TextAction operation;
    TextLayout& layout;
};




//==============================================================================
mcl::TextAction::TextAction()
{
}

mcl::TextAction::TextAction (Callback callback, juce::Array<Selection> targetSelection)
: callback (callback)
, navigationFwd (targetSelection)
{
}

mcl::TextAction::TextAction (Callback callback, const juce::StringArray& contentToInsert)
: callback (callback)
, replacementFwd (contentToInsert)
{
}

bool mcl::TextAction::perform (TextLayout& layout)
{
    navigationRev = layout.getSelections();
    Report report;

    if (! navigationFwd.isEmpty() && navigationFwd != navigationRev)
    {
        layout.setSelections (navigationFwd);
        report.navigationOcurred = true;
    }
    else if (! replacementFwd.isEmpty())
    {
//        replacementRev = layout.replaceSelectedText (replacementFwd);
//        report.textAreaAffected = layout.getBounds();
//        report.navigationOcurred = true;
    }

    if (callback)
        callback (report);

    return true;
}

mcl::TextAction mcl::TextAction::inverted()
{
    TextAction op;
    op.callback       = callback;
    op.replacementFwd = replacementRev;
    op.replacementRev = replacementFwd;
    op.navigationFwd  = navigationRev;
    op.navigationRev  = navigationFwd;
    return op;
}

UndoableAction* mcl::TextAction::on (TextLayout& layout) const
{
    return new Undoable (*this, layout);
}




//==============================================================================
bool mcl::Selection::isOriented() const
{
    return ! (head.x > tail.x || (head.x == tail.x && head.y > tail.y));
}

mcl::Selection mcl::Selection::oriented() const
{
    Selection s = *this;

    if (! isOriented())
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

Array<Rectangle<float>> mcl::TextLayout::getSelectionRegion (Selection selection) const
{
    Array<Rectangle<float>> patches;

    if (selection.head.x == selection.tail.x)
    {
        int c0 = jmin (selection.head.y, selection.tail.y);
        int c1 = jmax (selection.head.y, selection.tail.y);
        patches.add (getBoundsOnRow (selection.head.x, Range<int> (c0, c1)));
    }
    else
    {
        int r0, c0, r1, c1;

        if (selection.head.x > selection.tail.x) // for a forward selection
        {
            r0 = selection.tail.x;
            c0 = selection.tail.y;
            r1 = selection.head.x;
            c1 = selection.head.y;
        }
        else // for a backward selection
        {
            r0 = selection.head.x;
            c0 = selection.head.y;
            r1 = selection.tail.x;
            c1 = selection.tail.y;
        }

        patches.add (getBoundsOnRow (r0, Range<int> (c0, getNumColumns (r0) + 1)));
        patches.add (getBoundsOnRow (r1, Range<int> (0, c1)));

        for (int n = r0 + 1; n < r1; ++n)
        {
            patches.add (getBoundsOnRow (n, Range<int> (0, getNumColumns (n) + 1)));
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

GlyphArrangement mcl::TextLayout::getGlyphsForRow (int row, bool withTrailingSpace) const
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

Array<mcl::Selection> mcl::TextLayout::getSelections (Navigation navigation, bool fixingTail) const
{
    auto S = selections;
    auto shrunken = fixingTail
    ? [] (Array<Selection> S) { return S; }
    : [] (Array<Selection> S)
    {
        for (auto& s : S)
            s.tail = s.head;
        return S;
    };

    switch (navigation)
    {
        case Navigation::identity: return S;
        case Navigation::forwardByChar : for (auto& s : S) next (s.head); return shrunken (S);
        case Navigation::backwardByChar: for (auto& s : S) prev (s.head); return shrunken (S);
        case Navigation::forwardByLine : for (auto& s : S) nextRow (s.head); return shrunken (S);
        case Navigation::backwardByLine: for (auto& s : S) prevRow (s.head); return shrunken (S);
        case Navigation::toLineStart   : for (auto& s : S) s.head.y = 0; return shrunken (S);
        case Navigation::toLineEnd     : for (auto& s : S) s.head.y = getNumColumns (s.head.x); return shrunken (S);
        default: return S;
    }
}

String mcl::TextLayout::getSelectionContent (Selection s) const
{
    s = s.oriented();

    if (s.head.x == s.tail.x)
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
    cachedBounds = Rectangle<float>(); // invalidate the bounds

    // 1. Get the content on the affected rows as a single string L.
    // 2. Compute the linear start index i = s.head.y
    // 3. Compute the linear end index j = L.lastIndexOf ("\n") + s.tail.y + 1.
    // 4. Replace L between i and j with content.
    // 5. Replace the affected lines of text.

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

    /*
     Computing the post selection tail column is not elegant...
     */
    int lengthOfFinalContentLine = t.content.length() - jmax (0, t.content.lastIndexOf ("\n"));
    int finalTailColumn;

    if (t.content == "\n")
    {
        finalTailColumn = 0;
    }
    else if (s.isSingleLine() || t.content.isEmpty())
    {
        finalTailColumn = lengthOfFinalContentLine + s.head.y;
    }
    else
    {
        finalTailColumn = lengthOfFinalContentLine;
    }

    auto inf = std::numeric_limits<float>::max();
    Transaction r;
    r.selection = Selection (s.head.x, s.head.y, row - 1, finalTailColumn);
    r.content = L.substring (i, j);
    r.affectedArea = Rectangle<float> (0, 0, inf, inf);
    return r;
}




//    t.content == "\n" ? 0 : lengthOfFinalContentLine + (s.isSingleLine() || t.content.isEmpty() ? s.head.y : 0));
//    DBG("removing " << s.tail.x - s.head.x + 1);
//    DBG("adding " << StringArray::fromLines (M).size());
//    DBG("i = " << i);
//    DBG("j = " << j);
//    DBG("L = " << L.replace ("\n", "NEWLINE"));
//    DBG("M = " << M.replace ("\n", "NEWLINE"));
//    DBG("lengthOfFinalContentLine = " << lengthOfFinalContentLine);
//    DBG("new tail = " << r.selection.tail.y);




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




//==============================================================================
mcl::TextEditor::TextEditor()
: caret (layout)
, gutter (layout)
, highlight (layout)
{
    callback = [this] (TextAction::Report report)
    {
        if (report.navigationOcurred)
        {
            highlight.refreshSelections();
            caret.refreshSelections();
        }
        if (! report.textAreaAffected.isEmpty())
        {
            repaint (report.textAreaAffected.transformedBy (transform).getSmallestIntegerContainer());
        }
    };

    layout.setSelections ({ Selection() });
    layout.setFont (Font ("Monaco", 16, 0));
    translateView (GUTTER_WIDTH, 0);
    setWantsKeyboardFocus (true);

    addAndMakeVisible (highlight);
    addAndMakeVisible (caret);
    addAndMakeVisible (gutter);
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
    layout.findGlyphsIntersecting (g.getClipBounds()
                                   .toFloat()
                                   .transformedBy (transform.inverted())
                                   ).draw (g, transform);
}

void mcl::TextEditor::paintOverChildren (Graphics& g)
{
}

void mcl::TextEditor::mouseDown (const MouseEvent& e)
{
    auto selections = layout.getSelections();
    auto index = layout.findIndexNearestPosition (e.position.transformedBy (transform.inverted()));

    if (selections.contains (index))
    {
        return;
    }
    if (! e.mods.isCommandDown())
    {
        selections.clear();
    }
    selections.add (index);

    undo.beginNewTransaction();
    undo.perform (TextAction (callback, selections).on (layout));
}

void mcl::TextEditor::mouseDrag (const MouseEvent& e)
{
    if (e.mouseWasDraggedSinceMouseDown())
    {
        auto selection = layout.getSelections().getFirst();
        selection.head = layout.findIndexNearestPosition (e.position.transformedBy (transform.inverted()));
        undo.beginNewTransaction();
        undo.perform (TextAction (callback, { selection }).on (layout));
    }
}

void mcl::TextEditor::mouseDoubleClick (const MouseEvent& e)
{
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

    auto makeUndoableNav = [this] (TextLayout::Navigation navigation)
    {
        auto action = TextAction (callback, layout.getSelections (navigation));
        undo.beginNewTransaction();
        return undo.perform (action.on (layout));
    };

    auto makeUndoableExpandSelection = [this] (TextLayout::Navigation navigation)
    {
        auto action = TextAction (callback, layout.getSelections (navigation, true));
        undo.beginNewTransaction();
        return undo.perform (action.on (layout));
    };

    if (key.getModifiers().isShiftDown())
    {
        if (key.isKeyCode (KeyPress::rightKey )) return makeUndoableExpandSelection (Navigation::forwardByChar);
        if (key.isKeyCode (KeyPress::leftKey  )) return makeUndoableExpandSelection (Navigation::backwardByChar);
        if (key.isKeyCode (KeyPress::downKey  )) return makeUndoableExpandSelection (Navigation::forwardByLine);
        if (key.isKeyCode (KeyPress::upKey    )) return makeUndoableExpandSelection (Navigation::backwardByLine);
    }
    else
    {
        if (key.isKeyCode (KeyPress::rightKey    )) return makeUndoableNav (Navigation::forwardByChar);
        if (key.isKeyCode (KeyPress::leftKey     )) return makeUndoableNav (Navigation::backwardByChar);
        if (key.isKeyCode (KeyPress::downKey     )) return makeUndoableNav (Navigation::forwardByLine);
        if (key.isKeyCode (KeyPress::upKey       )) return makeUndoableNav (Navigation::backwardByLine);
    }

    if (key == KeyPress ('a', ModifierKeys::ctrlModifier, 0)) return makeUndoableNav (Navigation::toLineStart);
    if (key == KeyPress ('e', ModifierKeys::ctrlModifier, 0)) return makeUndoableNav (Navigation::toLineEnd);
    if (key == KeyPress ('z', ModifierKeys::commandModifier, 0)) return undo.undo();
    if (key == KeyPress ('r', ModifierKeys::commandModifier, 0)) return undo.redo();


    auto insert = [this] (String insertion)
    {
        Transaction t;
        t.content = insertion;
        t.selection = layout.getSelections().getFirst();

        auto r = layout.fulfill (t);

        layout.setSelections ({ Selection (r.selection.tail) });
        caret.refreshSelections();
        gutter.refreshSelections();
        highlight.refreshSelections();

        if (! r.affectedArea.isEmpty())
        {
            repaint (r.affectedArea.transformedBy (transform).getSmallestIntegerContainer());
        }
        return true;
    };

    if (key == KeyPress ('v', ModifierKeys::commandModifier, 0))
    {
        return insert (SystemClipboard::getTextFromClipboard());
    }
    if (key.isKeyCode (KeyPress::returnKey))
    {
        return insert ("\n");
    }
    if (key == KeyPress ('d', ModifierKeys::ctrlModifier, 0))
    {
        return insert (String::charToString (KeyPress::deleteKey));
    }
    if (key.getTextCharacter() >= ' ' || (tabKeyUsed && (key.getTextCharacter() == '\t')))
    {
        return insert (String::charToString (key.getTextCharacter()));
    }
    return false;
}

MouseCursor mcl::TextEditor::getMouseCursor()
{
    return getMouseXYRelative().x < GUTTER_WIDTH ? MouseCursor::NormalCursor : MouseCursor::IBeamCursor;
}
