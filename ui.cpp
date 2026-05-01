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

static std::string lureStatusText(const GameState &gs) {
    if (gs.enemy.lureTimer > 0 &&
        gs.enemy.lureTarget >= 0 &&
        gs.enemy.lureTarget < gs.gameMap.totalRooms) {
        std::stringstream ss;
        ss << "Active at " << gs.gameMap.rooms[gs.enemy.lureTarget].abbrev
           << " (" << gs.enemy.lureTimer << " turns left)";
        return ss.str();
    }
    return "None";
}

static std::string lureCooldownText(const GameState &gs) {
    if (gs.currentLureCooldown > 0) {
        std::stringstream ss;
        ss << "available in " << gs.currentLureCooldown << " turns";
        return ss.str();
    }
    return "Ready";
}

// Cursor navigation uses a simple spatial layout derived from the rendered map template.
struct MapPos { int id; int col; int row; };

static std::vector<MapPos> getFallbackLayout(int totalRooms) {
    if (totalRooms <= 8) {
        return {
            {7, 14, 0},  // HW
            {5, 32, 0},  // CYM
            {3,  0, 2},  // KAD
            {6, 16, 2},  // HC
            {0,  0, 4},  // MB
            {2, 16, 4},  // LIB
            {1, 32, 4},  // KKL
            {4,  0, 6},  // KNOW
        };
    }

    if (totalRooms <= 10) {
        return {
            {8, 26, 0},  // MW
            {7, 12, 2},  // HW
            {5, 30, 2},  // CYM
            {9, 48, 2},  // RM
            {3,  0, 4},  // KAD
            {6, 16, 4},  // HC
            {2, 30, 4},  // LIB
            {1, 46, 4},  // KKL
            {0,  0, 6},  // MB
            {4,  0, 8},  // KNOW
        };
    }

    return {
        {8,   7,  2},  // MW
        {10, 32,  2},  // RHS
        {12, 57,  2},  // JL
        {7,   7,  7},  // HW
        {5,  32,  7},  // CYM
        {11, 57,  7},  // RR
        {3,   7, 12},  // KAD
        {6,  32, 12},  // HC
        {9,  57, 12},  // RM
        {2,  32, 16},  // LIB
        {4,  57, 16},  // KNOW
        {0,  32, 22},  // MB
        {1,  82, 16},  // KKL, hidden on Hard 3-gate map but kept navigable if connected later
    };
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

static std::vector<MapPos> getLayoutFromTemplate(const GameState &gs) {
    std::vector<std::string> mapLines = loadMapTemplate(mapTemplateFileForDifficulty(gs.difficulty));
    std::vector<MapPos> layout;
    std::vector<MapPos> fallback = getFallbackLayout(gs.gameMap.totalRooms);
    std::map<int, MapPos> fallbackById;

    for (const auto &pos : fallback)
        fallbackById[pos.id] = pos;

    for (const Room &room : gs.gameMap.rooms) {
        std::string placeholder = "{" + padRoomCode(room.abbrev) + "}";
        bool found = false;

        for (size_t row = 0; row < mapLines.size() && !found; row++) {
            std::string::size_type col = mapLines[row].find(placeholder);
            if (col != std::string::npos) {
                layout.push_back({room.id, (int)col, (int)row});
                found = true;
            }
        }

        // Some difficulty maps intentionally hide isolated/unused graph rooms.
        // Keep template positions for visible rooms instead of falling back wholesale.
        if (!found && fallbackById.count(room.id))
            layout.push_back(fallbackById[room.id]);
    }

    return layout.empty() ? fallback : layout;
}

static void replaceAll(std::string &line, const std::string &target, const std::string &replacement) {
    if (target.empty()) return;
    std::string::size_type pos = 0;
    while ((pos = line.find(target, pos)) != std::string::npos) {
        line.replace(pos, target.size(), replacement);
        pos += replacement.size();
    }
}

static std::string renderRoom(const GameState &gs, const Room &room, bool isCursor) {
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
        std::string signalColor = isCursor ? std::string(CLR_CYAN CLR_BOLD)
                                           : std::string(CLR_RED);
        return signalColor + "[!!  ]" + CLR_RESET;
    } else if (weakSignal) {
        color = CLR_YELLOW;
        left = "[";
        right = "]";
        padded = "?   ";
    }

    if (isCursor)
        color = CLR_CYAN CLR_BOLD;

    return color + left + padded + right + CLR_RESET;
}

