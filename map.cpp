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
    gm.numCameraGroups = 4;

    gm.rooms.resize(n);
    for (int i = 0; i < n; i++) {
        gm.rooms[i].id = i;
        gm.rooms[i].abbrev = allBuildings[i].abbrev;
        gm.rooms[i].name = allBuildings[i].name;
        gm.rooms[i].isOffice = allBuildings[i].isOffice;
        gm.rooms[i].isCamera = true;
    }

    // Primary/default camera groups.
    // Normal mode uses overlapping helper-based groups below, so these are only
    // a fallback / default assignment.
    int groups13[] = {-1, 2, 1, 0, 0, 2, 1, 2, 2, 2, 2, 2, 2};
    for (int i = 0; i < n; i++)
        gm.rooms[i].cameraGroup = groups13[i];

    if (diff == EASY) {
        addEdge(gm, 0, 3);  // MB -- KAD
        addEdge(gm, 0, 4);  // MB -- KNOW
        addEdge(gm, 3, 6);  // KAD -- HC
        addEdge(gm, 6, 5);  // HC -- CYM
        addEdge(gm, 6, 7);  // HC -- HW
        addEdge(gm, 7, 5);  // HW -- CYM
        addEdge(gm, 2, 4);  // LIB -- KNOW
        addEdge(gm, 2, 1);  // LIB -- KKL
        addEdge(gm, 2, 6);  // LIB -- HC
    } else if (diff == NORMAL) {
        addEdge(gm, 0, 3);  // MB -- KAD
        addEdge(gm, 0, 4);  // MB -- KNOW
        addEdge(gm, 3, 6);  // KAD -- HC
        addEdge(gm, 6, 5);  // HC -- CYM
        addEdge(gm, 6, 7);  // HC -- HW
        addEdge(gm, 7, 5);  // HW -- CYM
        addEdge(gm, 2, 4);  // LIB -- KNOW
        addEdge(gm, 2, 1);  // LIB -- KKL
        addEdge(gm, 2, 6);  // LIB -- HC
        addEdge(gm, 2, 9);  // LIB -- RM
        addEdge(gm, 7, 8);  // HW -- MW
        addEdge(gm, 5, 9);  // CYM -- RM
    } else {
        addEdge(gm, 8, 10);  // MW -- RHS
        addEdge(gm, 10, 12); // RHS -- JL
        addEdge(gm, 8, 7);   // MW -- HW
        addEdge(gm, 10, 5);  // RHS -- CYM
        addEdge(gm, 12, 11); // JL -- RR
        addEdge(gm, 7, 5);   // HW -- CYM
        addEdge(gm, 5, 11);  // CYM -- RR
        addEdge(gm, 7, 6);   // HW -- HC
        addEdge(gm, 5, 2);   // CYM -- LIB
        addEdge(gm, 3, 6);   // KAD -- HC
        addEdge(gm, 6, 2);   // HC -- LIB
        addEdge(gm, 2, 1);   // LIB -- KKL
        addEdge(gm, 6, 4);   // HC -- KNOW
        addEdge(gm, 2, 9);   // LIB -- RM
        addEdge(gm, 4, 9);   // KNOW -- RM
        addEdge(gm, 0, 3);   // MB -- KAD
        addEdge(gm, 0, 4);   // MB -- KNOW
    }

    return gm;
}

std::string cameraGroupLabel(const GameMap &map, int group) {
    if (map.totalRooms == 8) {
        if (group == 0) return "West";
        if (group == 1) return "East";
        if (group == 2) return "Nexus";
        if (group == 3) return "Threshold";
        return "?";
    }

    if (map.totalRooms == 10) {
        if (group == 0) return "North Link";
        if (group == 1) return "West Flank";
        if (group == 2) return "East Corridor";
        if (group == 3) return "Doorstep";
        return "?";
    }

    if (map.totalRooms == 13) {
        if (group == 0) return "Upper Perimeter";
        if (group == 1) return "Central Hubs";
        if (group == 2) return "West Access";
        if (group == 3) return "East Access";
        return "?";
    }

    return "?";
}

int effectiveCameraGroupCount(const GameMap &map) {
    if (map.numCameraGroups >= 1 && map.numCameraGroups <= 4)
        return map.numCameraGroups;

    if (map.totalRooms == 8 || map.totalRooms == 10 || map.totalRooms == 13)
        return 4;

    return 0;
}

bool roomInCameraGroup(const GameMap &map, int roomId, int group) {
    if (roomId < 0 || roomId >= map.totalRooms) return false;

    if (roomId == map.officeId) return false;

    // Overlapping strategic camera groups for EASY mode.
    if (map.totalRooms == 8) {
        if (group == 0) return roomId == 1 || roomId == 2 || roomId == 4;      // KKL, LIB, KNOW
        if (group == 1) return roomId == 7 || roomId == 5 || roomId == 6 ||
                               roomId == 3;                                     // HW, CYM, HC, KAD
        if (group == 2) return roomId == 2 || roomId == 6;                       // LIB, HC
        if (group == 3) return roomId == 4 || roomId == 3;                       // KNOW, KAD
        return false;
    }

    // Overlapping strategic camera groups for NORMAL mode.
    if (map.totalRooms == 10) {
        if (group == 0) return roomId == 8 || roomId == 5 || roomId == 9; // MW, CYM, RM
        if (group == 1) return roomId == 9 || roomId == 2 || roomId == 1; // RM, LIB, KKL
        if (group == 2) return roomId == 5 || roomId == 7 || roomId == 6; // CYM, HW, HC
        if (group == 3) return roomId == 4 || roomId == 3;                 // KNOW, KAD
        return false;
    }

    // Overlapping strategic camera groups for HARD mode.
    if (map.totalRooms == 13) {
        if (group == 0) return roomId == 8 || roomId == 10 || roomId == 12 ||
                               roomId == 7 || roomId == 11;                 // MW, RHS, JL, HW, RR
        if (group == 1) return roomId == 5 || roomId == 6 || roomId == 2 ||
                               roomId == 1;                                  // CYM, HC, LIB, KKL
        if (group == 2) return roomId == 7 || roomId == 6 || roomId == 3;   // HW, HC, KAD
        if (group == 3) return roomId == 9 || roomId == 4;                   // RM, KNOW
        return false;
    }

    return map.rooms[roomId].cameraGroup == group;
}

std::vector<int> roomsInGroup(const GameMap &map, int group) {
    std::vector<int> result;
    for (auto &r : map.rooms)
        if (roomInCameraGroup(map, r.id, group))
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
        if (p.second == maxDist)
            farRooms.push_back(p.first);

    return farRooms[std::rand() % farRooms.size()];
}
