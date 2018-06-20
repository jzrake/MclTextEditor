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
void mcl::TextLayout::replaceAll (const juce::String& content)
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

juce::Rectangle<float> mcl::TextLayout::getBoundsOnRow (int row, juce::Range<int> columns) const
{
    return getGlyphsForRow (row, true)
        .getBoundingBox (columns.getStart(), columns.getLength(), true)
        .withTop    (getVerticalPosition (row, Metric::top))
        .withBottom (getVerticalPosition (row, Metric::bottom));
}

juce::GlyphArrangement mcl::TextLayout::getGlyphsForRow (int row, bool withTrailingSpace) const
{
    GlyphArrangement glyphs;

    if (withTrailingSpace)
    {
        glyphs.addLineOfText (font, lines[row], 0.f, getVerticalPosition (row, Metric::baseline));
    }
    else
    {
        glyphs.addLineOfText (font, lines[row] + " ", 0.f, getVerticalPosition (row, Metric::baseline));
    }
    return glyphs;
}

juce::GlyphArrangement mcl::TextLayout::findGlyphsIntersecting (juce::Rectangle<float> area) const
{
    auto lineHeight = font.getHeight() * lineSpacing;
    auto row0 = jlimit (0, getNumRows() - 1, int (area.getY() / lineHeight));
    auto row1 = jlimit (0, getNumRows() - 1, int (area.getBottom() / lineHeight));
    auto glyphs = GlyphArrangement();

    for (int n = row0; n <= row1; ++n)
    {
        glyphs.addGlyphArrangement (getGlyphsForRow (n));
    }
    return glyphs;
}

juce::Point<int> mcl::TextLayout::findRowAndColumnNearestPosition (juce::Point<float> position) const
{
    auto lineHeight = font.getHeight() * lineSpacing;
    auto row = jlimit (0, getNumRows() - 1, int (position.y / lineHeight));
    auto col = 0;
    auto glyphs = getGlyphsForRow (row);

    if (position.x > 0.f)
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




//==============================================================================
mcl::TextEditor::TextEditor()
{
    layout.setFont (Font ("Monaco", 16, 0));
}

void mcl::TextEditor::setText (const juce::String& text)
{
    layout.replaceAll (text);
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
    // selection.setViewTransform (transform);
    repaint();
}

void mcl::TextEditor::paint (Graphics& g)
{
    g.fillAll (Colours::white);
}

void mcl::TextEditor::paintOverChildren (Graphics& g)
{
    g.setColour (Colours::black);
    layout.findGlyphsIntersecting (g.getClipBounds().toFloat().transformedBy (transform.inverted())).draw (g, transform);
}

void mcl::TextEditor::resized()
{
    // selection.setBounds (getLocalBounds());
}

void mcl::TextEditor::mouseDown (const juce::MouseEvent& e)
{
}

void mcl::TextEditor::mouseDrag (const juce::MouseEvent& e)
{
}

void mcl::TextEditor::mouseDoubleClick (const MouseEvent& e)
{
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
    // if (key.getModifiers().isShiftDown())
    // {
    //     if (key.isKeyCode (KeyPress::leftKey )) return selection.extendSelectionBackward();
    //     if (key.isKeyCode (KeyPress::rightKey)) return selection.extendSelectionForward();
    //     if (key.isKeyCode (KeyPress::upKey   )) return selection.extendSelectionUp();
    //     if (key.isKeyCode (KeyPress::downKey )) return selection.extendSelectionDown();
    // }
    // else
    // {
    //     if (key.isKeyCode (KeyPress::backspaceKey)) return selection.deleteBackward();
    //     if (key.isKeyCode (KeyPress::leftKey     )) return selection.moveCaretBackward();
    //     if (key.isKeyCode (KeyPress::rightKey    )) return selection.moveCaretForward();
    //     if (key.isKeyCode (KeyPress::upKey       )) return selection.moveCaretUp();
    //     if (key.isKeyCode (KeyPress::downKey     )) return selection.moveCaretDown();
    //     if (key.isKeyCode (KeyPress::returnKey   )) return selection.insertLineBreakAtCaret();
    // }

    // if (key == KeyPress ('a', ModifierKeys::ctrlModifier, 0)) return selection.moveCaretToLineStart();
    // if (key == KeyPress ('e', ModifierKeys::ctrlModifier, 0)) return selection.moveCaretToLineEnd();
    // if (key == KeyPress ('d', ModifierKeys::ctrlModifier, 0)) return selection.deleteForward();

    // if (key == KeyPress ('v', ModifierKeys::commandModifier, 0)) return selection.insertTextAtCaret (SystemClipboard::getTextFromClipboard());

    // if (key.getTextCharacter() >= ' ' || (tabKeyUsed && (key.getTextCharacter() == '\t')))
    // {
    //     return selection.insertCharacterAtCaret (key.getTextCharacter());
    // }
    return false;
}

MouseCursor mcl::TextEditor::getMouseCursor()
{
    return MouseCursor::IBeamCursor;
}







//String text = "The quick brown fox";
//GlyphArrangement glyphs;
//Font font ("Times", 54, 0);
//
//float baseline = 0.f;
//float lineSpacing = 1.25f;
//float gap = font.getHeight() * (lineSpacing - 1.f) * 0.5f;
//
//glyphs.addLineOfText (font, text, 0.f, baseline);
//
//g.saveState();
//g.addTransform (AffineTransform::translation (0, 200));
//
//g.setColour (Colours::white);
//g.fillAll();
//
//g.setColour (Colours::lightgrey);
//g.drawHorizontalLine (baseline, 0.f, getWidth());
//g.drawHorizontalLine (baseline + font.getDescent(), 0.f, getWidth());
//g.drawHorizontalLine (baseline - font.getAscent(), 0.f, getWidth());
//
//g.setColour (Colours::whitesmoke);
//g.fillRect (Rectangle<int> (0, baseline - font.getAscent() - gap, getWidth(), gap));
//g.fillRect (Rectangle<int> (0, baseline + font.getDescent(), getWidth(), gap));
//
//g.setColour (Colours::black);
//glyphs.draw (g);
//
//g.restoreState();

