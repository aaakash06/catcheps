#include "game.h"
#include "ui.h"
#include "terminal.h"
#include "save_load.h"
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <unistd.h>

#define CLR_RESET   "\033[0m"
#define CLR_CYAN    "\033[36m"

static int promptNumberedMenu(const std::string &title,
                              const std::vector<std::string> &options) {
    restoreTerminal();

    while (true) {
        clearScreen();
        std::cout << "\n" << title << "\n";
        for (size_t i = 0; i < options.size(); i++)
            std::cout << "  " << CLR_CYAN << "[" << (i + 1) << "]" << CLR_RESET
                      << " " << options[i] << "\n";
        std::cout << "  " << CLR_CYAN << "[0]" << CLR_RESET << " Cancel\n\n";

        int choice = promptInt("  Select: ", 0, static_cast<int>(options.size()));
        if (choice == 0) {
            initTerminal();
            return -1;
        }
        if (choice >= 1 && choice <= static_cast<int>(options.size())) {
            initTerminal();
            return choice - 1;
        }

        std::cout << "  Invalid choice.\n";
        pause("  Press Enter to try again...");
    }
}

static int selectCameraGroup(GameState &gs, int cursorRoom) {
    std::vector<std::string> options;
    for (int i = 0; i < gs.gameMap.numCameraGroups; i++) {
        auto roomIds = roomsInGroup(gs.gameMap, i);
        std::string option = cameraGroupLabel(gs.gameMap, i) + " (";
        for (size_t j = 0; j < roomIds.size(); j++) {
            if (j > 0) option += ", ";
            option += gs.gameMap.rooms[roomIds[j]].abbrev;
        }
        option += ")";
        if (roomInCameraGroup(gs.gameMap, cursorRoom, i))
            option += " <- cursor location";
        options.push_back(option);
    }
    return promptNumberedMenu("  Select camera cluster:", options);
}

static void runGameLoop(GameState &gs) {
    int cursorRoom = gs.gameMap.officeId;

    while (gs.status == STATUS_PLAYING) {
        drawGame(gs, cursorRoom);

        Key k = getKey();

        if (k == KEY_UP) {
            cursorRoom = getNextRoomNav(gs, cursorRoom, 0);
            continue;
        }
        if (k == KEY_DOWN) {
            cursorRoom = getNextRoomNav(gs, cursorRoom, 1);
            continue;
        }
        if (k == KEY_LEFT) {
            cursorRoom = getNextRoomNav(gs, cursorRoom, 2);
            continue;
        }
        if (k == KEY_RIGHT) {
            cursorRoom = getNextRoomNav(gs, cursorRoom, 3);
            continue;
        }

        // Actions
        if (k == KEY_A) {
            // Quick Sweep
            int group = selectCameraGroup(gs, cursorRoom);
            if (group >= 0)
                gs.doTurn(1, group);
        } else if (k == KEY_ENTER) {
            // Deep Scan selected building
            gs.doTurn(2, cursorRoom);
        } else if (k == KEY_G) {
            // Toggle selected gate if present
            gs.doTurn(3, cursorRoom);
        } else if (k == KEY_L) {
            // Lure at selected building
            gs.doTurn(4, cursorRoom);
        } else if (k == KEY_W || k == KEY_SPACE) {
            // End turn (wait / listen)
            gs.doTurn(5, -1);
        } else if (k == KEY_Q) {
            // Save & quit
            restoreTerminal();
            if (saveGame(gs, SAVE_FILE))
                std::cout << "\n  Game saved! Returning to menu...\n";
            else
                std::cout << "\n  Failed to save game.\n";
            pause("  Press Enter to continue...");
            return;
        } else if (k == KEY_H || k == KEY_QUESTION) {
            // Help overlay
            drawHelp();
        }
    }

    drawEndGame(gs);
}

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    while (true) {
        drawMainMenu();

        // Menu uses simple std::cin since terminal is in cooked mode
        int choice = promptInt("  Select: ", 0, 3);
        if (choice == -1) continue;

        if (choice == 0) {
            std::cout << "  Thanks for playing Camera Watch - HKU Campus!\n";
            break;
        } else if (choice == 1) {
            drawDifficultyMenu();
            int diff = promptInt("  Select difficulty: ", 1, 3);
            if (diff == -1) continue;

            GameState gs;
            gs.init(static_cast<Difficulty>(diff - 1));

            initTerminal();
            runGameLoop(gs);
            restoreTerminal();
        } else if (choice == 2) {
            GameState gs;
            if (loadGame(gs, SAVE_FILE)) {
                std::cout << "  Game loaded!\n";
                pause("  Press Enter to continue...");
                initTerminal();
                runGameLoop(gs);
                restoreTerminal();
            } else {
                std::cout << "  No save file found.\n";
                pause("  Press Enter to continue...");
            }
        } else if (choice == 3) {
            drawHelp();
        }
    }

    return 0;
}
