#include "pch.h"

#include "core/ui/scaling_behavior.h"
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
    // Замени на ScalingSystem::onResize() (новая ECS версия).
    //
    void applyUniformScaling(sf::Sprite& sprite, const sf::View& view,
                             const sf::Vector2f& baseViewSize, float& lastUniform) {
        // DEPRECATED NOTICE
        static bool warningShown = false;
        if (!warningShown) {
// В Debug сборке показываем предупреждение
#ifndef NDEBUG
            LOG_WARN(core::log::cat::UI,
                     "applyUniformScaling(sf::Sprite&) is DEPRECATED! "
                     "Use ScalingSystem::onResize() instead (Phase 3 ID-based ECS).");
#endif
            warningShown = true;
        }

        // Сохраняем старую логику для обратной совместимости
        const sf::Vector2f safeBaseSize{std::max(baseViewSize.x, 1.f),
                                        std::max(baseViewSize.y, 1.f)};

        const sf::Vector2f currentViewSize = view.getSize();

        const float scaleX = currentViewSize.x / safeBaseSize.x;
        const float scaleY = currentViewSize.y / safeBaseSize.y;

        const float newUniform = std::min(scaleX, scaleY);

        float ratio = 1.f;
        if (lastUniform > 0.f) {
            ratio = newUniform / lastUniform;
        }

        const sf::Vector2f currentScale = sprite.getScale();
        sprite.setScale({currentScale.x * ratio, currentScale.y * ratio});

        lastUniform = newUniform;
    }

} // namespace core::ui