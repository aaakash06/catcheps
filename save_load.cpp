#include "save_load.h"
#include <fstream>
#include <limits>
#include <sstream>

static const char *SAVE_MAGIC = "CW_SAVE_V5";
static const char *SAVE_MAGIC_V4 = "CW_SAVE_V4";
static const char *SAVE_MAGIC_V3 = "CW_SAVE_V3";
static const char *SAVE_MAGIC_V2 = "CW_SAVE_V2";

template <typename T>
static bool readValue(std::istream &in, T &value) {
    return static_cast<bool>(in >> value);
}

static bool failLoad(GameState &gs, const std::string &message) {
    gs.statusMessage = message;
    gs.eventLog.clear();
    gs.eventLog.push_back(message);
    return false;
}

static void normalizeLoadedProbabilityMap(GameState &gs) {
    double total = 0.0;
    for (int roomId = 0; roomId < gs.gameMap.totalRooms; roomId++) {
        double value = gs.probMap.count(roomId) ? gs.probMap[roomId] : 0.0;
        if (value < 0.0 || value != value)
            value = 0.0;
        gs.probMap[roomId] = value;
        total += value;
    }

    if (total <= 0.0) {
        for (int roomId = 0; roomId < gs.gameMap.totalRooms; roomId++)
            gs.probMap[roomId] = 0.0;

        if (gs.lastKnownEnemyRoom >= 0 && gs.lastKnownEnemyRoom < gs.gameMap.totalRooms) {
            gs.probMap[gs.lastKnownEnemyRoom] = 1.0;
            return;
        }

        if (gs.gameMap.totalRooms <= 0)
            return;

        double share = 1.0 / gs.gameMap.totalRooms;
        for (int roomId = 0; roomId < gs.gameMap.totalRooms; roomId++)
            gs.probMap[roomId] = share;
        return;
    }

    for (int roomId = 0; roomId < gs.gameMap.totalRooms; roomId++)
        gs.probMap[roomId] /= total;
}

// Saves the current game state to a plain-text file that can be reloaded later.
bool saveGame(const GameState &gs, const std::string &filename) {
    std::ofstream out(filename.c_str());
    if (!out.is_open()) return false;

    out << SAVE_MAGIC << "\n";
    out << gs.difficulty << "\n";
    out << gs.currentNight << "\n";
    out << gs.totalNights << "\n";
    out << gs.turn << "\n";
    out << gs.maxTurns << "\n";
    out << gs.power << "\n";
    out << gs.maxPower << "\n";
    out << gs.cameraPowerCost << "\n";
    out << gs.deepScanPowerCost << "\n";
    out << gs.lurePowerCost << "\n";
    out << gs.gateClosePowerCost << "\n";
    out << gs.gateUpkeepPowerCost << "\n";
    out << gs.signalDecayTurns << "\n";
    out << gs.audioProbDistance3 << "\n";
    out << gs.audioProbDistance2 << "\n";
    out << gs.audioProbDistance1 << "\n";
    out << gs.moveCloserProb << "\n";
    out << gs.moveSidewaysProb << "\n";
    out << gs.moveRandomProb << "\n";
    out << gs.lureCooldownMax << "\n";
    out << gs.lureDurationTurns << "\n";
    out << gs.lureWeightMultiplier << "\n";
    out << gs.currentLureCooldown << "\n";
    out << gs.lastKnownEnemyRoom << "\n";
    out << gs.lastKnownEnemyTurn << "\n";
    out << gs.scansUsed << "\n";

    out << gs.enemy.currentRoom << "\n";
    out << gs.enemy.lastRoom << "\n";
    out << gs.enemy.state << "\n";
    out << gs.enemy.lureTarget << "\n";
    out << gs.enemy.lureTimer << "\n";

    out << gs.gameMap.totalRooms << "\n";
    out << gs.gameMap.officeId << "\n";
    out << effectiveCameraGroupCount(gs.gameMap) << "\n";
    out << gs.leftGateClosed << " " << gs.centerGateClosed << " "
        << gs.rightGateClosed << "\n";

    for (size_t i = 0; i < gs.gameMap.rooms.size(); i++) {
        const Room &r = gs.gameMap.rooms[i];
        out << r.id << " " << r.isCamera << " " << r.isOffice
            << " " << r.cameraGroup << "\n";
        out << r.abbrev << "\n";
        out << r.name << "\n";
        out << r.neighbors.size() << "\n";
        for (size_t j = 0; j < r.neighbors.size(); j++)
            out << r.neighbors[j] << " ";
        out << "\n";
    }

    out << gs.probMap.size() << "\n";
    for (std::map<int, double>::const_iterator it = gs.probMap.begin(); it != gs.probMap.end(); ++it)
        out << it->first << " " << it->second << "\n";

    return static_cast<bool>(out);
}

