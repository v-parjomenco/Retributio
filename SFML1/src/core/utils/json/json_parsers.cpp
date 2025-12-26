#include "pch.h"

#include "core/utils/json/json_parsers.h"
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <utility>

#include "third_party/json/json_silent.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5045)
#endif

namespace {

    using core::utils::json::json;

    static constexpr std::uint8_t kInvalidIndex = 255;

    [[nodiscard]] bool tryGetU8FromNumber(const json& v, std::uint8_t& out) noexcept;

    // Запрещаем вызов helper-а с временным json (rvalue), т.к. ColorParseResult::rawText
    // хранит string_view на строку внутри json, и при уничтожении временного json станет dangling.
    [[nodiscard]] core::utils::json::ColorParseResult
    parseColorFromValueWithIssue(const json&&, const sf::Color&) noexcept = delete;

    [[nodiscard]] bool parseHexByte(std::string_view hex, std::uint8_t& out) noexcept {
        if (hex.size() != 2) {
            return false;
        }

        unsigned int value = 0;
        const char* begin = hex.data();
        const char* end = begin + hex.size();

        const auto [ptr, ec] = std::from_chars(begin, end, value, 16);
        if (ec != std::errc() || ptr != end || value > 255u) {
            return false;
        }

        out = static_cast<std::uint8_t>(value);
        return true;
    }

    [[nodiscard]] core::utils::json::ColorParseResult
    parseColorFromValueWithIssue(const json& value, const sf::Color& fallback) noexcept {
        using Issue = core::utils::json::ColorParseIssue;
        using Kind = Issue::Kind;
        using Ch = Issue::Channel;

        core::utils::json::ColorParseResult r{};
        r.value = fallback;

        // Строка "#RRGGBB" / "#RRGGBBAA"
        if (const auto* strPtr = value.get_ptr<const std::string*>()) {
            std::string_view s{*strPtr};
            r.rawText = s;

            if (s.empty() || s.front() != '#') {
                r.issue.kind = Kind::InvalidStringFormat;
                return r;
            }

            s.remove_prefix(1);

            const bool hasAlpha = (s.size() == 8);
            if (s.size() != 6 && !hasAlpha) {
                r.issue.kind = Kind::InvalidStringFormat;
                return r;
            }

            std::uint8_t rr = 0;
            std::uint8_t gg = 0;
            std::uint8_t bb = 0;
            std::uint8_t aa = 255;

            if (!parseHexByte(s.substr(0, 2), rr)) {
                r.issue.kind = Kind::InvalidHex;
                r.issue.channel = Ch::R;
                return r;
            }
            if (!parseHexByte(s.substr(2, 2), gg)) {
                r.issue.kind = Kind::InvalidHex;
                r.issue.channel = Ch::G;
                return r;
            }
            if (!parseHexByte(s.substr(4, 2), bb)) {
                r.issue.kind = Kind::InvalidHex;
                r.issue.channel = Ch::B;
                return r;
            }
            if (hasAlpha) {
                if (!parseHexByte(s.substr(6, 2), aa)) {
                    r.issue.kind = Kind::InvalidHex;
                    r.issue.channel = Ch::A;
                    return r;
                }
            }

            r.value = sf::Color{rr, gg, bb, aa};
            return r;
        }

        // Объект { "r": ..., "g": ..., "b": ..., "a": ... }
        if (value.is_object()) {
            auto rr = fallback.r;
            auto gg = fallback.g;
            auto bb = fallback.b;
            auto aa = fallback.a;

            auto parseChannel = [&](const char* field, Issue::Channel ch,
                                    std::uint8_t& dst) -> bool {
                const json* v = core::utils::json::detail::findObjectMemberNoAlloc(value, field);
                if (v == nullptr) {
                    return true; // поля нет -> оставляем fallback
                }

                if (!v->is_number()) {
                    r.issue.kind = Kind::ObjectFieldNotNumber;
                    r.issue.channel = ch;
                    return false;
                }

                std::uint8_t tmp = 0;
                if (!tryGetU8FromNumber(*v, tmp)) {
                    r.issue.kind = Kind::ObjectFieldOutOfRange;
                    r.issue.channel = ch;
                    return false;
                }

                dst = tmp;
                return true;
            };

            if (!parseChannel("r", Ch::R, rr) || !parseChannel("g", Ch::G, gg) ||
                !parseChannel("b", Ch::B, bb) || !parseChannel("a", Ch::A, aa)) {
                return r;
            }

            r.value = sf::Color{rr, gg, bb, aa};
            return r;
        }

        r.issue.kind = Kind::WrongTopType;
        return r;
    }

