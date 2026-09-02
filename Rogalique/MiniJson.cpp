#include "MiniJson.h"
#include <cctype>
#include <cstdlib>

namespace
{
    const JsonValue NULL_VALUE;
}

const JsonValue& JsonValue::operator[](const std::string& key) const
{
    if (m_type != JsonType::Object) {
        return NULL_VALUE;
    }
    auto it = m_objectValue.find(key);
    return it == m_objectValue.end() ? NULL_VALUE : it->second;
}

const JsonValue& JsonValue::operator[](size_t index) const
{
    if (m_type != JsonType::Array || index >= m_arrayValue.size()) {
        return NULL_VALUE;
    }
    return m_arrayValue[index];
}

size_t JsonValue::size() const
{
    if (m_type == JsonType::Array) {
        return m_arrayValue.size();
    }
    if (m_type == JsonType::Object) {
        return m_objectValue.size();
    }
    return 0;
}

std::string JsonValue::asString(const std::string& fallback) const
{
    return m_type == JsonType::String ? m_stringValue : fallback;
}

int JsonValue::asInt(int fallback) const
{
    return m_type == JsonType::Number ? static_cast<int>(m_numberValue) : fallback;
}

unsigned JsonValue::asUInt(unsigned fallback) const
{
    return m_type == JsonType::Number ? static_cast<unsigned>(m_numberValue) : fallback;
}

float JsonValue::asFloat(float fallback) const
{
    return m_type == JsonType::Number ? static_cast<float>(m_numberValue) : fallback;
}

bool JsonValue::asBool(bool fallback) const
{
    return m_type == JsonType::Bool ? m_boolValue : fallback;
}

const std::vector<JsonValue>& JsonValue::items() const
{
    static const std::vector<JsonValue> empty;
    return m_type == JsonType::Array ? m_arrayValue : empty;
}

const std::map<std::string, JsonValue>& JsonValue::members() const
{
    static const std::map<std::string, JsonValue> empty;
    return m_type == JsonType::Object ? m_objectValue : empty;
}

// Рекурсивный спуск по строке text/pos — весь парсер живёт в одном классе, чтобы не таскать text/pos параметрами
// через десяток свободных функций.
class JsonParser {
public:
    explicit JsonParser(const std::string& text)
        : m_text(text)
    {
    }

    bool parseValue(JsonValue& out)
    {
        skipWhitespace();
        if (m_pos >= m_text.size()) {
            return false;
        }
        char c = m_text[m_pos];
        if (c == '{') {
            return parseObject(out);
        }
        if (c == '[') {
            return parseArray(out);
        }
        if (c == '"') {
            std::string s;
            if (!parseString(s)) {
                return false;
            }
            out = JsonValue();
            out.m_type = JsonType::String;
            out.m_stringValue = std::move(s);
            return true;
        }
        if (c == 't' && m_text.compare(m_pos, 4, "true") == 0) {
            m_pos += 4;
            out = JsonValue();
            out.m_type = JsonType::Bool;
            out.m_boolValue = true;
            return true;
        }
        if (c == 'f' && m_text.compare(m_pos, 5, "false") == 0) {
            m_pos += 5;
            out = JsonValue();
            out.m_type = JsonType::Bool;
            out.m_boolValue = false;
            return true;
        }
        if (c == 'n' && m_text.compare(m_pos, 4, "null") == 0) {
            m_pos += 4;
            out = JsonValue();
            return true;
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            return parseNumber(out);
        }
        return false; // Неожиданный символ — синтаксическая ошибка.
    }

