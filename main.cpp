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

static int selectCameraGroup(GameState &gs) {
    std::vector<std::string> options;
    for (int i = 0; i < gs.gameMap.numCameraGroups; i++) {
        auto roomIds = roomsInGroup(gs.gameMap, i);
        std::string option = cameraGroupLabel(gs.gameMap, i) + " (";
        for (size_t j = 0; j < roomIds.size(); j++) {
            if (j > 0) option += ", ";
            option += gs.gameMap.rooms[roomIds[j]].abbrev;
        }
        option += ")";
        options.push_back(option);
    }
    return promptNumberedMenu("  Select camera cluster:", options);
}

static int selectDeepScanRoom(GameState &gs) {
    std::vector<int> roomIds;
    std::vector<std::string> options;

    for (int roomId = 0; roomId < gs.gameMap.totalRooms; roomId++) {
        if (roomId == gs.gameMap.officeId)
            continue;
        roomIds.push_back(roomId);
        options.push_back(gs.gameMap.rooms[roomId].abbrev + " - " + gs.gameMap.rooms[roomId].name);
    }

    int choice = promptNumberedMenu("  Select building for Deep Scan:", options);
    if (choice < 0)
        return -1;
    return roomIds[choice];
}

static void runGameLoop(GameState &gs) {
    while (gs.status == STATUS_PLAYING) {
        drawGame(gs);

        Key k = getKey();

        // Actions
        if (k == KEY_A) {
            // Quick Sweep
            int group = selectCameraGroup(gs);
            if (group >= 0)
                gs.doTurn(1, group);
        } else if (k == KEY_S) {
            // Deep Scan one building from a numbered menu
            int roomId = selectDeepScanRoom(gs);
            if (roomId >= 0)
                gs.doTurn(2, roomId);
        } else if (k == KEY_Z) {
            // Toggle KNOW gate
            gs.doTurn(3, 4);
        } else if (k == KEY_X) {
            // Toggle KAD gate
            gs.doTurn(3, 3);
        } else if (k == KEY_C) {
            // Toggle both office gates
            gs.doTurn(8, -1);
        } else if (k == KEY_L) {
            // Lure
            int group = selectCameraGroup(gs);
            if (group >= 0)
                gs.doTurn(4, group);
        } else if (k == KEY_W) {
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
