#ifndef ENEMY_H
#define ENEMY_H

#include <vector>
#include <map>
#include <string>
#include "map.h"

struct DifficultySettings;

enum EnemyState { ROAMING, INVESTIGATING, ATTACKING, AT_OFFICE };

struct Enemy {
    int currentRoom;
    EnemyState state;
    int alertLevel;      // 0-100
    int lureTarget;      // room id or -1
    int lureTimer;       // turns remaining on lure
    int lastRoom;        // previous room
    double officeBias;   // how much the enemy gravitates toward office
    double moveChance;   // probability of moving each turn (difficulty)

    Enemy();
    void init(int startRoom, Difficulty diff);
    void move(const GameMap &map);
    void moveWeighted(const GameMap &map, const DifficultySettings &settings);
    void applyLure(int targetRoom, int duration);
    void tickLure();
};

#endif
