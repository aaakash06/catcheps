#ifndef GAME_H
#define GAME_H

#include <string>
#include <vector>
#include <map>
#include "map.h"
#include "enemy.h"

enum GameStatus { STATUS_PLAYING, STATUS_WIN, STATUS_LOSE_ENEMY, STATUS_LOSE_POWER };
enum SignalStrength { SIGNAL_NONE, SIGNAL_WEAK, SIGNAL_STRONG };

struct GameState {
    Difficulty difficulty;
    int currentNight;
    int totalNights;
    int turn;
    int maxTurns;        // turns per night (6AM arrival)
    int power;
    int maxPower;
    int cameraPowerCost;      // Quick Sweep cost
    int deepScanPowerCost;    // Deep Scan cost
    int lurePowerCost;
    int doorPowerCost;        // Closed-gate upkeep per active turn
    int signalDecayTurns;
    int audioProbDistance3;
    int audioProbDistance2;
    int audioProbDistance1;
    int moveCloserProb;
    int moveSidewaysProb;
    int moveRandomProb;
    int lureCooldownMax;
    int currentLureCooldown;
    bool leftGateClosed;
    bool rightGateClosed;

    GameMap gameMap;
    Enemy enemy;

    int lastKnownEnemyRoom;
    int lastKnownEnemyTurn;
    std::map<int, double> probMap;
    std::vector<std::string> lastScanOutput;

    GameStatus status;
    std::string statusMessage;

    // Night log
    std::vector<std::string> eventLog;

    GameState();
    void init(Difficulty diff);
    void newNight(bool preserveEventLog = false);
    void doTurn(int action, int param);
    void enemyTurn();
    void checkConditions();
    void quickSweep(int group);
    void deepScan(int roomId);
    void playLure(int roomId);
    void toggleGate(int roomId);
    void updateProbMap();
};

// Difficulty parameters
void setDifficultyParams(GameState &gs, Difficulty diff);
bool isBlockedEdge(const GameState& gs, int from, int to);
SignalStrength getSignalStrength(const GameState &gs);
std::string getSignalDisplay(const GameState &gs);

#endif
