#include "game.h"
#include "graph_algos.h"
#include <cstdlib>
#include <sstream>

static bool isInnerRingApproachRoom(int roomId) {
    return roomId == 3 || roomId == 4;
}

static bool isActiveCameraBroken(const GameState &gs) {
    return gs.activeCameraGroup >= 0 &&
           gs.brokenCameraGroup == gs.activeCameraGroup &&
           gs.brokenCameraTurns > 0;
}

static bool canSeeEnemyLive(const GameState &gs) {
    if (gs.activeCameraGroup < 0) return false;
    if (isActiveCameraBroken(gs)) return false;
    if (gs.enemy.currentRoom < 0 || gs.enemy.currentRoom >= gs.gameMap.totalRooms) return false;
    return gs.gameMap.rooms[gs.enemy.currentRoom].cameraGroup == gs.activeCameraGroup;
}

GameState::GameState() : difficulty(EASY), currentNight(1), totalNights(5),
    turn(0), maxTurns(30), power(100), maxPower(100),
    cameraPowerCost(3), lurePowerCost(8), doorPowerCost(5), scanPowerCost(10),
    lureCooldown(0), lureCooldownMax(2), currentLureCooldown(0),
    leftGateClosed(false), rightGateClosed(false),
    activeCameraGroup(2), brokenCameraGroup(-1), brokenCameraTurns(0), lastBrokenCameraGroup(-1),
    lastKnownEnemyRoom(-1), lastCameraGroupChecked(-1), lastCameraFeedUnavailable(false),
    status(STATUS_PLAYING) {}

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

void setDifficultyParams(GameState &gs, Difficulty diff) {
    gs.difficulty = diff;
    if (diff == EASY) {
        gs.totalNights = 3;
        gs.maxPower = 150;
        gs.maxTurns = 30;
        gs.cameraPowerCost = 3;
        gs.lurePowerCost = 8;
        gs.doorPowerCost = 5;
        gs.scanPowerCost = 10;
        gs.lureCooldownMax = 2;
    } else if (diff == NORMAL) {
        gs.totalNights = 4;
        gs.maxPower = 120;
        gs.maxTurns = 35;
        gs.cameraPowerCost = 5;
        gs.lurePowerCost = 12;
        gs.doorPowerCost = 8;
        gs.scanPowerCost = 15;
        gs.lureCooldownMax = 3;
    } else {
        gs.totalNights = 5;
        gs.maxPower = 100;
        gs.maxTurns = 40;
        gs.cameraPowerCost = 7;
        gs.lurePowerCost = 15;
        gs.doorPowerCost = 10;
        gs.scanPowerCost = 18;
        gs.lureCooldownMax = 4;
    }
}

void GameState::init(Difficulty diff) {
    setDifficultyParams(*this, diff);
    gameMap = buildMap(diff);
    power = maxPower;
    currentNight = 1;
    status = STATUS_PLAYING;
    statusMessage = "";
    eventLog.clear();

    // Initialize probability map (uniform)
    probMap.clear();
    for (size_t i = 0; i < gameMap.rooms.size(); i++)
        probMap[i] = 1.0 / gameMap.totalRooms;

    newNight();
}

void GameState::newNight() {
    turn = 0;
    int spawn = findSpawnRoom(gameMap);
    enemy.init(spawn, difficulty);
    activeCameraGroup = (gameMap.numCameraGroups > 2) ? 2 : gameMap.numCameraGroups - 1;
    brokenCameraGroup = -1;
    brokenCameraTurns = 0;
    lastBrokenCameraGroup = -1;
    lastKnownEnemyRoom = -1;
    lastCameraCheck.clear();
    lastCameraGroupChecked = -1;
    lastCameraFeedUnavailable = false;
    currentLureCooldown = 0;
    leftGateClosed = false;
    rightGateClosed = false;

    // Reset probability
    probMap.clear();
    for (size_t i = 0; i < gameMap.rooms.size(); i++)
        probMap[i] = 1.0 / gameMap.totalRooms;

    std::stringstream ss;
    ss << "--- Night " << currentNight << " begins ---";
    eventLog.push_back(ss.str());
    status = STATUS_PLAYING;
    statusMessage = "";
}

