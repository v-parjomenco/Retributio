// ================================================================================================
// File: core/resources/config/font_resource_config.h
// Purpose: Data-only low-level flags; file path is stored in ResourceEntry::path
// Used by: ResourceRegistry, ResourceManager, tools
// Notes:
//  - Higher-level text rendering (character size, color, layout) lives in TextProperties/UI.
// ================================================================================================
#pragma once

namespace core::resources::config {

    /**
     * @brief Конфигурация шрифтового ресурса.
     *
     * Пока пуста. Поля (hinting, kerning-профили,
     * предзагрузка глифов) могут быть добавлены позже,
     * без изменения интерфейса ResourceRegistry.
     */
    struct FontResourceConfig {
        // TODO: будущие параметры низкоуровневой загрузки шрифтов:
        //  - режим hinting'а,
        //  - предзагрузка диапазонов глифов,
        //  - специфичные флаги движка (sRGB, и т.п.).
    };

} // namespace core::resources::config