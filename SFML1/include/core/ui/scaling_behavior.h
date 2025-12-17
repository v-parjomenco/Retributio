// ================================================================================================
// File: core/ui/scaling_behavior.h
// Purpose: Scaling behavior types and helper functions for resize handling
// Used by: AnchorProperties, ScalingBehaviorComponent, ScalingSystem
// Related headers: core/ui/ids/ui_id_utils.h
// ================================================================================================
#pragma once

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

namespace core::ui {

    /**
     * @brief Тип политики масштабирования при изменении размера окна.
     */
    enum class ScalingBehaviorKind {
        None,   // без дополнительного масштабирования
        Uniform // равномерное масштабирование
    };

    /**
     * @brief Реализация Uniform-скейлинга (СТАРАЯ ВЕРСИЯ для sf::Sprite).
     *
     * DEPRECATED: Используется только старыми системами.
     * Новые системы должны использовать computeUniformFactor().
     */
    void applyUniformScaling(sf::Sprite& sprite, const sf::View& view,
                             const sf::Vector2f& baseViewSize, float& lastUniform);

    // ============================================================================================
    // ID-BASED ECS HELPERS (Phase 3)
    // ============================================================================================

    /**
     * @brief Вычисление uniform factor БЕЗ мутации sf::Sprite (data-driven).
     *
     * Старый подход (инкрементальный, накапливает ошибки):
     *   applyUniformScaling(sprite, view, baseViewSize, lastUniform);
     *
     * Новый подход (детерминированный):
     *   float uniformFactor = computeUniformFactor(view, baseViewSize);
     *   spriteComp.scale = spriteComp.baseScale * uniformFactor;
     */
    inline float computeUniformFactor(const sf::View& view, const sf::Vector2f& baseViewSize) {
        const sf::Vector2f safeBaseSize{std::max(baseViewSize.x, 1.f),
                                        std::max(baseViewSize.y, 1.f)};

        const sf::Vector2f currentViewSize = view.getSize();

        const float scaleX = currentViewSize.x / safeBaseSize.x;
        const float scaleY = currentViewSize.y / safeBaseSize.y;

        return std::min(scaleX, scaleY);
    }

} // namespace core::ui