// ================================================================================================
// File: core/resources/ids/resource_ids.h
// Purpose: Enum identifiers for engine resources (textures, fonts, sounds)
// Used by: ResourceManager, ResourcePaths, id_to_string, gameplay code
// Notes:
//  - Enum values describe the *current build's* resource set (e.g. SkyGuard).
//  - For multi-game setups these IDs can be split per-game while keeping the same patterns.
//
//  - COLD PATH ONLY: these helpers are for logs/diagnostics/errors.
//    Do not call them in tight loops / hot paths (e.g., per-entity RenderSystem code).
// ================================================================================================
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace core::resources::ids {

    // --------------------------------------------------------------------------------------------
    // Текстуры
    // --------------------------------------------------------------------------------------------
    enum class TextureID : std::uint16_t {
        Unknown = 0,
        Player,
        BackgroundDesert,
        Count,
        // TODO: добавлять другие ID текстур здесь (н-р, UIFrame, BackgroundMain и т.д.)
    };

    // --------------------------------------------------------------------------------------------
    // Шрифты
    // --------------------------------------------------------------------------------------------
    enum class FontID : std::uint16_t {
        Unknown = 0,
        Default,
        Count,
        // TODO: добавлять другие ID шрифтов здесь (н-р, DebugMono, TitleFont и т.д.)
    };

    // --------------------------------------------------------------------------------------------
    // Звуки
    // --------------------------------------------------------------------------------------------
    enum class SoundID : std::uint16_t {
        Unknown = 0,
        Count,
        // TODO: добавлять ID звуков, когда звуки появятся в resources.json
    };

    // --------------------------------------------------------------------------------------------
    // ResourceId concept
    // --------------------------------------------------------------------------------------------

    template <typename T>
    inline constexpr bool isResourceId =
        std::is_same_v<std::remove_cvref_t<T>, TextureID>
        || std::is_same_v<std::remove_cvref_t<T>, FontID>
        || std::is_same_v<std::remove_cvref_t<T>, SoundID>;

    template <typename T>
    concept ResourceId = isResourceId<T>;

    // --------------------------------------------------------------------------------------------
    // Единый источник правды: enum -> string_view (O(1)) и string_view -> enum (O(log N)).
    // --------------------------------------------------------------------------------------------

    inline constexpr std::array<std::string_view,
                                static_cast<std::size_t>(TextureID::Count)>
        textureNamesByIndex = {
            "UnknownTexture",
            "Player",
            "BackgroundDesert",
        };

    inline constexpr std::array<std::pair<std::string_view, TextureID>,
                                static_cast<std::size_t>(TextureID::Count)>
        textureNamesSorted = {
            std::pair<std::string_view, TextureID>{"BackgroundDesert", TextureID::BackgroundDesert},
            std::pair<std::string_view, TextureID>{"Player", TextureID::Player},
            std::pair<std::string_view, TextureID>{"UnknownTexture", TextureID::Unknown},
        };

    inline constexpr std::array<std::string_view,
                                static_cast<std::size_t>(FontID::Count)>
        fontNamesByIndex = {
            "UnknownFont",
            "Default",
        };

    inline constexpr std::array<std::pair<std::string_view, FontID>,
                                static_cast<std::size_t>(FontID::Count)>
        fontNamesSorted = {
            std::pair<std::string_view, FontID>{"Default", FontID::Default},
            std::pair<std::string_view, FontID>{"UnknownFont", FontID::Unknown},
        };

    inline constexpr std::array<std::string_view,
                                static_cast<std::size_t>(SoundID::Count)>
        soundNamesByIndex = {
            "UnknownSound",
        };

    inline constexpr std::array<std::pair<std::string_view, SoundID>,
                                static_cast<std::size_t>(SoundID::Count)>
        soundNamesSorted = {
            std::pair<std::string_view, SoundID>{"UnknownSound", SoundID::Unknown},
        };

    namespace detail {

        template <ResourceId Enum>
        [[nodiscard]] constexpr const auto& namesByIndex() noexcept {
            if constexpr (std::is_same_v<std::remove_cvref_t<Enum>, TextureID>) {
                return textureNamesByIndex;
            } else if constexpr (std::is_same_v<std::remove_cvref_t<Enum>, FontID>) {
                return fontNamesByIndex;
            } else {
                return soundNamesByIndex;
            }
        }

        template <ResourceId Enum>
        [[nodiscard]] constexpr const auto& namesSorted() noexcept {
            if constexpr (std::is_same_v<std::remove_cvref_t<Enum>, TextureID>) {
                return textureNamesSorted;
            } else if constexpr (std::is_same_v<std::remove_cvref_t<Enum>, FontID>) {
                return fontNamesSorted;
            } else {
                return soundNamesSorted;
            }
        }

        [[nodiscard]] constexpr bool isSortedByName(auto&& names) noexcept {
            for (std::size_t index = 1; index < names.size(); ++index) {
                if (!(names[index - 1].first < names[index].first)) {
                    return false;
                }
            }
            return true;
        }

        template <ResourceId Enum>
        [[nodiscard]] constexpr std::string_view invalidName() noexcept {
            if constexpr (std::is_same_v<std::remove_cvref_t<Enum>, TextureID>) {
                return "InvalidTexture";
            } else if constexpr (std::is_same_v<std::remove_cvref_t<Enum>, FontID>) {
                return "InvalidFont";
            } else {
                return "InvalidSound";
            }
        }

        // Жёсткая compile-time валидация таблиц:
        //  - каждый Enum встречается РОВНО 1 раз в namesSorted
        //  - byIndex[id] == name для каждой пары
        //  - нет выходов за диапазон
        template <ResourceId Enum>
        [[nodiscard]] constexpr bool areNamesConsistent() noexcept {
            const auto& byIndex = namesByIndex<Enum>();
            const auto& sorted  = namesSorted<Enum>();

            if (sorted.size() != byIndex.size()) {
                return false;
            }

            std::array<bool, byIndex.size()> seen{}; // все false

            for (const auto& [name, id] : sorted) {
                using Underlying = std::underlying_type_t<std::remove_cvref_t<Enum>>;
                const auto index = static_cast<std::size_t>(static_cast<Underlying>(id));

                if (index >= byIndex.size()) {
                    return false;
                }

                if (seen[index]) {
                    // Дубликат Enum -> не биекция.
                    return false;
                }
                seen[index] = true;

                if (byIndex[index] != name) {
                    return false;
                }
            }

            // Проверяем, что каждый индекс был покрыт.
            for (bool hit : seen) {
                if (!hit) {
                    return false;
                }
            }

            return true;
        }

    } // namespace detail

    static_assert(textureNamesByIndex.size() == static_cast<std::size_t>(TextureID::Count));
    static_assert(fontNamesByIndex.size() == static_cast<std::size_t>(FontID::Count));
    static_assert(soundNamesByIndex.size() == static_cast<std::size_t>(SoundID::Count));

    static_assert(detail::isSortedByName(textureNamesSorted));
    static_assert(detail::isSortedByName(fontNamesSorted));
    static_assert(detail::isSortedByName(soundNamesSorted));

    static_assert(detail::areNamesConsistent<TextureID>());
    static_assert(detail::areNamesConsistent<FontID>());
    static_assert(detail::areNamesConsistent<SoundID>());

    // --------------------------------------------------------------------------------------------
    // toString/fromString — единый API для enum ↔ string_view
    //
    // Контракт:
    //  - toString() safe (не UB): invalid/Count/out-of-range -> "Invalid*".
    //  - cold path only (логи/диагностика/загрузка).
    // --------------------------------------------------------------------------------------------

    template <ResourceId Enum>
    [[nodiscard]] constexpr std::string_view toString(Enum id) noexcept {
        using Underlying = std::underlying_type_t<std::remove_cvref_t<Enum>>;
        const auto index = static_cast<std::size_t>(static_cast<Underlying>(id));

        const auto& table = detail::namesByIndex<Enum>();
        if (index >= table.size()) {
            return detail::invalidName<Enum>();
        }

        return table[index];
    }

    template <ResourceId Enum>
    [[nodiscard]] inline std::optional<std::remove_cvref_t<Enum>>
    fromString(std::string_view name) noexcept {
        using CleanEnum = std::remove_cvref_t<Enum>;

        const auto& names = detail::namesSorted<CleanEnum>();
        const auto  it    = std::lower_bound(
            names.begin(),
            names.end(),
            name,
            [](const auto& pair, std::string_view key) { return pair.first < key; });

        if (it != names.end() && it->first == name) {
            return it->second;
        }

        return std::nullopt;
    }

    // --------------------------------------------------------------------------------------------
    // Каноничное сравнение/сортировка ID: перевод в underlying type.
    // --------------------------------------------------------------------------------------------
    template <typename Enum>
    [[nodiscard]] constexpr std::underlying_type_t<Enum> toUnderlying(Enum id) noexcept {
        static_assert(std::is_enum_v<Enum>, "toUnderlying expects enum type");
        return static_cast<std::underlying_type_t<Enum>>(id);
    }

} // namespace core::resources::ids

// ------------------------------------------------------------------------------------------------
// Hash-специализации для resource ids
// ------------------------------------------------------------------------------------------------
namespace std {

    template <> struct hash<core::resources::ids::TextureID> {
        std::size_t operator()(core::resources::ids::TextureID id) const noexcept {
            using Underlying = std::underlying_type_t<core::resources::ids::TextureID>;
            return std::hash<Underlying>{}(static_cast<Underlying>(id));
        }
    };

    template <> struct hash<core::resources::ids::FontID> {
        std::size_t operator()(core::resources::ids::FontID id) const noexcept {
            using Underlying = std::underlying_type_t<core::resources::ids::FontID>;
            return std::hash<Underlying>{}(static_cast<Underlying>(id));
        }
    };

    template <> struct hash<core::resources::ids::SoundID> {
        std::size_t operator()(core::resources::ids::SoundID id) const noexcept {
            using Underlying = std::underlying_type_t<core::resources::ids::SoundID>;
            return std::hash<Underlying>{}(static_cast<Underlying>(id));
        }
    };

} // namespace std