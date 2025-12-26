#include "pch.h"

#include "core/config/loader/debug_overlay_loader.h"

#include "core/config/config_keys.h"
#include "core/log/log_macros.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_document.h"
#include "core/utils/json/json_parsers.h"

namespace {

    using core::utils::FileLoader;

    namespace keys = core::config::keys;
    namespace json_utils = core::utils::json;

    using Json = json_utils::json;

} // namespace

namespace core::config {

    DebugOverlayBlueprint loadDebugOverlayBlueprint(const std::string& path) {
        // Стартуем с безопасных дефолтов (источник истины №1 для значений по умолчанию).
        DebugOverlayBlueprint cfg{};

        /**
        * @brief Низкоуровневое чтение файла через FileLoader.
        * Debug overlay — вспомогательный модуль, поэтому при ошибках файла
        * мы не останавливаем игру, а просто работаем с дефолтными настройками.
        */
        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            // FileLoader уже залогировал низкоуровневую I/O-проблему (Engine/DEBUG).
            // Здесь фиксируем высокоуровневый эффект: overlay уходит на дефолт (Type C → WARN).
            LOG_WARN(core::log::cat::Config,
                     "[DebugOverlayLoader] Не удалось прочитать '{}'. "
                     "Используется конфигурация debug overlay по умолчанию.",
                     path);
            return cfg;
        }

        const std::string& fileContent = *fileContentOpt;

        /**
        * @brief Общий helper: парсинг + валидация НЕ критичного конфига.
        * При любой ошибке:
        *  - пишется сообщение уровня DEBUG (если включено),
        *  - возвращаем std::nullopt.
        */
        const auto dataOpt = json_utils::parseAndValidateNonCritical(
            fileContent,
            path,
            "DebugOverlayLoader",
            {},
            json_utils::kConfigParseOnlyOptions);

        // Если парсинг/валидация не удалась — остаёмся на дефолтных настройках.
        if (!dataOpt) {
            LOG_WARN(core::log::cat::Config,
                     "[DebugOverlayLoader] Не удалось разобрать JSON в '{}'. "
                     "Используется конфигурация debug overlay по умолчанию. "
                     "Подробности — в логах уровня DEBUG (если включены).",
                     path);
            return cfg;
        }

        const Json& data = *dataOpt;
        if (!data.is_object()) {
            LOG_WARN(core::log::cat::Config,
                     "[DebugOverlayLoader] Корневой JSON в '{}' должен быть object. "
                     "Используется конфигурация debug overlay по умолчанию.",
                     path);
            return cfg;
        }

        // ----------------------------------------------------------------------------------------
        // Заполнение полей на основе JSON-данных, считанных с помощью json_utils:
        //  - если ключ есть и валиден → используем значение из JSON,
        //  - если ключа нет → оставляем значение по умолчанию из структуры DebugOverlayBlueprint.
        // ----------------------------------------------------------------------------------------

        // enabled (bool)
        {
            const auto it = data.find(keys::DebugOverlay::ENABLED);
            if (it != data.end()) {
                if (it->is_boolean()) {
                    cfg.enabled = it->get<bool>();
                } else {
                    LOG_WARN(core::log::cat::Config,
                             "[DebugOverlayLoader] Некорректное поле '{}' в '{}': "
                             "ожидался boolean. Применён дефолт ({}).",
                             keys::DebugOverlay::ENABLED,
                             path,
                             cfg.enabled);
                }
            }
        }

        // position (Vector2f с поддержкой number/array/object)
        {
            const auto defaultPos = cfg.text.position;
            const auto posRes = json_utils::parseVec2fWithIssue(
                data,
                keys::DebugOverlay::POSITION,
                defaultPos
            );

            cfg.text.position = posRes.value;

            using Kind = json_utils::Vec2ParseIssue::Kind;
            if (posRes.issue.kind != Kind::None && posRes.issue.kind != Kind::MissingKey) {

                LOG_WARN(core::log::cat::Config,
                         "[DebugOverlayLoader] Некорректное поле '{}' в '{}': {}. "
                         "Применён дефолт position=({:.3f}, {:.3f}).",
                         keys::DebugOverlay::POSITION,
                         path,
                         json_utils::describe(posRes.issue),
                         defaultPos.x,
                         defaultPos.y);
            }
        }

