#ifndef UI_H
#define UI_H

#include <string>
#include <vector>
#include "game.h"

// Clear terminal screen.
void clearScreen();

// Dynamic terminal centering helpers.
int getCenterY(int totalRows);
int getCenterX(int textLength);
void printAt(int y, int x, const std::string &text);
void printCentered(int y, const std::string &text);
void drawCenteredArt(int startY, const std::string &art);

// Start/stop the ncurses viewport used by the in-game HUD.
bool initializeCurses();
void shutdownCurses();
void startGameViewport();
void stopGameViewport();

enum MainMenuChoice {
    MENU_START_GAME,
    MENU_LOAD_GAME,
    MENU_HOW_TO_PLAY,
    MENU_QUIT
};

// Ncurses startup flow.
bool ensureStartupTerminalSize();
void showTitleScreen();
void showStoryline();
MainMenuChoice showMainMenu();
bool showModeMenu(Difficulty &difficulty);

// Draw the full game HUD with spatial map.
void drawGame(const GameState &gs, int cursorRoom);

// Draw the game HUD with an in-panel Quick Sweep cluster picker.
void drawSweepSelection(const GameState &gs, int cursorRoom, int selectedGroup);

// Draw the spatial ASCII map.
void drawMap(const GameState &gs, int cursorRoom);

// Draw camera check results.
void drawCameraFeed(const GameState &gs);

// Draw end game screen.
void drawEndGame(const GameState &gs);

// Display a message and wait for enter.
void pause(const std::string &msg);

// Show help screen.
void drawHelp();

// Get next room for cursor navigation (following neighbor order).
int getNextRoomNav(const GameState &gs, int currentCursor, int direction);

#endif
