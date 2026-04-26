#include "game.h"
#include "graph_algos.h"
#include <cstdlib>
#include <sstream>

CameraSystem::CameraSystem() {
    reset();
}

void CameraSystem::reset() {
    lastKnownRoom = -1;
    lastKnownCluster = -1;
    lastDetectedTurn = -999;
    lastSignalLabel = "";
    lastSignalExact = false;
}

void CameraSystem::updateLastKnownSignal(int roomId, int clusterId, const std::string &label,
                                         bool exact, int currentTurn) {
    lastKnownRoom = roomId;
    lastKnownCluster = clusterId;
    lastSignalLabel = label;
    lastSignalExact = exact;
    lastDetectedTurn = currentTurn;
}

std::string CameraSystem::getSignalDisplay(int currentTurn, int decayTurns) const {
    if (lastSignalLabel.empty()) return "No reliable signal.";
    int age = currentTurn - lastDetectedTurn;
    if (age < 0 || age >= decayTurns) return "No reliable signal.";
    if (age == 0) return lastSignalLabel + " [!!]";
    return lastSignalLabel + " [?]";
}

std::string AudioHintSystem::maybeGenerateHint(const GameMap &map, int enemyRoom,
                                               int distanceToOffice, Difficulty diff,
                                               const DifficultySettings &settings) {
    int chance = 0;
    if (distanceToOffice == 1) chance = settings.audioProbDistance1;
    else if (distanceToOffice == 2) chance = settings.audioProbDistance2;
    else if (distanceToOffice == 3) chance = settings.audioProbDistance3;
    else return "";

    if ((std::rand() % 100) >= chance) return "";

    if (distanceToOffice == 1)
        return "You hear urgent movement just outside the office approach.";

    int cluster = (enemyRoom >= 0 && enemyRoom < map.totalRooms)
        ? map.rooms[enemyRoom].cameraGroup : -1;
    std::string area = cameraGroupLabel(cluster);

    if (diff == EASY)
        return "You hear footsteps somewhere near the " + area + " area...";
    if (diff == NORMAL)
        return "You hear a faint sound from " + area + " campus...";
    return "Something echoes in the distance...";
}

GameState::GameState() : difficulty(EASY), currentNight(1), totalNights(5),
    turn(0), maxTurns(30), power(100), maxPower(100),
    cameraPowerCost(1), quickSweepPowerCost(1), deepScanPowerCost(3),
    lurePowerCost(3), doorPowerCost(1), scanPowerCost(3),
    lureCooldown(0), lureCooldownMax(2), currentLureCooldown(0),
    lastKnownEnemyRoom(-1), lastKnownTurn(-999), lastCameraGroupChecked(-1),
    lastCameraWasDeepScan(false), status(STATUS_PLAYING) {}

