#ifndef SAVE_LOAD_H
#define SAVE_LOAD_H

#include <string>
#include "game.h"

// Save game state to file. Returns true on success.
bool saveGame(const GameState &gs, const std::string &filename);

// Load game state from file. Returns true on success.
bool loadGame(GameState &gs, const std::string &filename);

// Default save filename.
const std::string SAVE_FILE = "protocol1911_save.txt";

#endif
