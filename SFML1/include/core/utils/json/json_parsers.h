// ================================================================================================
// File: core/utils/json/json_parsers.h
// Purpose: "WithIssue" diagnostic parsers for enum and common SFML value types (no logs, noexcept).
// Used by: Config loaders, json_accessors.cpp (parseValue specializations), other parsers.
// Notes:
//  - All public parsers are noexcept and must not trigger allocations/exceptions.
//  - string_view fields in results are valid only while the source json object is alive.
// ================================================================================================
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Keyboard.hpp>

#include "core/utils/json/json_common.h"

namespace core::utils::json {

    // --------------------------------------------------------------------------------------------
    // Диагностический парсер enum (без логов, без исключений наружу)
    // --------------------------------------------------------------------------------------------

    struct EnumParseIssue {
        enum class Kind : std::uint8_t { None, MissingKey, WrongType, UnknownValue };
        Kind kind{Kind::None};
        std::string_view raw{}; // валиден только пока жив исходный json
    };

    template <typename Enum>
    struct EnumParseResult {
        Enum value{};
        EnumParseIssue issue{};
    };

    [[nodiscard]] std::string_view describe(const EnumParseIssue& issue) noexcept;

    namespace detail {

        struct StringViewParseIssue {
            enum class Kind : std::uint8_t { None, MissingKey, WrongType };
            Kind kind{Kind::None};
        };

        struct StringViewParseResult {
            std::string_view value{};
            StringViewParseIssue issue{};
        };

        [[nodiscard]] StringViewParseResult parseStringViewWithIssue(
            const json& data,
            std::string_view key) noexcept;

        StringViewParseResult parseStringViewWithIssue(
            const json&&,
            std::string_view) noexcept = delete;

        [[nodiscard]] bool tryGetFiniteFloat(const json& v, float& out) noexcept;

    } // namespace detail

    template <typename Enum, typename Mapper>
    [[nodiscard]] EnumParseResult<Enum> parseEnumWithIssue(
        const json& data,
        std::string_view key,
        Enum defaultValue,
        Mapper mapper) noexcept
    {
        EnumParseResult<Enum> r{};
        r.value = defaultValue;

        const auto s = detail::parseStringViewWithIssue(data, key);
        using SK = detail::StringViewParseIssue::Kind;

        if (s.issue.kind == SK::MissingKey) {
            r.issue.kind = EnumParseIssue::Kind::MissingKey;
            return r;
        }
        if (s.issue.kind == SK::WrongType) {
            r.issue.kind = EnumParseIssue::Kind::WrongType;
            return r;
        }

        if (auto idOpt = mapper(s.value)) {
            r.value = *idOpt;
            return r;
        }

        r.issue.kind = EnumParseIssue::Kind::UnknownValue;
        r.issue.raw  = s.value;
        return r;
    }

    template <typename Enum, typename Mapper>
    EnumParseResult<Enum> parseEnumWithIssue(
        const json&&,
        std::string_view,
        Enum,
        Mapper) noexcept = delete;

    // --------------------------------------------------------------------------------------------
    // Диагностический парсер sf::Vector2f (без логов, без исключений наружу)
    // --------------------------------------------------------------------------------------------

    struct Vec2ParseIssue {
        enum class Kind : std::uint8_t {
            None,
            MissingKey,
            WrongTopType,
            ArrayTooSmall,
            ArrayElementNotNumber,
            ObjectFieldNotNumber,
            NumberNotFiniteOrOutOfRange
        };

        enum class Axis : std::uint8_t { None, X, Y };

        Kind kind{Kind::None};
        std::uint8_t elementIndex{0};
        Axis axis{Axis::None};
    };

    struct Vec2ParseResult {
        sf::Vector2f value{0.f, 0.f};
        Vec2ParseIssue issue{};
    };

    [[nodiscard]] Vec2ParseResult parseVec2fWithIssue(
        const json& data,
        std::string_view key,
        const sf::Vector2f& defaultValue) noexcept;

    [[nodiscard]] std::string_view describe(const Vec2ParseIssue& issue) noexcept;

    // --------------------------------------------------------------------------------------------
    // Диагностический парсер sf::Keyboard::Key (без логов, без исключений наружу)
    // --------------------------------------------------------------------------------------------

