# Camera Watch - HKU Campus

Camera Watch is a terminal survival-horror game set on the HKU campus. You are the night security guard in Main Building, trying to survive until 6 AM while an intruder moves secretly through a graph of connected campus buildings.

## Build and Run

```bash
g++ *.cpp -o camerawatch -std=c++11
./camerawatch
```

On Windows, the program also builds as `camerawatch.exe` and uses a `_getch()` terminal fallback.

## Core Loop

Each turn, choose one action:

1. Quick Sweep a camera cluster
2. Deep Scan one building
3. Close or restore a door
4. Use a lure
5. Wait and listen

After the player action, power is deducted, the intruder moves secretly, audio may provide a truthful hint, camera signal memory decays, and win/loss conditions are checked.

## Cluster-Based Per-Turn Ping System

Cameras no longer stay on permanently. A scan is a temporary ping: it reports the result, then the camera turns off. If you want updated information on a later turn, you must spend energy to scan again.

### Quick Sweep

Quick Sweep checks one camera cluster:

- `WEST WING`: MB, KKL, LIB, KAD, KNOW
- `CORE CAMPUS`: HC, HW, CYM
- `EAST WING`: MW, RM, RHS, RR, JL

It reports only whether movement is detected in that cluster. It does not reveal the exact room.

Example:

```text
SCAN CORE CAMPUS
Movement detected in CORE CAMPUS.
```

### Deep Scan

Deep Scan checks the exact building currently selected by the cursor. It costs more power than Quick Sweep, but if the intruder is there, it reveals the exact position.

Example:

```text
SCAN LIB
Enemy detected at LIB.
```

## Energy Costs

| Difficulty | Quick Sweep | Deep Scan | Door Active Cost | Lure |
| --- | ---: | ---: | ---: | ---: |
| Easy | 1% | 3% | 1% per turn | 3% |
| Normal | 2% | 5% | 1% per turn | 4% |
| Hard | 3% | 6% | 2% per turn | 5% |

Doors do not use a large one-time power cost anymore. Their pressure comes from ongoing upkeep while they remain closed.

## Audio Hint System

Audio hints are reliable but probabilistic: they never intentionally lie, but they are not guaranteed. Cameras remain the main source of reliable information.

| Distance From Office | Easy | Normal | Hard |
| ---: | ---: | ---: | ---: |
| 3 steps | 25% | 20% | 10% |
| 2 steps | 40% | 35% | 25% |
| 1 step | 65% | 60% | 45% |
| 4+ steps | 0% | 0% | 0% |

Easy hints are clearer, Normal hints are vague but useful, and Hard hints are less frequent and more ambiguous. Audio does not reveal the exact room unless the intruder is directly outside the office approach.

## Signal Decay

When a camera detects movement, the game stores a last-known signal and the turn it was detected. The signal becomes less trustworthy as turns pass.

| Difficulty | Signal Pattern |
| --- | --- |
| Easy | Strong -> Weak -> Weak -> Gone |
| Normal | Strong -> Weak -> Gone |
| Hard | Strong -> Gone |

Display examples:

```text
LIB [!!]       current turn detection
LIB [?]        old signal, enemy may have moved
No reliable signal.
```

Quick Sweep stores a cluster-level signal such as `WEST WING [!!]`. Deep Scan stores an exact building signal such as `LIB [!!]`.

## Weighted Enemy Movement

The intruder remains hidden and moves using graph distance toward the office or an active lure target. For each valid neighboring room, the game classifies movement as closer, sideways, or random/farther relative to the current target.

| Difficulty | Move Closer | Sideways | Random |
| --- | ---: | ---: | ---: |
| Easy | 55% | 30% | 15% |
| Normal | 65% | 25% | 10% |
| Hard | 85% | 15% | 0% |

This keeps the enemy from feeling purely random while still leaving room for prediction, lures, and difficulty identity.

## Why This Adds Tension

The player cannot camp one permanent camera cluster anymore. Quick Sweep gives cheap broad information, Deep Scan gives expensive precise information, audio gives truthful but unreliable support, and signal decay forces the player to reason from stale evidence. The result is a more active survival-horror loop: spend power to know, spend power to defend, or wait and risk uncertainty.

## COMP2113 Requirement Relevance

- Random events: audio hint probability, weighted enemy movement selection, lure-influenced movement.
- Data structures: graph map of rooms, vectors for adjacency lists and camera results, maps for probability and danger estimates, camera signal memory, enemy state.
- Dynamic memory: STL containers such as `vector` and `map` dynamically manage room lists, graph edges, probability maps, camera history, and event logs.
