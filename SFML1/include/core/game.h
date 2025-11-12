// ============================================================================
// File: core/game.h
// Role: High-level game application (window, loop, systems wiring)
// Notes:
//  - Owns TimeService (outside ECS) and wires ECS systems
//  - Handles window events and view resize propagation
// ============================================================================
#pragma once

#include <SFML/Graphics.hpp>
#include "core/config.h"
#include "core/ecs/components/keyboard_control_component.h"
#include "core/ecs/systems/debug_overlay_system.h"
#include "core/ecs/systems/input_system.h"
#include "core/ecs/systems/lock_system.h"
#include "core/ecs/systems/movement_system.h"
#include "core/ecs/systems/player_init_system.h"
#include "core/ecs/systems/render_system.h"
#include "core/ecs/systems/scaling_system.h"
#include "core/ecs/world.h"
#include "core/resources/resource_manager.h"
#include "core/time/time_service.h"
#include "core/utils/message.h"

namespace core {

	class Game {
	public:
		Game();
		void run();
	private:
		void processEvents();
		void update(const sf::Time& dt);
		void render();
		void initWorld(); // создаёт ECS-мир и инициализирует сущности
	private:
		sf::RenderWindow mWindow;
		core::resources::ResourceManager mResources;

		core::time::TimeService mTime;						 // сервис времени (вне ECS)
		core::ecs::World mWorld;							 // ECS-мир
		core::ecs::Entity mPlayerEntity;					 // сущность игрока

		// Системы, к которым нужен прямой доступ
		core::ecs::ScalingSystem* mScalingSystem{ nullptr }; // кэшируем систему масштабирования
		core::ecs::LockSystem* mLockSystem{ nullptr };	     // кэшируем систему фиксации
		core::ecs::InputSystem* mInputSystem{ nullptr };     // клавиатурный ввод
		core::ecs::DebugOverlaySystem* mDebugOverlay{ nullptr };

	};

} // namespace core