    struct KeyParseIssue {
        enum class Kind : std::uint8_t { None, MissingKey, WrongType, UnknownName };
        Kind kind{Kind::None};
    };

    struct KeyParseResult {
        sf::Keyboard::Key value{sf::Keyboard::Key::Unknown};
        KeyParseIssue issue{};
        std::string_view rawName{}; // валиден только пока жив исходный json
    };

    [[nodiscard]] KeyParseResult parseKeyWithIssue(
        const json& data,
        std::string_view key,
        sf::Keyboard::Key defaultValue) noexcept;

    KeyParseResult parseKeyWithIssue(
        const json&&,
        std::string_view,
        sf::Keyboard::Key) noexcept = delete;

    [[nodiscard]] std::string_view describe(const KeyParseIssue& issue) noexcept;

    // --------------------------------------------------------------------------------------------
    // Диагностический парсер sf::Color (без логов, без исключений наружу)
    // --------------------------------------------------------------------------------------------

    struct ColorParseIssue {
        enum class Kind : std::uint8_t {
            None,
            MissingKey,
            WrongTopType,
            InvalidStringFormat,
            InvalidHex,
            ObjectFieldNotNumber,
            ObjectFieldOutOfRange
        };

        enum class Channel : std::uint8_t { None, R, G, B, A };

        Kind kind{Kind::None};
        Channel channel{Channel::None};
    };

    struct ColorParseResult {
        sf::Color value{0, 0, 0, 255};
        ColorParseIssue issue{};
        std::string_view rawText{}; // валиден только пока жив исходный json
    };

    [[nodiscard]] ColorParseResult parseColorWithIssue(
        const json& data,
        std::string_view key,
        const sf::Color& defaultValue) noexcept;

    ColorParseResult parseColorWithIssue(
        const json&&,
        std::string_view,
        const sf::Color&) noexcept = delete;

    [[nodiscard]] std::string_view describe(const ColorParseIssue& issue) noexcept;

    // --------------------------------------------------------------------------------------------
    // Диагностический парсер unsigned (без логов, без исключений наружу)
    // --------------------------------------------------------------------------------------------

    struct UnsignedParseIssue {
        enum class Kind : std::uint8_t { None, MissingKey, WrongType, Negative, OutOfRange };
        Kind kind{Kind::None};
    };

    struct UnsignedParseResult {
        unsigned value{0u};
        UnsignedParseIssue issue{};
        std::int64_t rawSigned{0};
        std::uint64_t rawUnsigned{0};
    };

    [[nodiscard]] UnsignedParseResult parseUnsignedWithIssue(
        const json& data,
        std::string_view key,
        unsigned defaultValue) noexcept;

    [[nodiscard]] std::string_view describe(const UnsignedParseIssue& issue) noexcept;

    // --------------------------------------------------------------------------------------------
    // Диагностический парсер float (без логов, без исключений наружу)
    // --------------------------------------------------------------------------------------------

    struct FloatParseIssue {
        enum class Kind : std::uint8_t {
            None,
            MissingKey,
            WrongType,
            NotFiniteOrOutOfRange,
            OutOfRange
        };

        Kind kind{Kind::None};
    };

    struct FloatParseResult {
        float value{0.f};
        FloatParseIssue issue{};
    };

    [[nodiscard]] FloatParseResult parseFloatWithIssue(
        const json& data,
        std::string_view key,
        float defaultValue) noexcept;

    [[nodiscard]] FloatParseResult parseFloatWithIssue(
        const json& data,
        std::string_view key,
        float defaultValue,
        float minValue,
        float maxValue) noexcept;

    [[nodiscard]] std::string_view describe(const FloatParseIssue& issue) noexcept;

    // --------------------------------------------------------------------------------------------
    // Упрощённые обёртки (без Issue)
    // --------------------------------------------------------------------------------------------

    [[nodiscard]] sf::Keyboard::Key parseKey(
        const json& data,
        std::string_view key,
        sf::Keyboard::Key defaultValue) noexcept;

    [[nodiscard]] sf::Color parseColor(
        const json& data,
        std::string_view key,
        const sf::Color& defaultValue) noexcept;

} // namespace core::utils::json