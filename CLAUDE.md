# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Compile
g++ *.cpp -o camerawatch -std=c++11

# Run (interactive terminal game, requires a terminal for key input)
./camerawatch
```

No external dependencies. Uses only the C++ standard library + POSIX termios for raw terminal input.

## Architecture

Camera Watch is a terminal-based survival horror game set on the HKU campus. The player is a night security guard monitoring campus buildings through cameras while an intruder moves through interconnected rooms.

### Module Structure

- **main.cpp** — Entry point, main menu (uses `std::cin`), and game loop (`runGameLoop`). Game loop uses raw terminal mode via `terminal.cpp` for single-keypress input.
- **game.cpp/h** — `GameState` struct and core game logic: difficulty params, power management, turn processing, camera checks, lures, doors, risk scans, probability map updates.
- **map.cpp/h** — `Room` struct (has `name`, `abbrev`, adjacency list, camera group) and `GameMap` struct. `buildMap()` creates the HKU campus graph: MB(Main Building/office), KKL, LIB, KAD, KNOW, CYM, HC, HW, MW, RM, RHS, RR, JL. Difficulty scales room count (Easy: 8, Normal: 10, Hard: 13). Camera groups: Upper/MW+RM+RHS+RR+JL, Central/HC+HW+CYM, Lower/MB+KKL+LIB+KAD+KNOW.
- **enemy.cpp/h** — `Enemy` struct with states (ROAMING, INVESTIGATING, ATTACKING, AT_OFFICE). Weighted random movement with office bias and lure attraction.
- **ui.cpp/h** — Terminal UI using ANSI escape codes with color. Spatial ASCII map renders HKU buildings at campus positions with connector lines. Hotkey action bar. `drawGame()`, `drawMap()`, `drawMainMenu()`, etc.
- **terminal.cpp/h** — Raw terminal mode via POSIX termios. `getKey()` returns single keypress codes (arrow keys, letters, numbers). `initTerminal()` / `restoreTerminal()`.
- **graph_algos.cpp/h** — BFS pathfinding/distance, articulation point detection, bridge detection, probability diffusion, danger level computation.
- **save_load.cpp/h** — Save/load to a text file (`SAVE_FILE` constant). Serializes all fields including `abbrev`.

### Key Data Flow

1. `GameState::init()` builds the HKU map, spawns the enemy far from MB (office), initializes probability map
2. Game loop: player moves cursor with arrow keys → presses action key (C/L/D/R/S/E/Q) → `doTurn()` processes it → `enemyTurn()` moves enemy → `checkConditions()` checks win/lose
3. Enemy position tracked via probability distribution (`probMap`) that diffuses each turn
4. Power depletes with actions and door maintenance; running out means losing

### Controls

- Arrow keys: move cursor on map
- C=check camera, L=lure, D=close door, R=restore door, S=risk scan, E=end turn, Q=quit, H=help

## Testing

No automated test suite. Testing is manual by running the game.
