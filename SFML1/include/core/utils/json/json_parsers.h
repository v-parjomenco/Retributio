// ================================================================================================
// File: core/utils/json/json_parsers.h
// Purpose: "WithIssue" diagnostic parsers for enum and common SFML value types (no logs, noexcept).
// Used by: Config loaders, other parsers.
// Notes:
//  - All public parsers are noexcept and must not trigger allocations/exceptions.
//  - string_view fields in results are valid only while the source json object is alive.
// ================================================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Keyboard.hpp>

#include "core/utils/json/json_common.h"

// ВАЖНО: здесь есть inline/template код, использующий методы и nested-типы nlohmann::json.
// json_common.h обычно даёт только forward-decl, поэтому подключаем полное определение.
#include "adapters/json/json_silent.hpp"

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

        // ----------------------------------------------------------------------------------------
        // No-alloc поиск по ключу в object:
        //  - nlohmann::json::find(key) обычно требует std::string (потенциально аллокации)
        //  - здесь сравниваем string_view с ключами объекта без выделений памяти
        // ----------------------------------------------------------------------------------------

        [[nodiscard]] inline bool equalsKeyNoAlloc(const std::string& s,
                                                   std::string_view key) noexcept {
            if (s.size() != key.size()) {
                return false;
            }
            if (key.empty()) {
                return true;
            }
            return std::memcmp(s.data(), key.data(), key.size()) == 0;
        }

        [[nodiscard]] inline const json* findObjectMemberNoAlloc(const json& obj,
                                                                 std::string_view key) noexcept {
            if (!obj.is_object()) {
                return nullptr;
            }

            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (equalsKeyNoAlloc(it.key(), key)) {
                    return &(*it);
                }
            }

            return nullptr;
        }

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

    template <typename TUInt>
    struct UIntParseResult {
        TUInt value{};
        UnsignedParseIssue issue{};
        std::int64_t rawSigned{0};
        std::uint64_t rawUnsigned{0};
    };

    template <typename TUInt>
    [[nodiscard]] UIntParseResult<TUInt> parseUIntWithIssue(
        const json& data,
        std::string_view key,
        TUInt defaultValue) noexcept;

    using UnsignedParseResult = UIntParseResult<unsigned>;

    [[nodiscard]] UnsignedParseResult parseUnsignedWithIssue(
        const json& data,
        std::string_view key,
        unsigned defaultValue) noexcept;

    [[nodiscard]] std::string_view describe(const UnsignedParseIssue& issue) noexcept;

    // --------------------------------------------------------------------------------------------
    // Inline реализация parseUIntWithIssue<TUInt>
    // --------------------------------------------------------------------------------------------

    template <typename TUInt>
    [[nodiscard]] inline UIntParseResult<TUInt> parseUIntWithIssue(
        const json& data,
        std::string_view key,
        TUInt defaultValue) noexcept
    {
        static_assert(std::is_unsigned_v<TUInt>,
                      "parseUIntWithIssue<TUInt> requires an unsigned integer type.");

        using Kind = UnsignedParseIssue::Kind;
        using IntT = json::number_integer_t;
        using UIntT = json::number_unsigned_t;

        UIntParseResult<TUInt> r{};
        r.value = defaultValue;

        const json* v = detail::findObjectMemberNoAlloc(data, key);
        if (v == nullptr) {
            r.issue.kind = Kind::MissingKey;
            return r;
        }

        constexpr std::uint64_t kMaxU64 =
            static_cast<std::uint64_t>(std::numeric_limits<TUInt>::max());

        if (const auto* pu = v->get_ptr<const UIntT*>()) {
            const std::uint64_t u = static_cast<std::uint64_t>(*pu);
            r.rawUnsigned = u;

            if (u > kMaxU64) {
                r.issue.kind = Kind::OutOfRange;
                return r;
            }

            r.value = static_cast<TUInt>(u);
            return r;
        }

        if (const auto* ps = v->get_ptr<const IntT*>()) {
            const std::int64_t s = static_cast<std::int64_t>(*ps);
            r.rawSigned = s;

            if (s < 0) {
                r.issue.kind = Kind::Negative;
                return r;
            }

            const std::uint64_t u = static_cast<std::uint64_t>(s);
            r.rawUnsigned = u;

            if (u > kMaxU64) {
                r.issue.kind = Kind::OutOfRange;
                return r;
            }

            r.value = static_cast<TUInt>(u);
            return r;
        }

        r.issue.kind = Kind::WrongType;
        return r;
    }

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