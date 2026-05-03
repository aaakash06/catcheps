#include "game.h"
#include "ui.h"
#include "terminal.h"
#include "save_load.h"
#include <cstdlib>
#include <ctime>

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
    shutdownCurses();
    restoreTerminal();
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
            clearScreen();
            if (saveGame(gs, SAVE_FILE)) {
                printCentered(getCenterY(3), "Game saved.");
                printCentered(getCenterY(3) + 1, "Returning to main menu...");
            } else {
                printCentered(getCenterY(2), "Failed to save game.");
            }
            pause("Press Enter to continue...");
            return;
        } else if (k == KEY_H || k == KEY_QUESTION) {
            // Help overlay
            stopGameViewport();
            drawHelp();
            startGameViewport();
        }
    }

    drawEndGame(gs);
}

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    initTerminal();
    if (!initializeCurses()) {
        restoreTerminal();
        return 1;
    }
    if (!ensureStartupTerminalSize()) {
        shutdownCurses();
        restoreTerminal();
        return 1;
    }

    showTitleScreen();
    if (terminalInterruptRequested())
        exitAfterInterrupt();
    showStoryline();
    if (terminalInterruptRequested())
        exitAfterInterrupt();

    while (true) {
        MainMenuChoice choice = showMainMenu();
        if (terminalInterruptRequested())
            exitAfterInterrupt();

        if (choice == MENU_QUIT) {
            break;
        } else if (choice == MENU_START_GAME) {
            Difficulty difficulty = NORMAL;
            if (!showModeMenu(difficulty)) {
                if (terminalInterruptRequested())
                    exitAfterInterrupt();
                continue;
            }

            GameState gs;
            gs.init(difficulty);

            startGameViewport();
            runGameLoop(gs);
            stopGameViewport();
        } else if (choice == MENU_LOAD_GAME) {
            GameState gs;
            clearScreen();
            if (loadGame(gs, SAVE_FILE)) {
                printCentered(getCenterY(2), "Game loaded.");
                pause("Press Enter to continue...");
                startGameViewport();
                runGameLoop(gs);
                stopGameViewport();
            } else {
                if (!gs.statusMessage.empty())
                    printCentered(getCenterY(2), gs.statusMessage);
                else
                    printCentered(getCenterY(2), "No save file found.");
                pause("Press Enter to return to main menu...");
            }
        } else if (choice == MENU_HOW_TO_PLAY) {
            drawHelp();
        }
    }

    clearScreen();
    printCentered(getCenterY(1), "Thanks for playing Protocol 1911.");
    pause("Press Enter to exit...");
    shutdownCurses();
    restoreTerminal();
    return 0;
}
