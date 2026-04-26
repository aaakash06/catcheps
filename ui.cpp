#include "ui.h"
#include "terminal.h"
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <vector>

// ANSI color codes
#define CLR_RESET   "\033[0m"
#define CLR_RED     "\033[31m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_BLUE    "\033[34m"
#define CLR_MAGENTA "\033[35m"
#define CLR_CYAN    "\033[36m"
#define CLR_WHITE   "\033[37m"
#define CLR_BOLD    "\033[1m"
#define CLR_DIM     "\033[2m"
#define CLR_BG_RED  "\033[41m"
#define CLR_BG_BLUE "\033[44m"

void clearScreen() {
    std::cout << "\033[2J\033[1;1H";
}

static std::string powerBar(int power, int maxPower) {
    int filled = (maxPower > 0) ? (power * 10 / maxPower) : 0;
    if (filled > 10) filled = 10;
    if (filled < 0) filled = 0;
    std::string bar = "[";
    for (int i = 0; i < filled; i++) bar += "#";
    for (int i = filled; i < 10; i++) bar += ".";
    bar += "]";
    return bar;
}

static std::string padRoomCode(const std::string &code) {
    std::string padded = code;
    while ((int)padded.size() < 4)
        padded += ' ';
    return padded;
}

static std::vector<std::string> loadMapTemplate(const std::string &filename) {
    std::ifstream in(filename.c_str());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line))
        lines.push_back(line);
    if (lines.empty())
        lines.push_back("Map template unavailable: " + filename);
    return lines;
}

static std::string mapTemplateFileForDifficulty(Difficulty diff) {
    if (diff == NORMAL) return "maps/map_normal.txt";
    if (diff == HARD) return "maps/map_hard.txt";
    return "maps/map_easy.txt";
}

static void replaceAll(std::string &line, const std::string &target, const std::string &replacement) {
    if (target.empty()) return;
    std::string::size_type pos = 0;
    while ((pos = line.find(target, pos)) != std::string::npos) {
        line.replace(pos, target.size(), replacement);
        pos += replacement.size();
    }
}

static std::string renderRoom(const GameState &gs, const Room &room) {
    std::string padded = padRoomCode(room.abbrev);
    SignalStrength signal = getSignalStrength(gs);
    bool strongSignal = (signal == SIGNAL_STRONG && room.id == gs.lastKnownEnemyRoom);
    bool weakSignal = (signal == SIGNAL_WEAK && room.id == gs.lastKnownEnemyRoom);

    std::string color = CLR_DIM;
    std::string left = " ";
    std::string right = " ";

    if (room.isOffice) {
        color = CLR_GREEN;
        left = "<";
        right = ">";
    } else if (strongSignal) {
        return std::string(CLR_RED) + "[!!  ]" + CLR_RESET;
    } else if (weakSignal) {
        color = CLR_YELLOW;
        left = "[";
        right = "]";
        padded = "?   ";
    }

    return color + left + padded + right + CLR_RESET;
}

static std::string renderGateSegment(bool closed) {
    if (closed)
        return std::string(CLR_BLUE) + " XX " + CLR_RESET;
    return std::string(CLR_DIM) + "====" + CLR_RESET;
}

void drawMap(const GameState &gs) {
    std::vector<std::string> mapLines = loadMapTemplate(mapTemplateFileForDifficulty(gs.difficulty));
    for (std::string line : mapLines) {
        bool hardSwappedOfficeSides = (gs.difficulty == HARD);
        replaceAll(line, "{LG}", renderGateSegment(hardSwappedOfficeSides ? gs.rightGateClosed
                                                                          : gs.leftGateClosed));
        replaceAll(line, "{RG}", renderGateSegment(hardSwappedOfficeSides ? gs.leftGateClosed
                                                                          : gs.rightGateClosed));
        for (const Room &room : gs.gameMap.rooms) {
            replaceAll(line, "{" + padRoomCode(room.abbrev) + "}",
                       renderRoom(gs, room));
        }
        std::cout << "  " << line << "\n";
    }

    std::cout << CLR_DIM << "  Office Gates: "
              << "KNOW " << (gs.leftGateClosed ? std::string(CLR_BLUE) + "[XX]" + CLR_RESET
                                               : std::string(CLR_DIM) + "open" + CLR_RESET)
              << CLR_DIM << "  KAD "
              << (gs.rightGateClosed ? std::string(CLR_BLUE) + "[XX]" + CLR_RESET
                                     : std::string(CLR_DIM) + "open" + CLR_RESET)
              << CLR_RESET << "\n";

    std::cout << CLR_DIM << "  "
              << CLR_GREEN << "<MB>" << CLR_RESET << CLR_DIM << "=Office  "
              << CLR_RED << "[!!]" << CLR_RESET << CLR_DIM << "=Strong Signal  "
              << CLR_YELLOW << "[?]" << CLR_RESET << CLR_DIM << "=Weak Signal  "
              << CLR_BLUE << "[XX]" << CLR_RESET << CLR_DIM << "=Office Gate Closed"
              << CLR_RESET << "\n";
}

