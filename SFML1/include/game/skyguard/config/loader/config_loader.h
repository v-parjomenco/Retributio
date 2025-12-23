// ================================================================================================
// File: game/skyguard/config/loader/config_loader.h
// Purpose: High-level player configuration loader (JSON -> Player)
// Used by: Game, PlayerInitSystem
// Related headers: game/skyguard/config/config_keys.h, core/utils/json/json_utils.h,
//                  core/utils/json/json_validator.h, core/utils/file_loader.h,
//                  game/skyguard/config/blueprints/player_blueprint.h
// ================================================================================================
#pragma once

#include <string>

#include "game/skyguard/config/blueprints/player_blueprint.h"

namespace game::skyguard::config {

    /**
     * @brief Высокоуровневый загрузчик, преобразующий JSON-файл в PlayerBlueprint.
     *
     * Обязанности:
     *  - Использует FileLoader, чтобы прочесть JSON-файл как текст.
     *  - Строит дерево Json из текста с помощью json::parse от nlohmann.
     *  - С помощью JsonValidator валидирует структуру JSON-файла (parseAndValidateCritical).
     *  - Превращает поля JSON-файла в набор properties::* и собирает PlayerBlueprint.
     *
     * НЕ:
     *  - занимается низкоуровневой загрузкой ресурсов (диском/Texture/Font),
     *  - знает что-либо о рендеринге, ECS и прочем.
     */
    class ConfigLoader {
      public:
        /**
     * @brief Загружает JSON из файла и парсит данные в PlayerBlueprint.
     *
     * Поведение:
     *  - Если файл не может быть прочитан:
     *      -> лог в core::log (Config/WARN) + возврат значения PlayerBlueprint по умолчанию.
     *  - Если информация в JSON невалидна или искажена:
     *      -> parseAndValidateCritical логирует через core::log и инициирует фатальное
     *         завершение (panic).
     *  - Если структура JSON неверна:
     *      -> parseAndValidateCritical логирует через core::log и инициирует фатальное
     *         завершение (panic).
     */
        static blueprints::PlayerBlueprint loadPlayerConfig(const std::string& path);
    };

} // namespace game::skyguard::config