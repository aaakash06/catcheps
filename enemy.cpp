#include "enemy.h"
#include "game.h"
#include "graph_algos.h"
#include <cstdlib>
#include <algorithm>
#include <cmath>

static bool isHubNode(int roomId) {
    return roomId == 2 || roomId == 5 || roomId == 6; // LIB, CYM, HC
}

static bool isOuterRingNode(const GameState &gs, int roomId) {
    return roomId >= 0 &&
           roomId < gs.gameMap.totalRooms &&
           gs.gameMap.rooms[roomId].cameraGroup == 2;
}

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

static int weightedChoice(const std::vector<int> &rooms, const std::vector<double> &weights) {
    if (rooms.empty()) return -1;

    double totalWeight = 0.0;
    for (double w : weights)
        totalWeight += w;

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

static int chooseNextRoom(const GameState &gs, const Enemy &enemy) {
    std::vector<int> neighbors = getOpenNeighbors(gs, enemy.currentRoom);
    if (neighbors.empty()) return enemy.currentRoom;

    auto distToOffice = bfsDistances(gs.gameMap.rooms, gs.gameMap.officeId,
                                     gs.gameMap.officeId,
                                     gs.leftGateClosed, gs.rightGateClosed);
    auto distToLure = (enemy.lureTarget >= 0)
        ? bfsDistances(gs.gameMap.rooms, enemy.lureTarget, gs.gameMap.officeId,
                       gs.leftGateClosed, gs.rightGateClosed)
        : std::map<int, int>();

    int currentDist = distToOffice.count(enemy.currentRoom) ? distToOffice[enemy.currentRoom] : 999;
    std::vector<int> toward;
    std::vector<int> sideways;
    std::vector<int> backward;

    for (int nb : neighbors) {
        int nbDist = distToOffice.count(nb) ? distToOffice[nb] : 999;
        if (enemy.lastRoom >= 0 && nb == enemy.lastRoom) {
            backward.push_back(nb);
        } else if (nbDist < currentDist) {
            toward.push_back(nb);
        } else {
            sideways.push_back(nb);
        }
    }

    if (gs.difficulty == EASY) {
        std::vector<int> nonBackward = neighbors;
        if (!sideways.empty() || !toward.empty()) {
            nonBackward.clear();
            nonBackward.insert(nonBackward.end(), toward.begin(), toward.end());
            nonBackward.insert(nonBackward.end(), sideways.begin(), sideways.end());
        }

        if (!toward.empty() && (std::rand() % 100) < 80)
            return toward[std::rand() % toward.size()];

        if (!nonBackward.empty())
            return nonBackward[std::rand() % nonBackward.size()];

        return backward[std::rand() % backward.size()];
    }

    if (gs.difficulty == HARD && enemy.currentRoom == 4) {
        std::vector<int> options;
        std::vector<double> weights;
        for (int nb : neighbors) {
            if (nb == gs.gameMap.officeId) {
                options.push_back(nb);
                weights.push_back(50.0);
            } else if (nb == 9) {
                options.push_back(nb);
                weights.push_back(30.0);
            } else {
                // KNOW has no direct HC edge in the current graph, so this
                // fallback maps the detour preference onto the remaining exit.
                options.push_back(nb);
                weights.push_back(20.0);
            }
        }
        return weightedChoice(options, weights);
    }

    double towardBase = (gs.difficulty == NORMAL) ? 60.0 : 40.0;
    double sidewaysBase = (gs.difficulty == NORMAL) ? 25.0 : 40.0;
    double backwardBase = (gs.difficulty == NORMAL) ? 15.0 : 20.0;
    double hubMultiplier = (gs.difficulty == NORMAL) ? 1.15 : 1.20;

    std::vector<double> weights;
    weights.reserve(neighbors.size());

    for (int nb : neighbors) {
        double base = sidewaysBase;
        bool isBackward = enemy.lastRoom >= 0 && nb == enemy.lastRoom;
        int nbDist = distToOffice.count(nb) ? distToOffice[nb] : 999;

        if (isBackward)
            base = backwardBase;
        else if (nbDist < currentDist)
            base = towardBase;

        if (isHubNode(nb))
            base *= hubMultiplier;

        if (enemy.lureTimer > 0 && !distToLure.empty() && distToLure.count(nb)) {
            int currentLureDist = distToLure.count(enemy.currentRoom) ? distToLure[enemy.currentRoom] : 999;
            if (distToLure[nb] < currentLureDist)
                base *= 1.75;
        }

        weights.push_back(base);
    }

    if (gs.difficulty == HARD && isOuterRingNode(gs, enemy.currentRoom)) {
        double totalWeight = 0.0;
        double outerWeight = 0.0;
        for (size_t i = 0; i < neighbors.size(); i++) {
            totalWeight += weights[i];
            if (isOuterRingNode(gs, neighbors[i]))
                outerWeight += weights[i];
        }

        if (outerWeight > 0.0 && totalWeight > 0.0) {
            double currentShare = outerWeight / totalWeight;
            if (currentShare < 0.35) {
                double boost = 0.35 / currentShare;
                for (size_t i = 0; i < neighbors.size(); i++) {
                    if (isOuterRingNode(gs, neighbors[i]))
                        weights[i] *= boost;
                }
            }
        }
    }

    return weightedChoice(neighbors, weights);
}

Enemy::Enemy() : currentRoom(-1), state(ROAMING), alertLevel(0),
    lureTarget(-1), lureTimer(0), lastRoom(-1), officeBias(0.1), moveChance(1.0) {}

void Enemy::init(int startRoom, Difficulty diff) {
    currentRoom = startRoom;
    state = ROAMING;
    alertLevel = 0;
    lureTarget = -1;
    lureTimer = 0;
    lastRoom = startRoom;

    if (diff == EASY) {
        officeBias = 0.08;
        moveChance = 0.85;
    } else if (diff == NORMAL) {
        officeBias = 0.15;
        moveChance = 0.92;
    } else {
        officeBias = 0.22;
        moveChance = 1.0;
    }
}

void Enemy::move(const GameState &gs) {
    const GameMap &map = gs.gameMap;
    if (currentRoom < 0 || currentRoom >= map.totalRooms) return;

    // Random chance to stay put based on difficulty
    if ((double)(std::rand() % 100) / 100.0 > moveChance) return;

    int chosen = chooseNextRoom(gs, *this);

    // Update state
    lastRoom = currentRoom;
    currentRoom = chosen;

    if (currentRoom == map.officeId)
        state = AT_OFFICE;
    else if (lureTimer > 0 && currentRoom == lureTarget)
        state = INVESTIGATING;
    else
        state = ROAMING;
}

void Enemy::applyLure(int targetRoom, int duration) {
    lureTarget = targetRoom;
    lureTimer = duration;
    state = INVESTIGATING;
}

void Enemy::tickLure() {
    if (lureTimer > 0) {
        lureTimer--;
        if (lureTimer == 0) {
            lureTarget = -1;
            state = ROAMING;
        }
    }
}