void drawCameraFeed(const GameState &gs) {
    if (gs.lastScanOutput.empty()) return;

    std::cout << CLR_BOLD << "  Scan Result:" << CLR_RESET << "\n";
    for (size_t i = 0; i < gs.lastScanOutput.size(); i++)
        std::cout << "  " << CLR_CYAN << gs.lastScanOutput[i] << CLR_RESET << "\n";
    std::cout << "\n";
}

void drawGame(const GameState &gs) {
    clearScreen();

    // Header
    std::cout << CLR_BOLD;
    std::cout << "================================================================\n";
    std::cout << "  NIGHT " << gs.currentNight << "/" << gs.totalNights
              << "   TURN " << gs.turn << "/" << gs.maxTurns << "\n";
    std::cout << "  ENERGY: " << (gs.maxPower > 0 ? (gs.power * 100 / gs.maxPower) : 0)
              << "% " << powerBar(gs.power, gs.maxPower) << "\n";
    std::cout << "  DOORS: KNOW " << (gs.leftGateClosed ? CLR_BLUE "CLOSED" : CLR_GREEN "OPEN")
              << CLR_RESET << "   KAD " << (gs.rightGateClosed ? CLR_BLUE "CLOSED" : CLR_GREEN "OPEN")
              << CLR_RESET;
    if (gs.currentLureCooldown > 0)
        std::cout << CLR_YELLOW << "   Lure CD: " << gs.currentLureCooldown << CLR_RESET;
    std::cout << "\n";
    std::cout << "  LAST KNOWN SIGNAL: " << CLR_YELLOW << getSignalDisplay(gs) << CLR_RESET << "\n";
    std::cout << "================================================================\n";
    std::cout << CLR_RESET;

    // Camera feed
    drawCameraFeed(gs);

    // Spatial Map
    drawMap(gs);

    // Event log
    if (!gs.eventLog.empty()) {
        std::cout << "\n" CLR_BOLD "  Events:" CLR_RESET "\n";
        for (auto &e : gs.eventLog)
            std::cout << "  " CLR_DIM ">" CLR_RESET " " << e << "\n";
    }

    // Action bar
    std::cout << "\n" CLR_BOLD;
    std::cout << "================================================================\n";
    std::cout << CLR_CYAN << "  [A]" CLR_RESET << CLR_BOLD " Quick Sweep  "
              << CLR_CYAN << "[S]" CLR_RESET << CLR_BOLD " Deep Scan  "
              << CLR_CYAN << "[Z]" CLR_RESET << CLR_BOLD " Close KNOW gate  "
              << CLR_CYAN << "[X]" CLR_RESET << CLR_BOLD " Close KAD gate  "
              << CLR_CYAN << "[C]" CLR_RESET << CLR_BOLD " Close both gates\n";
    std::cout << CLR_CYAN << "  [L]" CLR_RESET << CLR_BOLD " Use Lure  "
              << CLR_CYAN << "[W]" CLR_RESET << CLR_BOLD " Wait / Listen  "
              << CLR_CYAN << "[Q]" CLR_RESET << CLR_BOLD " Quit  "
              << CLR_CYAN << "[H]" CLR_RESET << CLR_BOLD " Help\n";
    std::cout << "  Camera clusters and Deep Scan targets use numbered menus.\n";
    std::cout << "  Menus: " CLR_CYAN "Number Keys + Enter" CLR_RESET "\n";
    std::cout << "================================================================\n";
    std::cout << CLR_RESET;
}

void drawMainMenu() {
    clearScreen();
    std::cout << CLR_BOLD CLR_CYAN;
    std::cout << "================================================================\n";
    std::cout << "       C A M E R A   W A T C H  -  H K U   C A M P U S\n";
    std::cout << "================================================================\n";
    std::cout << CLR_RESET;
    std::cout << "\n";
    std::cout << "  You are a night security guard at the\n";
    std::cout << "  University of Hong Kong. An intruder stalks\n";
    std::cout << "  through the campus buildings. Monitor cameras,\n";
    std::cout << "  use sound lures, close doors, and survive\n";
    std::cout << "  until 6 AM each night.\n";
    std::cout << "\n";
    std::cout << CLR_BOLD;
    std::cout << "  " CLR_CYAN "[1]" CLR_RESET CLR_BOLD " New Game\n";
    std::cout << "  " CLR_CYAN "[2]" CLR_RESET CLR_BOLD " Load Game\n";
    std::cout << "  " CLR_CYAN "[3]" CLR_RESET CLR_BOLD " How to Play\n";
    std::cout << "  " CLR_CYAN "[0]" CLR_RESET CLR_BOLD " Quit\n";
    std::cout << CLR_RESET;
    std::cout << "\n================================================================\n";
}

