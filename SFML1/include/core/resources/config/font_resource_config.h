// ================================================================================================
// File: core/resources/config/font_resource_config.h
// Purpose: Data-only configuration for font resources (path, future low-level flags)
// Used by: ResourceRegistry, ResourceManager, tools
// Notes:
//  - Describes where the font file is located.
//  - Higher-level text rendering (character size, color, layout) lives in TextProperties/UI.
// ================================================================================================
#pragma once

#include <string>

namespace core::resources::config {

    /**
     * @brief Конфигурация шрифтового ресурса.
     *
     * Сейчас здесь только путь к файлу. Дополнительные поля (hinting, kerning-профили,
     * предзагрузка глифов) могут быть добавлены позже, без изменения интерфейса ResourceRegistry.
     */
    struct FontResourceConfig {
        std::string path;

        // TODO: будущие параметры низкоуровневой загрузки шрифтов:
        //  - режим hinting'а,
        //  - предзагрузка диапазонов глифов,
        //  - специфичные флаги движка (sRGB, и т.п.).
    };

} // namespace core::resources::config