#include <SFML/Graphics.hpp>
#include "core/anchor_policy.h"
#include "core/anchor_utils.h"
#include "core/config.h"
#include "core/config_keys.h"
#include "core/config_loader.h"
#include "core/resize_behavior_factory.h"
#include "entities/player.h"
#include "utils/message.h"

namespace entities {

	// TODO: Если появятся другие объекты (UI, NPC и т.п.), рассмотреть выделение общего базового класса Entity,
	// содержащего mSprite, mResizeBehavior и базовую реализацию onResize().

	// TODO: Вынести маппинг AnchorType в отдельный EnumConverter,
	// чтобы фабрика, анкоры и JSON - парсер все читали из одного источника.

	// Конструктор принимает текстуру по ссылке, запрещая неявные преобразования типов благодаря explicit
    // Данные здесь лишь по умолчанию, если не загрузится конфигурация из JSON
	Player::Player(const sf::Texture& texture, sf::Vector2f scale, float speed) : mSprite(texture), mSpeed(speed) {

		// Поведение по умолчанию при изменении размера окна
		mResizeBehavior = std::make_unique<core::FixedWorldNoScalingBehavior>();
	}

    // Инициализация игрока из конфигурации
    void Player::initFromConfig(const core::PlayerConfig& cfg) {

        // Масштабируем спрайт согласно конфигурации по умолчанию
        mSprite.setScale(cfg.scale);

		// Задаём скорость из конфигурации по умолчанию
		setSpeed(cfg.speed);

		// Используем временный view для корректного расчёта якоря до отображения окна, как позицию по умолчанию
		sf::View defaultView(sf::FloatRect({ 0.f, 0.f },
			{ static_cast<float>(config::WINDOW_WIDTH), static_cast<float>(config::WINDOW_HEIGHT) }));

		// Преобразуем строку из JSON в enum AnchorType через безопасный хелпер
		core::AnchorType anchor = core::anchors::fromString(cfg.anchor);

		// Если якорь не распознан — логируем предупреждение (только в Debug)
#ifdef _DEBUG
		if (anchor == core::AnchorType::None && !cfg.anchor.empty()) {
			DEBUG_MSG("[Player]\nНеизвестный якорь в JSON: " + cfg.anchor +
				". Вместо него будет использовано значение по умолчанию: AnchorType::None.");
		}
#endif

		// Если задан якорь — применяем его для позиционирования.
		// Если якорь отсутствует или нераспознан, используем startPosition из конфига.
		if (anchor != core::AnchorType::None) {
			core::AnchorPolicy(anchor).apply(mSprite, defaultView);
		}
		else {
			mSprite.setPosition(cfg.startPosition);
		}

		// Установка поведения при изменении окна
		mResizeBehavior = core::ResizeBehaviorFactory::create(cfg.resizeBehavior);
     }

	// Обработка ввода для управления игроком
	void Player::handleInput(sf::Keyboard::Key key, bool isPressed) {
		if (key == sf::Keyboard::Key::W)
			mIsMovingUp = isPressed;
		else if (key == sf::Keyboard::Key::S)
			mIsMovingDown = isPressed;
		else if (key == sf::Keyboard::Key::A)
			mIsMovingLeft = isPressed;
		else if (key == sf::Keyboard::Key::D)
			mIsMovingRight = isPressed;
	}

	// Обновление позиции игрока на основе состояния клавиш
	void Player::update(const sf::Time& dt) { // dt - delta time, время, прошедшее с предыдущего кадра
		
		sf::Vector2f movement(0.f, 0.f);
		if (mIsMovingUp)
			movement.y -= 1.f;
		if (mIsMovingDown)
			movement.y += 1.f;
		if (mIsMovingLeft)
			movement.x -= 1.f;
		if (mIsMovingRight)
			movement.x += 1.f;
		
		mSprite.move(movement * mSpeed * dt.asSeconds());
	}

	// Отрисовка игрока
	void Player::draw(sf::RenderWindow& window) const {
		window.draw(mSprite);
	}

    // Получение глобальных границ спрайта (т.е. границ после применения трансформаций)
	sf::FloatRect Player::getGlobalBounds() const {
		return mSprite.getGlobalBounds();
	}

    // Поведение при изменении размера окна
	void Player::onResize(const sf::View& currentView, sf::RenderWindow* window) {
		if (mResizeBehavior)
			mResizeBehavior->onResize(mSprite, currentView);
	}

} // namespace entities