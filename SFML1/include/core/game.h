#pragma once

#include <SFML/Graphics.hpp>
#include "core/ecs/world.h"
#include "core/ecs/components/keyboard_control_component.h"
#include "core/ecs/systems/input_system.h"
#include "core/ecs/systems/movement_system.h"
#include "core/ecs/systems/player_init_system.h"
#include "core/ecs/systems/lock_system.h"
#include "core/ecs/systems/render_system.h"
#include "core/ecs/systems/scaling_system.h"
#include "core/ecs/systems/time_system.h"
#include "core/ecs/systems/text_output.h"
#include "core/config.h"
#include "core/resources/resource_manager.h"
#include "utils/message.h"

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
		ecs::TimeSystem mTime;		  // система для отслеживания времени
		ecs::TextOutput mTextOutput; // система для вывода текста
		std::optional<sf::Text> mText;
		core::ecs::World mWorld; // ECS-мир
		core::ecs::Entity mPlayerEntity; // сущность игрока
		core::ecs::ScalingSystem* mScalingSystem{ nullptr }; // кэшируем систему масштабирования
		core::ecs::LockSystem* mLockSystem{ nullptr };	     // кэшируем систему фиксации
		core::ecs::InputSystem* mInputSystem{ nullptr };     // клавиатурный ввод

	};

} // namespace core