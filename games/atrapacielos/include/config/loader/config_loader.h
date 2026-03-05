// ================================================================================================
// File: config/loader/config_loader.h
// Purpose: High-level player configuration loader (JSON -> Player)
// Used by: Game, PlayerInitSystem
// Related headers: config/config_keys.h,
//                  core/utils/file_loader.h,
//                  core/utils/json/json_document.h, core/utils/json/json_parsers.h,
//                  core/resources/resource_manager.h,
//                  config/blueprints/player_blueprint.h
// ================================================================================================
#pragma once

#include <string>

#include "config/blueprints/player_blueprint.h"

namespace core::resources {
    class ResourceManager;
}

namespace game::atrapacielos::config {

    /**
     * @brief Высокоуровневый загрузчик player-конфига (JSON -> PlayerBlueprint).
     *
     * Контракт:
     *  - читает файл как текст (FileLoader);
     *  - парсит JSON в критическом режиме (ошибка -> LOG_PANIC);
     *  - запрещает дубли ключей (authoring safety);
     *  - извлекает значения через парсеры *WithIssue (единая точка проверки типов/форматов).
     *
     * Этот класс НЕ:
     *  - загружает ресурсы (текстуры/шрифты/звуки);
     *  - знает что-либо о рендеринге, ECS и прочем runtime.
     */
    class ConfigLoader {
      public:
        /**
         * @brief Загружает и разбирает player-конфиг.
         *
         * Ошибки:
         *  - I/O (файл не читается) -> LOG_PANIC (обязательный контент).
         *  - синтаксис JSON / дубли ключей -> LOG_PANIC.
         *  - некорректные обязательные поля (тип/формат/диапазон/семантика) -> LOG_PANIC.
         *  - некорректные опциональные поля -> LOG_WARN и остаются значения по умолчанию.
         */
        [[nodiscard]] static blueprints::PlayerBlueprint loadPlayerConfig(
            core::resources::ResourceManager& resources,
            const std::string& path);
    };

} // namespace game::atrapacielos::config