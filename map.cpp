#include "map.h"
#include "graph_algos.h"
#include <cstdlib>
#include <algorithm>

static void addEdge(GameMap &gm, int a, int b) {
    gm.rooms[a].neighbors.push_back(b);
    gm.rooms[b].neighbors.push_back(a);
}

GameMap buildMap(Difficulty diff) {
    GameMap gm;

    // All 13 HKU buildings (ordered so first N are used for each difficulty)
    // 0:MB  1:KKL  2:LIB  3:KAD  4:KNOW  5:CYM  6:HC  7:HW  8:MW  9:RM  10:RHS  11:RR  12:JL
    struct Bldg { const char *abbrev; const char *name; bool isOffice; };
    Bldg allBuildings[] = {
        {"MB",   "Main Building",        true},
        {"KKL",  "K K Leung Building",   false},
        {"LIB",  "Main Library",         false},
        {"KAD",  "Kadoorie Building",    false},
        {"KNOW", "Knowles Building",     false},
        {"CYM",  "Chong Yuet Ming",      false},
        {"HC",   "Haking Wong",          false},
        {"HW",   "Haking Wong Eng",      false},
        {"MW",   "Meng Wah Complex",     false},
        {"RM",   "Runme Shaw",           false},
        {"RHS",  "Rayson Hsu Shaw",      false},
        {"RR",   "Run Run Shaw",         false},
        {"JL",   "J Lee Building",       false},
    };

    int n;
    if (diff == EASY) n = 8;
    else if (diff == NORMAL) n = 10;
    else n = 13;

    gm.totalRooms = n;
    gm.officeId = 0; // MB is always the office
    gm.numCameraGroups = 3;

    gm.rooms.resize(n);
    for (int i = 0; i < n; i++) {
        gm.rooms[i].id = i;
        gm.rooms[i].abbrev = allBuildings[i].abbrev;
        gm.rooms[i].name = allBuildings[i].name;
        gm.rooms[i].isOffice = allBuildings[i].isOffice;
        gm.rooms[i].doorClosed = false;
        gm.rooms[i].isCamera = true;
    }

    // Camera groups:
    // 0=Upper: MW(8), RM(9), RHS(10), RR(11), JL(12)
    // 1=Central: HC(6), HW(7), CYM(5)
    // 2=Lower: LIB(2), KKL(1), KAD(3), KNOW(4), MB(0)
    int groups13[] = {2, 2, 2, 2, 2, 1, 1, 1, 0, 0, 0, 0, 0};
    for (int i = 0; i < n; i++)
        gm.rooms[i].cameraGroup = groups13[i];

    // Edges — built incrementally so subsets remain connected

    // Core edges (always present, rooms 0-7 for EASY)
    addEdge(gm, 0, 3);  // MB -- KAD
    addEdge(gm, 0, 4);  // MB -- KNOW
    addEdge(gm, 3, 2);  // KAD -- LIB
    addEdge(gm, 3, 6);  // KAD -- HC
    addEdge(gm, 2, 1);  // LIB -- KKL
    addEdge(gm, 2, 4);  // LIB -- KNOW
    addEdge(gm, 2, 6);  // LIB -- HC
    addEdge(gm, 6, 5);  // HC -- CYM
    addEdge(gm, 6, 7);  // HC -- HW
    addEdge(gm, 1, 5);  // KKL -- CYM

    if (n >= 10) {
        // NORMAL+: add MW(8), RM(9) connections
        addEdge(gm, 7, 8);  // HW -- MW
        addEdge(gm, 8, 9);  // MW -- RM
        addEdge(gm, 6, 9);  // HC -- RM
    }

    if (n >= 13) {
        // HARD: add RHS(10), RR(11), JL(12)
        addEdge(gm, 9, 10);  // RM -- RHS
        addEdge(gm, 10, 11); // RHS -- RR
        addEdge(gm, 11, 12); // RR -- JL
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
    if (group == 0) return "Upper";
    if (group == 1) return "Central";
    if (group == 2) return "Lower";
    return "?";
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