void GameState::doTurn(int action, int param) {
    if (status != STATUS_PLAYING) return;

    turn++;
    eventLog.clear();
    lastCameraCheck.clear();
    lastCameraFeedUnavailable = false;
    bool drainPowerThisTurn = true;

    switch (action) {
        case 1: checkCamera(param); break;
        case 2: playLure(param); break;
        case 3: toggleGate(param); break;
        case 4: openGate(param); break;
        case 5: riskScan(); break;
        case 6: // end turn, no action
            eventLog.push_back("You wait...");
            drainPowerThisTurn = false;
            break;
        default:
            eventLog.push_back("Invalid action.");
            turn--;
            return;
    }

    if (status != STATUS_PLAYING) return;

    // Cooldowns
    if (currentLureCooldown > 0) currentLureCooldown--;

    // Enemy moves
    enemyTurn();

    if (canSeeEnemyLive(*this))
        lastKnownEnemyRoom = enemy.currentRoom;

    bool outageRestoredThisTurn = false;
    if (brokenCameraTurns > 0) {
        brokenCameraTurns--;
        if (brokenCameraTurns == 0) {
            eventLog.push_back(cameraGroupLabel(brokenCameraGroup) + " cameras restored.");
            brokenCameraGroup = -1;
            outageRestoredThisTurn = true;
        }
    }

    if (drainPowerThisTurn) {
        // Passive power drain
        int drain = 1 + currentNight / 2;
        power -= drain;
        if (power < 0) power = 0;

        if (leftGateClosed) power -= 1;
        if (rightGateClosed) power -= 1;
        if (power < 0) power = 0;
    }

    if (difficulty == EASY && activeCameraGroup >= 0 && brokenCameraTurns == 0 &&
        !outageRestoredThisTurn && turn > 2) {
        bool canBreakActiveRing = true;
        if (activeCameraGroup == 0 && isInnerRingApproachRoom(enemy.currentRoom))
            canBreakActiveRing = false;

        if (canBreakActiveRing && (std::rand() % 100) < 10) {
            brokenCameraGroup = activeCameraGroup;
            brokenCameraTurns = 1;
            lastBrokenCameraGroup = activeCameraGroup;
            eventLog.push_back("Warning: " + cameraGroupLabel(activeCameraGroup) + " cameras offline!");
        }
    }

    checkConditions();
    updateProbMap();
}

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

void GameState::checkConditions() {
    if (enemy.currentRoom == gameMap.officeId) {
        status = STATUS_LOSE_ENEMY;
        statusMessage = "The enemy has reached the office! GAME OVER.";
        return;
    }
    if (power <= 0) {
        status = STATUS_LOSE_POWER;
        statusMessage = "Power depleted! You cannot monitor the building. GAME OVER.";
        return;
    }
    if (turn >= maxTurns) {
        if (currentNight < totalNights) {
            currentNight++;
            eventLog.push_back("*** 6 AM reached! You survived the night! ***");
            newNight();
        } else {
            status = STATUS_WIN;
            statusMessage = "You survived all nights! YOU WIN!";
        }
    }
}

void GameState::checkCamera(int group) {
    if (power < cameraPowerCost) {
        eventLog.push_back("Not enough power for camera check!");
        turn--;
        return;
    }
    power -= cameraPowerCost;
    activeCameraGroup = group;
    lastCameraGroupChecked = group;
    eventLog.push_back("Now watching: " + cameraGroupLabel(group));

    if (isActiveCameraBroken(*this)) {
        lastCameraFeedUnavailable = true;
        eventLog.push_back(cameraGroupLabel(group) + " camera feed unavailable.");

        std::stringstream ss;
        ss << "Checked Camera " << cameraGroupLabel(group) << " (-" << cameraPowerCost << " power)";
        eventLog.push_back(ss.str());
        return;
    }

    auto roomIds = roomsInGroup(gameMap, group);
    for (int rid : roomIds) {
        CameraSighting cs;
        cs.roomId = rid;

        bool detected = (enemy.currentRoom == rid);
        // On harder difficulties, chance to miss detection
        if (detected && difficulty == NORMAL && (std::rand() % 100) < 10)
            detected = false;
        if (detected && difficulty == HARD && (std::rand() % 100) < 20)
            detected = false;

        cs.enemyPresent = detected;
        if (detected) {
            cs.status = "ENEMY DETECTED!";
            lastKnownEnemyRoom = rid;
            eventLog.push_back("Camera " + cameraGroupLabel(group) +
                ": Enemy spotted in " + gameMap.rooms[rid].name + "!");
        } else {
            cs.status = "Clear";
        }
        lastCameraCheck.push_back(cs);
    }

    std::stringstream ss;
    ss << "Checked Camera " << cameraGroupLabel(group) << " (-" << cameraPowerCost << " power)";
    eventLog.push_back(ss.str());
}

