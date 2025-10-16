#pragma once

#include <SFML/Graphics.hpp>
#include "core/config.h"
#include "core/config_loader.h"
#include "core/resize_behavior.h"

namespace entities {
    /**
    * @class Player
    * @brief Игровой объект, представляющий самолёт игрока.
    * Управляется с клавиатуры и автоматически реагирует на изменение размера окна.
    */
    class Player {
    public:

        // Конструктор принимает текстуру, масштаб картинки и скорость игрока по ссылке,
        // запрещая неявные преобразования типов благодаря explicit
        explicit Player(const sf::Texture& texture, sf::Vector2f scale = { 1.f, 1.f }, float speed = 100.f);

        // Инициализация игрока из конфигурации
        void initFromConfig(const core::PlayerConfig& cfg);

        // Обработка ввода для управления игроком
        void handleInput(sf::Keyboard::Key key, bool isPressed);

        // Обновление позиции игрока на основе состояния клавиш
        void update(const sf::Time& dt);

        // Отрисовка игрока
        void draw(sf::RenderWindow& window) const;

        // Получение глобальных границ спрайта
        sf::FloatRect getGlobalBounds() const;

        // Ресайз-метод, вызываемый при изменении размера окна
        void onResize(const sf::View& view, sf::RenderWindow* window = nullptr);

        // Сеттеры
        void setSpeed(float speed) { mSpeed = speed; }
        void setScale(sf::Vector2f scale) { mSprite.setScale(scale); }
        void setPosition(sf::Vector2f pos) { mSprite.setPosition(pos); }

    private:
        sf::Sprite mSprite; // Спрайт, представляющий самолёт игрока
        float mSpeed{ 100.f }; // Скорость движения игрока в пикселях в секунду,
                               // значение по умолчанию, если JSON не загрузится

        // Флаги для отслеживания состояния клавиш движения
        bool mIsMovingUp{ false };
        bool mIsMovingDown{ false };
        bool mIsMovingLeft{ false };
        bool mIsMovingRight{ false };

        // Объект, для управления поведением при изменении размера окна
        std::unique_ptr<core::IResizeBehavior> mResizeBehavior;

    };
} // namespace entities