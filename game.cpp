#include "game.h"
#include "graph_algos.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

static std::string toLowerCopy(const std::string &text) {
    std::string lowered = text;
    for (size_t i = 0; i < lowered.size(); i++)
        lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
    return lowered;
}

static int primaryClusterForRoom(const GameMap &map, int roomId);
static bool clusterContainsOfficeGate(const GameMap &map, int group);
static bool isOfficeGateRoom(const GameState &gs, int roomId);
static std::string officeEntryGateMessage(const GameState &gs);
static bool hasGraphEdge(const GameMap &map, int a, int b);

static void normalizeProbabilityMap(const GameMap &map, std::map<int, double> &probMap) {
    double total = 0.0;
    for (int roomId = 0; roomId < map.totalRooms; roomId++) {
        double value = probMap.count(roomId) ? probMap[roomId] : 0.0;
        if (value < 0.0 || value != value)
            value = 0.0;
        probMap[roomId] = value;
        total += value;
    }

    if (total <= 0.0) {
        if (map.totalRooms <= 0)
            return;
        double share = 1.0 / map.totalRooms;
        for (int roomId = 0; roomId < map.totalRooms; roomId++)
            probMap[roomId] = share;
        return;
    }

    for (int roomId = 0; roomId < map.totalRooms; roomId++)
        probMap[roomId] /= total;
}

static std::string initialEnemyLog(const GameState &gs, int roomId) {
    const std::string roomCode = gs.gameMap.rooms[roomId].abbrev;
    const int group = primaryClusterForRoom(gs.gameMap, roomId);
    const std::string groupLabel = (group >= 0) ? cameraGroupLabel(gs.gameMap, group) : "Outer Campus";

    if (gs.difficulty == EASY)
        return "Enemy detected at " + roomCode + ".";
    if (gs.difficulty == NORMAL)
        return "Movement detected near the " + groupLabel + ": " + roomCode + " area.";
    return "Enemy detected at " + roomCode + ".";
}

static int primaryClusterForRoom(const GameMap &map, int roomId) {
    for (int group = 0; group < effectiveCameraGroupCount(map); group++) {
        if (roomInCameraGroup(map, roomId, group))
            return group;
    }
    return -1;
}

static bool clusterContainsOfficeGate(const GameMap &map, int group) {
    return roomInCameraGroup(map, 3, group) ||
           roomInCameraGroup(map, 4, group) ||
           (hasGraphEdge(map, 2, map.officeId) && roomInCameraGroup(map, 2, group));
}

static bool isOfficeGateRoom(const GameState &gs, int roomId) {
    return roomId == 3 ||
           roomId == 4 ||
           (roomId == 2 && hasGraphEdge(gs.gameMap, roomId, gs.gameMap.officeId));
}

static std::string officeEntryGateMessage(const GameState &gs) {
    if (gs.enemy.lastRoom == 4)
        return "The intruder entered MB through the KNOW gate.";
    if (gs.enemy.lastRoom == 3)
        return "The intruder entered MB through the KAD gate.";
    if (gs.enemy.lastRoom == 2)
        return "The intruder entered MB through the LIB center gate.";
    return "The intruder entered MB through an unknown gate.";
}

static bool hasGraphEdge(const GameMap &map, int a, int b) {
    if (a < 0 || b < 0 || a >= map.totalRooms || b >= map.totalRooms)
        return false;

    const std::vector<int> &neighbors = map.rooms[a].neighbors;
    return std::find(neighbors.begin(), neighbors.end(), b) != neighbors.end();
}

static int enemyDistanceToOffice(const GameState &gs) {
    auto dist = bfsDistances(gs.gameMap.rooms, gs.gameMap.officeId,
                             gs.gameMap.officeId,
                             gs.leftGateClosed, gs.rightGateClosed,
                             gs.centerGateClosed);
    if (!dist.count(gs.enemy.currentRoom))
        return 999;
    return dist[gs.enemy.currentRoom];
}

static int audioHintChance(const GameState &gs, int distance) {
    if (distance == 3) return gs.audioProbDistance3;
    if (distance == 2) return gs.audioProbDistance2;
    if (distance == 1) return gs.audioProbDistance1;
    return 0;
}

