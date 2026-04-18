#include "save_load.h"
#include <fstream>
#include <sstream>

bool saveGame(const GameState &gs, const std::string &filename) {
    std::ofstream out(filename);
    if (!out.is_open()) return false;

    out << gs.difficulty << "\n";
    out << gs.currentNight << "\n";
    out << gs.totalNights << "\n";
    out << gs.turn << "\n";
    out << gs.maxTurns << "\n";
    out << gs.power << "\n";
    out << gs.maxPower << "\n";
    out << gs.cameraPowerCost << "\n";
    out << gs.lurePowerCost << "\n";
    out << gs.doorPowerCost << "\n";
    out << gs.scanPowerCost << "\n";
    out << gs.lureCooldownMax << "\n";
    out << gs.currentLureCooldown << "\n";
    out << gs.lastKnownEnemyRoom << "\n";
    out << gs.lastCameraGroupChecked << "\n";

    // Enemy
    out << gs.enemy.currentRoom << "\n";
    out << gs.enemy.lastRoom << "\n";
    out << gs.enemy.state << "\n";
    out << gs.enemy.alertLevel << "\n";
    out << gs.enemy.lureTarget << "\n";
    out << gs.enemy.lureTimer << "\n";
    out << gs.enemy.officeBias << "\n";
    out << gs.enemy.moveChance << "\n";

    // Map
    out << gs.gameMap.totalRooms << "\n";
    out << gs.gameMap.officeId << "\n";
    out << gs.gameMap.numCameraGroups << "\n";

    for (auto &r : gs.gameMap.rooms) {
        out << r.id << " " << r.isCamera << " " << r.isOffice
            << " " << r.doorClosed << " " << r.cameraGroup << "\n";
        out << r.abbrev << "\n";
        out << r.name << "\n";
        out << r.neighbors.size() << "\n";
        for (int nb : r.neighbors)
            out << nb << " ";
        out << "\n";
    }

    // Probability map
    out << gs.probMap.size() << "\n";
    for (auto &p : gs.probMap)
        out << p.first << " " << p.second << "\n";

    out.close();
    return true;
}

bool loadGame(GameState &gs, const std::string &filename) {
    std::ifstream in(filename);
    if (!in.is_open()) return false;

    int diffInt;
    in >> diffInt;
    gs.difficulty = (Difficulty)diffInt;
    in >> gs.currentNight >> gs.totalNights >> gs.turn >> gs.maxTurns;
    in >> gs.power >> gs.maxPower >> gs.cameraPowerCost >> gs.lurePowerCost;
    in >> gs.doorPowerCost >> gs.scanPowerCost >> gs.lureCooldownMax >> gs.currentLureCooldown;
    in >> gs.lastKnownEnemyRoom >> gs.lastCameraGroupChecked;

    int enemyState;
    in >> gs.enemy.currentRoom >> gs.enemy.lastRoom >> enemyState;
    gs.enemy.state = (EnemyState)enemyState;
    in >> gs.enemy.alertLevel >> gs.enemy.lureTarget >> gs.enemy.lureTimer;
    in >> gs.enemy.officeBias >> gs.enemy.moveChance;

    int totalRooms, officeId, numGroups;
    in >> totalRooms >> officeId >> numGroups;
    gs.gameMap.totalRooms = totalRooms;
    gs.gameMap.officeId = officeId;
    gs.gameMap.numCameraGroups = numGroups;
    gs.gameMap.rooms.resize(totalRooms);

    in.ignore(); // consume newline before room names

    for (int i = 0; i < totalRooms; i++) {
        auto &r = gs.gameMap.rooms[i];
        in >> r.id >> r.isCamera >> r.isOffice >> r.doorClosed >> r.cameraGroup;
        in.ignore();
        std::getline(in, r.abbrev);
        std::getline(in, r.name);

        int nbCount;
        in >> nbCount;
        r.neighbors.resize(nbCount);
        for (int j = 0; j < nbCount; j++)
            in >> r.neighbors[j];
        in.ignore();
    }

    int probSize;
    in >> probSize;
    gs.probMap.clear();
    for (int i = 0; i < probSize; i++) {
        int rid;
        double prob;
        in >> rid >> prob;
        gs.probMap[rid] = prob;
    }

    gs.status = STATUS_PLAYING;
    gs.statusMessage = "";
    gs.lastCameraCheck.clear();
    gs.eventLog.clear();
    gs.eventLog.push_back("Game loaded successfully.");

    in.close();
    return true;
}
