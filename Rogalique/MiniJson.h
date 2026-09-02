#pragma once
#include <map>
#include <string>
#include <vector>

// Минимальный JSON-парсер общего назначения (object/array/string/number/bool/null) — нужен только для чтения
// экспортированных из Tiled Map Editor .tmj-карт (см. TiledLevel.h), но сам ничего не знает про схему Tiled: это
// осознанно, чтения "по форме" (жёстко зашитые ожидаемые ключи в конкретном порядке) — более хрупкий вариант,
// который ломается от любой мелочи, которую Tiled решит поменять между версиями. Полноценного стандарта не
// покрывает (например, \uXXXX escape в строках не поддержан — в реальных .tmj-файлах не встречается), но всего,
// что реально попадается в экспорте Tiled, достаточно. Без внешних зависимостей (nlohmann/json и т.п.) — в духе
// уже принятого в проекте подхода "минимум внешних библиотек", единственная вендоренная зависимость проекта — SFML.
enum class JsonType { Null, Bool, Number, String, Array, Object };

class JsonValue {
public:
    JsonValue() = default;

    JsonType type() const
    {
        return m_type;
    }
    bool isNull() const
    {
        return m_type == JsonType::Null;
    }

    // Тихий доступ — nullptr-объект (isNull()==true), если ключа/индекса нет или тип не совпадает, а не исключение:
    // .tmj хранит много опциональных полей (custom properties и т.п.), проверять наличие каждого через try/catch
    // было бы избыточно на каждый вызов.
    const JsonValue& operator[](const std::string& key) const;
    const JsonValue& operator[](size_t index) const;
    size_t size() const; // 0, если не Array/Object

    std::string asString(const std::string& fallback = "") const;
    int asInt(int fallback = 0) const;
    // Tiled хранит GID тайлов как 32-битные БЕЗ ЗНАКА числа — с выставленными флагами поворота/отражения (старшие
    // биты) значение может превышать INT_MAX. asInt() тут дал бы UB/обрезание, поэтому отдельный accessor,
    // приводящий из double (у double 52 бита мантиссы — 32-битное целое влезает без потери точности).
    unsigned asUInt(unsigned fallback = 0) const;
    float asFloat(float fallback = 0.f) const;
    bool asBool(bool fallback = false) const;

    const std::vector<JsonValue>& items() const;
    const std::map<std::string, JsonValue>& members() const;

    // false при синтаксической ошибке (out не трогается) — вызывающий код сам решает, LOG_ERROR это или нет.
    static bool parse(const std::string& text, JsonValue& out);

private:
    JsonType m_type = JsonType::Null;
    bool m_boolValue = false;
    double m_numberValue = 0.0;
    std::string m_stringValue;
    std::vector<JsonValue> m_arrayValue;
    std::map<std::string, JsonValue> m_objectValue;

    friend class JsonParser;
};
