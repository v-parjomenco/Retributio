#include <SFML/Graphics.hpp>
#include "entities/player.h"
#include "core/config.h"
#include "core/config_loader.h"
#include "core/resize_behavior_factory.h"

namespace entities {

	// Конструктор принимает текстуру по ссылке, запрещая неявные преобразования типов благодаря explicit
    // Данные здесь лишь по умолчанию, если не загрузится конфигурация из JSON
	Player::Player(const sf::Texture& texture, sf::Vector2f scale, float speed) : mSprite(texture), mSpeed(speed) {
		// позиция по умолчанию, переопределяется void Player::initFromConfig()
		mSprite.setPosition({ 0.f, 0.f });

		// Поведение по умолчанию при изменении размера окна
		// Координаты фиксированы, масштаб не меняется, но растягивается по экрану
		mResizeBehavior = std::make_unique<core::FixedWorldNoScalingBehavior>();
	}

    // Инициализация игрока из конфигурации
    void Player::initFromConfig(const core::PlayerConfig& cfg) {

        // Масштабируем спрайт согласно конфигурации по умолчанию
        mSprite.setScale(cfg.scale);

		// Получаем локальные границы спрайта (т.е. границы до применения трансформаций, они не зависят от scale)
		sf::FloatRect bounds = mSprite.getLocalBounds();

		// Центрируем спрайт по X, по Y ставим на нижний край
		float originX = bounds.position.x + bounds.size.x / 2.f;  // центр по X внутри спрайта
		float originY = bounds.position.y + bounds.size.y; // нижний край
		mSprite.setOrigin({originX, originY});

		// Теперь позиционируем самолёт игрока внизу по центру экрана
		mSprite.setPosition(
			{ config::WINDOW_WIDTH / 2.f,   // центр по X
			config::WINDOW_HEIGHT }         // нижний край экрана
		);

        // Задаём скорость из конфигурации по умолчанию
        setSpeed(cfg.speed);

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

    // Получение глобальных границ спрайта (т.е. границы после применения трансформаций)
	sf::FloatRect Player::getGlobalBounds() const {
		return mSprite.getGlobalBounds();
	}

    // Обновление стартовой позициюи игрока в зависимости от размера окна
	void Player::updateScreenPosition(const sf::View& view)
	{
        const sf::Vector2f viewSize = view.getSize();
        const sf::Vector2f viewCenter = view.getCenter();
        float x = viewCenter.x; // центр по X
        float y = viewCenter.y + viewSize.y / 2.f; // нижний край экрана по Y
        mSprite.setPosition({ x, y });
	}

    // Поведение при изменении размера окна
	void Player::onResize(const sf::View& currentView, sf::RenderWindow* window) {
		if (mResizeBehavior)
			mResizeBehavior->onResize(mSprite, currentView);
	}

} // namespace entities