// Loads a saved game from disk and rejects obviously corrupted or out-of-range data.
bool loadGame(GameState &gs, const std::string &filename) {
    std::ifstream in(filename.c_str());
    if (!in.is_open()) return false;

    std::string firstToken;
    if (!(in >> firstToken)) return false;

    bool legacyFormat = false;
    bool version2Format = false;
    bool version3Format = false;
    bool version4Format = false;
    int diffInt = -1;
    if (firstToken == SAVE_MAGIC) {
        if (!readValue(in, diffInt)) return false;
    } else if (firstToken == SAVE_MAGIC_V4) {
        if (!readValue(in, diffInt)) return false;
        version4Format = true;
    } else if (firstToken == SAVE_MAGIC_V3) {
        if (!readValue(in, diffInt)) return false;
        version3Format = true;
    } else if (firstToken == SAVE_MAGIC_V2) {
        if (!readValue(in, diffInt)) return false;
        version2Format = true;
    } else {
        std::istringstream ss(firstToken);
        if (!(ss >> diffInt) || !ss.eof()) return false;
        legacyFormat = true;
    }

    if (diffInt < EASY || diffInt > HARD) return failLoad(gs, "Save rejected: invalid difficulty.");
    gs.difficulty = static_cast<Difficulty>(diffInt);
    setDifficultyParams(gs, gs.difficulty);

    int savedCurrentNight;
    int savedTotalNights;
    int savedTurn;
    int savedMaxTurns;
    int savedPower;
    int savedMaxPower;

    int savedCameraCost;
    int savedDeepScanCost;
    int savedLureCost;
    int savedGateCloseCost;
    int savedGateUpkeepCost = gs.gateUpkeepPowerCost;
    int savedSignalDecayTurns;
    int savedAudioProbDistance3;
    int savedAudioProbDistance2;
    int savedAudioProbDistance1;
    int savedMoveCloserProb;
    int savedMoveSidewaysProb;
    int savedMoveRandomProb;
    int savedLureCooldownMax;
    int savedLureDurationTurns = gs.lureDurationTurns;
    double savedLureWeightMultiplier = gs.lureWeightMultiplier;

    if (!readValue(in, gs.currentNight) ||
        !readValue(in, gs.totalNights) ||
        !readValue(in, gs.turn) ||
        !readValue(in, gs.maxTurns) ||
        !readValue(in, gs.power) ||
        !readValue(in, gs.maxPower)) {
        return false;
    }

    savedCurrentNight = gs.currentNight;
    savedTotalNights = gs.totalNights;
    savedTurn = gs.turn;
    savedMaxTurns = gs.maxTurns;
    savedPower = gs.power;
    savedMaxPower = gs.maxPower;

    if (!readValue(in, savedCameraCost) ||
        !readValue(in, savedDeepScanCost) ||
        !readValue(in, savedLureCost) ||
        !readValue(in, savedGateCloseCost)) {
        return false;
    }

    if (firstToken == SAVE_MAGIC || version4Format || version3Format) {
        if (!readValue(in, savedGateUpkeepCost))
            return failLoad(gs, "Save rejected: invalid gate upkeep state.");
    } else if (legacyFormat) {
        int ignoredScanPowerCost;
        if (!readValue(in, ignoredScanPowerCost))
            return failLoad(gs, "Save rejected: incomplete legacy save header.");
    }

    if (!readValue(in, savedSignalDecayTurns) ||
        !readValue(in, savedAudioProbDistance3) ||
        !readValue(in, savedAudioProbDistance2) ||
        !readValue(in, savedAudioProbDistance1) ||
        !readValue(in, savedMoveCloserProb) ||
        !readValue(in, savedMoveSidewaysProb) ||
        !readValue(in, savedMoveRandomProb) ||
        !readValue(in, savedLureCooldownMax)) {
        return failLoad(gs, "Save rejected: invalid core tuning values.");
    }

    if (firstToken == SAVE_MAGIC || version4Format || version3Format) {
        if (!readValue(in, savedLureDurationTurns) ||
            !readValue(in, savedLureWeightMultiplier)) {
            return failLoad(gs, "Save rejected: invalid lure tuning values.");
        }
    }

    if (!readValue(in, gs.currentLureCooldown) ||
        !readValue(in, gs.lastKnownEnemyRoom) ||
        !readValue(in, gs.lastKnownEnemyTurn)) {
        return failLoad(gs, "Save rejected: invalid signal memory state.");
    }
    if (firstToken == SAVE_MAGIC) {
        if (!readValue(in, gs.scansUsed))
            return failLoad(gs, "Save rejected: invalid scan counter.");
    } else {
        gs.scansUsed = 0;
    }

    gs.currentNight = savedCurrentNight;
    gs.totalNights = savedTotalNights;
    gs.turn = savedTurn;
    gs.maxTurns = savedMaxTurns;
    gs.power = savedPower;
    gs.maxPower = savedMaxPower;

    if (!legacyFormat && !version2Format) {
        gs.cameraPowerCost = savedCameraCost;
        gs.deepScanPowerCost = savedDeepScanCost;
        gs.lurePowerCost = savedLureCost;
        gs.gateClosePowerCost = savedGateCloseCost;
        gs.gateUpkeepPowerCost = savedGateUpkeepCost;
        gs.signalDecayTurns = savedSignalDecayTurns;
        gs.audioProbDistance3 = savedAudioProbDistance3;
        gs.audioProbDistance2 = savedAudioProbDistance2;
        gs.audioProbDistance1 = savedAudioProbDistance1;
        gs.moveCloserProb = savedMoveCloserProb;
        gs.moveSidewaysProb = savedMoveSidewaysProb;
        gs.moveRandomProb = savedMoveRandomProb;
        gs.lureCooldownMax = savedLureCooldownMax;
        gs.lureDurationTurns = savedLureDurationTurns;
        gs.lureWeightMultiplier = savedLureWeightMultiplier;
    }

    if (gs.currentNight < 1 || gs.currentNight > gs.totalNights ||
        gs.turn < 0 || gs.turn > gs.maxTurns ||
        gs.maxTurns <= 0 || gs.totalNights <= 0 ||
        gs.power < 0 || gs.power > gs.maxPower || gs.maxPower <= 0 ||
        gs.cameraPowerCost < 0 || gs.deepScanPowerCost < 0 || gs.lurePowerCost < 0 ||
        gs.gateClosePowerCost < 0 || gs.gateUpkeepPowerCost < 0 ||
        gs.signalDecayTurns < 1 ||
        gs.audioProbDistance3 < 0 || gs.audioProbDistance3 > 100 ||
        gs.audioProbDistance2 < 0 || gs.audioProbDistance2 > 100 ||
        gs.audioProbDistance1 < 0 || gs.audioProbDistance1 > 100 ||
        gs.moveCloserProb < 0 || gs.moveSidewaysProb < 0 || gs.moveRandomProb < 0 ||
        gs.lureCooldownMax < 0 || gs.lureDurationTurns < 0 ||
        gs.lureWeightMultiplier <= 0.0 ||
        gs.currentLureCooldown < 0 || gs.currentLureCooldown > gs.lureCooldownMax ||
        gs.scansUsed < 0) {
        return failLoad(gs, "Save rejected: invalid difficulty settings.");
    }

    int enemyState;
    if (!readValue(in, gs.enemy.currentRoom) ||
        !readValue(in, gs.enemy.lastRoom) ||
        !readValue(in, enemyState) ||
        !readValue(in, gs.enemy.lureTarget) ||
        !readValue(in, gs.enemy.lureTimer)) {
        return failLoad(gs, "Save rejected: invalid enemy state.");
    }
    if (legacyFormat) {
        double legacyMoveChance;
        if (!readValue(in, legacyMoveChance))
            return failLoad(gs, "Save rejected: incomplete legacy enemy state.");
    }
    if (enemyState < ROAMING || enemyState > AT_OFFICE)
        return failLoad(gs, "Save rejected: invalid enemy state.");
    gs.enemy.state = static_cast<EnemyState>(enemyState);

    if (!readValue(in, gs.gameMap.totalRooms) ||
        !readValue(in, gs.gameMap.officeId) ||
        !readValue(in, gs.gameMap.numCameraGroups)) {
        return failLoad(gs, "Save rejected: invalid map header.");
    }
    if (gs.gameMap.totalRooms <= 0 || gs.gameMap.totalRooms > 13 ||
        gs.gameMap.officeId < 0 || gs.gameMap.officeId >= gs.gameMap.totalRooms) {
        return failLoad(gs, "Save rejected: invalid map dimensions.");
    }

    gs.gameMap.rooms.clear();
    gs.gameMap.rooms.resize(gs.gameMap.totalRooms);
    if (!readValue(in, gs.leftGateClosed))
        return failLoad(gs, "Save rejected: invalid gate state.");
    if (firstToken == SAVE_MAGIC || version4Format) {
        if (!readValue(in, gs.centerGateClosed))
            return failLoad(gs, "Save rejected: invalid gate state.");
    } else {
        gs.centerGateClosed = false;
    }
    if (!readValue(in, gs.rightGateClosed))
        return failLoad(gs, "Save rejected: invalid gate state.");

    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    for (int i = 0; i < gs.gameMap.totalRooms; i++) {
        Room &r = gs.gameMap.rooms[i];
        int nbCount;
        if (!readValue(in, r.id) ||
            !readValue(in, r.isCamera) ||
            !readValue(in, r.isOffice) ||
            !readValue(in, r.cameraGroup)) {
            return failLoad(gs, "Save rejected: invalid room metadata.");
        }
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        if (!std::getline(in, r.abbrev) || !std::getline(in, r.name))
            return failLoad(gs, "Save rejected: incomplete room data.");
        if (!readValue(in, nbCount) || nbCount < 0 || nbCount > gs.gameMap.totalRooms)
            return failLoad(gs, "Save rejected: invalid room adjacency data.");
        r.neighbors.resize(nbCount);
        for (int j = 0; j < nbCount; j++) {
            if (!readValue(in, r.neighbors[j]) ||
                r.neighbors[j] < 0 || r.neighbors[j] >= gs.gameMap.totalRooms) {
                return failLoad(gs, "Save rejected: invalid room adjacency data.");
            }
        }
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    int probSize;
    if (!readValue(in, probSize) || probSize < 0 || probSize > gs.gameMap.totalRooms)
        return failLoad(gs, "Save rejected: invalid probability map header.");

    gs.probMap.clear();
    for (int i = 0; i < probSize; i++) {
        int roomId;
        double prob;
        if (!readValue(in, roomId) || !readValue(in, prob) ||
            roomId < 0 || roomId >= gs.gameMap.totalRooms || prob < 0.0 ||
            prob != prob) {
            return failLoad(gs, "Save rejected: invalid probability map data.");
        }
        gs.probMap[roomId] = prob;
    }

    GameMap currentMap = buildMap(gs.difficulty);
    if (gs.gameMap.totalRooms != currentMap.totalRooms ||
        gs.gameMap.officeId != currentMap.officeId) {
        return failLoad(gs, "Save rejected: map does not match the selected difficulty.");
    }
    gs.gameMap = currentMap;
    gs.gameMap.numCameraGroups = effectiveCameraGroupCount(gs.gameMap);
    if (gs.gameMap.numCameraGroups <= 0 || gs.gameMap.numCameraGroups > 5)
        return failLoad(gs, "Save rejected: invalid camera cluster configuration.");

    if (gs.enemy.currentRoom < 0 || gs.enemy.currentRoom >= gs.gameMap.totalRooms)
        return failLoad(gs, "Save rejected: enemy location is out of range.");
    if (gs.enemy.currentRoom == gs.gameMap.officeId)
        return failLoad(gs, "Save rejected: enemy is already in the office.");

    if (gs.enemy.lastRoom < 0 || gs.enemy.lastRoom >= gs.gameMap.totalRooms)
        gs.enemy.lastRoom = gs.enemy.currentRoom;

    if (gs.lastKnownEnemyRoom < 0 || gs.lastKnownEnemyRoom >= gs.gameMap.totalRooms) {
        gs.lastKnownEnemyRoom = -1;
        gs.lastKnownEnemyTurn = -9999;
    }

    if (gs.enemy.lureTimer <= 0 ||
        gs.enemy.lureTarget < 0 ||
        gs.enemy.lureTarget >= gs.gameMap.totalRooms ||
        gs.enemy.lureTarget == gs.gameMap.officeId) {
        gs.enemy.lureTarget = -1;
        gs.enemy.lureTimer = 0;
    }

    if (gs.enemy.state == INVESTIGATING && gs.enemy.lureTarget < 0)
        gs.enemy.state = ROAMING;
    if (gs.enemy.state == LEGACY_ATTACKING || gs.enemy.state == AT_OFFICE)
        gs.enemy.state = ROAMING;

    normalizeLoadedProbabilityMap(gs);

    gs.status = STATUS_PLAYING;
    gs.statusMessage = "";
    gs.lastScanOutput.clear();
    gs.eventLog.clear();
    gs.eventLog.push_back("Game loaded successfully.");
    return true;
}
