#ifndef UI_H
#define UI_H

#include <string>
#include <vector>
#include "game.h"

// Clear terminal screen.
void clearScreen();

// Draw the full game HUD with spatial map.
void drawGame(const GameState &gs, int cursorRoom);

// Draw the main menu.
void drawMainMenu();

// Draw difficulty selection.
void drawDifficultyMenu();

// Draw the spatial ASCII map.
void drawMap(const GameState &gs, int cursorRoom);

// Draw camera check results.
void drawCameraFeed(const GameState &gs);

// Draw risk scan results.
void drawRiskScan(const GameState &gs);

// Draw end game screen.
void drawEndGame(const GameState &gs);

// Prompt for an integer in range [lo, hi]. Returns -1 on failure.
int promptInt(const std::string &msg, int lo, int hi);

// Display a message and wait for enter.
void pause(const std::string &msg);

// Show help screen.
void drawHelp();

// Get next room for cursor navigation (following neighbor order).
int getNextRoomNav(const GameState &gs, int currentCursor, int direction);

#endif
