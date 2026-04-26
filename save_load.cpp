#include "save_load.h"
#include <fstream>
#include <limits>

template <typename T>
static bool readValue(std::istream &in, T &value) {
    return static_cast<bool>(in >> value);
}

// Saves the current game state to a plain-text file that can be reloaded later.
bool saveGame(const GameState &gs, const std::string &filename) {
    std::ofstream out(filename.c_str());
    if (!out.is_open()) return false;

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
    out << gs.doorPowerCost << "\n";
    out << gs.scanPowerCost << "\n";
    out << gs.signalDecayTurns << "\n";
    out << gs.audioProbDistance3 << "\n";
    out << gs.audioProbDistance2 << "\n";
    out << gs.audioProbDistance1 << "\n";
    out << gs.moveCloserProb << "\n";
    out << gs.moveSidewaysProb << "\n";
    out << gs.moveRandomProb << "\n";
    out << gs.lureCooldownMax << "\n";
    out << gs.currentLureCooldown << "\n";
    out << gs.lastKnownEnemyRoom << "\n";
    out << gs.lastKnownEnemyTurn << "\n";

    out << gs.enemy.currentRoom << "\n";
    out << gs.enemy.lastRoom << "\n";
    out << gs.enemy.state << "\n";
    out << gs.enemy.lureTarget << "\n";
    out << gs.enemy.lureTimer << "\n";
    out << gs.enemy.moveChance << "\n";

    out << gs.gameMap.totalRooms << "\n";
    out << gs.gameMap.officeId << "\n";
    out << gs.gameMap.numCameraGroups << "\n";
    out << gs.leftGateClosed << " " << gs.rightGateClosed << "\n";

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

    int diffInt;
    if (!readValue(in, diffInt) || diffInt < EASY || diffInt > HARD) return false;
    gs.difficulty = static_cast<Difficulty>(diffInt);

    if (!readValue(in, gs.currentNight) ||
        !readValue(in, gs.totalNights) ||
        !readValue(in, gs.turn) ||
        !readValue(in, gs.maxTurns) ||
        !readValue(in, gs.power) ||
        !readValue(in, gs.maxPower) ||
        !readValue(in, gs.cameraPowerCost) ||
        !readValue(in, gs.deepScanPowerCost) ||
        !readValue(in, gs.lurePowerCost) ||
        !readValue(in, gs.doorPowerCost) ||
        !readValue(in, gs.scanPowerCost) ||
        !readValue(in, gs.signalDecayTurns) ||
        !readValue(in, gs.audioProbDistance3) ||
        !readValue(in, gs.audioProbDistance2) ||
        !readValue(in, gs.audioProbDistance1) ||
        !readValue(in, gs.moveCloserProb) ||
        !readValue(in, gs.moveSidewaysProb) ||
        !readValue(in, gs.moveRandomProb) ||
        !readValue(in, gs.lureCooldownMax) ||
        !readValue(in, gs.currentLureCooldown) ||
        !readValue(in, gs.lastKnownEnemyRoom) ||
        !readValue(in, gs.lastKnownEnemyTurn)) {
        return false;
    }

    int enemyState;
    if (!readValue(in, gs.enemy.currentRoom) ||
        !readValue(in, gs.enemy.lastRoom) ||
        !readValue(in, enemyState) ||
        !readValue(in, gs.enemy.lureTarget) ||
        !readValue(in, gs.enemy.lureTimer) ||
        !readValue(in, gs.enemy.moveChance)) {
        return false;
    }
    if (enemyState < ROAMING || enemyState > AT_OFFICE) return false;
    gs.enemy.state = static_cast<EnemyState>(enemyState);

    if (!readValue(in, gs.gameMap.totalRooms) ||
        !readValue(in, gs.gameMap.officeId) ||
        !readValue(in, gs.gameMap.numCameraGroups)) {
        return false;
    }
    if (gs.gameMap.totalRooms <= 0 || gs.gameMap.totalRooms > 13 ||
        gs.gameMap.officeId < 0 || gs.gameMap.officeId >= gs.gameMap.totalRooms ||
        gs.gameMap.numCameraGroups <= 0 || gs.gameMap.numCameraGroups > 4) {
        return false;
    }

    gs.gameMap.rooms.clear();
    gs.gameMap.rooms.resize(gs.gameMap.totalRooms);
    if (!readValue(in, gs.leftGateClosed) || !readValue(in, gs.rightGateClosed))
        return false;

    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    for (int i = 0; i < gs.gameMap.totalRooms; i++) {
        Room &r = gs.gameMap.rooms[i];
        int nbCount;
        if (!readValue(in, r.id) ||
            !readValue(in, r.isCamera) ||
            !readValue(in, r.isOffice) ||
            !readValue(in, r.cameraGroup)) {
            return false;
        }
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        if (!std::getline(in, r.abbrev) || !std::getline(in, r.name))
            return false;
        if (!readValue(in, nbCount) || nbCount < 0 || nbCount > gs.gameMap.totalRooms)
            return false;
        r.neighbors.resize(nbCount);
        for (int j = 0; j < nbCount; j++) {
            if (!readValue(in, r.neighbors[j]) ||
                r.neighbors[j] < 0 || r.neighbors[j] >= gs.gameMap.totalRooms) {
                return false;
            }
        }
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    int probSize;
    if (!readValue(in, probSize) || probSize < 0 || probSize > gs.gameMap.totalRooms)
        return false;

    gs.probMap.clear();
    for (int i = 0; i < probSize; i++) {
        int roomId;
        double prob;
        if (!readValue(in, roomId) || !readValue(in, prob) ||
            roomId < 0 || roomId >= gs.gameMap.totalRooms || prob < 0.0) {
            return false;
        }
        gs.probMap[roomId] = prob;
    }

    gs.status = STATUS_PLAYING;
    gs.statusMessage = "";
    gs.lastScanOutput.clear();
    gs.eventLog.clear();
    gs.eventLog.push_back("Game loaded successfully.");
    return true;
}
