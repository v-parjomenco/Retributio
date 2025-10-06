#include <SFML/Graphics.hpp>
#include "entities/player.h"
#include "core/config_loader.h"

namespace entities {

	// Конструктор принимает текстуру по ссылке, запрещая неявные преобразования типов благодаря explicit
    // Данные здесь лишь по умолчанию, если не загрузится конфигурация из JSON
	Player::Player(const sf::Texture& texture, sf::Vector2f scale, float speed) : mSprite(texture), mSpeed(speed) {
		mSprite.setScale(scale);
		mSprite.setPosition({ 0.f, 0.f });
	}

    // Инициализация игрока из конфигурации
    void Player::initFromConfig(const core::PlayerConfig& cfg) {
        setSpeed(cfg.speed);
        setScale(cfg.scale); // масштаб устанавливаем первым — но локальные границы не зависят от scale
		setPosition(cfg.startPosition);
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

    // Получение глобальных границ спрайта
	sf::FloatRect Player::getGlobalBounds() const {
		return mSprite.getGlobalBounds();
	}

} // namespace entities