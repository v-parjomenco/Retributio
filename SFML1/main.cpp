#include "core/config.h"
#include <exception>
#include "utils/message.h"
#include <SFML/Graphics.hpp>

// TODO: подключить ResourceManager для хранения шрифтов, текстур и звуков

class TimeSystem {
public:
    // Сброс таймера и обновление дельты времени и всех значений, зависящих от времени
    void update(float smoothingAlpha = 0.1f) {
        mDeltaTime = mClock.restart();

        // Подсчёт мгновенного frames per second (FPS)
        mFps = (mDeltaTime.asSeconds() > 0.f) ? (1.f / mDeltaTime.asSeconds()) : 0.f;

        // Чтобы не было резкого скачка на первом кадре при старте с нуля
        if (mSmoothedFps == 0.f)
            mSmoothedFps = mFps; // первый кадр
        else
            mSmoothedFps = mSmoothedFps * (1.f - smoothingAlpha) + mFps * smoothingAlpha; // экспоненциальное сглаживание
    }

    // Получение дельты времени
    sf::Time getDeltaTime() const {
        return mDeltaTime;
    }

    // Получение дельты времени в секундах
    float getDeltaTimeSeconds() const {
        return mDeltaTime.asSeconds();
    }

    // Получение мгновенного FPS
    float getFps() const {
        return mFps;
    }

    // Получение сглаженного FPS
    float getSmoothedFps() const {
        return mSmoothedFps;
    }

private:
    sf::Clock mClock; // таймер
    sf::Time mDeltaTime; // дельта времени - время, прошедшее с прошлого кадра
    float mFps{ 0.f }; // frames per second
    float mSmoothedFps{ 0.f }; // сглаженный frames per second
};

class TextOutput {
public:
    void init(sf::Font& font, std::optional<sf::Text>& text) const {

        text.emplace(font); // инициализация объекта std::optional   
        text->setCharacterSize(35); // установка размера текста
        text->setFillColor(sf::Color::Red); // установка цвета текста красным
        text->setPosition({ 10.f, 10.f }); // установка позиции текста
    }

    void updateFpsText(std::optional<sf::Text>& text, float fps) const {
        if (text)
            text->setString("FPS: " + std::to_string(static_cast<int>(fps)));
    }

    void draw(sf::RenderWindow& window, std::optional<sf::Text>& text) const {
        if (text) {
            window.draw(*text);
        }
    }
};


class Game {
public:
    Game();
    void run();
private:
    void processEvents();
    void update();
    void render();
    void handlePlayerInput(sf::Keyboard::Key key, bool isPressed);
private:
    sf::RenderWindow mWindow;
    sf::CircleShape mPlayer;
    TimeSystem mTime; // система для отслеживания времени
    TextOutput mTextOutput; // система для вывода текста
    sf::Font mFont;
    std::optional<sf::Text> mText;
  
    // Флаги для отслеживания состояния клавиш движения
    bool mIsMovingUp{ false };
    bool mIsMovingDown{ false };
    bool mIsMovingLeft{ false };
    bool mIsMovingRight{ false };
};

Game::Game() :
    mWindow(sf::VideoMode({ config::WINDOW_WIDTH, config::WINDOW_HEIGHT }), config::WINDOW_TITLE),
    mPlayer() {
    mPlayer.setRadius(40.f);
    mPlayer.setPosition({ 100.f, 100.f });
    mPlayer.setFillColor(sf::Color::Cyan);

    // Этот кусок кода лучше поместить в ResourceManager
    // Загружаем шрифт и проверяем корректность загрузки
    if (!mFont.openFromFile("assets/fonts/Wolgadeutsche.otf")) {
        // обработка ошибки загрузки шрифта
        throw std::runtime_error("Не удалось загрузить шрифт!");
    }

    // Инициализация текста FPS
    mTextOutput.init(mFont, mText);
}

void Game::run() {
    while (mWindow.isOpen()) {
        // сбрасываем таймер и записыаем данные в дельту времени, с чьей помощью считаем FPS и сглаженный FPS
        mTime.update();

        processEvents();
        update();
        render();
    }
}



void Game::render() {
    mWindow.clear();
    mWindow.draw(mPlayer);
    mTextOutput.draw(mWindow, mText);
    mWindow.display();
}

void Game::processEvents() {

    while (const std::optional<sf::Event> event = mWindow.pollEvent()) {

        if (event->is<sf::Event::Closed>()) {
            // обработка закрытия окна
            mWindow.close();
        }
        else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            // обработка нажатия клавиши
            handlePlayerInput(keyPressed->code, true);
        }
        else if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>()) {
            // обработка отпускания клавиши
            handlePlayerInput(keyReleased->code, false);
        }
    }
}

void Game::handlePlayerInput(sf::Keyboard::Key key, bool isPressed) {
    if (key == sf::Keyboard::Key::W)
        mIsMovingUp = isPressed;
    else if (key == sf::Keyboard::Key::S)
        mIsMovingDown = isPressed;
    else if (key == sf::Keyboard::Key::A)
        mIsMovingLeft = isPressed;
    else if (key == sf::Keyboard::Key::D)
        mIsMovingRight = isPressed;
}

void Game::update() {
    
    mTextOutput.updateFpsText(mText, mTime.getSmoothedFps());// выводим сглаженный FPS

    sf::Vector2f movement(0.f, 0.f);
    if (mIsMovingUp)
        movement.y -= 1.f;
    if (mIsMovingDown)
        movement.y += 1.f;
    if (mIsMovingLeft)
        movement.x -= 1.f;
    if (mIsMovingRight)
        movement.x += 1.f;
    mPlayer.move(movement * config::PLAYER_SPEED * mTime.getDeltaTimeSeconds());
}


int main() {	

    try {
        Game game;
        game.run();
    }
    catch (const std::exception& e) {
        utils::message::showError(e.what());
        utils::message::holdOnExit(); // если включён флаг в config.h, окно появится
        return EXIT_FAILURE;
    }
	
    utils::message::holdOnExit(); // задержка на выходе (если DEBUG_HOLD_ON_EXIT == true)
	return EXIT_SUCCESS;	
}