    sf::Keyboard::Key parseKeyString(std::string_view name) noexcept {
        using K = sf::Keyboard::Key;

        static constexpr std::array<std::pair<std::string_view, K>, 15> kMap{{
            {"W", K::W},
            {"A", K::A},
            {"S", K::S},
            {"D", K::D},
            {"Up", K::Up},
            {"Down", K::Down},
            {"Left", K::Left},
            {"Right", K::Right},
            {"Space", K::Space},
            {"LShift", K::LShift},
            {"RShift", K::RShift},
            {"LControl", K::LControl},
            {"RControl", K::RControl},
            {"LAlt", K::LAlt},
            {"RAlt", K::RAlt},
        }};

        for (const auto& [s, k] : kMap) {
            if (name == s) {
                return k;
            }
        }

        return K::Unknown;
    }

    [[nodiscard]] bool tryGetU8FromNumber(const json& v, std::uint8_t& out) noexcept {
        if (const auto* pu = v.get_ptr<const json::number_unsigned_t*>()) {
            const auto u = *pu;
            if (u > 255u) {
                return false;
            }
            out = static_cast<std::uint8_t>(u);
            return true;
        }

        if (const auto* ps = v.get_ptr<const json::number_integer_t*>()) {
            const auto s = *ps;
            if (s < 0 || s > 255) {
                return false;
            }
            out = static_cast<std::uint8_t>(s);
            return true;
        }

        if (const auto* pf = v.get_ptr<const json::number_float_t*>()) {
            const double d = *pf;
            if (!std::isfinite(d)) {
                return false;
            }
            const double t = std::trunc(d);
            if (d != t || t < 0.0 || t > 255.0) {
                return false;
            }
            out = static_cast<std::uint8_t>(t);
            return true;
        }

        return false;
    }

    [[nodiscard]] core::utils::json::Vec2ParseResult
    parseVec2fFromValueWithIssue(const json& value, const sf::Vector2f& defaultValue) noexcept {
        using core::utils::json::Vec2ParseIssue;
        using core::utils::json::Vec2ParseResult;

        Vec2ParseResult r{};
        r.value = defaultValue;

        if (value.is_number()) {
            float v = 0.f;
            if (!core::utils::json::detail::tryGetFiniteFloat(value, v)) {
                r.issue.kind = Vec2ParseIssue::Kind::NumberNotFiniteOrOutOfRange;
                r.issue.elementIndex = kInvalidIndex;
                r.issue.axis = Vec2ParseIssue::Axis::None;
                return r;
            }
            r.value = {v, v};
            return r;
        }

        if (value.is_array()) {
            if (value.size() < 2) {
                r.issue.kind = Vec2ParseIssue::Kind::ArrayTooSmall;
                return r;
            }
            // Безопасно: size() >= 2 уже проверено выше.
            const auto& x = value[0];
            const auto& y = value[1];

            if (!x.is_number()) {
                r.issue.kind = Vec2ParseIssue::Kind::ArrayElementNotNumber;
                r.issue.elementIndex = 0;
                return r;
            }

            if (!y.is_number()) {
                r.issue.kind = Vec2ParseIssue::Kind::ArrayElementNotNumber;
                r.issue.elementIndex = 1;
                return r;
            }

            float fx = 0.f;
            if (!core::utils::json::detail::tryGetFiniteFloat(x, fx)) {
                r.issue.kind = Vec2ParseIssue::Kind::NumberNotFiniteOrOutOfRange;
                r.issue.elementIndex = 0;
                r.issue.axis = Vec2ParseIssue::Axis::None;
                return r;
            }

            float fy = 0.f;
            if (!core::utils::json::detail::tryGetFiniteFloat(y, fy)) {
                r.issue.kind = Vec2ParseIssue::Kind::NumberNotFiniteOrOutOfRange;
                r.issue.elementIndex = 1;
                r.issue.axis = Vec2ParseIssue::Axis::None;
                return r;
            }

            r.value = {fx, fy};
            return r;
        }

        if (value.is_object()) {
            float x = defaultValue.x;
            float y = defaultValue.y;

            if (const json* xV = core::utils::json::detail::findObjectMemberNoAlloc(value, "x")) {
                if (!xV->is_number()) {
                    r.issue.kind = Vec2ParseIssue::Kind::ObjectFieldNotNumber;
                    r.issue.axis = Vec2ParseIssue::Axis::X;
                    return r;
                }

                float fx = 0.f;
                if (!core::utils::json::detail::tryGetFiniteFloat(*xV, fx)) {
                    r.issue.kind = Vec2ParseIssue::Kind::NumberNotFiniteOrOutOfRange;
                    r.issue.axis = Vec2ParseIssue::Axis::X;
                    r.issue.elementIndex = kInvalidIndex;
                    return r;
                }

                x = fx;
            }

            if (const json* yV = core::utils::json::detail::findObjectMemberNoAlloc(value, "y")) {
                if (!yV->is_number()) {
                    r.issue.kind = Vec2ParseIssue::Kind::ObjectFieldNotNumber;
                    r.issue.axis = Vec2ParseIssue::Axis::Y;
                    return r;
                }

                float fy = 0.f;
                if (!core::utils::json::detail::tryGetFiniteFloat(*yV, fy)) {
                    r.issue.kind = Vec2ParseIssue::Kind::NumberNotFiniteOrOutOfRange;
                    r.issue.axis = Vec2ParseIssue::Axis::Y;
                    r.issue.elementIndex = kInvalidIndex;
                    return r;
                }

                y = fy;
            }

            r.value = {x, y};
            return r;
        }

        r.issue.kind = Vec2ParseIssue::Kind::WrongTopType;
        return r;
    }