static std::string audioHintText(const GameState &gs, int roomId, int distance) {
    if (distance <= 1) {
        if (gs.difficulty == EASY)
            return "You hear urgent footsteps near the office entrances...";
        if (gs.difficulty == NORMAL)
            return "You hear something close to the inner campus...";
        return "Something is very close to the office...";
    }

    int group = primaryClusterForRoom(gs.gameMap, roomId);
    std::string label = (group >= 0) ? cameraGroupLabel(gs.gameMap, group) : "campus";

    if (gs.difficulty == EASY)
        return "You hear footsteps somewhere near the " + label + "...";
    if (gs.difficulty == NORMAL)
        return "You hear a faint sound from the " + toLowerCopy(label) + "...";
    return "Something echoes in the distance...";
}

static void maybeGenerateAudioHint(GameState &gs) {
    int distance = enemyDistanceToOffice(gs);
    int chance = audioHintChance(gs, distance);
    if (chance <= 0) return;
    if ((std::rand() % 100) >= chance) return;
    gs.eventLog.push_back(audioHintText(gs, gs.enemy.currentRoom, distance));
}

// Returns whether a movement edge is blocked by an office gate.
bool isBlockedEdge(const GameState& gs, int from, int to) {
    const int MB = gs.gameMap.officeId;
    const int LIB = 2;
    const int KAD = 3;
    const int KNOW = 4;

    if ((from == MB && to == KNOW) || (from == KNOW && to == MB))
        return gs.leftGateClosed;

    if ((from == MB && to == KAD) || (from == KAD && to == MB))
        return gs.rightGateClosed;

    if ((from == MB && to == LIB) || (from == LIB && to == MB))
        return gs.centerGateClosed;

    return false;
}

// Applies the mode-specific energy costs, signal decay, audio reliability, and
// weighted enemy movement profile.
void setDifficultyParams(GameState &gs, Difficulty diff) {
    gs.difficulty = diff;
    gs.maxPower = 100;

    if (diff == EASY) {
        gs.totalNights = 2;
        gs.maxTurns = 20;
        gs.cameraPowerCost = 1;
        gs.deepScanPowerCost = 2;
        gs.lurePowerCost = 2;
        gs.gateClosePowerCost = 2;
        gs.gateUpkeepPowerCost = 1;
        gs.signalDecayTurns = 4;
        gs.audioProbDistance3 = 25;
        gs.audioProbDistance2 = 40;
        gs.audioProbDistance1 = 65;
        gs.moveCloserProb = 50;
        gs.moveSidewaysProb = 30;
        gs.moveRandomProb = 20;
        gs.lureCooldownMax = 3;
        gs.lureDurationTurns = 3;
        gs.lureWeightMultiplier = 2.5;
    } else if (diff == NORMAL) {
        gs.totalNights = 2;
        gs.maxTurns = 25;
        gs.cameraPowerCost = 2;
        gs.deepScanPowerCost = 4;
        gs.lurePowerCost = 4;
        gs.gateClosePowerCost = 2;
        gs.gateUpkeepPowerCost = 1;
        gs.signalDecayTurns = 3;
        gs.audioProbDistance3 = 20;
        gs.audioProbDistance2 = 35;
        gs.audioProbDistance1 = 60;
        gs.moveCloserProb = 65;
        gs.moveSidewaysProb = 25;
        gs.moveRandomProb = 10;
        gs.lureCooldownMax = 4;
        gs.lureDurationTurns = 3;
        gs.lureWeightMultiplier = 2.75;
    } else {
        gs.totalNights = 2;
        gs.maxTurns = 20;
        gs.cameraPowerCost = 1;
        gs.deepScanPowerCost = 5;
        gs.lurePowerCost = 3;
        gs.gateClosePowerCost = 2;
        gs.gateUpkeepPowerCost = 1;
        gs.signalDecayTurns = 3;
        gs.audioProbDistance3 = 10;
        gs.audioProbDistance2 = 25;
        gs.audioProbDistance1 = 45;
        gs.moveCloserProb = 70;
        gs.moveSidewaysProb = 20;
        gs.moveRandomProb = 10;
        gs.lureCooldownMax = 4;
        gs.lureDurationTurns = 2;
        gs.lureWeightMultiplier = 3.0;
    }
}

SignalStrength getSignalStrength(const GameState &gs) {
    if (gs.lastKnownEnemyRoom < 0 || gs.lastKnownEnemyRoom >= gs.gameMap.totalRooms)
        return SIGNAL_NONE;

    int age = gs.turn - gs.lastKnownEnemyTurn;
    if (age < 0) return SIGNAL_NONE;
    if (age == 0) return SIGNAL_STRONG;
    if (age < gs.signalDecayTurns) return SIGNAL_WEAK;
    return SIGNAL_NONE;
}

