#include "pch.h"

#include "core/runtime/entry/logging_lifetime.h"

#include <cassert>

#include "core/log/logging.h"

namespace core::runtime::entry {

    LoggingLifetime::LoggingLifetime(core::log::Config cfg) {
        // Нарушение entry-layer политики: LOG_* вызван до создания LoggingLifetime.
        // В Debug ловим нарушителя немедленно через assert.
        // В Release ленивая инициализация core::log уже произошла — повторный init()
        // корректно обработан в initLocked() (idempotent при совпадении конфига,
        // переоткрывает файл при отличии). Поведение детерминировано и безопасно.
        assert(!core::log::isInitialized() &&
               "LoggingLifetime: логгер уже активен до вызова конструктора. "
               "Вероятно, LOG_* вызван до создания LoggingLifetime в main().");

        core::log::init(cfg);
    }

    LoggingLifetime::~LoggingLifetime() noexcept {
        try {
            core::log::shutdown();
        } catch (...) {
            // shutdown() не должен бросать, но если что-то пошло вразнос —
            // деструктор не имеет права пропустить исключение наружу.
        }
    }

} // namespace core::runtime::entry