    [[nodiscard]] core::utils::json::FloatParseResult
    parseFloatFromValueWithIssue(const json& v, float defaultValue, float minValue, float maxValue,
                                 bool useRange) noexcept {
        using core::utils::json::FloatParseIssue;
        using core::utils::json::FloatParseResult;

        FloatParseResult r{};
        r.value = defaultValue;

        if (!v.is_number()) {
            r.issue.kind = FloatParseIssue::Kind::WrongType;
            return r;
        }

        float out = 0.f;
        if (!core::utils::json::detail::tryGetFiniteFloat(v, out)) {
            r.issue.kind = FloatParseIssue::Kind::NotFiniteOrOutOfRange;
            return r;
        }

        if (useRange) {
            if (out < minValue || out > maxValue) {
                r.issue.kind = FloatParseIssue::Kind::OutOfRange;
                return r;
            }
        }

        r.value = out;
        return r;
    }

} // namespace

namespace core::utils::json::detail {

    [[nodiscard]] StringViewParseResult parseStringViewWithIssue(const json& data,
                                                                 std::string_view key) noexcept {
        StringViewParseResult r{};

        const json* v = findObjectMemberNoAlloc(data, key);
        if (v == nullptr) {
            r.issue.kind = StringViewParseIssue::Kind::MissingKey;
            return r;
        }

        const auto* ptr = v->get_ptr<const std::string*>();
        if (ptr == nullptr) {
            r.issue.kind = StringViewParseIssue::Kind::WrongType;
            return r;
        }

        r.value = std::string_view{*ptr};
        return r;
    }

    [[nodiscard]] bool tryGetFiniteFloat(const json& v, float& out) noexcept {
        using FloatT = json::number_float_t;
        using IntT = json::number_integer_t;
        using UIntT = json::number_unsigned_t;

        const long double minF = static_cast<long double>(std::numeric_limits<float>::lowest());
        const long double maxF = static_cast<long double>(std::numeric_limits<float>::max());

        if (const auto* pf = v.get_ptr<const FloatT*>()) {
            const FloatT dRaw = *pf;
            if (!std::isfinite(dRaw)) {
                return false;
            }

            const long double d = static_cast<long double>(dRaw);
            if (d < minF || d > maxF) {
                return false;
            }

            out = static_cast<float>(dRaw);
            return true;
        }

        if (const auto* pi = v.get_ptr<const IntT*>()) {
            // Примечание: Конвертация целых в float может потерять точность
            // (особенно на больших значениях). Это ожидаемая семантика парсера конфигов:
            // мы гарантируем конечность и попадание в диапазон float,
            // но не требуем точного представления каждого большого целого.
            const IntT s = *pi;
            const long double d = static_cast<long double>(s);
            if (d < minF || d > maxF) {
                return false;
            }
            out = static_cast<float>(d);
            return true;
        }

        if (const auto* pu = v.get_ptr<const UIntT*>()) {
            // Примечание: Конвертация целых в float может потерять точность
            // (особенно на больших значениях). Это ожидаемая семантика парсера конфигов:
            // мы гарантируем конечность и попадание в диапазон float,
            // но не требуем точного представления каждого большого целого.
            const UIntT u = *pu;
            const long double d = static_cast<long double>(u);
            if (d < minF || d > maxF) {
                return false;
            }
            out = static_cast<float>(d);
            return true;
        }

        return false;
    }

} // namespace core::utils::json::detail

namespace core::utils::json {

