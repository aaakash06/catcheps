#include "save_load.h"
#include <fstream>
#include <sstream>
#include <vector>

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
    out << gs.leftGateClosed << " " << gs.rightGateClosed << "\n";

    for (auto &r : gs.gameMap.rooms) {
        out << r.id << " " << r.isCamera << " " << r.isOffice
            << " " << r.cameraGroup << "\n";
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

    out << gs.activeCameraGroup << "\n";
    out << gs.brokenCameraGroup << "\n";
    out << gs.brokenCameraTurns << "\n";
    out << gs.lastBrokenCameraGroup << "\n";

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
    gs.leftGateClosed = false;
    gs.rightGateClosed = false;

    in.ignore(); // consume newline before room names
    std::string headerLine;
    std::getline(in, headerLine);
    std::istringstream headerStream(headerLine);
    std::vector<int> headerValues;
    int value;
    while (headerStream >> value)
        headerValues.push_back(value);

    auto readRoomBody = [&](Room &r) {
        std::getline(in, r.abbrev);
        std::getline(in, r.name);

        int nbCount;
        in >> nbCount;
        r.neighbors.resize(nbCount);
        for (int j = 0; j < nbCount; j++)
            in >> r.neighbors[j];
        in.ignore();
    };

    int startRoom = 0;
    if (headerValues.size() == 2) {
        gs.leftGateClosed = (headerValues[0] != 0);
        gs.rightGateClosed = (headerValues[1] != 0);
    } else if (headerValues.size() == 5 || headerValues.size() == 4) {
        auto &r = gs.gameMap.rooms[0];
        r.id = headerValues[0];
        r.isCamera = (headerValues[1] != 0);
        r.isOffice = (headerValues[2] != 0);
        r.cameraGroup = headerValues.back();
        readRoomBody(r);
        startRoom = 1;
    } else {
        return false;
    }

    for (int i = startRoom; i < totalRooms; i++) {
        auto &r = gs.gameMap.rooms[i];
        in >> r.id >> r.isCamera >> r.isOffice >> r.cameraGroup;
        in.ignore();
        readRoomBody(r);
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

    gs.activeCameraGroup = (gs.gameMap.numCameraGroups > 2) ? 2 : gs.gameMap.numCameraGroups - 1;
    gs.brokenCameraGroup = -1;
    gs.brokenCameraTurns = 0;
    gs.lastBrokenCameraGroup = -1;
    std::vector<int> cameraState;
    int cameraValue;
    while (in >> cameraValue)
        cameraState.push_back(cameraValue);
    if (!cameraState.empty())
        gs.activeCameraGroup = cameraState[0];
    if (cameraState.size() >= 4) {
        gs.brokenCameraGroup = cameraState[1];
        gs.brokenCameraTurns = cameraState[2];
        gs.lastBrokenCameraGroup = cameraState[3];
    }

    gs.status = STATUS_PLAYING;
    gs.statusMessage = "";
    gs.lastCameraCheck.clear();
    gs.lastCameraFeedUnavailable = false;
    gs.eventLog.clear();
    gs.eventLog.push_back("Game loaded successfully.");

    in.close();
    return true;
}