static std::string renderGateMarker(bool closed) {
    if (closed)
        return std::string(CLR_RED) + "[XX]" + CLR_RESET;
    return std::string(CLR_GREEN) + "[--]" + CLR_RESET;
}

static bool roomConnectsToOffice(const GameState &gs, int roomId) {
    if (roomId < 0 || roomId >= gs.gameMap.totalRooms)
        return false;

    const std::vector<int> &neighbors = gs.gameMap.rooms[roomId].neighbors;
    return std::find(neighbors.begin(), neighbors.end(), gs.gameMap.officeId) != neighbors.end();
}

void drawMap(const GameState &gs, int cursorRoom) {
    std::vector<std::string> mapLines = loadMapTemplate(mapTemplateFileForDifficulty(gs.difficulty));
    for (std::string line : mapLines) {
        bool hardSwappedOfficeSides = (gs.difficulty == HARD);
        bool leftVisualGateClosed = hardSwappedOfficeSides ? gs.rightGateClosed
                                                           : gs.leftGateClosed;
        bool rightVisualGateClosed = hardSwappedOfficeSides ? gs.leftGateClosed
                                                            : gs.rightGateClosed;
        replaceAll(line, "{LG}", renderGateMarker(leftVisualGateClosed));
        replaceAll(line, "{RG}", renderGateMarker(rightVisualGateClosed));
        replaceAll(line, "{CG}", renderGateMarker(gs.centerGateClosed));
        replaceAll(line, "[LG]", renderGateMarker(leftVisualGateClosed));
        replaceAll(line, "[RG]", renderGateMarker(rightVisualGateClosed));
        replaceAll(line, "[CG]", renderGateMarker(gs.centerGateClosed));
        for (const Room &room : gs.gameMap.rooms) {
            replaceAll(line, "{" + padRoomCode(room.abbrev) + "}",
                       renderRoom(gs, room, room.id == cursorRoom));
        }
        std::cout << "  " << line << "\n";
    }

    std::cout << CLR_DIM << "  Office Gates: ";
    if (roomConnectsToOffice(gs, 3)) {
        std::cout << "KAD " << renderGateMarker(gs.rightGateClosed)
                  << CLR_DIM << "  ";
    }
    if (roomConnectsToOffice(gs, 2)) {
        std::cout << "LIB " << renderGateMarker(gs.centerGateClosed)
                  << CLR_DIM << "  ";
    }
    if (roomConnectsToOffice(gs, 4)) {
        std::cout << "KNOW " << renderGateMarker(gs.leftGateClosed);
    }
    std::cout << CLR_RESET << "\n";

    std::cout << CLR_DIM << "  "
              << CLR_GREEN << "<MB>" << CLR_RESET << CLR_DIM << "=Office  "
              << CLR_RED << "[!!]" << CLR_RESET << CLR_DIM << "=Strong Signal  "
              << CLR_YELLOW << "[?]" << CLR_RESET << CLR_DIM << "=Weak Signal  "
              << CLR_GREEN << "[--]" << CLR_RESET << CLR_DIM << "=Office Gate Open  "
              << CLR_RED << "[XX]" << CLR_RESET << CLR_DIM << "=Office Gate Closed  "
              << CLR_CYAN << "[CUR]" << CLR_RESET << CLR_DIM << "=Cursor"
              << CLR_RESET << "\n";
}

