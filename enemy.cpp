#include "enemy.h"
#include "game.h"
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

void Enemy::moveWeighted(const GameMap &map, const DifficultySettings &settings) {
    if (currentRoom < 0 || currentRoom >= map.totalRooms) return;

    auto &room = map.rooms[currentRoom];
    if (room.doorClosed) return;

    std::vector<int> candidates;
    for (int nb : room.neighbors)
        if (!map.rooms[nb].doorClosed)
            candidates.push_back(nb);
    if (candidates.empty()) return;

    std::map<int, int> targetDist;
    int target = map.officeId;
    if (lureTimer > 0 && lureTarget >= 0) {
        target = lureTarget;
        targetDist = bfsDistances(map.rooms, lureTarget);
    } else {
        targetDist = bfsDistances(map.rooms, map.officeId);
    }

    int currentDist = targetDist.count(currentRoom) ? targetDist[currentRoom] : 999;
    std::vector<int> closer;
    std::vector<int> sideways;
    std::vector<int> farther;

    for (int nb : candidates) {
        int nd = targetDist.count(nb) ? targetDist[nb] : 999;
        if (nd < currentDist)
            closer.push_back(nb);
        else if (nd == currentDist)
            sideways.push_back(nb);
        else
            farther.push_back(nb);
    }

    std::vector<int> pool;
    int roll = std::rand() % 100;
    if (roll < settings.moveCloserProb && !closer.empty()) {
        pool = closer;
    } else if (roll < settings.moveCloserProb + settings.moveSidewaysProb && !sideways.empty()) {
        pool = sideways;
    } else if (settings.moveRandomProb > 0) {
        pool = candidates;
    }

    if (pool.empty()) {
        if (!closer.empty()) pool = closer;
        else if (!sideways.empty()) pool = sideways;
        else pool = candidates;
    }

    std::vector<int> preferred;
    for (int rid : pool)
        if (rid != lastRoom)
            preferred.push_back(rid);
    if (!preferred.empty())
        pool = preferred;

    int chosen = pool[std::rand() % pool.size()];
    lastRoom = currentRoom;
    currentRoom = chosen;

    if (currentRoom == map.officeId)
        state = AT_OFFICE;
    else if (lureTimer > 0 && currentRoom == target)
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
