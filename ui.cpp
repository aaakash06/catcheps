#include "ui.h"
#include "terminal.h"
#include <unistd.h>
#include <algorithm>
#include <iostream>
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

// Map layout: each building has a fixed (col, row) position for ASCII rendering
struct MapPos { int id; int col; int row; };

static std::vector<MapPos> getLayout(int totalRooms) {
    // Positions for all 13 buildings on a grid
    // Layout designed so all graph edges are either same-row or 1-row-apart
    // Row 0: MW(8)           RHS(10)  RR(11)  JL(12)
    // Row 1: HW(7)  RM(9)             CYM(5)
    // Row 2: KAD(3) HC(6)   LIB(2)    KKL(1)
    // Row 3: MB(0)           KNOW(4)
    MapPos all[] = {
        {0,   0, 3},  // MB
        {1,  32, 2},  // KKL
        {2,  24, 2},  // LIB
        {3,   0, 2},  // KAD
        {4,  24, 3},  // KNOW
        {5,  24, 1},  // CYM
        {6,   8, 2},  // HC
        {7,   0, 1},  // HW
        {8,   0, 0},  // MW
        {9,   8, 1},  // RM
        {10, 16, 0},  // RHS
        {11, 24, 0},  // RR
        {12, 32, 0},  // JL
    };

    std::vector<MapPos> result;
    for (int i = 0; i < totalRooms; i++) {
        for (auto &p : all) {
            if (p.id == i) {
                result.push_back(p);
                break;
            }
        }
    }
    return result;
}

// Render a building label with status coloring (fixed 6-char width)
static std::string buildingLabel(const Room &r, int enemyRoom, int lastKnown, bool isCursor) {
    std::string label = r.abbrev;
    while ((int)label.size() < 4) label += " ";

    std::string color = CLR_RESET;
    if (isCursor) {
        color = CLR_CYAN CLR_BOLD;
    } else if (r.doorClosed) {
        color = CLR_BLUE;
    } else if (r.id == enemyRoom) {
        color = CLR_RED CLR_BOLD;
    } else if (r.id == lastKnown) {
        color = CLR_YELLOW;
    } else if (r.isOffice) {
        color = CLR_GREEN;
    }

    std::string brackL = "[";
    std::string brackR = "]";
    if (r.isOffice) { brackL = "<"; brackR = ">"; }

    return color + brackL + label + brackR + CLR_RESET;
}

// Pad to a specific column position
static void padTo(int currentCol, int targetCol) {
    for (int i = currentCol; i < targetCol; i++)
        std::cout << ' ';
}