void GameState::playLure(int group) {
    if (currentLureCooldown > 0) {
        std::stringstream ss;
        ss << "Lure on cooldown! " << currentLureCooldown << " turns remaining.";
        eventLog.push_back(ss.str());
        turn--;
        return;
    }
    if (power < lurePowerCost) {
        eventLog.push_back("Not enough power for lure!");
        turn--;
        return;
    }
    power -= lurePowerCost;
    currentLureCooldown = lureCooldownMax;

    // Pick a random camera room in the group as the lure target
    auto roomIds = roomsInGroup(gameMap, group);
    int target = roomIds[std::rand() % roomIds.size()];

    int duration = (difficulty == EASY) ? 4 : (difficulty == NORMAL) ? 3 : 2;
    enemy.applyLure(target, duration);

    std::stringstream ss;
    ss << "Played sound at Camera " << cameraGroupLabel(group)
       << " (" << gameMap.rooms[target].name << ") (-" << lurePowerCost << " power)";
    eventLog.push_back(ss.str());
    eventLog.push_back("Enemy is now investigating the sound!");
}

void GameState::toggleGate(int roomId) {
    if (roomId < 0 || roomId >= gameMap.totalRooms) {
        eventLog.push_back("Invalid room!");
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
        eventLog.push_back("Only office entrance gates can be controlled.");
        turn--;
        return;
    }

    if (!*gate && power < doorPowerCost) {
        eventLog.push_back("Not enough power!");
        turn--;
        return;
    }

    if (!*gate) power -= doorPowerCost;
    *gate = !*gate;

    std::stringstream ss;
    ss << gateLabel << ' ' << (*gate ? "closed" : "opened");
    if (*gate) ss << " (-" << doorPowerCost << " power)";
    eventLog.push_back(ss.str());
}

void GameState::openGate(int roomId) {
    if (roomId < 0 || roomId >= gameMap.totalRooms) {
        eventLog.push_back("Invalid room!");
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
        eventLog.push_back("Only office entrance gates can be controlled.");
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

void GameState::riskScan() {
    if (power < scanPowerCost) {
        eventLog.push_back("Not enough power for risk scan!");
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

    auto path = bfsShortestPath(gameMap.rooms, enemy.currentRoom, gameMap.officeId,
                                gameMap.officeId, leftGateClosed, rightGateClosed);
    if (!path.empty()) {
        std::stringstream ss;
        ss << "Shortest path from last known to office: " << path.size() - 1 << " steps";
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
    ss << "(-" << scanPowerCost << " power)";
    eventLog.push_back(ss.str());
}

void GameState::updateProbMap() {
    // If we know enemy location, spike that room
    if (lastKnownEnemyRoom >= 0) {
        for (auto &p : probMap)
            p.second = 0.01;
        probMap[lastKnownEnemyRoom] = 0.5;
        // Diffuse remaining to neighbors
        auto &neighbors = gameMap.rooms[lastKnownEnemyRoom].neighbors;
        double spread = 0.5 / (neighbors.size() + 1);
        for (int nb : neighbors)
            if (!isBlockedEdge(*this, lastKnownEnemyRoom, nb))
                probMap[nb] += spread;
        probMap[lastKnownEnemyRoom] += spread;
    }
    diffuseProbability(gameMap.rooms, probMap, gameMap.officeId,
                       leftGateClosed, rightGateClosed);
}