void drawDifficultyMenu() {
    clearScreen();
    std::cout << CLR_BOLD;
    std::cout << "================================================================\n";
    std::cout << "                    SELECT DIFFICULTY\n";
    std::cout << "================================================================\n\n";
    std::cout << CLR_CYAN << "  [1]" CLR_RESET " " CLR_BOLD "Easy" CLR_RESET "   - 8 buildings, 3 nights, high power\n";
    std::cout << CLR_CYAN << "  [2]" CLR_RESET " " CLR_BOLD "Normal" CLR_RESET " - 10 buildings, 4 nights, moderate power\n";
    std::cout << CLR_CYAN << "  [3]" CLR_RESET " " CLR_BOLD "Hard" CLR_RESET "   - 13 buildings, 5 nights, low power\n";
    std::cout << "\n================================================================\n";
}

void drawEndGame(const GameState &gs) {
    clearScreen();
    std::cout << CLR_BOLD;
    std::cout << "================================================================\n";
    if (gs.status == STATUS_WIN) {
        std::cout << CLR_GREEN;
        std::cout << "                Y O U   W I N !\n";
        std::cout << CLR_RESET CLR_BOLD;
        std::cout << "================================================================\n";
        std::cout << "  You survived all " << gs.totalNights << " nights at HKU!\n";
    } else if (gs.status == STATUS_LOSE_ENEMY) {
        std::cout << CLR_RED;
        std::cout << "              G A M E   O V E R\n";
        std::cout << CLR_RESET CLR_BOLD;
        std::cout << "================================================================\n";
        std::cout << "  The intruder reached Main Building on\n";
        std::cout << "  Night " << gs.currentNight << ", Turn " << gs.turn << ".\n";
    } else if (gs.status == STATUS_LOSE_POWER) {
        std::cout << CLR_RED;
        std::cout << "              G A M E   O V E R\n";
        std::cout << CLR_RESET CLR_BOLD;
        std::cout << "================================================================\n";
        std::cout << "  Power ran out on Night " << gs.currentNight << ".\n";
    }
    std::cout << "\n  " << gs.statusMessage << "\n";
    std::cout << "\n================================================================\n";
    std::cout << CLR_RESET;
    std::cout << "  Press Enter to return to main menu...\n";
    // Terminal is already in raw mode while the game loop is running.
    while (true) {
        Key k = getKey();
        if (k == KEY_ENTER)
            break;
    }
}

void drawHelp() {
    clearScreen();
    std::cout << CLR_BOLD;
    std::cout << "================================================================\n";
    std::cout << "                HOW TO PLAY - HKU CAMPUS\n";
    std::cout << "================================================================\n\n";
    std::cout << CLR_RESET;
    std::cout << "  OBJECTIVE:\n";
    std::cout << "  Survive each night until 6 AM by monitoring\n";
    std::cout << "  temporary camera scans and redirecting the intruder away from\n";
    std::cout << "  " CLR_GREEN "Main Building (MB)" CLR_RESET " — your office.\n\n";
    std::cout << CLR_BOLD << "  CONTROLS:\n" CLR_RESET;
    std::cout << "  " CLR_CYAN "[A]" CLR_RESET " - Quick Sweep a camera cluster\n";
    std::cout << "  " CLR_CYAN "[S]" CLR_RESET " - Deep Scan a building from a numbered menu\n";
    std::cout << "  " CLR_CYAN "[Z]" CLR_RESET " - Close the KNOW office gate\n";
    std::cout << "  " CLR_CYAN "[X]" CLR_RESET " - Close the KAD office gate\n";
    std::cout << "  " CLR_CYAN "[C]" CLR_RESET " - Close both office gates\n";
    std::cout << "  " CLR_CYAN "[L]" CLR_RESET " - Use a sound lure in one cluster\n";
    std::cout << "  " CLR_CYAN "[W]" CLR_RESET " - Wait / listen\n";
    std::cout << "  " CLR_CYAN "[H]" CLR_RESET " - Open this help screen\n";
    std::cout << "  " CLR_CYAN "[Q]" CLR_RESET " - Save & quit\n\n";
    std::cout << CLR_BOLD << "  CAMERA RINGS:\n" CLR_RESET;
    std::cout << "  - Quick Sweep reports movement in one cluster only.\n";
    std::cout << "  - Deep Scan checks one exact building from a numbered menu.\n";
    std::cout << "  - MB is never part of a camera cluster.\n";
    std::cout << "  - Signals decay after a few turns depending on difficulty.\n\n";
    std::cout << CLR_BOLD << "  TIPS:\n" CLR_RESET;
    std::cout << "  - Quick Sweep is cheap but only tells you whether a cluster is active.\n";
    std::cout << "  - Deep Scan is expensive, but it gives an exact last known signal.\n";
    std::cout << "  - KAD and KNOW control the two office gates.\n";
    std::cout << "  - Audio hints help, but they are not a substitute for scanning.\n";
    std::cout << "  - Lure the intruder away before sealing MB.\n";
    std::cout << "  - Manage energy carefully!\n";
    std::cout << "\n================================================================\n";
    std::cout << "  Press Enter to go back...\n";
    while (true) {
        Key k = getKey();
        if (k == KEY_ENTER)
            break;
    }
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
