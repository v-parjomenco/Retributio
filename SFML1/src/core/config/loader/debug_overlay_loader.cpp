#include "pch.h"

#include "core/config/loader/debug_overlay_loader.h"

#include "core/config/config_keys.h"
#include "core/log/log_macros.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_accessors.h"
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
        auto dataOpt = json_utils::parseAndValidateNonCritical(
            fileContent, path, "DebugOverlayLoader",
            {
                {keys::DebugOverlay::ENABLED, {Json::value_t::boolean}, false},
                {keys::DebugOverlay::POSITION,
                 {Json::value_t::array, Json::value_t::object, Json::value_t::number_float,
                  Json::value_t::number_integer, Json::value_t::number_unsigned},
                 false},
                {keys::DebugOverlay::CHARACTER_SIZE,
                 {Json::value_t::number_integer, Json::value_t::number_unsigned},
                 false},
                {keys::DebugOverlay::COLOR, {Json::value_t::string, Json::value_t::object}, false},
                {keys::DebugOverlay::UPDATE_INTERVAL_MS,
                 {Json::value_t::number_integer, Json::value_t::number_unsigned},
                 false},
                {keys::DebugOverlay::SMOOTHING_SHIFT,
                 {Json::value_t::number_integer, Json::value_t::number_unsigned},
                 false},
            });

        // Если парсинг/валидация не удалась — остаёмся на дефолтных настройках.
        if (!dataOpt) {
            LOG_WARN(core::log::cat::Config,
                     "[DebugOverlayLoader] Некорректный JSON или структура в '{}'. "
                     "Используется конфигурация debug overlay по умолчанию. "
                     "Подробности — в логах уровня DEBUG (если включены).",
                     path);
            return cfg;
        }

        const Json& data = *dataOpt;

        // ----------------------------------------------------------------------------------------
        // Заполнение полей на основе JSON-данных, считанных с помощью json_utils:
        //  - если ключ есть и валиден → используем значение из JSON,
        //  - если ключа нет → оставляем значение по умолчанию из структуры DebugOverlayBlueprint.
        // ----------------------------------------------------------------------------------------

        // enabled (bool, если значение с JSON-файла не получено —> остаётся по умолчанию "true")
        cfg.enabled = json_utils::parseValue(data, keys::DebugOverlay::ENABLED, cfg.enabled);

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

        // characterSize (unsigned int, если значения нет —> остаётся значение по умолчанию)
        {
            const unsigned defaultSize = cfg.text.characterSize;

            const auto sizeRes = json_utils::parseUnsignedWithIssue(
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
            const unsigned defaultMs = cfg.runtime.updateIntervalMs;
            const auto msRes = json_utils::parseUnsignedWithIssue(
                data,
                keys::DebugOverlay::UPDATE_INTERVAL_MS,
                defaultMs
            );

            unsigned ms = msRes.value;
            // Мягкая защита от идиотских значений (не критично, но держим UX вменяемым).
            if (ms > 10'000u) {
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
            const unsigned defaultShift = cfg.runtime.smoothingShift;
            const auto shRes = json_utils::parseUnsignedWithIssue(
                data,
                keys::DebugOverlay::SMOOTHING_SHIFT,
                defaultShift
            );

            unsigned sh = shRes.value;
            if (sh > 8u) {
                sh = 8u;
            }
            cfg.runtime.smoothingShift = static_cast<std::uint8_t>(sh);

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