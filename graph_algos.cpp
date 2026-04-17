#include "graph_algos.h"
#include "map.h"
#include <queue>
#include <set>
#include <algorithm>
#include <cmath>

std::vector<int> bfsShortestPath(const std::vector<Room> &rooms, int src, int dst) {
    if (src < 0 || src >= (int)rooms.size() || dst < 0 || dst >= (int)rooms.size())
        return {};

    std::vector<int> prev(rooms.size(), -1);
    std::vector<bool> visited(rooms.size(), false);
    std::queue<int> q;
    q.push(src);
    visited[src] = true;

    while (!q.empty()) {
        int cur = q.front();
        q.pop();
        if (cur == dst) break;

        for (int nb : rooms[cur].neighbors) {
            if (!visited[nb] && !rooms[cur].doorClosed) {
                visited[nb] = true;
                prev[nb] = cur;
                q.push(nb);
            }
        }
    }

    if (!visited[dst]) return {};

    std::vector<int> path;
    for (int at = dst; at != -1; at = prev[at])
        path.push_back(at);
    std::reverse(path.begin(), path.end());
    return path;
}

std::map<int, int> bfsDistances(const std::vector<Room> &rooms, int src) {
    std::map<int, int> dist;
    if (src < 0 || src >= (int)rooms.size()) return dist;

    std::vector<bool> visited(rooms.size(), false);
    std::queue<int> q;
    q.push(src);
    visited[src] = true;
    dist[src] = 0;

    while (!q.empty()) {
        int cur = q.front();
        q.pop();
        for (int nb : rooms[cur].neighbors) {
            if (!visited[nb] && !rooms[cur].doorClosed) {
                visited[nb] = true;
                dist[nb] = dist[cur] + 1;
                q.push(nb);
            }
        }
    }
    return dist;
}

static void apDfs(const std::vector<Room> &rooms, int u, int &timer,
                  std::vector<int> &disc, std::vector<int> &low,
                  std::vector<int> &parent, std::vector<bool> &isAP,
                  std::vector<std::pair<int,int>> &bridges) {
    disc[u] = low[u] = timer++;
    int children = 0;

    for (int v : rooms[u].neighbors) {
        if (rooms[u].doorClosed) continue;

        if (disc[v] == -1) {
            children++;
            parent[v] = u;
            apDfs(rooms, v, timer, disc, low, parent, isAP, bridges);
            low[u] = std::min(low[u], low[v]);

            if (parent[u] == -1 && children > 1)
                isAP[u] = true;
            if (parent[u] != -1 && low[v] >= disc[u])
                isAP[u] = true;

            if (low[v] > disc[u])
                bridges.push_back({std::min(u,v), std::max(u,v)});
        } else if (v != parent[u]) {
            low[u] = std::min(low[u], disc[v]);
        }
    }
}

std::vector<int> findArticulationPoints(const std::vector<Room> &rooms) {
    int n = rooms.size();
    std::vector<int> disc(n, -1), low(n, -1), parent(n, -1);
    std::vector<bool> isAP(n, false);
    std::vector<std::pair<int,int>> bridges;
    int timer = 0;

    for (int i = 0; i < n; i++)
        if (disc[i] == -1)
            apDfs(rooms, i, timer, disc, low, parent, isAP, bridges);

    std::vector<int> result;
    for (int i = 0; i < n; i++)
        if (isAP[i]) result.push_back(i);
    return result;
}

std::vector<std::pair<int, int>> findBridges(const std::vector<Room> &rooms) {
    int n = rooms.size();
    std::vector<int> disc(n, -1), low(n, -1), parent(n, -1);
    std::vector<bool> isAP(n, false);
    std::vector<std::pair<int,int>> bridges;
    int timer = 0;

    for (int i = 0; i < n; i++)
        if (disc[i] == -1)
            apDfs(rooms, i, timer, disc, low, parent, isAP, bridges);

    return bridges;
}

void diffuseProbability(const std::vector<Room> &rooms, std::map<int, double> &probMap) {
    std::map<int, double> next;
    for (size_t i = 0; i < rooms.size(); i++)
        next[i] = 0.0;

    for (size_t i = 0; i < rooms.size(); i++) {
        if (probMap[i] <= 0.0) continue;

        std::vector<int> openNeighbors;
        for (int nb : rooms[i].neighbors)
            if (!rooms[i].doorClosed)
                openNeighbors.push_back(nb);

        if (openNeighbors.empty()) {
            next[i] += probMap[i];
            continue;
        }

        double stay = probMap[i] * 0.3;
        double spread = probMap[i] * 0.7 / openNeighbors.size();
        next[i] += stay;
        for (int nb : openNeighbors)
            next[nb] += spread;
    }

    probMap = next;
}

std::map<int, double> computeDangerLevels(const std::vector<Room> &rooms,
                                           int officeId,
                                           int enemyLastKnown,
                                           const std::map<int, double> &probMap) {
    std::map<int, double> danger;
    auto distFromOffice = bfsDistances(rooms, officeId);

    int maxDist = 1;
    for (auto &p : distFromOffice)
        if (p.second > maxDist) maxDist = p.second;

    for (size_t i = 0; i < rooms.size(); i++) {
        double distFactor = 0.0;
        if (distFromOffice.count(i))
            distFactor = 1.0 - (double)distFromOffice[i] / maxDist;

        double probFactor = 0.0;
        if (probMap.count(i))
            probFactor = std::min(probMap.at(i) * 5.0, 1.0);

        double proximity = 0.0;
        if (enemyLastKnown >= 0) {
            auto path = bfsShortestPath(rooms, enemyLastKnown, i);
            if (!path.empty())
                proximity = 1.0 / (1.0 + path.size());
        }

        danger[i] = std::min(distFactor * 0.3 + probFactor * 0.4 + proximity * 0.3, 1.0);
    }
    return danger;
}
