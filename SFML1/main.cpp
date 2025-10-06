#include <exception>
#include <core/game.h>

int main() {

    try {
        core::Game game;
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