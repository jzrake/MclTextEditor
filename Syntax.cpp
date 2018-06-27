/** ============================================================================
 *
 * Syntax.cpp
 *
 * Copyright (C) Jonathan Zrake
 *
 * You may use, distribute and modify this code under the terms of the GPL3
 * license.
 * =============================================================================
 */
#include <regex>
#include "Syntax.hpp"
using namespace juce;




//==============================================================================
class mcl::Scanner::Pattern
{
public:

    struct Result
    {
        Result (const String& string) : tokenStart (string.length()) {}
        int tokenStart;
        int tokenEnd;
        Identifier token;
    };

    Pattern (const Identifier& identifier, const String& pattern)
    : identifier (identifier)
    , r (pattern.toRawUTF8())
    {
    }

    std::pair<int, int> search (const String& target, int start) const
    {
        auto m = std::cmatch();

        if (std::regex_search (target.toRawUTF8() + start, m, r))
        {
            return std::make_pair (start + int (m.position()),
                                   start + int (m.position() + m.length()));
        }
        return std::make_pair (-1, -1);
    }

    static Result searchMany (const Array<std::unique_ptr<Pattern>>& patterns,
                              const String& target,
                              int start)
    {
        auto bestPatternSoFar = Result (target);

        for (const auto& pattern : patterns)
        {
            auto result = pattern->search (target, start);

            if (0 <= result.first && result.first < bestPatternSoFar.tokenStart)
            {
                bestPatternSoFar.tokenStart = result.first;
                bestPatternSoFar.tokenEnd = result.second;
                bestPatternSoFar.token = pattern->identifier;
            }
        }
        return bestPatternSoFar;
    }
private:
    friend class Scanner;
    Identifier identifier;
    std::regex r;
};




//==============================================================================
mcl::Scanner::Scanner (const TextDocument& document) : document (document)
{
}

mcl::Scanner::~Scanner()
{
}

void mcl::Scanner::addPattern (const Identifier& identifier, const String& pattern)
{
    patterns.add (std::make_unique<Pattern> (identifier, pattern));
}

void mcl::Scanner::clear()
{
    patterns.clear();
}

bool mcl::Scanner::next()
{
    while (index.x < document.getNumRows())
    {
        auto result = Pattern::searchMany (patterns, document.getLine (index.x), index.y);

        if (result.token.isValid())
        {
            tokenIndex = { index.x, result.tokenStart };
            token = result.token;
            index.y = result.tokenEnd;
            return true;
        }

        index.x += 1; // we're at the end of the line
        index.y = 0;
    }
    return false;
}