    std::string_view describe(const EnumParseIssue& issue) noexcept {
        using Kind = EnumParseIssue::Kind;

        switch (issue.kind) {
        case Kind::None:
            return "OK";
        case Kind::MissingKey:
            return "ключ отсутствует";
        case Kind::WrongType:
            return "ожидалась строка";
        case Kind::UnknownValue:
            return "неизвестное значение";
        default:
            return "неизвестная ошибка";
        }
    }

    std::string_view describe(const Vec2ParseIssue& issue) noexcept {
        using Kind = Vec2ParseIssue::Kind;

        switch (issue.kind) {
        case Kind::None:
            return "OK";
        case Kind::MissingKey:
            return "ключ отсутствует";
        case Kind::WrongTopType:
            return "ожидалось число/массив/объект";
        case Kind::ArrayTooSmall:
            return "массив должен содержать минимум 2 элемента";
        case Kind::ArrayElementNotNumber:
            return (issue.elementIndex == 0) ? "элемент массива [0] не число"
                                             : "элемент массива [1] не число";
        case Kind::ObjectFieldNotNumber:
            switch (issue.axis) {
            case Vec2ParseIssue::Axis::None:
                return "поле объекта не число";
            case Vec2ParseIssue::Axis::X:
                return "поле объекта 'x' не число";
            case Vec2ParseIssue::Axis::Y:
                return "поле объекта 'y' не число";
            default:
                return "поле объекта не число";
            }
        case Kind::NumberNotFiniteOrOutOfRange:
            switch (issue.axis) {
            case Vec2ParseIssue::Axis::None:
                break;
            case Vec2ParseIssue::Axis::X:
                return "значение 'x' не конечное или вне диапазона float";
            case Vec2ParseIssue::Axis::Y:
                return "значение 'y' не конечное или вне диапазона float";
            default:
                if (issue.elementIndex == 0) {
                    return "элемент массива [0] не конечный или вне диапазона float";
                }
                if (issue.elementIndex == 1) {
                    return "элемент массива [1] не конечный или вне диапазона float";
                }
                return "значение не конечное или вне диапазона float";
            }
        default:
            return "неизвестная ошибка";
        }
    }

    Vec2ParseResult parseVec2fWithIssue(const json& data, std::string_view key,
                                        const sf::Vector2f& defaultValue) noexcept {
        const json* v = detail::findObjectMemberNoAlloc(data, key);
        if (v == nullptr) {
            Vec2ParseResult r{};
            r.value = defaultValue;
            r.issue.kind = Vec2ParseIssue::Kind::MissingKey;
            return r;
        }

        return ::parseVec2fFromValueWithIssue(*v, defaultValue);
    }

    std::string_view describe(const KeyParseIssue& issue) noexcept {
        using Kind = KeyParseIssue::Kind;

        switch (issue.kind) {
        case Kind::None:
            return "OK";
        case Kind::MissingKey:
            return "ключ отсутствует";
        case Kind::WrongType:
            return "ожидалась строка";
        case Kind::UnknownName:
            return "неизвестное имя клавиши";
        default:
            return "неизвестная ошибка";
        }
    }

    KeyParseResult parseKeyWithIssue(const json& data, std::string_view key,
                                     sf::Keyboard::Key defaultValue) noexcept {
        KeyParseResult r{};
        r.value = defaultValue;

        const auto s = detail::parseStringViewWithIssue(data, key);
        using SK = detail::StringViewParseIssue::Kind;

        if (s.issue.kind == SK::MissingKey) {
            r.issue.kind = KeyParseIssue::Kind::MissingKey;
            return r;
        }
        if (s.issue.kind == SK::WrongType) {
            r.issue.kind = KeyParseIssue::Kind::WrongType;
            return r;
        }

        const sf::Keyboard::Key parsed = ::parseKeyString(s.value);
        if (parsed == sf::Keyboard::Key::Unknown) {
            r.issue.kind = KeyParseIssue::Kind::UnknownName;
            r.rawName = s.value;
            return r;
        }

        r.value = parsed;
        return r;
    }

