#ifndef GAME_H
#define GAME_H

#include <string>
#include <vector>
#include <map>
#include "map.h"
#include "enemy.h"

enum GameStatus { STATUS_PLAYING, STATUS_WIN, STATUS_LOSE_ENEMY, STATUS_LOSE_POWER };

enum ActionType {
    ACTION_QUICK_SWEEP = 1,
    ACTION_LURE = 2,
    ACTION_CLOSE_DOOR = 3,
    ACTION_RESTORE_DOOR = 4,
    ACTION_DEEP_SCAN = 5,
    ACTION_WAIT = 6
};

struct DifficultySettings {
    int quickSweepCost;
    int deepScanCost;
    int doorCost;
    int lureCost;
    int audioProbDistance3;
    int audioProbDistance2;
    int audioProbDistance1;
    int signalDecayTurns;
    int moveCloserProb;
    int moveSidewaysProb;
    int moveRandomProb;
};

struct CameraSighting {
    int roomId;
    int clusterId;
    bool enemyPresent;
    bool deepScan;
    std::string status;
};

struct CameraSystem {
    int lastKnownRoom;
    int lastKnownCluster;
    int lastDetectedTurn;
    std::string lastSignalLabel;
    bool lastSignalExact;

    CameraSystem();
    void reset();
    void updateLastKnownSignal(int roomId, int clusterId, const std::string &label,
                               bool exact, int currentTurn);
    std::string getSignalDisplay(int currentTurn, int decayTurns) const;
};

struct AudioHintSystem {
    static std::string maybeGenerateHint(const GameMap &map, int enemyRoom,
                                         int distanceToOffice, Difficulty diff,
                                         const DifficultySettings &settings);
};

struct GameState {
    Difficulty difficulty;
    DifficultySettings settings;
    int currentNight;
    int totalNights;
    int turn;
    int maxTurns;        // turns per night (6AM arrival)
    int power;
    int maxPower;
    int cameraPowerCost;
    int quickSweepPowerCost;
    int deepScanPowerCost;
    int lurePowerCost;
    int doorPowerCost;
    int scanPowerCost;
    int lureCooldown;
    int lureCooldownMax;
    int currentLureCooldown;

    GameMap gameMap;
    Enemy enemy;

    int lastKnownEnemyRoom;
    int lastKnownTurn;
    std::map<int, double> probMap;
    std::vector<CameraSighting> lastCameraCheck;
    int lastCameraGroupChecked;
    bool lastCameraWasDeepScan;
    CameraSystem cameraSystem;

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
    void quickSweep(int group);
    void deepScan(int roomId);
    void playLure(int group);
    void closeDoor(int roomId);
    void restoreDoor(int roomId);
    void riskScan();
    void updateProbMap();
};

// Difficulty parameters
void setDifficultyParams(GameState &gs, Difficulty diff);

#endif
