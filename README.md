# Protocol 1911 - HKU Campus

## Overview
Protocol 1911 is a terminal-based survival / strategy game inspired by Five Nights-style information management. The player defends Main Building (`MB`) by scanning camera clusters, performing exact room scans, using lures, and controlling office gates at key approach buildings.

The goal is to survive until 6 AM across multiple nights while managing limited energy and incomplete information.

## Build and Run
Compile on the HKU Linux / academy server from the project root:

```bash
g++ -std=c++11 main.cpp game.cpp enemy.cpp graph_algos.cpp map.cpp save_load.cpp ui.cpp terminal.cpp -o protocol1911 -lncurses
./protocol1911
```

Run the executable from the repository root so the ASCII templates in `maps/` can be loaded correctly.
The in-game HUD uses the standard Linux `ncurses` library for its centered bordered viewport, so link with `-lncurses`.

## Controls
- `Arrow Keys`: move the cursor between connected buildings
- `Enter`: Deep Scan the selected building
- `G`: toggle the gate at the selected building (`KAD` / `KNOW`, plus `LIB` in Hard mode)
- `A`: Quick Sweep a camera cluster
- `L`: place a lure at the selected building
- `W`: wait / listen
- `Q`: save and quit
- `H`: help

## Cluster-Based Per-Turn Ping System
The camera system is no longer always-on.

Each turn, the player must actively choose whether to spend energy on information:
- `Quick Sweep`: scans one camera cluster and only reports whether movement is present in that cluster
- `Deep Scan`: scans one exact room and can confirm the enemy's precise location

After the turn ends, the scan is gone. If the player wants updated information on a later turn, they must scan again.

This creates survival-horror tension because the player must keep deciding between:
- broad but cheap information
- precise but expensive confirmation
- cursor-based gate defense
- cursor-based lure placement
- waiting and relying on audio

## Quick Sweep vs Deep Scan
### Quick Sweep
- Checks one camera cluster
- Uses a numbered cluster menu
- Cheaper than Deep Scan
- Does not reveal the exact room

Example:
- `QUICK SWEEP - West Flank`
- `Movement detected in West Flank.`

### Deep Scan
- Checks one exact building
- Uses the current cursor location
- Costs more energy
- Reveals the exact room if the enemy is there

Example:
- `DEEP SCAN - LIB`
- `Enemy detected at LIB.`

## Gate Control
- Gates are controlled with the cursor and `G`
- `KAD` and `KNOW` are the standard gate nodes
- Hard mode adds a third center gate at `LIB`
- Closing a gate costs energy
- Opening a gate costs `0` energy but still uses the turn
- There is no command to close both gates at once

## Energy Cost by Difficulty
### Easy
- Quick Sweep: `1%`
- Deep Scan: `2%`
- Gate close cost: `1%`
- Door upkeep: `1%` per turn while the gate stays closed
- Lure: `2%`

### Normal
- Quick Sweep: `2%`
- Deep Scan: `4%`
- Gate close cost: `2%`
- Door upkeep: `1%` per turn while the gate stays closed
- Lure: `4%`

### Hard
- Quick Sweep: `2%`
- Deep Scan: `5%`
- Gate close cost: `2%`
- Door upkeep: `1%` per turn while the gate stays closed
- Lure: `4%`

### Wait Rule
`Wait / Listen` is intentionally free.
- the turn still advances
- the enemy still moves
- win/loss checks still happen
- no direct action energy is drained on a pure wait turn

### Door Upkeep Rule
- upkeep is charged per `closed` office gate, not per open gate
- `0` closed gates = `0` door upkeep
- `1` closed gate = upkeep for `1`
- `2` closed gates = upkeep for `2`
- Hard mode can have `3` closed gates = upkeep for `3`
- this upkeep applies every turn while a gate remains closed, including `Wait` turns

This keeps waiting free as an action, but closed gates still tax the power grid.

## Audio Hint System
Audio hints are probabilistic but never fake. If a hint appears, it is based on the enemy's real distance from the office after the enemy moves.

If the enemy is 4 or more steps away from the office, no audio hint is generated.

### Audio Hint Probability by Distance
#### Easy
- 3 steps away: `25%`
- 2 steps away: `40%`
- 1 step away: `65%`

