#ifndef GAME_H
#define GAME_H

#include <string>
#include <vector>
#include <map>
#include "map.h"
#include "enemy.h"

enum GameStatus { STATUS_PLAYING, STATUS_WIN, STATUS_LOSE_ENEMY, STATUS_LOSE_POWER };

struct CameraSighting {
    int roomId;
    bool enemyPresent;
    std::string status;
};

struct GameState {
    Difficulty difficulty;
    int currentNight;
    int totalNights;
    int turn;
    int maxTurns;        // turns per night (6AM arrival)
    int power;
    int maxPower;
    int cameraPowerCost;
    int lurePowerCost;
    int doorPowerCost;
    int scanPowerCost;
    int lureCooldown;
    int lureCooldownMax;
    int currentLureCooldown;

    GameMap gameMap;
    Enemy enemy;

    int lastKnownEnemyRoom;
    std::map<int, double> probMap;
    std::vector<CameraSighting> lastCameraCheck;
    int lastCameraGroupChecked;

    GameStatus status;
    std::string statusMessage;

    // Night log
    std::vector<std::string> eventLog;

    GameState();
    void init(Difficulty diff);
    void newNight();
    void doTurn(int action, int param);
    void enemyTurn();
    void checkConditions();
    void checkCamera(int group);
    void playLure(int group);
    void closeDoor(int roomId);
    void restoreDoor(int roomId);
    void riskScan();
    void updateProbMap();
};

// Difficulty parameters
void setDifficultyParams(GameState &gs, Difficulty diff);

#endif
