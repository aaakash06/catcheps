#include "ui.h"
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cstdio>

void clearScreen() {
    // ANSI escape to clear screen and move cursor to top
    std::cout << "\033[2J\033[1;1H";
}

static std::string dangerBar(double val) {
    int filled = (int)(val * 10);
    if (filled > 10) filled = 10;
    std::string bar = "[";
    for (int i = 0; i < filled; i++) bar += "#";
    for (int i = filled; i < 10; i++) bar += ".";
    bar += "]";
    return bar;
}

void drawGame(const GameState &gs) {
    clearScreen();

    // Header
    std::cout << "========================================\n";
    std::cout << "  NIGHT " << gs.currentNight << " / " << gs.totalNights
              << "     TURN " << gs.turn << " / " << gs.maxTurns
              << "     POWER: " << gs.power << " / " << gs.maxPower << "\n";

    // Power bar
    double pct = (gs.maxPower > 0) ? (double)gs.power / gs.maxPower : 0;
    std::cout << "  Power: " << dangerBar(pct) << " " << (int)(pct*100) << "%\n";

    // Office status
    bool officeSafe = (gs.enemy.currentRoom != gs.gameMap.officeId);
    std::cout << "  Office: " << (officeSafe ? "SAFE" : "BREACHED!") << "\n";
    std::cout << "========================================\n\n";

    // Camera feed from last check
    drawCameraFeed(gs);

    // Map
    drawMap(gs);

    // Event log
    if (!gs.eventLog.empty()) {
        std::cout << "\n--- Events ---\n";
        for (auto &e : gs.eventLog)
            std::cout << "  " << e << "\n";
    }

    // Lure cooldown
    if (gs.currentLureCooldown > 0)
        std::cout << "\n  [Lure cooldown: " << gs.currentLureCooldown << " turns]\n";

    // Actions
    std::cout << "\n========================================\n";
    std::cout << "  ACTIONS:\n";
    std::cout << "  1. Check camera\n";
    std::cout << "  2. Play sound lure\n";
    std::cout << "  3. Close door\n";
    std::cout << "  4. Restore door\n";
    std::cout << "  5. Risk scan\n";
    std::cout << "  6. End turn (wait)\n";
    std::cout << "  0. Save & Quit\n";
    std::cout << "========================================\n";
}

void drawMainMenu() {
    clearScreen();
    std::cout << "========================================\n";
    std::cout << "         C A M E R A   W A T C H\n";
    std::cout << "========================================\n";
    std::cout << "\n";
    std::cout << "  You are a night security guard trapped\n";
    std::cout << "  in a building. An enemy stalks through\n";
    std::cout << "  the rooms. Monitor cameras, use sound\n";
    std::cout << "  lures, close doors, and survive until\n";
    std::cout << "  6 AM each night.\n";
    std::cout << "\n";
    std::cout << "  1. New Game\n";
    std::cout << "  2. Load Game\n";
    std::cout << "  3. How to Play\n";
    std::cout << "  0. Quit\n";
    std::cout << "\n========================================\n";
}

void drawDifficultyMenu() {
    clearScreen();
    std::cout << "========================================\n";
    std::cout << "        SELECT DIFFICULTY\n";
    std::cout << "========================================\n\n";
    std::cout << "  1. Easy   - 8 rooms, 3 nights, high power\n";
    std::cout << "  2. Normal - 10 rooms, 4 nights, moderate power\n";
    std::cout << "  3. Hard   - 12 rooms, 5 nights, low power\n";
    std::cout << "\n========================================\n";
}

void drawMap(const GameState &gs) {
    std::cout << "  Map Status:\n";
    std::cout << "  +--------+-----------------------------+----------+\n";
    std::cout << "  | Room   | Name                        | Status   |\n";
    std::cout << "  +--------+-----------------------------+----------+\n";

    for (auto &r : gs.gameMap.rooms) {
        std::string status = roomStatusChar(r, gs.enemy.currentRoom, gs.lastKnownEnemyRoom);

        // Pad name to 27 chars
        std::string name = r.name;
        while ((int)name.size() < 27) name += " ";

        printf("  | %-6d | %s | %-8s |\n", r.id, name.c_str(), status.c_str());
    }
    std::cout << "  +--------+-----------------------------+----------+\n";
    std::cout << "  Legend: [SAFE]=Office  [!]=Enemy here  [?]=Last seen  [X]=Door closed  [ ]=Clear\n";
}

