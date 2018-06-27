/** ============================================================================
 *
 * Syntax.hpp
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
#include "TextEditor.hpp"




//==============================================================================
class mcl::Scanner
{
public:
    Scanner (const TextDocument& document);
    ~Scanner();
    void addPattern (const juce::Identifier& identifier, const juce::String& pattern);
    void reset() { index = {0, 0}; tokenIndex = {0, 0}; token = juce::Identifier(); }
    void clear();
    bool next();
    const juce::Identifier& getToken() const { return token; }
    const juce::Point<int>& getIndex() const { return tokenIndex; }
    Selection getZone() const { return { tokenIndex, index }; }
private:
    class Pattern;
    const TextDocument& document;
    juce::Identifier token;
    juce::Point<int> index;
    juce::Point<int> tokenIndex;
    juce::Array<std::unique_ptr<Pattern>> patterns;
};