void drawMap(const GameState &gs, int cursorRoom) {
    auto layout = getLayout(gs.gameMap.totalRooms);
    if (layout.empty()) return;

    // Build position lookup
    std::map<int, MapPos> posMap;
    for (auto &p : layout) posMap[p.id] = p;

    int maxRow = 0;
    for (auto &p : layout)
        if (p.row > maxRow) maxRow = p.row;

    // Group buildings by row, sorted by col. Find first/last non-empty rows.
    std::vector<std::vector<int>> rowBldgs(maxRow + 1);
    for (auto &p : layout)
        rowBldgs[p.row].push_back(p.id);
    for (auto &row : rowBldgs)
        std::sort(row.begin(), row.end(), [&](int a, int b) {
            return posMap[a].col < posMap[b].col;
        });

    // Find first and last non-empty rows
    int firstRow = 0, lastRow = maxRow;
    while (firstRow < maxRow && rowBldgs[firstRow].empty()) firstRow++;
    while (lastRow > 0 && rowBldgs[lastRow].empty()) lastRow--;

    // Build a character buffer
    // Only include rows from firstRow to lastRow
    int numRows = lastRow - firstRow + 1;
    int scrW = 42;
    int scrH = numRows * 2 - 1;
    std::vector<std::string> scr(scrH, std::string(scrW, ' '));

    auto setCh = [&](int r, int c, char ch) {
        if (r >= 0 && r < scrH && c >= 0 && c < scrW)
            scr[r][c] = ch;
    };

    // Stamp buildings (as plain text — we'll colorize during output)
    for (auto &p : layout) {
        if (rowBldgs[p.row].empty()) continue;
        int sr = (p.row - firstRow) * 2;
        std::string label = "[";
        std::string abbr = gs.gameMap.rooms[p.id].abbrev;
        while ((int)abbr.size() < 4) abbr += " ";
        if (gs.gameMap.rooms[p.id].isOffice) label = "<";
        for (int i = 0; i < 4 && p.col + i < scrW; i++)
            setCh(sr, p.col + 1 + i, abbr[i]);
        setCh(sr, p.col, label[0]);
        setCh(sr, p.col + 5, gs.gameMap.rooms[p.id].isOffice ? '>' : ']');
    }

    // Stamp connections
    for (auto &r : gs.gameMap.rooms) {
        for (int nb : r.neighbors) {
            if (nb <= r.id) continue;
            if (posMap.find(r.id) == posMap.end() || posMap.find(nb) == posMap.end()) continue;
            MapPos &pa = posMap[r.id], &pb = posMap[nb];

            bool blocked = r.doorClosed || gs.gameMap.rooms[nb].doorClosed;
            char ch = blocked ? 'x' : '-';

            if (pa.row == pb.row) {
                // Horizontal: dashes between buildings
                int minC = pa.col + 6;
                int maxC = pb.col - 1;
                int sr = (pa.row - firstRow) * 2;
                for (int c = minC; c <= maxC; c++)
                    setCh(sr, c, ch);
            } else {
                // Vertical/diagonal connection
                int topRow = std::min(pa.row, pb.row);
                int botRow = std::max(pa.row, pb.row);
                int connRow = (topRow - firstRow) * 2 + 1;
                int colA = pa.col + 2;
                int colB = pb.col + 2;
                char hCh = blocked ? 'x' : '-';
                char vCh = blocked ? 'x' : '|';

                // Only draw if both rows are in the visible range
                if (botRow - topRow == 1 && connRow >= 0 && connRow < scrH) {
                    // Horizontal on connector row if needed
                    if (colA != colB) {
                        int minC = std::min(colA, colB);
                        int maxC = std::max(colA, colB);
                        for (int c = minC; c <= maxC; c++)
                            setCh(connRow, c, hCh);
                    } else {
                        setCh(connRow, colA, vCh);
                    }
                }
            }
        }
    }

    // Output with colorization
    for (int sr = 0; sr < scrH; sr++) {
        std::cout << "  ";
        for (int c = 0; c < scrW; c++) {
            // Check if this position is inside a building label
            int bldgId = -1;
            for (auto &p : layout) {
                int pSr = (p.row - firstRow) * 2;
                if (pSr == sr && c >= p.col && c < p.col + 6) {
                    bldgId = p.id;
                    break;
                }
            }

            if (bldgId >= 0 && c == posMap[bldgId].col) {
                // Start of building — print colored label
                std::cout << buildingLabel(gs.gameMap.rooms[bldgId],
                                            gs.enemy.currentRoom,
                                            gs.lastKnownEnemyRoom,
                                            bldgId == cursorRoom);
                c += 5; // skip the rest of the label
            } else {
                std::cout << scr[sr][c];
            }
        }
        std::cout << "\n";
    }

    // Legend
    std::cout << CLR_DIM << "  " << CLR_GREEN << "<MB>" << CLR_RESET << CLR_DIM
              << "=Office  " << CLR_RED << "[!!]" << CLR_RESET << CLR_DIM
              << "=Enemy  " << CLR_YELLOW << "[??]" << CLR_RESET << CLR_DIM
              << "=Last Seen  " << CLR_BLUE << "[XX]" << CLR_RESET << CLR_DIM
              << "=Door  " << CLR_CYAN << "[CUR]" << CLR_RESET << CLR_DIM
              << "=Cursor" << CLR_RESET << "\n";
}

void drawCameraFeed(const GameState &gs) {
    if (gs.lastCameraCheck.empty()) return;

    std::cout << CLR_BOLD << "  Camera " << cameraGroupLabel(gs.lastCameraGroupChecked)
              << " Feed:" << CLR_RESET << "\n";
    for (auto &cs : gs.lastCameraCheck) {
        std::string roomName = gs.gameMap.rooms[cs.roomId].abbrev;
        if (cs.enemyPresent)
            std::cout << "  " << CLR_RED << "[!!] " << roomName << ": " << cs.status << CLR_RESET << "\n";
        else
            std::cout << "  " << CLR_GREEN << "[OK] " << roomName << ": " << cs.status << CLR_RESET << "\n";
    }
    std::cout << "\n";
}

