#pragma once
#include <SFML/System/Utf.hpp>
#include <iterator>
#include <string>

// Общая логика посимвольного ввода короткой строки (например, имени для таблицы рекордов) — портировано из
// старого Arkanoid-проекта этого автора (см. титры), тот же файл почти дословно. Отдельно от остального ввода
// движка (см. FocusedInput.h) — символы приходят через TextInputBuffer (sf::Event::TextEntered), не через
// polling: только так учитываются раскладка/Shift/IME, а не голые коды клавиш.
namespace NameEntryInput
{
    constexpr sf::Uint32 BACKSPACE = 8;
    constexpr int MAX_NAME_LENGTH = 12; // В символах, не байтах UTF-8.

    // Печатаемая ASCII или кириллица (можно писать по-русски).
    inline bool isAllowedChar(sf::Uint32 unicode)
    {
        bool isAsciiPrintable = unicode > 32 && unicode < 127;
        bool isCyrillic = unicode >= 0x0400 && unicode <= 0x04FF;
        return isAsciiPrintable || isCyrillic;
    }

    // Удаляет последний символ (не байт) UTF-8-строки — кириллица занимает 2 байта, наивный pop_back() отрезал
    // бы только половину символа и оставлял битую строку.
    inline void popLastUtf8Char(std::string& s)
    {
        if (s.empty()) {
            return;
        }
        std::size_t i = s.size() - 1;
        while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) {
            --i;
        }
        s.erase(i);
    }

    // Обрабатывает один введённый символ (см. TextInputBuffer::charsThisFrame()) для поля ввода имени.
    inline void handleChar(sf::Uint32 unicode, std::string& name, int& nameLength)
    {
        if (unicode == BACKSPACE) {
            if (!name.empty()) {
                popLastUtf8Char(name);
                --nameLength;
            }
        } else if (isAllowedChar(unicode)) {
            if (nameLength < MAX_NAME_LENGTH) {
                sf::Utf8::encode(unicode, std::back_inserter(name));
                ++nameLength;
            }
        }
    }
} // namespace NameEntryInput
