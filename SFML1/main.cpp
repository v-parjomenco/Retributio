#include "pch.h"

#include <cstdlib>
#include <exception>

#include "game/skyguard/game.h"
#include "core/utils/message.h"

int main() {

    try {
        game::skyguard::Game game;
        game.run();
    } catch (const std::exception& e) {
        core::utils::message::showError(e.what());
        core::utils::message::holdOnExit(); // если включён флаг в config.h, окно появится
        return EXIT_FAILURE;
    }

    core::utils::message::
        holdOnExit(); // препятствует быстрому закрытию окна при выходе,
                      // если DEBUG_HOLD_ON_EXIT == true
    return EXIT_SUCCESS;
}