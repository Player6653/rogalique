#include "pch.h"
#include "TextInputBuffer.h"

namespace
{
    std::vector<sf::Uint32> g_charsThisFrame;
}

namespace TextInputBuffer
{
    const std::vector<sf::Uint32>& charsThisFrame()
    {
        return g_charsThisFrame;
    }

    void pushChar(sf::Uint32 unicode)
    {
        g_charsThisFrame.push_back(unicode);
    }

    void clear()
    {
        g_charsThisFrame.clear();
    }
} // namespace TextInputBuffer
