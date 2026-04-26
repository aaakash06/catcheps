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

static int selectCameraGroup(GameState &gs) {
    // Show groups and wait for key selection
    std::cout << "\n  Select camera cluster:\n";
    for (int i = 0; i < gs.gameMap.numCameraGroups; i++) {
        auto roomIds = roomsInGroup(gs.gameMap, i);
        std::cout << "  " << CLR_CYAN << "[" << (i+1) << "]" << CLR_RESET
                  << " " << cameraGroupLabel(gs.gameMap, i) << " (";
        for (size_t j = 0; j < roomIds.size(); j++) {
            if (j > 0) std::cout << ",";
            std::cout << gs.gameMap.rooms[roomIds[j]].abbrev;
        }
        std::cout << ")\n";
    }
    std::cout << "  " << CLR_CYAN << "[0]" << CLR_RESET << " Cancel\n";
    std::cout << "  > " << std::flush;

    while (true) {
        Key k = getKey();
        if (k == KEY_1 && gs.gameMap.numCameraGroups >= 1) return 0;
        if (k == KEY_2 && gs.gameMap.numCameraGroups >= 2) return 1;
        if (k == KEY_3 && gs.gameMap.numCameraGroups >= 3) return 2;
        if (k == KEY_4 && gs.gameMap.numCameraGroups >= 4) return 3;
        if (k == KEY_0 || k == KEY_ESCAPE) return -1;
    }
}

static void runGameLoop(GameState &gs) {
    int cursorRoom = gs.gameMap.officeId; // start cursor at office

    while (gs.status == STATUS_PLAYING) {
        drawGame(gs, cursorRoom);

        Key k = getKey();

        // Cursor navigation (arrow keys only)
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
        if (k == KEY_C) {
            // Quick Sweep
            int group = selectCameraGroup(gs);
            if (group >= 0)
                gs.doTurn(1, group);
        } else if (k == KEY_2 || k == KEY_S) {
            // Deep Scan current room
            gs.doTurn(2, cursorRoom);
        } else if (k == KEY_4 || k == KEY_L) {
            // Lure
            int group = selectCameraGroup(gs);
            if (group >= 0)
                gs.doTurn(4, group);
        } else if (k == KEY_3 || k == KEY_D) {
            // Toggle gate at cursor
            gs.doTurn(3, cursorRoom);
        } else if (k == KEY_R) {
            // Open gate at cursor
            gs.doTurn(6, cursorRoom);
        } else if (k == KEY_A) {
            // Risk scan
            gs.doTurn(7, -1);
        } else if (k == KEY_5 || k == KEY_E || k == KEY_DOT || k == KEY_SPACE) {
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
        } else if (k == KEY_ENTER) {
            // Enter on a building — toggle gate action
            gs.doTurn(3, cursorRoom);
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
