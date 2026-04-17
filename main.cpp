#include "game.h"
#include "ui.h"
#include "save_load.h"
#include <iostream>
#include <cstdlib>
#include <ctime>

static void runGameLoop(GameState &gs) {
    while (gs.status == STATUS_PLAYING) {
        drawGame(gs);

        int action = promptInt("Choose action: ", 0, 6);
        if (action == -1) continue;

        if (action == 0) {
            if (saveGame(gs, SAVE_FILE))
                std::cout << "Game saved! Returning to menu...\n";
            else
                std::cout << "Failed to save game.\n";
            pause("Press Enter to continue...");
            return;
        }

        int param = -1;
        if (action == 1 || action == 2) {
            // Ask which camera group
            std::cout << "Camera groups available: ";
            for (int i = 0; i < gs.gameMap.numCameraGroups; i++)
                std::cout << cameraGroupLabel(i) << "(" << i << ") ";
            std::cout << "\n";
            param = promptInt("Select camera group: ", 0, gs.gameMap.numCameraGroups - 1);
            if (param == -1) continue;
        } else if (action == 3 || action == 4) {
            for (auto &r : gs.gameMap.rooms)
                std::cout << "  " << r.id << ": " << r.name
                          << (r.doorClosed ? " [CLOSED]" : "") << "\n";
            param = promptInt("Select room: ", 0, gs.gameMap.totalRooms - 1);
            if (param == -1) continue;
        }

        gs.doTurn(action, param);
    }

    drawEndGame(gs);
}

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    while (true) {
        drawMainMenu();
        int choice = promptInt("Select: ", 0, 3);
        if (choice == -1) continue;

        if (choice == 0) {
            std::cout << "Thanks for playing Camera Watch!\n";
            break;
        } else if (choice == 1) {
            drawDifficultyMenu();
            int diff = promptInt("Select difficulty: ", 1, 3);
            if (diff == -1) continue;

            GameState gs;
            gs.init(static_cast<Difficulty>(diff - 1));
            runGameLoop(gs);
        } else if (choice == 2) {
            GameState gs;
            if (loadGame(gs, SAVE_FILE)) {
                std::cout << "Game loaded!\n";
                pause("Press Enter to continue...");
                runGameLoop(gs);
            } else {
                std::cout << "No save file found.\n";
                pause("Press Enter to continue...");
            }
        } else if (choice == 3) {
            drawHelp();
        }
    }

    return 0;
}