void drawCameraFeed(const GameState &gs) {
    if (gs.lastCameraCheck.empty()) return;

    std::cout << "  --- Camera " << cameraGroupLabel(gs.lastCameraGroupChecked) << " Feed ---\n";
    for (auto &cs : gs.lastCameraCheck) {
        std::string roomName = gs.gameMap.rooms[cs.roomId].name;
        if (cs.enemyPresent)
            std::cout << "  [!!] " << roomName << ": " << cs.status << "\n";
        else
            std::cout << "  [OK] " << roomName << ": " << cs.status << "\n";
    }
    std::cout << "\n";
}

void drawRiskScan(const GameState &) {
    // Risk scan output is handled via event log
}

void drawEndGame(const GameState &gs) {
    clearScreen();
    std::cout << "========================================\n";
    if (gs.status == STATUS_WIN) {
        std::cout << "          Y O U   W I N !\n";
        std::cout << "========================================\n";
        std::cout << "  You survived all " << gs.totalNights << " nights!\n";
    } else if (gs.status == STATUS_LOSE_ENEMY) {
        std::cout << "        G A M E   O V E R\n";
        std::cout << "========================================\n";
        std::cout << "  The enemy reached the office on\n";
        std::cout << "  Night " << gs.currentNight << ", Turn " << gs.turn << ".\n";
    } else if (gs.status == STATUS_LOSE_POWER) {
        std::cout << "        G A M E   O V E R\n";
        std::cout << "========================================\n";
        std::cout << "  Power ran out on Night " << gs.currentNight << ".\n";
    }
    std::cout << "\n  " << gs.statusMessage << "\n";
    std::cout << "\n========================================\n";
    std::cout << "  Press Enter to return to main menu...\n";
    std::cin.ignore();
    std::cin.get();
}

void drawHelp() {
    clearScreen();
    std::cout << "========================================\n";
    std::cout << "         HOW TO PLAY\n";
    std::cout << "========================================\n\n";
    std::cout << "  OBJECTIVE:\n";
    std::cout << "  Survive each night until 6 AM by monitoring\n";
    std::cout << "  cameras and redirecting the enemy away from\n";
    std::cout << "  the office (Room 0).\n\n";
    std::cout << "  ACTIONS:\n";
    std::cout << "  Check camera - View rooms in a camera group.\n";
    std::cout << "                 Costs power. Reveals enemy if\n";
    std::cout << "                 they are in that group's rooms.\n\n";
    std::cout << "  Play sound   - Lures enemy toward a camera\n";
    std::cout << "                 group's area. Has cooldown.\n\n";
    std::cout << "  Close door   - Blocks enemy movement through\n";
    std::cout << "                 that room. Drains power each turn.\n\n";
    std::cout << "  Restore door - Reopens a closed door.\n\n";
    std::cout << "  Risk scan    - Shows articulation points,\n";
    std::cout << "                 bridges, danger levels.\n\n";
    std::cout << "  End turn     - Do nothing and pass the turn.\n\n";
    std::cout << "  TIPS:\n";
    std::cout << "  - Use BFS risk scans to find chokepoints.\n";
    std::cout << "  - Close doors at articulation points to cut\n";
    std::cout << "    off paths to the office.\n";
    std::cout << "  - Lure the enemy to the far side of the map.\n";
    std::cout << "  - Manage power carefully!\n";
    std::cout << "\n========================================\n";
    std::cout << "  Press Enter to go back...\n";
    std::cin.ignore();
    std::cin.get();
}

int promptInt(const std::string &msg, int lo, int hi) {
    int val;
    std::cout << msg;
    std::cin >> val;
    if (std::cin.fail() || val < lo || val > hi) {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        return -1;
    }
    return val;
}

void pause(const std::string &msg) {
    std::cout << msg;
    std::cin.ignore();
    std::cin.get();
}
