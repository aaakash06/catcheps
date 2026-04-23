#include "enemy.h"
#include "graph_algos.h"
#include <cstdlib>
#include <algorithm>
#include <cmath>

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

void Enemy::move(const GameMap &map) {
    if (currentRoom < 0 || currentRoom >= map.totalRooms) return;

    // Random chance to stay put based on difficulty
    if ((double)(std::rand() % 100) / 100.0 > moveChance) return;

    auto &room = map.rooms[currentRoom];
    std::vector<int> candidates;
    std::vector<double> weights;

    for (int nb : room.neighbors) {
        if (room.doorClosed) continue;
        // Don't allow going back to where we just were unless it's the only option
        candidates.push_back(nb);
    }

    if (candidates.empty()) return;

    // Compute weights
    auto distToOffice = bfsDistances(map.rooms, map.officeId);
    auto distToLure = (lureTarget >= 0) ? bfsDistances(map.rooms, lureTarget) : std::map<int,int>();

    for (int nb : candidates) {
        double w = 1.0;

        // Prefer rooms closer to office (office bias)
        if (distToOffice.count(nb) && distToOffice.count(currentRoom)) {
            if (distToOffice[nb] < distToOffice[currentRoom])
                w += officeBias * 3.0;
            else if (distToOffice[nb] == distToOffice[currentRoom])
                w += officeBias;
        }

        // Strong preference toward lure
        if (lureTimer > 0 && !distToLure.empty() && distToLure.count(nb)) {
            if (distToLure[nb] < (distToLure.count(currentRoom) ? distToLure[currentRoom] : 999))
                w += 2.0;
        }

        // Slight preference for unvisited / different rooms (avoid back-and-forth)
        if (nb != lastRoom)
            w += 0.3;
        else
            w += 0.05;

        weights.push_back(w);
    }

    // Weighted random selection
    double totalWeight = 0;
    for (double w : weights) totalWeight += w;

    double r = (double)(std::rand() % 10000) / 10000.0 * totalWeight;
    double cumulative = 0;
    int chosen = candidates[0];
    for (size_t i = 0; i < candidates.size(); i++) {
        cumulative += weights[i];
        if (r <= cumulative) {
            chosen = candidates[i];
            break;
        }
    }

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
