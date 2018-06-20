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




/**
 Factoring of responsibilities in the text editor classes:
 
 - CaretRange: stores leading and trailing edges of an editing region
 - TextAction: visits a layout, operates on text and caret ranges, is undoable
 - TextLayout: stores text data and caret ranges, supplies metrics, accepts actions
 - TextEditor: is a component, issues actions, computes view transform
 - CaretComponent: draws the caret symbol(s) and highlighted region(s)
 */




struct CaretRange
{
    juce::Point<int> leadingEdge;
    juce::Point<int> trailingEdge;
};




struct TextLayout
{
    juce::StringArray lines;
};
