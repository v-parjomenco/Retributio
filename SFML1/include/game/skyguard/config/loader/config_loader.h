// ================================================================================================
// File: game/skyguard/config/loader/config_loader.h
// Purpose: High-level player configuration loader (JSON -> Player)
// Used by: Game, PlayerInitSystem
// Related headers: game/skyguard/config/config_keys.h,
//                  core/utils/file_loader.h,
//                  core/utils/json/json_accessors.h, core/utils/json/json_document.h,
//                  core/utils/json/json_parsers.h,
//                  core/resources/ids/resource_id_utils.h, core/ui/ids/ui_id_utils.h,
//                  game/skyguard/config/blueprints/player_blueprint.h
// ================================================================================================
#pragma once

#include <string>

#include "game/skyguard/config/blueprints/player_blueprint.h"

namespace game::skyguard::config {

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
        [[nodiscard]] static blueprints::PlayerBlueprint loadPlayerConfig(const std::string& path);
    };

} // namespace game::skyguard::config