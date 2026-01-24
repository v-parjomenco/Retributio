// ================================================================================================
// File: core/resources/config/texture_resource_config.h
// Purpose: Data-only low-level flags; file path is stored in ResourceEntry::path
// Used by: ResourceRegistry, ResourceManager, tools
// Notes:
//  - Describes how a texture should be loaded and configured at resource level.
//  - High-level usage (UI, terrain, entities) lives in other layers (blueprints/properties).
// ================================================================================================
#pragma once

namespace core::resources::config {

    /**
     * @brief Конфигурация текстурного ресурса.
     *
     * smooth         — сглаживание (антиалиасинг, мыльность);
     * repeated       — режим повтора (tiling) для тайлов/фонов;
     * generateMipmap — нужно ли генерировать mipmap для дальних планов.
     * srgb           — трактовать текстуру как sRGB (true для color-данных).
     *
     * Эти флаги относятся к уровню ресурса (sf::Texture), а не к уровню сущности/игры.
     */
    struct TextureResourceConfig {
        bool smooth = true;
        bool repeated = false;
        bool generateMipmap = false;
        bool srgb = true;
    };

} // namespace core::resources::config