std::string getSignalDisplay(const GameState &gs) {
    SignalStrength strength = getSignalStrength(gs);
    if (strength == SIGNAL_NONE)
        return "Unknown";

    std::stringstream ss;
    ss << gs.gameMap.rooms[gs.lastKnownEnemyRoom].abbrev << " "
       << (strength == SIGNAL_STRONG ? "[!!]" : "[?]");
    return ss.str();
}

GameState::GameState() : difficulty(EASY), currentNight(1), totalNights(3),
    turn(0), maxTurns(30), power(100), maxPower(100),
    cameraPowerCost(1), deepScanPowerCost(2), lurePowerCost(2),
    gateClosePowerCost(1), gateUpkeepPowerCost(1),
    signalDecayTurns(4), audioProbDistance3(25), audioProbDistance2(40), audioProbDistance1(65),
    moveCloserProb(50), moveSidewaysProb(30), moveRandomProb(20),
    lureCooldownMax(3), lureDurationTurns(3), lureWeightMultiplier(2.5), currentLureCooldown(0),
    leftGateClosed(false), centerGateClosed(false), rightGateClosed(false),
    lastKnownEnemyRoom(-1), lastKnownEnemyTurn(-9999),
    status(STATUS_PLAYING) {}

// Builds a fresh game state for the selected difficulty and starts Night 1.
void GameState::init(Difficulty diff) {
    setDifficultyParams(*this, diff);
    gameMap = buildMap(diff);
    power = maxPower;
    currentNight = 1;
    status = STATUS_PLAYING;
    statusMessage = "";
    eventLog.clear();
    newNight();
}

// Resets transient night state while optionally preserving earlier log entries
// such as the "6 AM reached" transition message.
void GameState::newNight(bool preserveEventLog) {
    if (!preserveEventLog)
        eventLog.clear();

    turn = 0;
    int spawn = findSpawnRoom(gameMap);
    enemy.init(spawn, difficulty);
    lastKnownEnemyRoom = spawn;
    lastKnownEnemyTurn = 0;
    lastScanOutput.clear();
    currentLureCooldown = 0;
    leftGateClosed = false;
    centerGateClosed = false;
    rightGateClosed = false;
    gameMap.numCameraGroups = effectiveCameraGroupCount(gameMap);

    probMap.clear();
    for (size_t i = 0; i < gameMap.rooms.size(); i++)
        probMap[(int)i] = 0.0;
    probMap[spawn] = 1.0;

    std::stringstream ss;
    ss << "--- Night " << currentNight << " begins ---";
    eventLog.push_back(ss.str());
    eventLog.push_back("SECURITY LOG -- 11:55 PM");
    eventLog.push_back(initialEnemyLog(*this, spawn));
    eventLog.push_back("The intruder appears to have entered from the outer campus.");
    status = STATUS_PLAYING;
    statusMessage = "";
}

// Processes one player action, then resolves enemy movement, audio hints,
// closed-gate upkeep, win/loss checks, and probability updates.
void GameState::doTurn(int action, int param) {
    if (status != STATUS_PLAYING) return;

    turn++;
    int attemptedTurn = turn;
    eventLog.clear();
    lastScanOutput.clear();

    switch (action) {
        case 1: quickSweep(param); break;
        case 2: deepScan(param); break;
        case 3: toggleGate(param); break;
        case 4: playLure(param); break;
        case 5:
            eventLog.push_back("You wait and listen...");
            break;
        default:
            eventLog.push_back("Invalid action.");
            turn--;
            return;
    }

    if (turn < attemptedTurn)
        return;

    if (status != STATUS_PLAYING) return;

    finishSuccessfulTurn(action);
}

// Resolves the common enemy/audio/upkeep/check pipeline after one successful player action.
void GameState::finishSuccessfulTurn(int action) {
    enemyTurn();
    maybeGenerateAudioHint(*this);

    if (currentLureCooldown > 0 && action != 4)
        currentLureCooldown--;

    int activeGateCount = 0;
    if (leftGateClosed) activeGateCount++;
    if (centerGateClosed) activeGateCount++;
    if (rightGateClosed) activeGateCount++;
    power -= activeGateCount * gateUpkeepPowerCost;
    if (power < 0) power = 0;

    checkConditions();
    updateProbMap();
}

