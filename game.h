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
    bool leftGateClosed;
    bool rightGateClosed;

    GameMap gameMap;
    Enemy enemy;

    int activeCameraGroup;
    int brokenCameraGroup;
    int brokenCameraTurns;
    int lastBrokenCameraGroup;
    int lastKnownEnemyRoom;
    std::map<int, double> probMap;
    std::vector<CameraSighting> lastCameraCheck;
    int lastCameraGroupChecked;
    bool lastCameraFeedUnavailable;

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
    void toggleGate(int roomId);
    void openGate(int roomId);
    void riskScan();
    void updateProbMap();
};

// Difficulty parameters
void setDifficultyParams(GameState &gs, Difficulty diff);
bool isBlockedEdge(const GameState& gs, int from, int to);

#endif
