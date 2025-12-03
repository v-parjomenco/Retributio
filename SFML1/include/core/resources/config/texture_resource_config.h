// ================================================================================================
// File: core/resources/config/texture_resource_config.h
// Purpose: Data-only configuration for texture resources (path + low-level GPU flags)
// Used by: ResourcePaths, ResourceManager, tools
// Notes:
//  - Describes how a texture should be loaded and configured at resource level.
//  - High-level usage (UI, terrain, entities) lives in other layers (blueprints/properties).
// ================================================================================================
#pragma once

#include <string>

namespace core::resources::config {

    /**
     * @brief Конфигурация текстурного ресурса.
     *
     * path           — путь к файлу на диске;
     * smooth         — сглаживание (антиалиасинг, мыльность);
     * repeated       — режим повтора (tiling) для тайлов/фонов;
     * generateMipmap — нужно ли генерировать mipmap для дальних планов.
     *
     * Эти флаги относятся к уровню ресурса (sf::Texture), а не к уровню сущности/игры.
     */
    struct TextureResourceConfig {
        std::string path;

        bool smooth = true;
        bool repeated = false;
        bool generateMipmap = false;
    };

} // namespace core::resources::config