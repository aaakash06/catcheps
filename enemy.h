#ifndef ENEMY_H
#define ENEMY_H

#include <vector>
#include <map>
#include <string>
#include "map.h"

struct GameState;

enum EnemyState {
    ROAMING = 0,
    INVESTIGATING = 1,
    // Reserved to keep older save files compatible with the previous enum layout.
    LEGACY_ATTACKING = 2,
    AT_OFFICE = 3
};

struct Enemy {
    int currentRoom;
    EnemyState state;
    int lureTarget;      // room id or -1
    int lureTimer;       // turns remaining on lure
    int lastRoom;        // previous room

    Enemy();
    void init(int startRoom, Difficulty diff);
    bool move(const GameState &gs);
    void applyLure(int targetRoom, int duration);
    bool tickLure();
};

#endif
