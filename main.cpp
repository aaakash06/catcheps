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

static int keyToCameraGroupIndex(Key key) {
    if (key == KEY_1) return 0;
    if (key == KEY_2) return 1;
    if (key == KEY_3) return 2;
    if (key == KEY_4) return 3;
    if (key == KEY_5) return 4;
    return -1;
}

static void exitAfterInterrupt() {
    stopGameViewport();
    restoreTerminal();
    std::cout << "\nInterrupted. Terminal restored.\n";
    std::exit(130);
}

static int initialCameraGroupSelection(GameState &gs, int cursorRoom) {
    int groupCount = effectiveCameraGroupCount(gs.gameMap);
    for (int i = 0; i < groupCount; i++) {
        if (roomInCameraGroup(gs.gameMap, cursorRoom, i))
            return i;
    }
    return 0;
}

static int selectCameraGroup(GameState &gs, int cursorRoom) {
    int groupCount = effectiveCameraGroupCount(gs.gameMap);
    if (groupCount <= 0)
        return -1;

    int selectedGroup = initialCameraGroupSelection(gs, cursorRoom);
    while (true) {
        drawSweepSelection(gs, cursorRoom, selectedGroup);
        Key key = getKey();

        if (key == KEY_INTERRUPT)
            exitAfterInterrupt();
        if (key == KEY_ENTER)
            return selectedGroup;
        if (key == KEY_ESCAPE || key == KEY_0 || key == KEY_A || key == KEY_Q)
            return -1;

        int directChoice = keyToCameraGroupIndex(key);
        if (directChoice >= 0 && directChoice < groupCount) {
            selectedGroup = directChoice;
            continue;
        }

        if (key == KEY_LEFT || key == KEY_UP) {
            selectedGroup = (selectedGroup + groupCount - 1) % groupCount;
        } else if (key == KEY_RIGHT || key == KEY_DOWN) {
            selectedGroup = (selectedGroup + 1) % groupCount;
        }
    }
}

static void runGameLoop(GameState &gs) {
    int cursorRoom = gs.gameMap.officeId;

    while (gs.status == STATUS_PLAYING) {
        drawGame(gs, cursorRoom);

        Key k = getKey();

        if (k == KEY_INTERRUPT)
            exitAfterInterrupt();

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
            stopGameViewport();
            restoreTerminal();
            if (saveGame(gs, SAVE_FILE))
                std::cout << "\n  Game saved! Returning to menu...\n";
            else
                std::cout << "\n  Failed to save game.\n";
            pause("  Press Enter to continue...");
            return;
        } else if (k == KEY_H || k == KEY_QUESTION) {
            // Help overlay
            stopGameViewport();
            restoreTerminal();
            drawHelp();
            initTerminal();
            startGameViewport();
        }
    }

    stopGameViewport();
    restoreTerminal();
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
            clearScreen();
            printCentered(getCenterY(1), "Thanks for playing Camera Watch - HKU Campus!");
            std::cout << "\n";
            break;
        } else if (choice == 1) {
            drawDifficultyMenu();
            int diff = promptInt("  Select difficulty: ", 1, 3);
            if (diff == -1) continue;

            GameState gs;
            gs.init(static_cast<Difficulty>(diff - 1));

            initTerminal();
            startGameViewport();
            runGameLoop(gs);
            stopGameViewport();
            restoreTerminal();
        } else if (choice == 2) {
            GameState gs;
            if (loadGame(gs, SAVE_FILE)) {
                std::cout << "  Game loaded!\n";
                pause("  Press Enter to continue...");
                initTerminal();
                startGameViewport();
                runGameLoop(gs);
                stopGameViewport();
                restoreTerminal();
            } else {
                if (!gs.statusMessage.empty())
                    std::cout << "  " << gs.statusMessage << "\n";
                else
                    std::cout << "  No save file found.\n";
                pause("  Press Enter to continue...");
            }
        } else if (choice == 3) {
            drawHelp();
        }
    }

    return 0;
}