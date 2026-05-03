# Protocol 1911

## Team Members

- Aakash Bagale Thapa (3036478588)
- Pham Khanh Huyen (3036517384)
- Lam Pun Chak (3036590453)

---

## Menu

1. [Overview](#overview)
2. [Build and Run](#build-and-run)
3. [Controls](#controls)
4. [Demo Video](#demo-video)
5. [Gameplay Features](#gameplay-features)
6. [Difficulty Levels](#difficulty-levels)
7. [Save and Load](#save-and-load)      
8. [Code Structure](#code-structure)
9. [COMP2113 Requirement Relevance](#comp2113-requirement-relevance)
10. [Known Limitations](#known-limitations)

---

## Overview

**Protocol 1911** is a terminal-based survival strategy game set on the HKU campus.

The player defends Main Building (`MB`) from an intruder by using camera scans, audio lures, and security gates while managing limited energy. The goal is to survive until **6 AM** across multiple nights.

The game is designed for the HKU **COMP2113** course project and runs in a Linux terminal through SSH.

---

## Build and Run

Compile and run from the project root on the HKU Linux / academy server:

```bash
make clean
make
./protocol1911
```

Equivalent manual compile command:

```bash
g++ -std=c++11 -Wall -Wextra -pedantic main.cpp game.cpp enemy.cpp graph_algos.cpp map.cpp save_load.cpp ui.cpp terminal.cpp -o protocol1911 -lncurses
./protocol1911
```

Run the executable from the repository root so that the ASCII map templates in `maps/` can be loaded correctly.

The game uses the standard Linux `ncurses` library for terminal UI rendering. No other non-standard libraries are required.

---

## Controls

| Key        | Action                                      |
| ---------- | ------------------------------------------- |
| Arrow Keys | Move the cursor between connected buildings |
| Enter      | Deep Scan the selected building             |
| G          | Toggle the gate at the selected building    |
| A          | Quick Sweep a camera cluster                |
| L          | Place a lure at the selected building       |
| W / Space  | Wait / listen                               |
| Q          | Save and quit                               |
| H / ?      | Show help screen                            |

Gate buildings:

| Difficulty    | Gate Buildings       |
| ------------- | -------------------- |
| Easy / Normal | `KAD`, `KNOW`        |
| Hard          | `KAD`, `LIB`, `KNOW` |

---

## Demo Video

A 2:30 minutes short Demonstration Video
Allows you to easily undestand the game

[Watch the Demo Video](https://files.fm/u/fquvt77y8d)
-Copy the link or open it directly to watch

---

## Gameplay Features

### Core Turn System

Each turn, the player chooses one action. Then the enemy may move, energy may be drained, hints may appear, and win/loss conditions are checked.

The player wins by surviving until **6 AM**.

The player loses if:

- the enemy reaches `MB`, or
- energy reaches `0%`.

---

### Camera System

The cameras are not always active. The player must spend energy to scan.

| Scan Type   | Description                                                                                                       |
| ----------- | ----------------------------------------------------------------------------------------------------------------- |
| Quick Sweep | Scans one camera cluster and reports whether movement is detected. It is cheap but imprecise.                     |
| Deep Scan   | Scans the selected building and confirms the enemy only if it is exactly there. It is more expensive but precise. |

Example Quick Sweep output:

```text
QUICK SWEEP - West
Movement detected in the selected camera cluster.
```

Example Deep Scan output:

```text
DEEP SCAN - LIB
Enemy detected at LIB.
```

---

### Signal Memory

Signal memory stores the last known enemy room and the turn when that information was recorded.

At the start of each night, the game initializes the last known enemy room to the enemy's spawn location. During gameplay, a successful Deep Scan can update this information with a newly confirmed room.

The signal then decays over time:

| Difficulty | Strong Signal | Weak Signal  | Gone       |
| ---------- | ------------- | ------------ | ---------- |
| Easy       | current turn  | next 3 turns | after that |
| Normal     | current turn  | next 2 turns | after that |
| Hard       | current turn  | next 2 turns | after that |

Display examples:

```text
LIB [!!]   current confirmed signal
LIB [?]    stale signal
Unknown    expired signal
```

---

### Gate Control

The player can close gates near `MB` to block the enemy.

- Gates are toggled using the cursor and `G`.
- Closing a gate costs energy.
- Opening a gate costs `0%` energy but still uses a turn.
- Closed gates drain upkeep every turn, including wait turns.
- There is no command to close multiple gates at once.

---

### Energy System

Energy is the main resource. The player loses if energy reaches `0%`.

| Action        |                 Easy |               Normal |                 Hard |
| ------------- | -------------------: | -------------------: | -------------------: |
| Quick Sweep   |                 `1%` |                 `2%` |                 `1%` |
| Deep Scan     |                 `2%` |                 `4%` |                 `5%` |
| Gate Close    |                 `2%` |                 `2%` |                 `2%` |
| Door Upkeep   | `1%` per closed gate | `1%` per closed gate | `1%` per closed gate |
| Lure          |                 `2%` |                 `4%` |                 `3%` |
| Wait / Listen |                 `0%` |                 `0%` |                 `0%` |

`Wait / Listen` is free as an action, but the turn still advances, the enemy still moves, and closed gates still drain upkeep.

Hard mode keeps Quick Sweep cheap because Hard has a larger map, weaker audio hints, and more expensive Deep Scan. This encourages broad but uncertain scanning while making exact confirmation costly.

---

### Audio Hints

Audio hints are random but truthful. If a hint appears, it is based on the enemy's real distance from `MB` after the enemy moves.

No audio hint is generated if the enemy is 4 or more steps away.

| Distance from `MB` |  Easy | Normal |  Hard |
| ------------------ | ----: | -----: | ----: |
| 3 steps away       | `25%` |  `20%` | `10%` |
| 2 steps away       | `40%` |  `35%` | `25%` |
| 1 step away        | `65%` |  `60%` | `45%` |

Audio supports deduction but does not replace cameras.

---

### Enemy Movement

Enemy movement is hidden from the player. Each turn, the enemy chooses among legal neighboring rooms using weighted randomness.

| Movement Type       |  Easy | Normal |  Hard |
| ------------------- | ----: | -----: | ----: |
| Move closer to `MB` | `50%` |  `65%` | `70%` |
| Move sideways       | `30%` |  `25%` | `20%` |
| Random valid move   | `20%` |  `10%` | `10%` |

This makes the enemy usually move toward `MB`, while still keeping movement unpredictable.

---

### Lure

The player can place a lure at the selected building.

While a lure is active, any legal enemy move that gets closer to the lure receives an extra weight multiplier. The lure does not force movement; it only biases the enemy's weighted choice.

| Difficulty | Cost |  Cooldown |  Duration | Multiplier |
| ---------- | ---: | --------: | --------: | ---------: |
| Easy       | `2%` | `3` turns | `3` turns |     `2.5x` |
| Normal     | `4%` | `4` turns | `3` turns |    `2.75x` |
| Hard       | `3%` | `4` turns | `2` turns |     `3.0x` |

---

## Difficulty Levels

Protocol 1911 has three difficulty levels with meaningful gameplay differences.

| Difficulty | Main Characteristics                                                                                                                                              |
| ---------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Easy       | Smaller map, cheaper scans, longer signal memory, clearer audio hints, and more forgiving enemy movement.                                                         |
| Normal     | Balanced scan costs, moderate audio support, short signal memory, and medium enemy aggression.                                                                    |
| Hard       | Larger Trident-style map, expensive Deep Scan, weaker audio support, shorter lure duration, stronger enemy movement toward `MB`, and three gate routes into `MB`. |

---

## Save and Load

The game saves to:

```text
protocol1911_save.txt
```

ASCII maps are loaded from:

```text
maps/map_easy.txt
maps/map_normal.txt
maps/map_hard.txt
```

Save/load behavior:

- Press `Q` during gameplay to save and return to the menu.
- Use `Load Game` from the main menu to resume.
- Corrupted or incompatible save files are rejected where possible.
- If a map template is missing, the game displays a fallback warning instead of crashing.

---

## Code Structure

```text
main.cpp
- Program entry point
- Menu flow
- Main game loop
- Keyboard action dispatch

game.cpp / game.h
- GameState structure
- Turn system
- Energy system
- Win/loss checks
- Scan, gate, lure, and difficulty logic

enemy.cpp / enemy.h
- Enemy state
- Weighted random movement
- Lure response

map.cpp / map.h
- Campus graph construction
- Room list
- Camera cluster definitions
- Spawn-room selection

graph_algos.cpp / graph_algos.h
- BFS shortest paths
- Distance maps
- Graph analysis helpers
- Probability diffusion

save_load.cpp / save_load.h
- Save file writing
- Save file loading
- Save data validation

ui.cpp / ui.h
- ncurses terminal interface
- Menus
- HUD
- ASCII map rendering
- Help screen
- End screens

terminal.cpp / terminal.h
- Raw terminal input
- Key parsing
- Terminal restoration
```

---

## COMP2113 Requirement Relevance

### 1. Random Events

The game includes meaningful randomness:

- random enemy spawn from far rooms,
- weighted random enemy movement,
- probabilistic audio hints,
- probabilistic lure influence.

These random events affect gameplay and replayability.

---

### 2. Data Structures

The project uses data structures to manage the game state:

- the campus is stored as a graph using rooms and adjacency lists,
- camera clusters are stored as room groups,
- signal memory stores room and turn information,
- game state tracks energy, difficulty, gates, lures, and enemy state,
- event logs store recent messages,
- probability analysis uses maps and vectors.

Important STL containers include:

- `std::vector`
- `std::map`
- `std::string`

---

### 3. Dynamic Memory Management

The project uses STL containers such as `std::vector`, `std::map`, and `std::string`.

These containers dynamically manage memory for rooms, adjacency lists, camera clusters, scan output, probability state, event logs, and save/load data.

No raw `new` or `delete` is required.

---

### 4. File Input / Output

The project uses file I/O for:

- loading ASCII map templates from the `maps/` folder,
- saving game state to `protocol1911_save.txt`,
- loading previous game state,
- rejecting invalid save data where possible.

---

### 5. Program Codes in Multiple Files

The project is split into multiple `.cpp` and `.h` files for game logic, enemy movement, map construction, graph algorithms, UI, terminal input, and save/load.

---

### 6. Multiple Difficulty Levels

Easy, Normal, and Hard affect:

- map size,
- scan costs,
- signal decay,
- audio hint probability,
- lure cooldown and duration,
- enemy movement behavior,
- gate layout,
- overall pressure on the player.

Difficulty levels therefore change actual gameplay, not only labels.

---

## Known Limitations

- The game is designed for a terminal of at least `58 x 22`; smaller terminals show a size warning.
- The UI depends on Linux terminal behavior and `ncurses`, so the intended grading environment is the HKU CS Linux / academy server over terminal or SSH.
- Save files are plain text for readability. Invalid edited values are rejected where possible.
- The game tracks one active enemy entity.