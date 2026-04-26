#include "graph_algos.h"
#include "map.h"
#include <queue>
#include <set>
#include <algorithm>
#include <cmath>

// Returns whether a graph edge is currently blocked by one of the two office gates.
static bool isBlockedOfficeEdge(int officeId, bool leftGateClosed, bool rightGateClosed,
                                int from, int to) {
    const int KAD = 3;
    const int KNOW = 4;

    if ((from == officeId && to == KNOW) || (from == KNOW && to == officeId))
        return leftGateClosed;

    if ((from == officeId && to == KAD) || (from == KAD && to == officeId))
        return rightGateClosed;

    return false;
}

// Computes a shortest path on the room graph while respecting blocked office edges.
std::vector<int> bfsShortestPath(const std::vector<Room> &rooms, int src, int dst,
                                 int officeId, bool leftGateClosed, bool rightGateClosed) {
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
            if (!visited[nb] &&
                !isBlockedOfficeEdge(officeId, leftGateClosed, rightGateClosed, cur, nb)) {
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

// Computes shortest-path distances from one source room to every reachable room.
std::map<int, int> bfsDistances(const std::vector<Room> &rooms, int src,
                                int officeId, bool leftGateClosed, bool rightGateClosed) {
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
            if (!visited[nb] &&
                !isBlockedOfficeEdge(officeId, leftGateClosed, rightGateClosed, cur, nb)) {
                visited[nb] = true;
                dist[nb] = dist[cur] + 1;
                q.push(nb);
            }
        }
    }
    return dist;
}

// DFS helper shared by articulation-point and bridge detection.
static void apDfs(const std::vector<Room> &rooms, int u, int &timer,
                  int officeId, bool leftGateClosed, bool rightGateClosed,
                  std::vector<int> &disc, std::vector<int> &low,
                  std::vector<int> &parent, std::vector<bool> &isAP,
                  std::vector<std::pair<int,int>> &bridges) {
    disc[u] = low[u] = timer++;
    int children = 0;

    for (int v : rooms[u].neighbors) {
        if (isBlockedOfficeEdge(officeId, leftGateClosed, rightGateClosed, u, v)) continue;

        if (disc[v] == -1) {
            children++;
            parent[v] = u;
            apDfs(rooms, v, timer, officeId, leftGateClosed, rightGateClosed,
                  disc, low, parent, isAP, bridges);
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

// Finds articulation points that would disconnect parts of the current campus graph.
std::vector<int> findArticulationPoints(const std::vector<Room> &rooms,
                                        int officeId, bool leftGateClosed, bool rightGateClosed) {
    int n = rooms.size();
    std::vector<int> disc(n, -1), low(n, -1), parent(n, -1);
    std::vector<bool> isAP(n, false);
    std::vector<std::pair<int,int>> bridges;
    int timer = 0;

    for (int i = 0; i < n; i++)
        if (disc[i] == -1)
            apDfs(rooms, i, timer, officeId, leftGateClosed, rightGateClosed,
                  disc, low, parent, isAP, bridges);

    std::vector<int> result;
    for (int i = 0; i < n; i++)
        if (isAP[i]) result.push_back(i);
    return result;
}

// Finds bridge edges whose removal would disconnect part of the current graph.
std::vector<std::pair<int, int>> findBridges(const std::vector<Room> &rooms,
                                             int officeId, bool leftGateClosed, bool rightGateClosed) {
    int n = rooms.size();
    std::vector<int> disc(n, -1), low(n, -1), parent(n, -1);
    std::vector<bool> isAP(n, false);
    std::vector<std::pair<int,int>> bridges;
    int timer = 0;

    for (int i = 0; i < n; i++)
        if (disc[i] == -1)
            apDfs(rooms, i, timer, officeId, leftGateClosed, rightGateClosed,
                  disc, low, parent, isAP, bridges);

    return bridges;
}

// Diffuses one step of enemy-location probability mass across legal graph edges.
void diffuseProbability(const std::vector<Room> &rooms, std::map<int, double> &probMap,
                        int officeId, bool leftGateClosed, bool rightGateClosed) {
    std::map<int, double> next;
    for (size_t i = 0; i < rooms.size(); i++)
        next[i] = 0.0;

    for (size_t i = 0; i < rooms.size(); i++) {
        if (probMap[i] <= 0.0) continue;

        std::vector<int> openNeighbors;
        for (int nb : rooms[i].neighbors)
            if (!isBlockedOfficeEdge(officeId, leftGateClosed, rightGateClosed, (int)i, nb))
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

// Combines office distance, current probability, and last-known proximity into
// a coarse danger score per room.
std::map<int, double> computeDangerLevels(const std::vector<Room> &rooms,
                                           int officeId,
                                           int enemyLastKnown,
                                           const std::map<int, double> &probMap,
                                           bool leftGateClosed,
                                           bool rightGateClosed) {
    std::map<int, double> danger;
    auto distFromOffice = bfsDistances(rooms, officeId, officeId,
                                       leftGateClosed, rightGateClosed);

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
            auto path = bfsShortestPath(rooms, enemyLastKnown, i,
                                        officeId, leftGateClosed, rightGateClosed);
            if (!path.empty())
                proximity = 1.0 / (1.0 + path.size());
        }

        danger[i] = std::min(distFactor * 0.3 + probFactor * 0.4 + proximity * 0.3, 1.0);
    }
    return danger;
}
