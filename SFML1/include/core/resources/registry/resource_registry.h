// ================================================================================================
// File: core/resources/registry/resource_registry.h
// Purpose: Runtime registry of resources with deterministic ordering and stable keys.
// ================================================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/resources/keys/resource_key.h"
#include "core/resources/registry/resource_entry.h"
#include "core/resources/registry/string_pool.h"

namespace core::resources {

    struct ResourceSource final {
        std::string path;
        int layerPriority = 0;
        int loadOrder = 0;
        std::string sourceName;
    };

    struct NameIndex final {
        std::string_view name{};
        std::uint32_t index = 0u;
    };

    class ResourceRegistry final {
      public:
        ResourceRegistry() = default;
        ~ResourceRegistry() = default;

        ResourceRegistry(const ResourceRegistry&) = delete;
        ResourceRegistry& operator=(const ResourceRegistry&) = delete;
        ResourceRegistry(ResourceRegistry&&) noexcept = default;
        ResourceRegistry& operator=(ResourceRegistry&&) noexcept = default;

        // ----------------------------------------------------------------------------------------
        // Load-time API
        // ----------------------------------------------------------------------------------------

        /// Разобрать и зафиксировать определения ресурсов из sources.
        ///
        /// Контракт:
        ///  - validate-on-write: любые ошибки структуры/значений → LOG_PANIC;
        ///  - детерминизм: RuntimeKey32 назначается в порядке StableKey64 (возрастающе);
        ///  - переопределения: layerPriority выше → побеждает; при равенстве loadOrder выше →
        ///    побеждает; полное равенство политики → LOG_PANIC (ошибка авторинга).
        void loadFromSources(std::span<const ResourceSource> sources);

        // ----------------------------------------------------------------------------------------
        // Runtime API — O(1) по индексу
        // ----------------------------------------------------------------------------------------

        [[nodiscard]] const TextureEntry& getTexture(TextureKey key) const noexcept;
        [[nodiscard]] const FontEntry& getFont(FontKey key) const noexcept;
        [[nodiscard]] const SoundEntry* tryGetSound(SoundKey key) const noexcept;

        // ----------------------------------------------------------------------------------------
        // Debug / Tooling API — O(log N)
        // ----------------------------------------------------------------------------------------

        [[nodiscard]] TextureKey findTextureByName(std::string_view name) const;
        [[nodiscard]] TextureKey findTextureByStableKey(std::uint64_t stableKey) const;
        [[nodiscard]] FontKey findFontByName(std::string_view name) const;
        [[nodiscard]] SoundKey findSoundByName(std::string_view name) const;

        // ----------------------------------------------------------------------------------------
        // Fallback keys (валидны после loadFromSources)
        // ----------------------------------------------------------------------------------------

        [[nodiscard]] TextureKey missingTextureKey() const noexcept;
        [[nodiscard]] FontKey missingFontKey() const noexcept;

        // ----------------------------------------------------------------------------------------
        // Metrics
        // ----------------------------------------------------------------------------------------

        [[nodiscard]] std::size_t textureCount() const noexcept;
        [[nodiscard]] std::size_t fontCount() const noexcept;
        [[nodiscard]] std::size_t soundCount() const noexcept;

      private:
        registry::StringPool mStrings;

        std::vector<TextureEntry> mTextures;
        std::vector<FontEntry> mFonts;
        std::vector<SoundEntry> mSounds;

        std::vector<NameIndex> mTextureNameIndex;
        std::vector<NameIndex> mFontNameIndex;
        std::vector<NameIndex> mSoundNameIndex;

        TextureKey mMissingTexture{};
        FontKey mMissingFont{};
    };

} // namespace core::resources