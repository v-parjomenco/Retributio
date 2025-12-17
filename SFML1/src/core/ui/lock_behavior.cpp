#include "pch.h"

#include "core/ui/lock_behavior.h"
#include "core/log/log_macros.h"

namespace core::ui {

    // ============================================================================================
    // DEPRECATED: Старая функция для sf::Sprite (Phase 2 и ранее)
    // ============================================================================================
    //
    // Эта функция больше НЕ используется в Phase 3 (ID-based ECS).
    // Оставлена для обратной совместимости, если где-то есть старый код.
    //
    // ЕСЛИ ТЫ ВИДИШЬ ВЫЗОВ ЭТОЙ ФУНКЦИИ — ЭТО ОШИБКА!
    // Замени на LockSystem::onResize() (новая ECS версия).
    //
    void applyScreenLock(sf::Sprite& sprite, const sf::View& view, sf::Vector2f& previousViewSize,
                         bool& initialized) {
        // DEPRECATED NOTICE
        static bool warningShown = false;
        if (!warningShown) {
#ifndef NDEBUG
            LOG_WARN(core::log::cat::UI,
                     "applyScreenLock(sf::Sprite&) is DEPRECATED! "
                     "Use LockSystem::onResize() instead (Phase 3 ID-based ECS).");
#endif
            warningShown = true;
        }

#ifndef NDEBUG
        assert(previousViewSize.x > 0.f && previousViewSize.y > 0.f &&
               "LockBehavior: previousViewSize must be initialized before use");
#endif

        const float previousWidth = std::max(1.f, previousViewSize.x);
        const float previousHeight = std::max(1.f, previousViewSize.y);

        const sf::Vector2f previousPosition = sprite.getPosition();

        const sf::Vector2f relativePosition = {previousPosition.x / previousWidth,
                                               previousPosition.y / previousHeight};

        const sf::Vector2f currentViewSize = view.getSize();

        const sf::Vector2f newPosition = {currentViewSize.x * relativePosition.x,
                                          currentViewSize.y * relativePosition.y};

        sprite.setPosition(newPosition);

        previousViewSize = currentViewSize;
        initialized = true;
    }

} // namespace core::ui