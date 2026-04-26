#include "enemy.h"
#include "game.h"
#include "graph_algos.h"
#include <cstdlib>
#include <map>
#include <vector>

// Returns all legal next rooms after applying the office-gate edge blocks.
static std::vector<int> getOpenNeighbors(const GameState &gs, int roomId) {
    std::vector<int> result;
    if (roomId < 0 || roomId >= gs.gameMap.totalRooms) return result;

    const std::vector<int> &neighbors = gs.gameMap.rooms[roomId].neighbors;
    for (int nb : neighbors) {
        if (!isBlockedEdge(gs, roomId, nb))
            result.push_back(nb);
    }
    return result;
}

// Picks one room according to the provided non-negative weights.
static int weightedChoice(const std::vector<int> &rooms, const std::vector<double> &weights) {
    if (rooms.empty()) return -1;

    double totalWeight = 0.0;
    for (size_t i = 0; i < weights.size(); i++)
        totalWeight += weights[i];

    if (totalWeight <= 0.0)
        return rooms[std::rand() % rooms.size()];

    double roll = (double)(std::rand() % 10000) / 10000.0 * totalWeight;
    double cumulative = 0.0;
    for (size_t i = 0; i < rooms.size(); i++) {
        cumulative += weights[i];
        if (roll <= cumulative)
            return rooms[i];
    }
    return rooms.back();
}

// Chooses the enemy's next room using distance-to-office categories plus lure bias.
static int chooseNextRoom(const GameState &gs, const Enemy &enemy) {
    std::vector<int> neighbors = getOpenNeighbors(gs, enemy.currentRoom);
    if (neighbors.empty()) return enemy.currentRoom;

    std::map<int, int> distToOffice = bfsDistances(gs.gameMap.rooms, gs.gameMap.officeId,
                                                   gs.gameMap.officeId,
                                                   gs.leftGateClosed, gs.rightGateClosed);
    std::map<int, int> distToLure = (enemy.lureTarget >= 0)
        ? bfsDistances(gs.gameMap.rooms, enemy.lureTarget, gs.gameMap.officeId,
                       gs.leftGateClosed, gs.rightGateClosed)
        : std::map<int, int>();

    int currentDist = distToOffice.count(enemy.currentRoom) ? distToOffice[enemy.currentRoom] : 999;
    std::vector<int> closer;
    std::vector<int> sideways;

    for (size_t i = 0; i < neighbors.size(); i++) {
        int nb = neighbors[i];
        int nbDist = distToOffice.count(nb) ? distToOffice[nb] : 999;
        if (nbDist < currentDist)
            closer.push_back(nb);
        else if (nbDist == currentDist)
            sideways.push_back(nb);
    }

    std::vector<double> weights(neighbors.size(), 0.0);
    for (size_t i = 0; i < neighbors.size(); i++)
        weights[i] += (double)gs.moveRandomProb / neighbors.size();

    if (!closer.empty()) {
        double share = (double)gs.moveCloserProb / closer.size();
        for (size_t i = 0; i < neighbors.size(); i++) {
            for (size_t j = 0; j < closer.size(); j++) {
                if (neighbors[i] == closer[j])
                    weights[i] += share;
            }
        }
    }

    if (!sideways.empty()) {
        double share = (double)gs.moveSidewaysProb / sideways.size();
        for (size_t i = 0; i < neighbors.size(); i++) {
            for (size_t j = 0; j < sideways.size(); j++) {
                if (neighbors[i] == sideways[j])
                    weights[i] += share;
            }
        }
    }

    if (enemy.lureTimer > 0 && !distToLure.empty()) {
        int currentLureDist = distToLure.count(enemy.currentRoom) ? distToLure[enemy.currentRoom] : 999;
        for (size_t i = 0; i < neighbors.size(); i++) {
            int nb = neighbors[i];
            if (distToLure.count(nb) && distToLure[nb] < currentLureDist)
                weights[i] *= 1.75;
        }
    }

    return weightedChoice(neighbors, weights);
}

// Builds a reset enemy in an invalid room until a night spawn is assigned.
Enemy::Enemy() : currentRoom(-1), state(ROAMING),
    lureTarget(-1), lureTimer(0), lastRoom(-1), moveChance(1.0) {}

// Initializes the enemy for a new night using the selected difficulty.
void Enemy::init(int startRoom, Difficulty) {
    currentRoom = startRoom;
    state = ROAMING;
    lureTarget = -1;
    lureTimer = 0;
    lastRoom = startRoom;
    moveChance = 1.0;
}

// Moves the enemy exactly one step per turn when a legal graph move exists.
void Enemy::move(const GameState &gs) {
    const GameMap &map = gs.gameMap;
    if (currentRoom < 0 || currentRoom >= map.totalRooms) return;

    int chosen = chooseNextRoom(gs, *this);
    lastRoom = currentRoom;
    currentRoom = chosen;

    if (currentRoom == map.officeId)
        state = AT_OFFICE;
    else if (lureTimer > 0 && currentRoom == lureTarget)
        state = INVESTIGATING;
    else
        state = ROAMING;
}

// Redirects the enemy toward a target room for a limited number of turns.
void Enemy::applyLure(int targetRoom, int duration) {
    lureTarget = targetRoom;
    lureTimer = duration;
    state = INVESTIGATING;
}

// Advances the lure timer and clears it when the distraction expires.
void Enemy::tickLure() {
    if (lureTimer > 0) {
        lureTimer--;
        if (lureTimer == 0) {
            lureTarget = -1;
            state = ROAMING;
        }
    }
}