// Advances the enemy exactly once for the turn and records a compact hidden-state log entry.
void GameState::enemyTurn() {
    bool lureFavoredMove = enemy.move(*this);
    int lureTargetBeforeTick = enemy.lureTarget;

    std::stringstream ss;
    ss << "Turn " << turn << ": Enemy ";
    if (enemy.currentRoom == enemy.lastRoom)
        ss << "waited.";
    else
        ss << "moved.";
    eventLog.push_back(ss.str());

    if (lureFavoredMove)
        eventLog.push_back("Something seems distracted by the lure...");

    bool lureExpired = enemy.tickLure();
    if (lureExpired && lureTargetBeforeTick >= 0 && lureTargetBeforeTick < gameMap.totalRooms)
        eventLog.push_back("The lure at " + gameMap.rooms[lureTargetBeforeTick].abbrev + " has gone silent.");
}

// Updates win/loss state after the player action, enemy move, and resource systems resolve.
void GameState::checkConditions() {
    if (enemy.currentRoom == gameMap.officeId) {
        bool validEntry = (enemy.lastRoom == 4 && !leftGateClosed) ||
                          (enemy.lastRoom == 3 && !rightGateClosed) ||
                          (enemy.lastRoom == 2 &&
                           hasGraphEdge(gameMap, enemy.lastRoom, gameMap.officeId) &&
                           !centerGateClosed);
        if (!validEntry) {
            if (enemy.lastRoom >= 0 && enemy.lastRoom < gameMap.totalRooms)
                enemy.currentRoom = enemy.lastRoom;
            enemy.state = ROAMING;
            eventLog.push_back("The intruder was stopped at the office gate.");
            return;
        }
        status = STATUS_LOSE_ENEMY;
        statusMessage = officeEntryGateMessage(*this) + " GAME OVER.";
        return;
    }
    if (power <= 0) {
        status = STATUS_LOSE_POWER;
        statusMessage = "Energy depleted. The campus defense systems shut down.";
        return;
    }
    if (turn >= maxTurns) {
        if (currentNight < totalNights) {
            currentNight++;
            eventLog.push_back("*** 6 AM reached! You survived the night! ***");
            newNight(true);
        } else {
            status = STATUS_WIN;
            statusMessage = "You survived all nights! YOU WIN!";
        }
    }
}

// Performs a cheap cluster-level scan that only reports whether any movement is present.
void GameState::quickSweep(int group) {
    if (group < 0 || group >= effectiveCameraGroupCount(gameMap)) {
        eventLog.push_back("Invalid camera cluster.");
        turn--;
        return;
    }
    if (power < cameraPowerCost) {
        eventLog.push_back("Not enough energy for Quick Sweep!");
        turn--;
        return;
    }

    power -= cameraPowerCost;
    std::string label = cameraGroupLabel(gameMap, group);
    auto roomIds = roomsInGroup(gameMap, group);
    bool movementDetected = std::find(roomIds.begin(), roomIds.end(), enemy.currentRoom) != roomIds.end();

    lastScanOutput.push_back("QUICK SWEEP - " + label);
    if (movementDetected)
        lastScanOutput.push_back(clusterContainsOfficeGate(gameMap, group)
            ? "Movement detected near the office gates."
            : "Movement detected in " + label + ".");
    else
        lastScanOutput.push_back("No movement detected in " + label + ".");

    std::stringstream ss;
    ss << "Quick Sweep on " << label << " (-" << cameraPowerCost << "% energy)";
    eventLog.push_back(ss.str());
}

// Performs a precise room scan that can reveal the enemy's exact location for signal memory.
void GameState::deepScan(int roomId) {
    if (roomId < 0 || roomId >= gameMap.totalRooms || roomId == gameMap.officeId) {
        eventLog.push_back("Deep Scan requires a non-office building target.");
        turn--;
        return;
    }
    if (power < deepScanPowerCost) {
        eventLog.push_back("Not enough energy for Deep Scan!");
        turn--;
        return;
    }

    power -= deepScanPowerCost;
    std::string label = gameMap.rooms[roomId].abbrev;
    lastScanOutput.push_back("DEEP SCAN - " + label);

    if (enemy.currentRoom == roomId) {
        lastKnownEnemyRoom = roomId;
        lastKnownEnemyTurn = turn;
        if (isOfficeGateRoom(*this, roomId)) {
            lastScanOutput.push_back("Enemy detected at " + label + " gate!");
            eventLog.push_back("Deep Scan confirmed the intruder at " + gameMap.rooms[roomId].name + ".");
            bool gateOpen = false;
            bool *gate = nullptr;
            if (roomId == 3) {
                gateOpen = !rightGateClosed;
                gate = &rightGateClosed;
            } else if (roomId == 4) {
                gateOpen = !leftGateClosed;
                gate = &leftGateClosed;
            } else {
                gateOpen = !centerGateClosed;
                gate = &centerGateClosed;
            }
            if (gateOpen) {
                if (power < gateClosePowerCost) {
                    lastScanOutput.push_back("Not enough energy to close the " + label + " gate!");
                    eventLog.push_back("Not enough energy to close the " + label + " gate!");
                } else {
                    power -= gateClosePowerCost;
                    *gate = true;
                    lastScanOutput.push_back(label + " gate closed automatically.");
                    eventLog.push_back(label + " gate closed automatically.");
                }
            }
        } else {
            lastScanOutput.push_back("Enemy detected at " + label + ".");
            eventLog.push_back("Deep Scan confirmed the intruder at " + gameMap.rooms[roomId].name + ".");
        }
    } else {
        lastScanOutput.push_back("No movement detected at " + label + ".");
        eventLog.push_back("Deep Scan found no movement at " + gameMap.rooms[roomId].name + ".");
    }
}