        // characterSize (uint32_t, если значения нет —> остаётся значение по умолчанию)
        {
            const std::uint32_t defaultSize = cfg.text.characterSize;

            const auto sizeRes = json_utils::parseUIntWithIssue<std::uint32_t>(
                data,
                keys::DebugOverlay::CHARACTER_SIZE,
                defaultSize
            );

            cfg.text.characterSize = sizeRes.value;

            using Kind = json_utils::UnsignedParseIssue::Kind;
            if (sizeRes.issue.kind != Kind::None && sizeRes.issue.kind != Kind::MissingKey) {

                if (sizeRes.issue.kind == Kind::Negative) {
                    LOG_WARN(core::log::cat::Config,
                             "[DebugOverlayLoader] Некорректное поле '{}': {} (rawSigned={}). "
                             "Применён дефолт ({}).",
                             keys::DebugOverlay::CHARACTER_SIZE,
                             json_utils::describe(sizeRes.issue),
                             sizeRes.rawSigned,
                             defaultSize);
                } else if (sizeRes.issue.kind == Kind::OutOfRange) {
                    LOG_WARN(core::log::cat::Config,
                             "[DebugOverlayLoader] Некорректное поле '{}': {} (rawUnsigned={}). "
                             "Применён дефолт ({}).",
                             keys::DebugOverlay::CHARACTER_SIZE,
                             json_utils::describe(sizeRes.issue),
                             sizeRes.rawUnsigned,
                             defaultSize);
                } else {
                    // WrongType
                    LOG_WARN(core::log::cat::Config,
                             "[DebugOverlayLoader] Некорректное поле '{}': {}. "
                             "Применён дефолт ({}).",
                             keys::DebugOverlay::CHARACTER_SIZE,
                             json_utils::describe(sizeRes.issue),
                             defaultSize);
                }
            }
        }

        // color (строка "#RRGGBB"/"#RRGGBBAA" или объект {r,g,b,a})
        {
            const auto res = json_utils::parseColorWithIssue(
                data,
                keys::DebugOverlay::COLOR,
                cfg.text.color
            );

            cfg.text.color = res.value;

            using Kind = json_utils::ColorParseIssue::Kind;
            if (res.issue.kind != Kind::None && res.issue.kind != Kind::MissingKey) {

                if (!res.rawText.empty()) {
                    LOG_WARN(core::log::cat::Config,
                             "[DebugOverlayLoader] Некорректное поле '{}' в '{}': {}. "
                             "Применён дефолт. Исходное значение:'{}'.",
                             keys::DebugOverlay::COLOR,
                             path,
                             json_utils::describe(res.issue),
                             res.rawText);
                } else {
                    LOG_WARN(core::log::cat::Config,
                             "[DebugOverlayLoader] Некорректное поле '{}' в '{}': {}. "
                             "Применён дефолт.",
                             keys::DebugOverlay::COLOR,
                             path,
                             json_utils::describe(res.issue));
                }
            }
        }

