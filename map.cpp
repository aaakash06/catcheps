#include "map.h"
#include "graph_algos.h"
#include <cstdlib>
#include <algorithm>

GameMap buildMap(Difficulty diff) {
    GameMap gm;
    int n;
    if (diff == EASY) n = 8;
    else if (diff == NORMAL) n = 10;
    else n = 12;

    gm.totalRooms = n;
    gm.officeId = 0;
    gm.numCameraGroups = (n <= 8) ? 3 : (n <= 10) ? 3 : 4;

    gm.rooms.resize(n);

    // Common layout: Office(0) is connected to Hall A(1) and Hall B(2).
    // Then it branches outward.
    if (diff == EASY) {
        // 8 rooms
        std::string names[] = {"Office", "Hall A", "Hall B", "Junction",
                               "Storage", "Camera Room", "Lobby", "Exit Hall"};
        // Edges: 0-1, 0-2, 1-3, 2-3, 3-4, 3-5, 4-6, 5-7, 6-7
        int edges[][2] = {{0,1},{0,2},{1,3},{2,3},{3,4},{3,5},{4,6},{5,7},{6,7}};
        int ne = 9;
        for (int i = 0; i < n; i++) {
            gm.rooms[i].id = i;
            gm.rooms[i].name = names[i];
            gm.rooms[i].isOffice = (i == 0);
            gm.rooms[i].doorClosed = false;
        }
        // Camera groups
        int groups[] = {0, 0, 0, 1, 1, 1, 2, 2};
        bool cams[]   = {0, 1, 1, 1, 0, 1, 0, 1};
        for (int i = 0; i < n; i++) {
            gm.rooms[i].cameraGroup = groups[i];
            gm.rooms[i].isCamera = cams[i];
        }
        for (int i = 0; i < ne; i++) {
            gm.rooms[edges[i][0]].neighbors.push_back(edges[i][1]);
            gm.rooms[edges[i][1]].neighbors.push_back(edges[i][0]);
        }
    } else if (diff == NORMAL) {
        // 10 rooms
        std::string names[] = {"Office", "Hall A", "Hall B", "Junction",
                               "Storage", "Camera Room", "Lobby", "Exit Hall",
                               "Server Room", "Break Room"};
        int edges[][2] = {{0,1},{0,2},{1,3},{2,3},{3,4},{3,5},{4,6},{5,7},
                          {6,7},{4,8},{5,9},{8,9}};
        int ne = 12;
        for (int i = 0; i < n; i++) {
            gm.rooms[i].id = i;
            gm.rooms[i].name = names[i];
            gm.rooms[i].isOffice = (i == 0);
            gm.rooms[i].doorClosed = false;
        }
        int groups[] = {0, 0, 0, 1, 1, 1, 2, 2, 2, 2};
        bool cams[]   = {0, 1, 1, 1, 0, 1, 1, 1, 0, 0};
        for (int i = 0; i < n; i++) {
            gm.rooms[i].cameraGroup = groups[i];
            gm.rooms[i].isCamera = cams[i];
        }
        for (int i = 0; i < ne; i++) {
            gm.rooms[edges[i][0]].neighbors.push_back(edges[i][1]);
            gm.rooms[edges[i][1]].neighbors.push_back(edges[i][0]);
        }
    } else {
        // 12 rooms (Hard)
        std::string names[] = {"Office", "Hall A", "Hall B", "Junction",
                               "Storage", "Camera Room", "Lobby", "Exit Hall",
                               "Server Room", "Break Room", "Lab", "Basement"};
        int edges[][2] = {{0,1},{0,2},{1,3},{2,3},{3,4},{3,5},{4,6},{5,7},
                          {6,7},{4,8},{5,9},{8,9},{8,10},{9,11},{10,11},{6,10}};
        int ne = 16;
        for (int i = 0; i < n; i++) {
            gm.rooms[i].id = i;
            gm.rooms[i].name = names[i];
            gm.rooms[i].isOffice = (i == 0);
            gm.rooms[i].doorClosed = false;
        }
        int groups[] = {0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3};
        bool cams[]   = {0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 0};
        for (int i = 0; i < n; i++) {
            gm.rooms[i].cameraGroup = groups[i];
            gm.rooms[i].isCamera = cams[i];
        }
        for (int i = 0; i < ne; i++) {
            gm.rooms[edges[i][0]].neighbors.push_back(edges[i][1]);
            gm.rooms[edges[i][1]].neighbors.push_back(edges[i][0]);
        }
    }

    return gm;
}

std::string roomStatusChar(const Room &r, int enemyRoom, int lastKnown) {
    if (r.isOffice) return "[SAFE]";
    if (r.doorClosed) return "[X]";
    if (r.id == enemyRoom) return "[!]";
    if (r.id == lastKnown) return "[?]";
    return "[ ]";
}

std::string cameraGroupLabel(int group) {
    if (group == 0) return "A";
    if (group == 1) return "B";
    if (group == 2) return "C";
    return "D";
}

std::vector<int> roomsInGroup(const GameMap &map, int group) {
    std::vector<int> result;
    for (auto &r : map.rooms)
        if (r.cameraGroup == group)
            result.push_back(r.id);
    return result;
}

int findSpawnRoom(const GameMap &map) {
    auto dist = bfsDistances(map.rooms, map.officeId);
    int maxDist = 0;
    for (auto &p : dist)
        if (p.second > maxDist) maxDist = p.second;

    std::vector<int> farRooms;
    for (auto &p : dist)
        if (p.second >= maxDist - 1)
            farRooms.push_back(p.first);

    return farRooms[std::rand() % farRooms.size()];
}
