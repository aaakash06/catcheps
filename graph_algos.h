#ifndef GRAPH_ALGOS_H
#define GRAPH_ALGOS_H

#include <vector>
#include <string>
#include <map>

struct Room;

// BFS shortest path from src to dst. Returns path as room IDs, empty if unreachable.
std::vector<int> bfsShortestPath(const std::vector<Room> &rooms, int src, int dst);

// BFS distance from src to every other room. Returns map of room->distance.
std::map<int, int> bfsDistances(const std::vector<Room> &rooms, int src);

// Find articulation points in the graph. Returns room IDs that are articulation points.
std::vector<int> findArticulationPoints(const std::vector<Room> &rooms);

// Find bridges in the graph. Returns pairs of (u, v) edges that are bridges.
std::vector<std::pair<int, int>> findBridges(const std::vector<Room> &rooms);

// Diffuse a probability distribution over the graph for one step.
// probMap: room->probability (should sum to ~1.0).
// Updates probMap in-place based on adjacency and door states.
void diffuseProbability(const std::vector<Room> &rooms, std::map<int, double> &probMap);

// Compute danger level for each room based on distance to office and enemy proximity.
// Returns map of room->danger (0.0 to 1.0).
std::map<int, double> computeDangerLevels(const std::vector<Room> &rooms,
                                           int officeId,
                                           int enemyLastKnown,
                                           const std::map<int, double> &probMap);

#endif