void drawGame(const GameState &gs, int cursorRoom) {
    clearScreen();

    // Header
    std::cout << CLR_BOLD;
    std::cout << "================================================================\n";
    std::cout << "  NIGHT " << gs.currentNight << "/" << gs.totalNights
              << "   TURN " << gs.turn << "/" << gs.maxTurns
              << "   POWER: " << gs.power << "/" << gs.maxPower
              << " " << powerBar(gs.power, gs.maxPower) << "\n";
    bool officeSafe = (gs.enemy.currentRoom != gs.gameMap.officeId);
    std::cout << "  OFFICE: " << (officeSafe ? CLR_GREEN "SAFE" : CLR_RED "BREACHED!") << CLR_RESET;
    if (gs.currentLureCooldown > 0)
        std::cout << CLR_YELLOW << "   Lure CD: " << gs.currentLureCooldown << CLR_RESET;
    std::cout << "\n";
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
    std::cout << CLR_CYAN << "  [C]" CLR_RESET << CLR_BOLD "amera  "
              << CLR_CYAN << "[L]" CLR_RESET << CLR_BOLD "ure  "
              << CLR_CYAN << "[D]" CLR_RESET << CLR_BOLD "oor  "
              << CLR_CYAN << "[R]" CLR_RESET << CLR_BOLD "estore  "
              << CLR_CYAN << "[S]" CLR_RESET << CLR_BOLD "can  "
              << CLR_CYAN << "[E]" CLR_RESET << CLR_BOLD "nd turn  "
              << CLR_CYAN << "[Q]" CLR_RESET << CLR_BOLD "uit  "
              << CLR_CYAN << "[H]" CLR_RESET << CLR_BOLD "elp\n";

    // Cursor info
    if (cursorRoom >= 0 && cursorRoom < gs.gameMap.totalRooms) {
        auto &curRoom = gs.gameMap.rooms[cursorRoom];
        std::cout << "  Cursor: " << CLR_CYAN << curRoom.abbrev << CLR_RESET
                  << " (" << curRoom.name << ")";
        if (curRoom.doorClosed) std::cout << CLR_BLUE " [DOOR CLOSED]" CLR_RESET;
    }
    std::cout << "\n";
    std::cout << "  Navigate: " CLR_CYAN "Arrow Keys" CLR_RESET "   Select: " CLR_CYAN "Enter" CLR_RESET "\n";
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

void drawRiskScan(const GameState &) {
    // Risk scan output is handled via event log
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
    initTerminal();
    // Wait for Enter
    char c;
    while (read(0, &c, 1) == 1 && c != '\n') {}
    restoreTerminal();
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
    std::cout << "  cameras and redirecting the intruder away from\n";
    std::cout << "  " CLR_GREEN "Main Building (MB)" CLR_RESET " — your office.\n\n";
    std::cout << CLR_BOLD << "  CONTROLS:\n" CLR_RESET;
    std::cout << "  Arrow Keys - Move cursor on map\n";
    std::cout << "  Enter      - Close door at cursor\n";
    std::cout << "  " CLR_CYAN "C" CLR_RESET " - Check camera group\n";
    std::cout << "  " CLR_CYAN "L" CLR_RESET " - Play sound lure at group\n";
    std::cout << "  " CLR_CYAN "D" CLR_RESET " - Close door at cursor building\n";
    std::cout << "  " CLR_CYAN "R" CLR_RESET " - Restore door at cursor building\n";
    std::cout << "  " CLR_CYAN "S" CLR_RESET " - Risk scan (analyze map)\n";
    std::cout << "  " CLR_CYAN "E" CLR_RESET " - End turn (wait)\n";
    std::cout << "  " CLR_CYAN "Q" CLR_RESET " - Save & quit\n\n";
    std::cout << CLR_BOLD << "  CAMERA GROUPS:\n" CLR_RESET;
    std::cout << "  " CLR_YELLOW "Upper" CLR_RESET " - MW, RM, RHS, RR, JL\n";
    std::cout << "  " CLR_YELLOW "Central" CLR_RESET " - HC, HW, CYM\n";
    std::cout << "  " CLR_YELLOW "Lower" CLR_RESET " - MB, KKL, LIB, KAD, KNOW\n\n";
    std::cout << CLR_BOLD << "  TIPS:\n" CLR_RESET;
    std::cout << "  - Use risk scans to find chokepoints.\n";
    std::cout << "  - Close doors at HC to cut off paths.\n";
    std::cout << "  - Lure the intruder to the upper campus.\n";
    std::cout << "  - Manage power carefully!\n";
    std::cout << "\n================================================================\n";
    std::cout << "  Press Enter to go back...\n";
    initTerminal();
    char c;
    while (read(0, &c, 1) == 1 && c != '\n') {}
    restoreTerminal();
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
    // direction: 0=up, 1=down, 2=left, 3=right
    // Find the layout positions
    auto layout = getLayout(gs.gameMap.totalRooms);
    MapPos *cur = nullptr;
    for (auto &p : layout) {
        if (p.id == currentCursor) { cur = &p; break; }
    }
    if (!cur) return 0;

    int bestId = currentCursor;
    int bestScore = 999999;

    for (auto &p : layout) {
        if (p.id == currentCursor) continue;
        int dr = p.row - cur->row;
        int dc = p.col - cur->col;

        bool valid = false;
        if (direction == 0 && dr < 0) valid = true;      // up
        if (direction == 1 && dr > 0) valid = true;      // down
        if (direction == 2 && dc < 0) valid = true;      // left
        if (direction == 3 && dc > 0) valid = true;      // right

        if (valid) {
            int score = abs(dr) + abs(dc);
            if (score < bestScore) {
                bestScore = score;
                bestId = p.id;
            }
        }
    }
    return bestId;
}
