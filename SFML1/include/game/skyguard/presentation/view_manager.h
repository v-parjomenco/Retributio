// ================================================================================================
// File: game/skyguard/presentation/view_manager.h
// Purpose: Manage WorldView and UiView for SkyGuard (letterbox + UI separation).
// Used by: game::skyguard::Game.
// Related headers: game/skyguard/config/view_config.h, core/ui/viewport_utils.h
// ================================================================================================
#pragma once

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

#include "game/skyguard/config/view_config.h"

namespace game::skyguard::presentation {

    /**
     * @brief Управляет WorldView и UiView для SkyGuard.
     *
     * Ответственность:
     *  - Инициализация view из конфига
     *  - Обработка ресайза (обновление вьюпорта, без изменения размера view)
     *  - Предоставление view для рендер-проходов
     *
     * НЕ ECS-система. Владелец — Game.
     */
    class ViewManager {
      public:
        /**
         * @brief Инициализировать view из конфигурации.
         *
         * @param config Конфигурация view
         * @param initialWindowSize Размер окна при старте
         */
        void init(const config::ViewConfig& config,
                  const sf::Vector2u& initialWindowSize) noexcept;

        /**
         * @brief Обработка изменения размера окна.
         *
         * Обновляет viewport (letterbox) только для WORLD view.
         * Размеры view не меняются — меняется только viewport.
         *
         * @param newWindowSize Новый размер окна в пикселях
         */
        void onResize(const sf::Vector2u& newWindowSize) noexcept;

        /**
         * @brief Обновить центр камеры (вызывать после движения игрока).
         *
         * X зафиксирован по центру мира (вертикальный скроллер). По Y камера следует за игроком.
         *
         * Контракт clamp по Y (SFML world space, +Y вниз):
         *  - Камера НЕ должна скроллить "назад вниз" ниже стартовой точки.
         *  - Значит ограничиваем центр камеры максимумом по числу Y:
         *      centerY = min(desiredCenterY, cameraCenterYMax)
         *
         * @param targetPosition Позиция игрока в world space (bottom-center).
         */
        void updateCamera(const sf::Vector2f& targetPosition) noexcept;

        [[nodiscard]] const sf::View& getWorldView() const noexcept { return mWorldView; }
        [[nodiscard]] const sf::View& getUiView() const noexcept { return mUiView; }
        [[nodiscard]] sf::Vector2f getWorldLogicalSize() const noexcept { return mWorldLogicalSize; }
        [[nodiscard]] sf::Vector2f getUiLogicalSize() const noexcept { return mUiLogicalSize; }
        [[nodiscard]] sf::Vector2f getCameraOffset() const noexcept { return mCameraOffset; }
        [[nodiscard]] float getCameraCenterYMax() const noexcept { return mCameraCenterYMax; }

      private:
        sf::View mWorldView;
        sf::View mUiView;

        sf::Vector2f mWorldLogicalSize{1920.f, 1080.f};
        sf::Vector2f mUiLogicalSize{1920.f, 1080.f};
        sf::Vector2f mCameraOffset{0.f, -100.f};

        // Максимально допустимый Y центра камеры (SFML world space, +Y вниз).
        // Камера не должна "откатываться вниз" ниже стартовой точки:
        // centerY = min(desiredCenterY, cameraCenterYMax).
        float mCameraCenterYMax{540.f};

        sf::Vector2u mCurrentWindowSize{1920u, 1080u};
    };

} // namespace game::skyguard::presentation