void setDifficultyParams(GameState &gs, Difficulty diff) {
    gs.difficulty = diff;
    if (diff == EASY) {
        gs.totalNights = 3;
        gs.maxPower = 150;
        gs.maxTurns = 30;
        gs.settings = {1, 3, 1, 3, 25, 40, 65, 3, 55, 30, 15};
        gs.lureCooldownMax = 2;
    } else if (diff == NORMAL) {
        gs.totalNights = 4;
        gs.maxPower = 120;
        gs.maxTurns = 35;
        gs.settings = {2, 5, 1, 4, 20, 35, 60, 2, 65, 25, 10};
        gs.lureCooldownMax = 3;
    } else {
        gs.totalNights = 5;
        gs.maxPower = 100;
        gs.maxTurns = 40;
        gs.settings = {3, 6, 2, 5, 10, 25, 45, 1, 85, 15, 0};
        gs.lureCooldownMax = 4;
    }
    gs.quickSweepPowerCost = gs.settings.quickSweepCost;
    gs.deepScanPowerCost = gs.settings.deepScanCost;
    gs.cameraPowerCost = gs.settings.quickSweepCost;
    gs.lurePowerCost = gs.settings.lureCost;
    gs.doorPowerCost = gs.settings.doorCost;
    gs.scanPowerCost = gs.settings.deepScanCost;
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
    lastKnownEnemyRoom = -1;
    lastKnownTurn = -999;
    lastCameraCheck.clear();
    lastCameraGroupChecked = -1;
    lastCameraWasDeepScan = false;
    cameraSystem.reset();
    currentLureCooldown = 0;

    // Reset doors
    for (auto &r : gameMap.rooms)
        r.doorClosed = false;

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

    switch (action) {
        case ACTION_QUICK_SWEEP: quickSweep(param); break;
        case ACTION_LURE: playLure(param); break;
        case ACTION_CLOSE_DOOR: closeDoor(param); break;
        case ACTION_RESTORE_DOOR: restoreDoor(param); break;
        case ACTION_DEEP_SCAN: deepScan(param); break;
        case ACTION_WAIT:
            eventLog.push_back("You wait and listen...");
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

    // Passive power drain
    int drain = 1 + currentNight / 2;
    power -= drain;
    if (power < 0) power = 0;

    // Door upkeep
    for (auto &r : gameMap.rooms) {
        if (r.doorClosed) {
            power -= settings.doorCost;
            if (power < 0) power = 0;
        }
    }

    checkConditions();
    updateProbMap();
}

void GameState::enemyTurn() {
    enemy.tickLure();
    enemy.moveWeighted(gameMap, settings);

    auto dist = bfsDistances(gameMap.rooms, gameMap.officeId);
    if (dist.count(enemy.currentRoom)) {
        std::string hint = AudioHintSystem::maybeGenerateHint(
            gameMap, enemy.currentRoom, dist[enemy.currentRoom], difficulty, settings);
        if (!hint.empty())
            eventLog.push_back("Audio: " + hint);
    }
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

void GameState::quickSweep(int group) {
    if (power < quickSweepPowerCost) {
        eventLog.push_back("Not enough power for quick sweep!");
        turn--;
        return;
    }
    power -= quickSweepPowerCost;
    lastCameraGroupChecked = group;
    lastCameraWasDeepScan = false;

    auto roomIds = roomsInGroup(gameMap, group);
    bool detected = false;
    for (int rid : roomIds) {
        if (enemy.currentRoom == rid) detected = true;
    }

    CameraSighting cs;
    cs.roomId = detected ? enemy.currentRoom : -1;
    cs.clusterId = group;
    cs.enemyPresent = detected;
    cs.deepScan = false;
    cs.status = detected ? "Movement detected." : "No movement detected.";
    lastCameraCheck.push_back(cs);

    std::string label = cameraGroupLabel(group);
    eventLog.push_back("SCAN " + label);
    if (detected) {
        lastKnownEnemyRoom = enemy.currentRoom;
        lastKnownTurn = turn;
        cameraSystem.updateLastKnownSignal(enemy.currentRoom, group, label, false, turn);
        eventLog.push_back("Movement detected in " + label + ".");
    } else {
        eventLog.push_back("No movement detected.");
    }

    std::stringstream ss;
    ss << "Quick Sweep cost: -" << quickSweepPowerCost << "% energy.";
    eventLog.push_back(ss.str());
}

void GameState::deepScan(int roomId) {
    if (roomId < 0 || roomId >= gameMap.totalRooms) {
        eventLog.push_back("Invalid building!");
        turn--;
        return;
    }
    if (power < deepScanPowerCost) {
        eventLog.push_back("Not enough power for deep scan!");
        turn--;
        return;
    }
    power -= deepScanPowerCost;
    lastCameraGroupChecked = gameMap.rooms[roomId].cameraGroup;
    lastCameraWasDeepScan = true;

    bool detected = (enemy.currentRoom == roomId);
    CameraSighting cs;
    cs.roomId = roomId;
    cs.clusterId = gameMap.rooms[roomId].cameraGroup;
    cs.enemyPresent = detected;
    cs.deepScan = true;
    cs.status = detected ? "Enemy detected." : "No movement detected.";
    lastCameraCheck.push_back(cs);

    eventLog.push_back("SCAN " + gameMap.rooms[roomId].abbrev);
    if (detected) {
        lastKnownEnemyRoom = roomId;
        lastKnownTurn = turn;
        cameraSystem.updateLastKnownSignal(
            roomId, cs.clusterId, gameMap.rooms[roomId].abbrev, true, turn);
        eventLog.push_back("Enemy detected at " + gameMap.rooms[roomId].abbrev + ".");
    } else {
        eventLog.push_back("No movement detected.");
    }

    std::stringstream ss;
    ss << "Deep Scan cost: -" << deepScanPowerCost << "% energy.";
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
    ss << "Played sound lure at " << cameraGroupLabel(group)
       << " (" << gameMap.rooms[target].name << ") (-" << lurePowerCost << "% energy)";
    eventLog.push_back(ss.str());
    eventLog.push_back("Enemy is now investigating the sound!");
}

void GameState::closeDoor(int roomId) {
    if (roomId < 0 || roomId >= gameMap.totalRooms) {
        eventLog.push_back("Invalid room!");
        turn--;
        return;
    }
    if (gameMap.rooms[roomId].isOffice) {
        eventLog.push_back("Cannot close the office door!");
        turn--;
        return;
    }
    if (gameMap.rooms[roomId].doorClosed) {
        eventLog.push_back("Door already closed!");
        turn--;
        return;
    }
    if (power < doorPowerCost) {
        eventLog.push_back("Not enough power!");
        turn--;
        return;
    }
    power -= doorPowerCost;
    gameMap.rooms[roomId].doorClosed = true;
    std::stringstream ss;
    ss << "Closed door at " << gameMap.rooms[roomId].name
       << ". Door drains -" << doorPowerCost << "% each turn while closed.";
    eventLog.push_back(ss.str());
}

void GameState::restoreDoor(int roomId) {
    if (roomId < 0 || roomId >= gameMap.totalRooms) {
        eventLog.push_back("Invalid room!");
        turn--;
        return;
    }
    if (!gameMap.rooms[roomId].doorClosed) {
        eventLog.push_back("Door is already open!");
        turn--;
        return;
    }
    gameMap.rooms[roomId].doorClosed = false;
    eventLog.push_back("Restored door at " + gameMap.rooms[roomId].name);
}

void GameState::riskScan() {
    if (power < scanPowerCost) {
        eventLog.push_back("Not enough power for risk scan!");
        turn--;
        return;
    }
    power -= scanPowerCost;
    eventLog.push_back("=== RISK ANALYSIS ===");

    auto aps = findArticulationPoints(gameMap.rooms);
    if (!aps.empty()) {
        eventLog.push_back("Critical rooms (articulation points):");
        for (int ap : aps)
            eventLog.push_back("  - " + gameMap.rooms[ap].name);
    } else {
        eventLog.push_back("No critical chokepoints detected.");
    }

    auto brs = findBridges(gameMap.rooms);
    if (!brs.empty()) {
        eventLog.push_back("Critical corridors (bridges):");
        for (auto &b : brs)
            eventLog.push_back("  - " + gameMap.rooms[b.first].name +
                " <-> " + gameMap.rooms[b.second].name);
    }

    auto path = bfsShortestPath(gameMap.rooms, enemy.currentRoom, gameMap.officeId);
    if (!path.empty()) {
        std::stringstream ss;
        ss << "Shortest path from last known to office: " << path.size() - 1 << " steps";
        eventLog.push_back(ss.str());
    }

    auto danger = computeDangerLevels(gameMap.rooms, gameMap.officeId,
                                       lastKnownEnemyRoom, probMap);
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
            if (!gameMap.rooms[lastKnownEnemyRoom].doorClosed)
                probMap[nb] += spread;
        probMap[lastKnownEnemyRoom] += spread;
    }
    diffuseProbability(gameMap.rooms, probMap);
}