        // updateIntervalMs (0 = каждый кадр, иначе throttle)
        {
            const std::uint32_t defaultMs = cfg.runtime.updateIntervalMs;
            const auto msRes = json_utils::parseUIntWithIssue<std::uint32_t>(
                data,
                keys::DebugOverlay::UPDATE_INTERVAL_MS,
                defaultMs
            );

            std::uint32_t ms = msRes.value;
            // Мягкая защита от идиотских значений (не критично, но держим UX вменяемым).
            if (ms > 10'000u) {
                if (msRes.issue.kind == json_utils::UnsignedParseIssue::Kind::None) {
                    LOG_WARN(core::log::cat::Config,
                             "[DebugOverlayLoader] Поле '{}' в '{}' обрезано: {} -> 10000.",
                             keys::DebugOverlay::UPDATE_INTERVAL_MS,
                             path,
                             ms);
                }
                ms = 10'000u;
            }
            cfg.runtime.updateIntervalMs = ms;

            using Kind = json_utils::UnsignedParseIssue::Kind;
            if (msRes.issue.kind != Kind::None && msRes.issue.kind != Kind::MissingKey) {
                if (msRes.issue.kind == Kind::Negative) {
                    LOG_WARN(core::log::cat::Config,
                             "[DebugOverlayLoader] Некорректное поле '{}': {} (rawSigned={}). "
                             "Применён дефолт ({}).",
                             keys::DebugOverlay::UPDATE_INTERVAL_MS,
                             json_utils::describe(msRes.issue),
                             msRes.rawSigned,
                             defaultMs);
                } else if (msRes.issue.kind == Kind::OutOfRange) {
                    LOG_WARN(core::log::cat::Config,
                             "[DebugOverlayLoader] Некорректное поле '{}': {} (rawUnsigned={}). "
                             "Применён дефолт ({}).",
                             keys::DebugOverlay::UPDATE_INTERVAL_MS,
                             json_utils::describe(msRes.issue),
                             msRes.rawUnsigned,
                             defaultMs);
                } else {
                    LOG_WARN(core::log::cat::Config,
                             "[DebugOverlayLoader] Некорректное поле '{}': {}. "
                             "Применён дефолт ({}).",
                             keys::DebugOverlay::UPDATE_INTERVAL_MS,
                             json_utils::describe(msRes.issue),
                             defaultMs);
                }
            }
        }

        // smoothingShift (0 = no smoothing, max clamp)
        {
            const std::uint8_t defaultShift = cfg.runtime.smoothingShift;
            const auto shRes = json_utils::parseUIntWithIssue<std::uint8_t>(
                data,
                keys::DebugOverlay::SMOOTHING_SHIFT,
                defaultShift
            );

            std::uint8_t sh = shRes.value;
            if (sh > 8u) {
                if (shRes.issue.kind == json_utils::UnsignedParseIssue::Kind::None) {
                    LOG_WARN(core::log::cat::Config,
                             "[DebugOverlayLoader] Поле '{}' в '{}' обрезано: {} -> 8.",
                             keys::DebugOverlay::SMOOTHING_SHIFT,
                             path,
                             static_cast<unsigned>(sh));
                }
                sh = 8u;
            }
            cfg.runtime.smoothingShift = sh;

            using Kind = json_utils::UnsignedParseIssue::Kind;
            if (shRes.issue.kind != Kind::None && shRes.issue.kind != Kind::MissingKey) {
                if (shRes.issue.kind == Kind::Negative) {
                    LOG_WARN(core::log::cat::Config,
                             "[DebugOverlayLoader] Некорректное поле '{}': {} (rawSigned={}). "
                             "Применён дефолт ({}).",
                             keys::DebugOverlay::SMOOTHING_SHIFT,
                             json_utils::describe(shRes.issue),
                             shRes.rawSigned,
                             defaultShift);
                } else if (shRes.issue.kind == Kind::OutOfRange) {
                    LOG_WARN(core::log::cat::Config,
                             "[DebugOverlayLoader] Некорректное поле '{}': {} (rawUnsigned={}). "
                             "Применён дефолт ({}).",
                             keys::DebugOverlay::SMOOTHING_SHIFT,
                             json_utils::describe(shRes.issue),
                             shRes.rawUnsigned,
                             defaultShift);
                } else {
                    LOG_WARN(core::log::cat::Config,
                             "[DebugOverlayLoader] Некорректное поле '{}': {}. "
                             "Применён дефолт ({}).",
                             keys::DebugOverlay::SMOOTHING_SHIFT,
                             json_utils::describe(shRes.issue),
                             defaultShift);
                }
            }
        }

        return cfg;
    }

} // namespace core::config