void drawCameraFeed(const GameState &gs) {
    if (gs.lastScanOutput.empty()) return;

    std::cout << CLR_BOLD << "  Scan Result:" << CLR_RESET << "\n";
    for (size_t i = 0; i < gs.lastScanOutput.size(); i++)
        std::cout << "  " << CLR_CYAN << gs.lastScanOutput[i] << CLR_RESET << "\n";
    std::cout << "\n";
}

void drawGame(const GameState &gs, int cursorRoom) {
    clearScreen();

    // Header
    std::cout << CLR_BOLD;
    std::cout << "================================================================\n";
    std::cout << "  NIGHT " << gs.currentNight << "/" << gs.totalNights
              << "   TURN " << gs.turn << "/" << gs.maxTurns << "\n";
    std::cout << "  ENERGY: " << (gs.maxPower > 0 ? (gs.power * 100 / gs.maxPower) : 0)
              << "% " << powerBar(gs.power, gs.maxPower) << "\n";
    std::cout << "  DOORS: KAD " << (gs.rightGateClosed ? CLR_RED "CLOSED" : CLR_GREEN "OPEN");
    if (roomConnectsToOffice(gs, 2))
        std::cout << CLR_RESET << "   LIB " << (gs.centerGateClosed ? CLR_RED "CLOSED" : CLR_GREEN "OPEN");
    std::cout << CLR_RESET << "   KNOW " << (gs.leftGateClosed ? CLR_RED "CLOSED" : CLR_GREEN "OPEN")
              << CLR_RESET << "\n";
    std::cout << "  LURE: " << CLR_CYAN << lureStatusText(gs) << CLR_RESET
              << "   NEXT LURE: " << CLR_YELLOW << lureCooldownText(gs) << CLR_RESET << "\n";
    std::cout << "  LAST KNOWN SIGNAL: " << CLR_YELLOW << getSignalDisplay(gs) << CLR_RESET << "\n";
    std::cout << "================================================================\n";
    std::cout << CLR_RESET;

    // Camera feed
    drawCameraFeed(gs);

    // Spatial Map
    drawMap(gs, cursorRoom);

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
              << CLR_CYAN << "[ENTER]" CLR_RESET << CLR_BOLD " Deep Scan selected building\n";
    std::cout << CLR_CYAN << "  [G]" CLR_RESET << CLR_BOLD " Toggle gate at cursor  "
              << CLR_CYAN << "[L]" CLR_RESET << CLR_BOLD " Use Lure at cursor  "
              << CLR_CYAN << "[W]" CLR_RESET << CLR_BOLD " Wait / Listen  "
              << CLR_CYAN << "[Q]" CLR_RESET << CLR_BOLD " Quit  "
              << CLR_CYAN << "[H]" CLR_RESET << CLR_BOLD " Help\n";
    std::cout << "  Move: " CLR_CYAN "Arrow Keys" CLR_RESET
              << "   Sweep Menu: " CLR_CYAN "Number Keys + Enter" CLR_RESET "\n";
    if (cursorRoom >= 0 && cursorRoom < gs.gameMap.totalRooms) {
        const Room &curRoom = gs.gameMap.rooms[cursorRoom];
        std::cout << "  Cursor: " << CLR_CYAN << curRoom.abbrev << CLR_RESET
                  << " (" << curRoom.name << ")";
        if (cursorRoom == 3)
            std::cout << (gs.rightGateClosed ? CLR_RED " [KAD GATE CLOSED]" : CLR_DIM " [KAD GATE OPEN]") << CLR_RESET;
        else if (cursorRoom == 2 && roomConnectsToOffice(gs, 2))
            std::cout << (gs.centerGateClosed ? CLR_RED " [LIB GATE CLOSED]" : CLR_DIM " [LIB GATE OPEN]") << CLR_RESET;
        else if (cursorRoom == 4)
            std::cout << (gs.leftGateClosed ? CLR_RED " [KNOW GATE CLOSED]" : CLR_DIM " [KNOW GATE OPEN]") << CLR_RESET;
        else
            std::cout << CLR_DIM << " [No gate here]" << CLR_RESET;
        std::cout << "\n";
    }
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
    std::cout << CLR_CYAN << "  [1]" CLR_RESET " " CLR_BOLD "Easy" CLR_RESET "   - 8 buildings, 2 nights, forgiving scans\n";
    std::cout << CLR_CYAN << "  [2]" CLR_RESET " " CLR_BOLD "Normal" CLR_RESET " - 10 buildings, 2 nights, balanced routes\n";
    std::cout << CLR_CYAN << "  [3]" CLR_RESET " " CLR_BOLD "Hard" CLR_RESET "   - 12 active buildings, 2 nights, three gates\n";
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
    std::cout << "  Arrow Keys        - Move cursor between connected buildings\n";
    std::cout << "  Enter             - Deep Scan the selected building\n";
    std::cout << "  " CLR_CYAN "[A]" CLR_RESET " - Quick Sweep a camera cluster\n";
    std::cout << "  " CLR_CYAN "[G]" CLR_RESET " - Toggle the gate at the selected building\n";
    std::cout << "  " CLR_CYAN "[L]" CLR_RESET " - Place a lure at the selected building\n";
    std::cout << "  " CLR_CYAN "[W]" CLR_RESET " - Wait / listen\n";
    std::cout << "  " CLR_CYAN "[H]" CLR_RESET " - Open this help screen\n";
    std::cout << "  " CLR_CYAN "[Q]" CLR_RESET " - Save & quit\n\n";
    std::cout << CLR_BOLD << "  CAMERA RINGS:\n" CLR_RESET;
    std::cout << "  - Quick Sweep reports movement in one cluster only.\n";
    std::cout << "  - Deep Scan checks the building under the cursor.\n";
    std::cout << "  - MB is never part of a camera cluster.\n";
    std::cout << "  - Signals decay after a few turns depending on difficulty.\n\n";
    std::cout << CLR_BOLD << "  TIPS:\n" CLR_RESET;
    std::cout << "  - Quick Sweep is cheap but only tells you whether a cluster is active.\n";
    std::cout << "  - Deep Scan is expensive, but it gives an exact last known signal.\n";
    std::cout << "  - Gate rooms next to MB control the office entrances.\n";
    std::cout << "  - Closed gates drain upkeep every turn, even while waiting.\n";
    std::cout << "  - Audio hints help, but they are not a substitute for scanning.\n";
    std::cout << "  - Lure: place a distraction at the selected building. If the enemy is nearby,\n";
    std::cout << "    it may move toward the lure instead of the office.\n";
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

int getNextRoomNav(const GameState &gs, int currentCursor, int direction) {
    if (gs.gameMap.rooms.empty()) return 0;
    auto layout = getLayoutFromTemplate(gs);
    MapPos *cur = nullptr;
    for (auto &p : layout) {
        if (p.id == currentCursor) { cur = &p; break; }
    }
    if (!cur) return 0;

    int bestId = currentCursor;
    int bestScore = 999999;
    std::map<int, MapPos> posById;
    for (const auto &p : layout)
        posById[p.id] = p;

    for (int neighborId : gs.gameMap.rooms[currentCursor].neighbors) {
        if (!posById.count(neighborId)) continue;
        const auto &p = posById[neighborId];
        int dr = p.row - cur->row;
        int dc = p.col - cur->col;

        bool valid = false;
        if (direction == 0 && dr < 0) valid = true;
        if (direction == 1 && dr > 0) valid = true;
        if (direction == 2 && dc < 0) valid = true;
        if (direction == 3 && dc > 0) valid = true;

        if (valid) {
            int score = abs(dr) * 100 + abs(dc);
            if (score < bestScore) {
                bestScore = score;
                bestId = neighborId;
            }
        }
    }
    return bestId;
}
