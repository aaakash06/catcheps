#ifndef MAP_H
#define MAP_H

#include <string>
#include <vector>

enum Difficulty { EASY, NORMAL, HARD };

struct Room {
    int id;
    std::string name;
    std::string abbrev; // 3-4 char abbreviation for map display
    std::vector<int> neighbors;
    bool isCamera;
    bool isOffice;
    int cameraGroup; // primary/default camera group, -1=none
};

struct GameMap {
    std::vector<Room> rooms;
    int officeId;
    int totalRooms;
    int numCameraGroups;
};

// Build a map for the given difficulty.
GameMap buildMap(Difficulty diff);

// Get camera group label.
std::string cameraGroupLabel(const GameMap &map, int group);

// Get all rooms in a camera group.
std::vector<int> roomsInGroup(const GameMap &map, int group);

// Check whether a room belongs to a camera group.
bool roomInCameraGroup(const GameMap &map, int roomId, int group);

// Find a random room far from the office for enemy spawn.
int findSpawnRoom(const GameMap &map);

#endif
