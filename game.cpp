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

static int primaryClusterForRoom(const GameMap &map, int roomId) {
    for (int group = 0; group < map.numCameraGroups; group++) {
        if (roomInCameraGroup(map, roomId, group))
            return group;
    }
    return -1;
}

static int enemyDistanceToOffice(const GameState &gs) {
    auto dist = bfsDistances(gs.gameMap.rooms, gs.gameMap.officeId,
                             gs.gameMap.officeId,
                             gs.leftGateClosed, gs.rightGateClosed);
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

// Returns whether a movement edge is blocked by one of the two office gates.
bool isBlockedEdge(const GameState& gs, int from, int to) {
    const int MB = gs.gameMap.officeId;
    const int KAD = 3;
    const int KNOW = 4;

    if ((from == MB && to == KNOW) || (from == KNOW && to == MB))
        return gs.leftGateClosed;

    if ((from == MB && to == KAD) || (from == KAD && to == MB))
        return gs.rightGateClosed;

    return false;
}

// Applies the mode-specific energy costs, signal decay, audio reliability, and
// weighted enemy movement profile.
void setDifficultyParams(GameState &gs, Difficulty diff) {
    gs.difficulty = diff;
    gs.maxPower = 100;

    if (diff == EASY) {
        gs.totalNights = 3;
        gs.maxTurns = 30;
        gs.cameraPowerCost = 1;
        gs.deepScanPowerCost = 3;
        gs.lurePowerCost = 3;
        gs.doorPowerCost = 1;
        gs.scanPowerCost = 5;
        gs.signalDecayTurns = 3;
        gs.audioProbDistance3 = 25;
        gs.audioProbDistance2 = 40;
        gs.audioProbDistance1 = 65;
        gs.moveCloserProb = 55;
        gs.moveSidewaysProb = 30;
        gs.moveRandomProb = 15;
        gs.lureCooldownMax = 2;
    } else if (diff == NORMAL) {
        gs.totalNights = 4;
        gs.maxTurns = 35;
        gs.cameraPowerCost = 2;
        gs.deepScanPowerCost = 5;
        gs.lurePowerCost = 4;
        gs.doorPowerCost = 1;
        gs.scanPowerCost = 7;
        gs.signalDecayTurns = 2;
        gs.audioProbDistance3 = 20;
        gs.audioProbDistance2 = 35;
        gs.audioProbDistance1 = 60;
        gs.moveCloserProb = 65;
        gs.moveSidewaysProb = 25;
        gs.moveRandomProb = 10;
        gs.lureCooldownMax = 3;
    } else {
        gs.totalNights = 5;
        gs.maxTurns = 40;
        gs.cameraPowerCost = 3;
        gs.deepScanPowerCost = 6;
        gs.lurePowerCost = 5;
        gs.doorPowerCost = 2;
        gs.scanPowerCost = 9;
        gs.signalDecayTurns = 1;
        gs.audioProbDistance3 = 10;
        gs.audioProbDistance2 = 25;
        gs.audioProbDistance1 = 45;
        gs.moveCloserProb = 85;
        gs.moveSidewaysProb = 15;
        gs.moveRandomProb = 0;
        gs.lureCooldownMax = 4;
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
        return "No reliable signal.";

    std::stringstream ss;
    ss << gs.gameMap.rooms[gs.lastKnownEnemyRoom].abbrev << " "
       << (strength == SIGNAL_STRONG ? "[!!]" : "[?]");
    return ss.str();
}

GameState::GameState() : difficulty(EASY), currentNight(1), totalNights(3),
    turn(0), maxTurns(30), power(100), maxPower(100),
    cameraPowerCost(1), deepScanPowerCost(3), lurePowerCost(3), doorPowerCost(1), scanPowerCost(5),
    signalDecayTurns(3), audioProbDistance3(25), audioProbDistance2(40), audioProbDistance1(65),
    moveCloserProb(55), moveSidewaysProb(30), moveRandomProb(15),
    lureCooldownMax(2), currentLureCooldown(0),
    leftGateClosed(false), rightGateClosed(false),
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
    lastKnownEnemyRoom = -1;
    lastKnownEnemyTurn = -9999;
    lastScanOutput.clear();
    currentLureCooldown = 0;
    leftGateClosed = false;
    rightGateClosed = false;

    probMap.clear();
    for (size_t i = 0; i < gameMap.rooms.size(); i++)
        probMap[(int)i] = 1.0 / gameMap.totalRooms;

    std::stringstream ss;
    ss << "--- Night " << currentNight << " begins ---";
    eventLog.push_back(ss.str());
    status = STATUS_PLAYING;
    statusMessage = "";
}

// Processes one player action, then resolves enemy movement, audio hints,
// resource upkeep, win/loss checks, and probability updates for that turn.
void GameState::doTurn(int action, int param) {
    if (status != STATUS_PLAYING) return;

    turn++;
    eventLog.clear();
    lastScanOutput.clear();
    bool drainPowerThisTurn = true;

    switch (action) {
        case 1: quickSweep(param); break;
        case 2: deepScan(param); break;
        case 3: toggleGate(param); break;
        case 4: playLure(param); break;
        case 5:
            eventLog.push_back("You wait and listen...");
            drainPowerThisTurn = false;
            break;
        case 6: openGate(param); break;
        case 7: riskScan(); break;
        default:
            eventLog.push_back("Invalid action.");
            turn--;
            return;
    }

    if (status != STATUS_PLAYING) return;

    if (currentLureCooldown > 0) currentLureCooldown--;

    enemyTurn();
    maybeGenerateAudioHint(*this);

    if (drainPowerThisTurn) {
        if (leftGateClosed) power -= doorPowerCost;
        if (rightGateClosed) power -= doorPowerCost;
        if (power < 0) power = 0;
    }

    checkConditions();
    updateProbMap();
}

// Advances the enemy exactly once for the turn and records a compact hidden-state log entry.
void GameState::enemyTurn() {
    enemy.tickLure();
    enemy.move(*this);

    std::stringstream ss;
    ss << "Turn " << turn << ": Enemy ";
    if (enemy.currentRoom == enemy.lastRoom)
        ss << "waited.";
    else
        ss << "moved.";
    eventLog.push_back(ss.str());
}

// Updates win/loss state after the player action, enemy move, and resource systems resolve.
void GameState::checkConditions() {
    if (enemy.currentRoom == gameMap.officeId) {
        status = STATUS_LOSE_ENEMY;
        statusMessage = "The intruder reached Main Building. GAME OVER.";
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
    if (group < 0 || group >= gameMap.numCameraGroups) {
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
        lastScanOutput.push_back("Movement detected in " + label + ".");
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
        lastScanOutput.push_back("Enemy detected at " + label + ".");
        eventLog.push_back("Deep Scan confirmed the intruder at " + gameMap.rooms[roomId].name + ".");
    } else {
        lastScanOutput.push_back("No movement detected at " + label + ".");
        eventLog.push_back("Deep Scan found no movement at " + gameMap.rooms[roomId].name + ".");
    }
}

// Plays a sound lure in one camera cluster to bias the enemy toward a random room there.
void GameState::playLure(int group) {
    if (group < 0 || group >= gameMap.numCameraGroups) {
        eventLog.push_back("Invalid lure target cluster.");
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

    auto roomIds = roomsInGroup(gameMap, group);
    if (roomIds.empty()) {
        eventLog.push_back("That cluster has no valid lure target.");
        turn--;
        return;
    }

    power -= lurePowerCost;
    currentLureCooldown = lureCooldownMax;

    int target = roomIds[std::rand() % roomIds.size()];
    int duration = (difficulty == EASY) ? 4 : (difficulty == NORMAL) ? 3 : 2;
    enemy.applyLure(target, duration);

    std::stringstream ss;
    ss << "Lure used in " << cameraGroupLabel(gameMap, group)
       << " (-" << lurePowerCost << "% energy)";
    eventLog.push_back(ss.str());
}

// Toggles one of the two office gates when the cursor is on KAD or KNOW.
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
    } else {
        eventLog.push_back("Only KAD and KNOW can be defended directly.");
        turn--;
        return;
    }

    *gate = !*gate;
    eventLog.push_back(gateLabel + (*gate ? " closed." : " opened."));
}

// Opens one office gate explicitly without spending energy.
void GameState::openGate(int roomId) {
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
    } else {
        eventLog.push_back("Only KAD and KNOW can be defended directly.");
        turn--;
        return;
    }

    if (!*gate) {
        eventLog.push_back(gateLabel + " is already open.");
        turn--;
        return;
    }

    *gate = false;
    eventLog.push_back(gateLabel + " opened.");
}