    std::string_view describe(const ColorParseIssue& issue) noexcept {
        using Kind = ColorParseIssue::Kind;
        using Ch = ColorParseIssue::Channel;

        switch (issue.kind) {
        case Kind::None:
            return "OK";
        case Kind::MissingKey:
            return "ключ отсутствует";
        case Kind::WrongTopType:
            return "ожидалась строка '#RRGGBB'/'#RRGGBBAA' или объект {r,g,b,a}";
        case Kind::InvalidStringFormat:
            return "неверный формат строки цвета";
        case Kind::InvalidHex:
            switch (issue.channel) {
            case Ch::None:
                return "неверные hex-цифры";
            case Ch::R:
                return "неверные hex-цифры канала R";
            case Ch::G:
                return "неверные hex-цифры канала G";
            case Ch::B:
                return "неверные hex-цифры канала B";
            case Ch::A:
                return "неверные hex-цифры канала A";
            default:
                return "неверные hex-цифры";
            }
        case Kind::ObjectFieldNotNumber:
            switch (issue.channel) {
            case Ch::None:
                return "поле объекта не число";
            case Ch::R:
                return "поле 'r' не число";
            case Ch::G:
                return "поле 'g' не число";
            case Ch::B:
                return "поле 'b' не число";
            case Ch::A:
                return "поле 'a' не число";
            default:
                return "поле объекта не число";
            }
        case Kind::ObjectFieldOutOfRange:
            switch (issue.channel) {
            case Ch::None:
                return "поле объекта вне диапазона 0..255";
            case Ch::R:
                return "поле 'r' вне диапазона 0..255";
            case Ch::G:
                return "поле 'g' вне диапазона 0..255";
            case Ch::B:
                return "поле 'b' вне диапазона 0..255";
            case Ch::A:
                return "поле 'a' вне диапазона 0..255";
            default:
                return "поле объекта вне диапазона 0..255";
            }
        default:
            return "неизвестная ошибка";
        }
    }

    ColorParseResult parseColorWithIssue(const json& data, std::string_view key,
                                         const sf::Color& defaultValue) noexcept {
        ColorParseResult r{};
        r.value = defaultValue;

        const json* v = detail::findObjectMemberNoAlloc(data, key);
        if (v == nullptr) {
            r.issue.kind = ColorParseIssue::Kind::MissingKey;
            return r;
        }

        return ::parseColorFromValueWithIssue(*v, defaultValue);
    }

    std::string_view describe(const UnsignedParseIssue& issue) noexcept {
        using Kind = UnsignedParseIssue::Kind;

        switch (issue.kind) {
        case Kind::None:
            return "OK";
        case Kind::MissingKey:
            return "ключ отсутствует";
        case Kind::WrongType:
            return "ожидалось целое число";
        case Kind::Negative:
            return "значение отрицательное";
        case Kind::OutOfRange:
            return "значение вне диапазона целевого типа";
        default:
            return "неизвестная ошибка";
        }
    }

    UnsignedParseResult parseUnsignedWithIssue(const json& data, std::string_view key,
                                               unsigned defaultValue) noexcept {
        return parseUIntWithIssue<unsigned>(data, key, defaultValue);
    }

    FloatParseResult parseFloatWithIssue(const json& data, std::string_view key,
                                         float defaultValue) noexcept {
        const json* v = detail::findObjectMemberNoAlloc(data, key);
        if (v == nullptr) {
            FloatParseResult r{};
            r.value = defaultValue;
            r.issue.kind = FloatParseIssue::Kind::MissingKey;
            return r;
        }

        return ::parseFloatFromValueWithIssue(*v, defaultValue, 0.f, 0.f, false);
    }

    FloatParseResult parseFloatWithIssue(const json& data, std::string_view key, float defaultValue,
                                         float minValue, float maxValue) noexcept {
        const json* v = detail::findObjectMemberNoAlloc(data, key);
        if (v == nullptr) {
            FloatParseResult r{};
            r.value = defaultValue;
            r.issue.kind = FloatParseIssue::Kind::MissingKey;
            return r;
        }

        return ::parseFloatFromValueWithIssue(*v, defaultValue, minValue, maxValue, true);
    }

    std::string_view describe(const FloatParseIssue& issue) noexcept {
        using Kind = FloatParseIssue::Kind;

        switch (issue.kind) {
        case Kind::None:
            return "OK";
        case Kind::MissingKey:
            return "ключ отсутствует";
        case Kind::WrongType:
            return "ожидалось число";
        case Kind::NotFiniteOrOutOfRange:
            return "значение не конечное или вне диапазона float";
        case Kind::OutOfRange:
            return "значение вне допустимого диапазона";
        default:
            return "неизвестная ошибка";
        }
    }

    sf::Keyboard::Key parseKey(const json& data, std::string_view key,
                               sf::Keyboard::Key defaultValue) noexcept {
        return parseKeyWithIssue(data, key, defaultValue).value;
    }

    sf::Color parseColor(const json& data, std::string_view key,
                         const sf::Color& defaultValue) noexcept {
        return parseColorWithIssue(data, key, defaultValue).value;
    }

} // namespace core::utils::json

#ifdef _MSC_VER
#pragma warning(pop)
#endif