// Places a sound lure at one exact building chosen by the cursor.
void GameState::playLure(int roomId) {
    if (roomId < 0 || roomId >= gameMap.totalRooms || roomId == gameMap.officeId) {
        eventLog.push_back("You can only place a lure at a non-office building.");
        turn--;
        return;
    }
    if (currentLureCooldown > 0) {
        std::stringstream ss;
        ss << "Lure on cooldown! " << currentLureCooldown << " turns remaining.";
        eventLog.push_back(ss.str());
        turn--;
        return;
    }
    if (power < lurePowerCost) {
        eventLog.push_back("Not enough energy for lure!");
        turn--;
        return;
    }

    power -= lurePowerCost;
    currentLureCooldown = lureCooldownMax;

    enemy.applyLure(roomId, lureDurationTurns);

    std::stringstream ss;
    ss << "Lure placed at " << gameMap.rooms[roomId].name
       << " (-" << lurePowerCost << "% energy)";
    eventLog.push_back(ss.str());
}

// Toggles an office gate when the cursor is on KAD, LIB, or KNOW.
void GameState::toggleGate(int roomId) {
    if (roomId < 0 || roomId >= gameMap.totalRooms) {
        eventLog.push_back("Invalid room.");
        turn--;
        return;
    }

    bool *gate = nullptr;
    std::string gateLabel;
    if (roomId == 3) {
        gate = &rightGateClosed;
        gateLabel = "KAD office gate";
    } else if (roomId == 4) {
        gate = &leftGateClosed;
        gateLabel = "KNOW office gate";
    } else if (roomId == 2 && hasGraphEdge(gameMap, roomId, gameMap.officeId)) {
        gate = &centerGateClosed;
        gateLabel = "LIB center gate";
    } else {
        eventLog.push_back("No gate installed at this location.");
        turn--;
        return;
    }

    if (!*gate) {
        if (power < gateClosePowerCost) {
            eventLog.push_back("Not enough energy to close this gate.");
            turn--;
            return;
        }
        power -= gateClosePowerCost;
        *gate = true;
        std::stringstream ss;
        ss << gateLabel << " closed (-" << gateClosePowerCost << "% energy).";
        eventLog.push_back(ss.str());
    } else {
        *gate = false;
        eventLog.push_back(gateLabel + " opened.");
    }
}

// Rebuilds the coarse enemy probability map from the last reliable signal and
// one diffusion step through the current graph.
void GameState::updateProbMap() {
    if (getSignalStrength(*this) != SIGNAL_NONE && lastKnownEnemyRoom >= 0) {
        for (auto &p : probMap)
            p.second = 0.0;

        const std::vector<int> &neighbors = gameMap.rooms[lastKnownEnemyRoom].neighbors;
        std::vector<int> openNeighbors;
        for (int nb : neighbors)
            if (!isBlockedEdge(*this, lastKnownEnemyRoom, nb))
                openNeighbors.push_back(nb);

        if (openNeighbors.empty()) {
            probMap[lastKnownEnemyRoom] = 1.0;
        } else {
            probMap[lastKnownEnemyRoom] = 0.5;
            double spread = 0.5 / openNeighbors.size();
            for (int nb : openNeighbors)
                probMap[nb] += spread;
        }
    } else if (probMap.empty()) {
        double share = 1.0 / gameMap.totalRooms;
        for (int roomId = 0; roomId < gameMap.totalRooms; roomId++)
            probMap[roomId] = share;
    }

    diffuseProbability(gameMap.rooms, probMap, gameMap.officeId,
                       leftGateClosed, rightGateClosed, centerGateClosed);
    normalizeProbabilityMap(gameMap, probMap);
}