// Runs the optional graph-analysis report used by the strategic risk scan action.
void GameState::riskScan() {
    if (power < scanPowerCost) {
        eventLog.push_back("Not enough energy for risk scan!");
        turn--;
        return;
    }
    power -= scanPowerCost;
    eventLog.push_back("=== RISK ANALYSIS ===");

    auto aps = findArticulationPoints(gameMap.rooms, gameMap.officeId,
                                      leftGateClosed, rightGateClosed);
    if (!aps.empty()) {
        eventLog.push_back("Critical rooms (articulation points):");
        for (int ap : aps)
            eventLog.push_back("  - " + gameMap.rooms[ap].name);
    } else {
        eventLog.push_back("No critical chokepoints detected.");
    }

    auto brs = findBridges(gameMap.rooms, gameMap.officeId,
                           leftGateClosed, rightGateClosed);
    if (!brs.empty()) {
        eventLog.push_back("Critical corridors (bridges):");
        for (auto &b : brs)
            eventLog.push_back("  - " + gameMap.rooms[b.first].name +
                " <-> " + gameMap.rooms[b.second].name);
    }

    int scanSource = (lastKnownEnemyRoom >= 0) ? lastKnownEnemyRoom : enemy.currentRoom;
    auto path = bfsShortestPath(gameMap.rooms, scanSource, gameMap.officeId,
                                gameMap.officeId, leftGateClosed, rightGateClosed);
    if (!path.empty()) {
        std::stringstream ss;
        ss << "Shortest known path to office: " << path.size() - 1 << " steps";
        eventLog.push_back(ss.str());
    }

    auto danger = computeDangerLevels(gameMap.rooms, gameMap.officeId,
                                      lastKnownEnemyRoom, probMap,
                                      leftGateClosed, rightGateClosed);
    eventLog.push_back("Danger levels:");
    for (auto &p : danger) {
        std::string level;
        if (p.second < 0.25) level = "LOW";
        else if (p.second < 0.55) level = "MEDIUM";
        else level = "HIGH";
        std::stringstream ss;
        ss << "  " << gameMap.rooms[p.first].name << ": " << level
           << " (" << (int)(p.second * 100) << "%)";
        eventLog.push_back(ss.str());
    }

    std::stringstream ss;
    ss << "Risk scan complete (-" << scanPowerCost << "% energy)";
    eventLog.push_back(ss.str());
}

// Rebuilds the coarse enemy probability map from the last reliable signal and
// one diffusion step through the current graph.
void GameState::updateProbMap() {
    if (getSignalStrength(*this) != SIGNAL_NONE && lastKnownEnemyRoom >= 0) {
        for (auto &p : probMap)
            p.second = 0.01;
        probMap[lastKnownEnemyRoom] = 0.5;

        const std::vector<int> &neighbors = gameMap.rooms[lastKnownEnemyRoom].neighbors;
        double spread = 0.5 / (neighbors.size() + 1);
        for (int nb : neighbors)
            if (!isBlockedEdge(*this, lastKnownEnemyRoom, nb))
                probMap[nb] += spread;
        probMap[lastKnownEnemyRoom] += spread;
    }

    diffuseProbability(gameMap.rooms, probMap, gameMap.officeId,
                       leftGateClosed, rightGateClosed);
}