#### Normal
- 3 steps away: `20%`
- 2 steps away: `35%`
- 1 step away: `60%`

#### Hard
- 3 steps away: `10%`
- 2 steps away: `25%`
- 1 step away: `45%`

### Design Intention
- Easy hints are clearer and more frequent
- Normal hints are useful but vague
- Hard hints are rarer and more atmospheric

Audio is a support tool. It does not replace cameras.

## Signal Decay System
When a Deep Scan finds the enemy, the game stores:
- the exact room
- the turn when that room was detected

The signal then decays over time:

### Easy
- current turn: `Strong`
- next three turns: `Weak`
- then: `Gone`

### Normal
- current turn: `Strong`
- next two turns: `Weak`
- then: `Gone`

### Hard
- current turn: `Strong`
- next turn: `Weak`
- then: `Gone`

Display examples:
- current turn: `LIB [!!]`
- stale but still useful: `LIB [?]`
- expired: `Unknown`

This forces the player to reason about where the enemy might have moved after the last confirmed sighting.

## Weighted Enemy Movement
Enemy movement is hidden from the player and uses graph distance to the office.

Each turn, the enemy chooses among legal neighboring rooms using:
- rooms that move closer to the office
- rooms that keep the same distance
- random valid neighbors

### Movement Probabilities
#### Easy
- closer: `50%`
- sideways: `30%`
- random: `20%`

#### Normal
- closer: `65%`
- sideways: `25%`
- random: `10%`

#### Hard
- closer: `70%`
- sideways: `20%`
- random: `10%`

This makes the enemy feel like it is stalking the player instead of wandering aimlessly, while still allowing some unpredictability on Easy and Normal.

## Lure Behavior
- Lure: place a distraction at the selected building. If the enemy is nearby, it may move toward the lure instead of the office.
- The effect is probabilistic, not guaranteed.
- After normal office-seeking weights are assigned, moves that get closer to the lure receive an extra multiplier.

### Lure Tuning by Difficulty
#### Easy
- cost: `2%`
- cooldown: `3` turns
- duration: `3` turns
- lure weight multiplier: `2.5x`

#### Normal
- cost: `4%`
- cooldown: `4` turns
- duration: `3` turns
- lure weight multiplier: `2.75x`

#### Hard
- cost: `4%`
- cooldown: `4` turns
- duration: `2` turns
- lure weight multiplier: `3.0x`

## Difficulty Identity
### Easy
- cheaper scans
- longer signal decay
- clearer audio hints
- more forgiving movement pattern

### Normal
- intended balanced mode
- useful but limited audio support
- short-lived signal memory

### Hard
- expensive scans
- weak audio support
- almost no stale signal memory
- strongly office-seeking enemy
- 12 active-building Trident map: Skyline opens into West, Center, and East lanes, with `HOC` as the main mid-map transfer hub
- `MB` is reachable through three gate rooms: `KAD`, `LIB`, and `KNOW`

## Save / Load
- The game saves to `protocol1911_save.txt`
- ASCII maps are loaded from:
  - `maps/map_easy.txt`
  - `maps/map_normal.txt`
  - `maps/map_hard.txt`

## COMP2113 Requirement Relevance
### Random Events
- audio hint probability is random but truthful
- enemy movement uses weighted random choice
- enemy spawn is chosen from rooms far from the office

### Data Structures
- the campus is stored as a graph using rooms and adjacency lists
- camera clusters are represented through room-group membership
- signal memory uses room/turn state
- probability analysis uses maps and vectors

### Dynamic Memory
- STL containers such as `std::vector` and `std::map` dynamically manage:
  - rooms
  - adjacency lists
  - scan output
  - probability state
  - event log

### File I/O
- save/load uses plain-text file serialization
- map templates are read from external text files

### Multiple Files
- logic is split across `main`, `game`, `enemy`, `map`, `graph_algos`, `ui`, `terminal`, and `save_load`

### Multiple Difficulty Levels
- Easy, Normal, and Hard each change:
  - map size
  - scan costs
  - signal decay
  - audio hint probability
  - lure cooldown
  - enemy movement behavior
