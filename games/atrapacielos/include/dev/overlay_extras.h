// ================================================================================================
// File: dev/overlay_extras.h
// Purpose: Atrapacielos-specific debug overlay extra lines (Background, View, Player, Stress stamp)
// Notes:
//  - Dev-only: compiled only in Debug/Profile builds.
//  - Game.cpp calls one function; all formatting logic is here.
// ================================================================================================
#pragma once

#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)

// Предварительные объявления: избегаем тяжёлых include'ов в заголовке.
namespace core::ecs {
    class DebugOverlaySystem;
    class SpatialIndexSystem;
    class World;
} // namespace core::ecs

namespace game::atrapacielos::presentation {
    class BackgroundRenderer;
    class ViewManager;
} // namespace game::atrapacielos::presentation

#if defined(RETRIBUTIO_PROFILE)
namespace game::atrapacielos::dev {
    struct StressRuntimeStamp;
} // namespace game::atrapacielos::dev
#endif

namespace game::atrapacielos::dev {

    /// Заполнить extra-строки debug overlay: Background, View, CellsHealth,
    /// Stress stamp (Profile), Camera (Debug), Player watch + PlayerVis.
    ///
    /// Вызывается один раз за кадр из Game::renderUiPass(), ПЕРЕД prepareFrame().
    /// Внутри вызывает overlay.clearExtraText() — вызывающий НЕ должен.
    ///
    /// Контракт:
    ///  - Функция read-only по отношению к World/системам (мутирует только overlay буфер).
    ///  - spatialIndex может быть nullptr (overlay без spatial-метрик).
    ///  - stressStamp: nullptr в non-Profile сборках; в Profile — указатель на stamp,
    ///    заполненный в initWorld().
    void populateDebugOverlayExtraLines(
        core::ecs::DebugOverlaySystem& overlay,
        core::ecs::World& world,
        const core::ecs::SpatialIndexSystem* spatialIndex,
        const presentation::BackgroundRenderer& background,
        const presentation::ViewManager& viewManager
#if defined(RETRIBUTIO_PROFILE)
        , const StressRuntimeStamp* stressStamp
#endif
    );

} // namespace game::atrapacielos::dev

#endif // !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)