    bool atEnd()
    {
        skipWhitespace();
        return m_pos >= m_text.size();
    }

private:
    void skipWhitespace()
    {
        while (m_pos < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_pos]))) {
            ++m_pos;
        }
    }

    bool parseObject(JsonValue& out)
    {
        ++m_pos; // '{'
        out = JsonValue();
        out.m_type = JsonType::Object;
        skipWhitespace();
        if (m_pos < m_text.size() && m_text[m_pos] == '}') {
            ++m_pos;
            return true;
        }
        while (true) {
            skipWhitespace();
            std::string key;
            if (m_pos >= m_text.size() || m_text[m_pos] != '"' || !parseString(key)) {
                return false;
            }
            skipWhitespace();
            if (m_pos >= m_text.size() || m_text[m_pos] != ':') {
                return false;
            }
            ++m_pos; // ':'
            JsonValue value;
            if (!parseValue(value)) {
                return false;
            }
            out.m_objectValue[key] = std::move(value);
            skipWhitespace();
            if (m_pos >= m_text.size()) {
                return false;
            }
            if (m_text[m_pos] == ',') {
                ++m_pos;
                continue;
            }
            if (m_text[m_pos] == '}') {
                ++m_pos;
                return true;
            }
            return false;
        }
    }

    bool parseArray(JsonValue& out)
    {
        ++m_pos; // '['
        out = JsonValue();
        out.m_type = JsonType::Array;
        skipWhitespace();
        if (m_pos < m_text.size() && m_text[m_pos] == ']') {
            ++m_pos;
            return true;
        }
        while (true) {
            JsonValue value;
            if (!parseValue(value)) {
                return false;
            }
            out.m_arrayValue.push_back(std::move(value));
            skipWhitespace();
            if (m_pos >= m_text.size()) {
                return false;
            }
            if (m_text[m_pos] == ',') {
                ++m_pos;
                continue;
            }
            if (m_text[m_pos] == ']') {
                ++m_pos;
                return true;
            }
            return false;
        }
    }

    bool parseString(std::string& out)
    {
        if (m_pos >= m_text.size() || m_text[m_pos] != '"') {
            return false;
        }
        ++m_pos; // '"'
        out.clear();
        while (m_pos < m_text.size() && m_text[m_pos] != '"') {
            char c = m_text[m_pos];
            if (c == '\\') {
                ++m_pos;
                if (m_pos >= m_text.size()) {
                    return false;
                }
                char escaped = m_text[m_pos];
                switch (escaped) {
                case '"':
                    out.push_back('"');
                    break;
                case '\\':
                    out.push_back('\\');
                    break;
                case '/':
                    out.push_back('/');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                default:
                    // \uXXXX и прочее не поддержаны (см. комментарий в MiniJson.h) — пропускаем символ как есть,
                    // не считаем это фатальной ошибкой разбора (в .tmj такое практически не встречается).
                    out.push_back(escaped);
                    break;
                }
                ++m_pos;
            } else {
                out.push_back(c);
                ++m_pos;
            }
        }
        if (m_pos >= m_text.size()) {
            return false; // Незакрытая строка.
        }
        ++m_pos; // закрывающая '"'
        return true;
    }

    bool parseNumber(JsonValue& out)
    {
        size_t start = m_pos;
        if (m_pos < m_text.size() && m_text[m_pos] == '-') {
            ++m_pos;
        }
        while (m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos]))) {
            ++m_pos;
        }
        if (m_pos < m_text.size() && m_text[m_pos] == '.') {
            ++m_pos;
            while (m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos]))) {
                ++m_pos;
            }
        }
        if (m_pos < m_text.size() && (m_text[m_pos] == 'e' || m_text[m_pos] == 'E')) {
            ++m_pos;
            if (m_pos < m_text.size() && (m_text[m_pos] == '+' || m_text[m_pos] == '-')) {
                ++m_pos;
            }
            while (m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos]))) {
                ++m_pos;
            }
        }
        if (m_pos == start) {
            return false;
        }
        out = JsonValue();
        out.m_type = JsonType::Number;
        out.m_numberValue = std::strtod(m_text.substr(start, m_pos - start).c_str(), nullptr);
        return true;
    }

    const std::string& m_text;
    size_t m_pos = 0;
};

bool JsonValue::parse(const std::string& text, JsonValue& out)
{
    JsonParser parser(text);
    JsonValue result;
    if (!parser.parseValue(result) || !parser.atEnd()) {
        return false;
    }
    out = std::move(result);
    return true;
}
