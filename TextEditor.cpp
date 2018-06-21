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
mcl::CaretComponent::CaretComponent (const TextLayout& layout) : layout (layout)
{
    setInterceptsMouseClicks (false, false);
    // startTimerHz (40);
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
        auto b = layout.getGlyphBounds (selection.head).removeFromLeft (3.f).translated (-1.5f, 0.f).expanded (0.f, 1.f);
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
        layout.replaceSelections (navigationFwd);
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
    auto bounds = Rectangle<float>();

    for (int n = 0; n < getNumRows(); ++n)
    {
        bounds = bounds.getUnion (getBoundsOnRow (n, Range<int> (0, getNumColumns (n))));
    }
    return bounds;
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
    /*
     During vertical navigations, the column index might be greater than the row
     length, in order to restore the column value if navigating back to longer
     lines. This mode is canceled by backward horizontal navigation.
     */
    index.y = jmin (index.y, getNumColumns (index.x));

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
        return true;
    }
    return false;
}

bool mcl::TextLayout::prevRow (juce::Point<int>& index) const
{
    if (index.x > 0)
    {
        index.x -= 1;
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

//StringArray mcl::TextLayout::replaceSelectedText (const StringArray& content)
//{
//    jassert (content.size() == selections.size());
//
//    StringArray existing;
//
//    for (int n = 0; n < content.size(); ++n)
//    {
//        auto& s = selections.getReference (n);
//        auto insertion = content[n];
//
//        if (insertion.getLastCharacter() == KeyPress::backspaceKey)
//        {
//            if (s.head.y == s.tail.y)
//            {
//                prev (s.head);
//            }
//            insertion.clear();
//        }
//        else if (insertion.getLastCharacter() == KeyPress::deleteKey)
//        {
//            if (s.head.y == s.tail.y)
//            {
//                next (s.head);
//            }
//            insertion.clear();
//        }
//
//        if (s.head.x == s.tail.x) // same row
//        {
//            const auto& line = lines[s.head.x];
//            const auto c0 = jmin (s.head.y, s.tail.y);
//            const auto c1 = jmax (s.head.y, s.tail.y);
//
//            auto modifiedLine = line.substring (0, c0) + insertion + line.substring (c1);
//
//            if (insertion.containsChar ('\n'))
//            {
//                lines.remove (s.head.x);
//
//                for (const auto& sub : StringArray::fromLines (modifiedLine))
//                {
//                    lines.insert (s.head.x++, sub);
//                }
//                s.head.x--;
//                s.head.y = 0;
//            }
//            else
//            {
//                lines.set (s.head.x, modifiedLine);
//
//                if (s.tail.y <= s.head.y)
//                {
//                    s.head.y += insertion.length() - (c1 - c0);
//                }
//            }
//            s.tail = s.head;
//        }
//        else
//        {
//            DBG("Replace text over multiple lines not implemented yet");
//        }
//    }
//    return existing;
//}

String mcl::TextLayout::getSelectionContent (Selection s) const
{
    if (s.head == s.tail)
    {
    }
    else if (s.head.x == s.tail.x)
    {
    }
    return String();
}

mcl::Transaction mcl::TextLayout::fulfill (const Transaction& transaction)
{
    if (transaction.selection.isSingleLine())
    {
        auto inf = std::numeric_limits<float>::max();
        Transaction t = transaction.accountingForSpecialCharacters (*this);
        Transaction r; // reciprocal
        Selection s = t.selection;

        const auto c0 = jmin (s.head.y, s.tail.y);
        const auto c1 = jmax (s.head.y, s.tail.y);
        const auto& line = lines[s.head.x];

        lines.set (s.head.x, line.substring (0, c0) + t.content + line.substring (c1));

        r.selection = Selection (s.head.x, c0, s.tail.x, c0 + t.content.length());
        r.content = line.substring (c0, c1); // the content being removed
        r.affectedArea = Rectangle<float> (0, 0, inf, inf);

        return r;
    }
    else
    {
        DBG("multi-line transactions not implemented yet");
        return Transaction();
    }
}




//==============================================================================
mcl::Transaction mcl::Transaction::accountingForSpecialCharacters (const TextLayout& layout) const
{
    Transaction t = *this;
    auto& s = t.selection;

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

Array<mcl::Transaction> mcl::Transaction::asSingleLineTransactions (const TextLayout& layout) const
{
    Array<Transaction> transactions;
    return transactions;
}




//==============================================================================
mcl::TextEditor::TextEditor() : highlight (layout), caret (layout)
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

    layout.replaceSelections ({ Selection() });
    layout.setFont (Font ("Monaco", 16, 0));
    translateView (10, 0);
    setWantsKeyboardFocus (true);

    addAndMakeVisible (highlight);
    addAndMakeVisible (caret);
}

void mcl::TextEditor::setText (const String& text)
{
    layout.replaceAll (text);
    repaint();
}

void mcl::TextEditor::translateView (float dx, float dy)
{
    translation.x += dx;
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
    highlight.setViewTransform (transform);
    caret.setViewTransform (transform);
    repaint();
}

void mcl::TextEditor::resized()
{
    highlight.setBounds (getLocalBounds());
    caret.setBounds (getLocalBounds());
}

void mcl::TextEditor::paint (Graphics& g)
{
    g.fillAll (Colours::white);
}

void mcl::TextEditor::paintOverChildren (Graphics& g)
{
    g.setColour (Colours::black);
    layout.findGlyphsIntersecting (g.getClipBounds()
                                   .toFloat()
                                   .transformedBy (transform.inverted())
                                   ).draw (g, transform);
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
    translateView (0, d.deltaY * 600);
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
        layout.replaceSelections ({ Selection (r.selection.tail) });

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
    return MouseCursor::IBeamCursor;
}

