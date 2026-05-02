#include "ui.h"
#include <ncurses.h>
#undef KEY_UP
#undef KEY_DOWN
#undef KEY_LEFT
#undef KEY_RIGHT
#undef KEY_ENTER
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
#include <sys/ioctl.h>

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

static const int GAME_WIDTH = 85;
static const int GAME_MAX_HEIGHT = 44;
static int activeGameHeight = GAME_MAX_HEIGHT;
static const int GAME_INNER_WIDTH = GAME_WIDTH - 2;

static WINDOW *gameWin = nullptr;
static bool viewportActive = false;

enum ViewportColor {
    VP_RED = 1,
    VP_GREEN,
    VP_YELLOW,
    VP_BLUE,
    VP_MAGENTA,
    VP_CYAN,
    VP_WHITE
};

static void syncTerminalSize() {
    if (viewportActive && stdscr != nullptr) {
        int rows = 0;
        int cols = 0;
        getmaxyx(stdscr, rows, cols);
        LINES = rows;
        COLS = cols;
        return;
    }

    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        COLS = ws.ws_col;
        LINES = ws.ws_row;
    } else {
        COLS = 80;
        LINES = 24;
    }
}

static std::string trimLeft(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    return start == std::string::npos ? "" : s.substr(start);
}

static int visibleLength(const std::string &s) {
    int len = 0;
    bool inEscape = false;
    for (size_t i = 0; i < s.size(); i++) {
        if (!inEscape && s[i] == '\033') {
            inEscape = true;
            continue;
        }
        if (inEscape) {
            if (s[i] == 'm')
                inEscape = false;
            continue;
        }
        len++;
    }
    return len;
}

int getCenterY(int totalRows) {
    syncTerminalSize();
    return std::max(0, (LINES - totalRows) / 2);
}

int getCenterX(int textLength) {
    syncTerminalSize();
    return std::max(0, (COLS - textLength) / 2);
}

void printAt(int y, int x, const std::string &text) {
    syncTerminalSize();
    y = std::max(0, y);
    x = std::max(0, x);
    if (viewportActive && stdscr != nullptr) {
        mvprintw(y, x, "%s", text.c_str());
    } else {
        std::cout << "\033[" << (y + 1) << ";" << (x + 1) << "H" << text;
    }
}

void printCentered(int y, const std::string &text) {
    printAt(y, getCenterX(visibleLength(text)), text);
}

void drawCenteredArt(int startY, const std::string &art) {
    std::stringstream ss(art);
    std::string line;
    int y = startY;
    while (std::getline(ss, line)) {
        printCentered(y++, line);
    }
}

static void moveCursorToBottom() {
    syncTerminalSize();
    if (!viewportActive)
        std::cout << "\033[" << LINES << ";1H";
}

static void waitForEnterInput() {
    std::cin.clear();
    std::cin.get();
}

static void drawTextBlock(int startY, int width, const std::vector<std::string> &lines) {
    int marginX = getCenterX(width);
    for (size_t i = 0; i < lines.size(); i++)
        printAt(startY + static_cast<int>(i), marginX, lines[i]);
}

void clearScreen() {
    std::cout << "\033[2J\033[1;1H";
}

void startGameViewport() {
    if (viewportActive)
        return;

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, FALSE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(VP_RED, COLOR_RED, -1);
        init_pair(VP_GREEN, COLOR_GREEN, -1);
        init_pair(VP_YELLOW, COLOR_YELLOW, -1);
        init_pair(VP_BLUE, COLOR_BLUE, -1);
        init_pair(VP_MAGENTA, COLOR_MAGENTA, -1);
        init_pair(VP_CYAN, COLOR_CYAN, -1);
        init_pair(VP_WHITE, COLOR_WHITE, -1);
    }

    viewportActive = true;
}

void stopGameViewport() {
    if (gameWin != nullptr) {
        delwin(gameWin);
        gameWin = nullptr;
    }
    if (viewportActive) {
        clear();
        refresh();
        endwin();
        viewportActive = false;
    }
}

static std::string powerBar(int power, int maxPower) {
    int filled = (maxPower > 0) ? (power * 10 / maxPower) : 0;
    if (filled > 10) filled = 10;
    if (filled < 0) filled = 0;
    std::string bar = "[";
    for (int i = 0; i < filled; i++) bar += "#";
    for (int i = filled; i < 10; i++) bar += " ";
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

static int centeredXForLine(const std::string &line) {
    return 1 + std::max(0, (GAME_INNER_WIDTH - visibleLength(line)) / 2);
}

static void applyAnsiCode(WINDOW *win, int code, int &attrs, int &colorPair) {
    if (code == 0) {
        attrs = 0;
        colorPair = 0;
    } else if (code == 1) {
        attrs |= A_BOLD;
    } else if (code == 2) {
        attrs |= A_DIM;
    } else if (code == 31) {
        colorPair = VP_RED;
    } else if (code == 32) {
        colorPair = VP_GREEN;
    } else if (code == 33) {
        colorPair = VP_YELLOW;
    } else if (code == 34) {
        colorPair = VP_BLUE;
    } else if (code == 35) {
        colorPair = VP_MAGENTA;
    } else if (code == 36) {
        colorPair = VP_CYAN;
    } else if (code == 37) {
        colorPair = VP_WHITE;
    }

    wattrset(win, attrs | (colorPair > 0 ? COLOR_PAIR(colorPair) : 0));
}

static void printWindowAnsi(WINDOW *win, int y, int x, const std::string &text, int maxWidth = GAME_INNER_WIDTH) {
    if (win == nullptr || y <= 0 || y >= activeGameHeight - 1 || x <= 0 || x >= GAME_WIDTH - 1)
        return;

    int attrs = 0;
    int colorPair = 0;
    int col = x;
    int printed = 0;
    wattrset(win, A_NORMAL);

    for (size_t i = 0; i < text.size() && printed < maxWidth && col < GAME_WIDTH - 1; i++) {
        if (text[i] == '\033' && i + 1 < text.size() && text[i + 1] == '[') {
            size_t end = text.find('m', i + 2);
            if (end != std::string::npos) {
                std::string codes = text.substr(i + 2, end - (i + 2));
                std::stringstream ss(codes);
                std::string item;
                while (std::getline(ss, item, ';')) {
                    if (!item.empty())
                        applyAnsiCode(win, std::atoi(item.c_str()), attrs, colorPair);
                }
                i = end;
                continue;
            }
        }

        mvwaddch(win, y, col, text[i]);
        col++;
        printed++;
    }
    wattrset(win, A_NORMAL);
}

static void printWindowCentered(WINDOW *win, int y, const std::string &text) {
    printWindowAnsi(win, y, centeredXForLine(text), text);
}

static void printWindowDivider(WINDOW *win, int y, char ch = '=') {
    if (win == nullptr || y <= 0 || y >= activeGameHeight - 1)
        return;
    std::string divider(GAME_INNER_WIDTH, ch);
    mvwprintw(win, y, 1, "%s", divider.c_str());
}

static bool terminalTooSmallForGameViewport() {
    return LINES < activeGameHeight || COLS < GAME_WIDTH;
}

static void drawViewportTooSmallMessage() {
    clear();
    std::string line1 = "Terminal too small. Please resize.";
    std::string line2 = "Minimum recommended size: 85 x " + std::to_string(activeGameHeight);
    int startY = getCenterY(3);
    printCentered(startY, line1);
    printCentered(startY + 2, line2);
    refresh();
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

static std::string padRoomCode(const std::string &code, int width = 4) {
    std::string padded = code;
    while ((int)padded.size() < width)
        padded += ' ';
    return padded;
}

static std::vector<int> placeholderWidthsForRoom(const Room &room) {
    std::vector<int> widths;
    int minWidth = static_cast<int>(room.abbrev.size());
    for (int width : {4, 3, minWidth}) {
        width = std::max(width, minWidth);
        if (std::find(widths.begin(), widths.end(), width) == widths.end())
            widths.push_back(width);
    }
    return widths;
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
        bool found = false;

        for (int width : placeholderWidthsForRoom(room)) {
            std::string placeholder = "{" + padRoomCode(room.abbrev, width) + "}";
            for (size_t row = 0; row < mapLines.size() && !found; row++) {
                std::string::size_type col = mapLines[row].find(placeholder);
                if (col != std::string::npos) {
                    layout.push_back({room.id, (int)col, (int)row});
                    found = true;
                }
            }
            if (found)
                break;
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

static std::string renderRoom(const GameState &gs, const Room &room, bool isCursor, int width = 4) {
    std::string padded = padRoomCode(room.abbrev, width);
    SignalStrength signal = getSignalStrength(gs);
    bool strongSignal = (signal == SIGNAL_STRONG && room.id == gs.lastKnownEnemyRoom);
    bool weakSignal = (signal == SIGNAL_WEAK && room.id == gs.lastKnownEnemyRoom);

    std::string color = CLR_DIM;
    std::string left = "{";
    std::string right = "}";

    if (room.isOffice) {
        color = CLR_GREEN;
        left = "<";
        right = ">";
    } else if (strongSignal) {
        color = CLR_RED;
    } else if (weakSignal) {
        color = CLR_YELLOW;
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

static std::vector<std::string> renderMapLines(const GameState &gs, int cursorRoom) {
    std::vector<std::string> rendered;
    std::vector<std::string> mapLines = loadMapTemplate(mapTemplateFileForDifficulty(gs.difficulty));
    size_t mapWidth = 0;
    for (const std::string &line : mapLines)
        mapWidth = std::max(mapWidth, line.size());

    for (std::string line : mapLines) {
        if (line.size() < mapWidth)
            line.append(mapWidth - line.size(), ' ');

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
            for (int width : placeholderWidthsForRoom(room)) {
                replaceAll(line, "{" + padRoomCode(room.abbrev, width) + "}",
                           renderRoom(gs, room, room.id == cursorRoom, width));
            }
        }
        rendered.push_back(line);
    }

    rendered.push_back(std::string(CLR_DIM) +
                       CLR_GREEN + "<MB>" + CLR_RESET + CLR_DIM + "=Office  " +
                       CLR_RED + "[!!]" + CLR_RESET + CLR_DIM + "=Strong Signal  " +
                       CLR_YELLOW + "[?]" + CLR_RESET + CLR_DIM + "=Weak Signal  " +
                       CLR_GREEN + "[--]" + CLR_RESET + CLR_DIM + "=Gate Open  " +
                       CLR_CYAN + "[CUR]" + CLR_RESET + CLR_DIM + "=Cursor" +
                       CLR_RESET);
    return rendered;
}

static std::string cameraGroupRoomsText(const GameState &gs, int group, int cursorRoom) {
    (void)cursorRoom;
    auto roomIds = roomsInGroup(gs.gameMap, group);
    std::string text = cameraGroupLabel(gs.gameMap, group) + " (";
    for (size_t i = 0; i < roomIds.size(); i++) {
        if (i > 0) text += ", ";
        text += gs.gameMap.rooms[roomIds[i]].abbrev;
    }
    text += ")";
    return text;
}

static std::string sweepOptionLine(const GameState &gs, int selectedGroup) {
    int groupCount = effectiveCameraGroupCount(gs.gameMap);
    std::string line = "SWEEP CLUSTER: ";
    for (int i = 0; i < groupCount; i++) {
        if (i > 0) line += "  ";
        if (i == selectedGroup) {
            line += std::string(CLR_CYAN CLR_BOLD) + ">[" + std::to_string(i + 1) + "] " +
                    cameraGroupLabel(gs.gameMap, i) + "<" + CLR_RESET;
        } else {
            line += std::string(CLR_CYAN) + "[" + std::to_string(i + 1) + "]" + CLR_RESET +
                    " " + cameraGroupLabel(gs.gameMap, i);
        }
    }
    return line;
}

static std::vector<std::string> commandCostLines(const GameState &gs) {
    std::vector<std::string> lines;
    std::stringstream primary;
    primary << "Sweep " << gs.cameraPowerCost << "%  "
            << "Deep " << gs.deepScanPowerCost << "%  "
            << "Lure " << gs.lurePowerCost << "%";
    lines.push_back(primary.str());

    std::stringstream secondary;
    secondary << "Gate " << gs.gateClosePowerCost << "%  "
              << "Keep " << gs.gateUpkeepPowerCost << "%/t  "
              << "Wait 0%";
    lines.push_back(secondary.str());
    return lines;
}

void drawMap(const GameState &gs, int cursorRoom) {
    std::vector<std::string> rendered = renderMapLines(gs, cursorRoom);
    for (const std::string &line : rendered)
        std::cout << "  " << line << "\n";
}

void drawCameraFeed(const GameState &gs) {
    if (gs.lastScanOutput.empty()) return;

    std::cout << CLR_BOLD << "  Scan Result:" << CLR_RESET << "\n";
    for (size_t i = 0; i < gs.lastScanOutput.size(); i++)
        std::cout << "  " << CLR_CYAN << gs.lastScanOutput[i] << CLR_RESET << "\n";
    std::cout << "\n";
}

static void drawGameInternal(const GameState &gs, int cursorRoom, bool selectingSweep, int selectedGroup) {
    startGameViewport();
    std::vector<std::string> mapLines = renderMapLines(gs, cursorRoom);
    const int mapStartY = 8;
    const int maxLogEvents = 2;
    int logContentRows = std::max(2, std::min(maxLogEvents, static_cast<int>(gs.eventLog.size())));
    int compactCommandPanelTop = mapStartY + static_cast<int>(mapLines.size()) + 1 + 2 + logContentRows;
    activeGameHeight = std::min(GAME_MAX_HEIGHT, std::max(30, compactCommandPanelTop + 5));

    if (terminalTooSmallForGameViewport()) {
        drawViewportTooSmallMessage();
        return;
    }

    clear();
    refresh();

    int startY = getCenterY(activeGameHeight);
    int startX = getCenterX(GAME_WIDTH);
    if (gameWin != nullptr)
        delwin(gameWin);
    gameWin = newwin(activeGameHeight, GAME_WIDTH, startY, startX);
    werase(gameWin);
    box(gameWin, 0, 0);

    int y = 1;
    std::stringstream header;
    header << CLR_BOLD << "CAMERA WATCH - HKU CAMPUS"
           << " | NIGHT " << gs.currentNight << "/" << gs.totalNights
           << " | TURN " << gs.turn << "/" << gs.maxTurns << CLR_RESET;
    printWindowCentered(gameWin, y++, header.str());
    printWindowDivider(gameWin, y++);

    std::stringstream energy;
    energy << CLR_BOLD << "ENERGY "
           << (gs.maxPower > 0 ? (gs.power * 100 / gs.maxPower) : 0)
           << "% " << powerBar(gs.power, gs.maxPower) << CLR_RESET
           << "   DOORS: KAD " << (gs.rightGateClosed ? CLR_RED "CLOSED" : CLR_GREEN "OPEN");
    if (roomConnectsToOffice(gs, 2))
        energy << CLR_RESET << " | LIB " << (gs.centerGateClosed ? CLR_RED "CLOSED" : CLR_GREEN "OPEN");
    energy << CLR_RESET << " | KNOW " << (gs.leftGateClosed ? CLR_RED "CLOSED" : CLR_GREEN "OPEN")
           << CLR_RESET;
    printWindowCentered(gameWin, y++, energy.str());

    std::stringstream lure;
    lure << "LURE: " << CLR_CYAN << lureStatusText(gs) << CLR_RESET
         << " | NEXT: " << CLR_YELLOW << lureCooldownText(gs) << CLR_RESET
         << " | SIGNAL: " << CLR_YELLOW << getSignalDisplay(gs) << CLR_RESET;
    printWindowDivider(gameWin, y++);
    printWindowCentered(gameWin, y++, lure.str());
    y++;

    int commandPanelTop = activeGameHeight - 5;
    int logPanelBottom = commandPanelTop;

    for (const std::string &line : mapLines) {
        if (y >= logPanelBottom - 1)
            break;
        printWindowAnsi(gameWin, y++, centeredXForLine(line), line);
    }

    if (y < logPanelBottom)
        y++;

    if (y < logPanelBottom) {
        printWindowDivider(gameWin, y++, '-');
        int panelTop = y;
        const int splitX = 53;
        const int leftX = 3;
        const int rightX = splitX + 2;
        const int leftWidth = splitX - leftX - 1;
        const int rightWidth = GAME_WIDTH - rightX - 2;

        for (int row = panelTop; row < logPanelBottom; row++)
            mvwaddch(gameWin, row, splitX, '|');

        printWindowAnsi(gameWin, panelTop, leftX,
                        std::string(CLR_BOLD) + "[ SYSTEM EVENT LOG ]" + CLR_RESET,
                        leftWidth);
        printWindowAnsi(gameWin, panelTop, rightX,
                        std::string(CLR_BOLD) + "[ ENERGY COST ]" + CLR_RESET,
                        rightWidth);

        y = panelTop + 1;
        int availableRows = std::max(0, logPanelBottom - y);
        int shownRows = std::min(availableRows, maxLogEvents);
        int startEvent = std::max(0, (int)gs.eventLog.size() - shownRows);
        for (size_t i = startEvent; i < gs.eventLog.size() && y < logPanelBottom; i++)
            printWindowAnsi(gameWin, y++, 2, std::string(CLR_DIM) + "> " + CLR_RESET + gs.eventLog[i],
                            leftWidth);

        std::vector<std::string> costs = commandCostLines(gs);
        for (size_t i = 0; i < costs.size() && panelTop + 1 + static_cast<int>(i) < logPanelBottom; i++)
            printWindowAnsi(gameWin, panelTop + 1 + static_cast<int>(i), rightX,
                            std::string(CLR_DIM) + costs[i] + CLR_RESET,
                            rightWidth);
    }

    printWindowDivider(gameWin, commandPanelTop);

    if (selectingSweep) {
        int groupCount = effectiveCameraGroupCount(gs.gameMap);
        if (groupCount > 0) {
            selectedGroup = std::max(0, std::min(selectedGroup, groupCount - 1));
            printWindowCentered(gameWin, commandPanelTop + 1, sweepOptionLine(gs, selectedGroup));
            printWindowCentered(gameWin, commandPanelTop + 2,
                                std::string(CLR_CYAN) + "[ENTER]" + CLR_RESET + CLR_BOLD + " Confirm   " +
                                CLR_CYAN + "[0/ESC]" + CLR_RESET + CLR_BOLD + " Cancel   " +
                                CLR_CYAN + "Arrows/1-5" + CLR_RESET + CLR_BOLD + " Select" + CLR_RESET);
            printWindowCentered(gameWin, commandPanelTop + 3,
                                std::string(CLR_DIM) + cameraGroupRoomsText(gs, selectedGroup, cursorRoom) +
                                " | Cost " + std::to_string(gs.cameraPowerCost) + "% energy" + CLR_RESET);
        } else {
            printWindowCentered(gameWin, commandPanelTop + 2,
                                std::string(CLR_RED) + "No camera clusters available." + CLR_RESET);
        }
    } else {
        printWindowCentered(gameWin, activeGameHeight - 4,
                            std::string(CLR_CYAN) + "[A]" + CLR_RESET + CLR_BOLD + " Quick Sweep  " +
                            CLR_CYAN + "[ENTER]" + CLR_RESET + CLR_BOLD + " Deep Scan  " +
                            CLR_CYAN + "[G]" + CLR_RESET + CLR_BOLD + " Gate  " +
                            CLR_CYAN + "[L]" + CLR_RESET + CLR_BOLD + " Lure" + CLR_RESET);
        printWindowCentered(gameWin, activeGameHeight - 3,
                            std::string(CLR_CYAN) + "[W]" + CLR_RESET + CLR_BOLD + " Wait  " +
                            CLR_CYAN + "[Q]" + CLR_RESET + CLR_BOLD + " Save/Quit  " +
                            CLR_CYAN + "[H]" + CLR_RESET + CLR_BOLD + " Help  Move: " +
                            CLR_CYAN + "Arrow Keys" + CLR_RESET);
    }

    if (!selectingSweep && cursorRoom >= 0 && cursorRoom < gs.gameMap.totalRooms) {
        const Room &curRoom = gs.gameMap.rooms[cursorRoom];
        std::string cursorLine = std::string("Cursor: ") + CLR_CYAN + curRoom.abbrev + CLR_RESET +
                                 " (" + curRoom.name + ")";
        if (cursorRoom == 3)
            cursorLine += (gs.rightGateClosed ? CLR_RED " [KAD GATE CLOSED]" : CLR_DIM " [KAD GATE OPEN]");
        else if (cursorRoom == 2 && roomConnectsToOffice(gs, 2))
            cursorLine += (gs.centerGateClosed ? CLR_RED " [LIB GATE CLOSED]" : CLR_DIM " [LIB GATE OPEN]");
        else if (cursorRoom == 4)
            cursorLine += (gs.leftGateClosed ? CLR_RED " [KNOW GATE CLOSED]" : CLR_DIM " [KNOW GATE OPEN]");
        else
            cursorLine += std::string(CLR_DIM) + " [No gate here]";
        cursorLine += CLR_RESET;
        printWindowCentered(gameWin, activeGameHeight - 2, cursorLine);
    }

    wrefresh(gameWin);
}

void drawGame(const GameState &gs, int cursorRoom) {
    drawGameInternal(gs, cursorRoom, false, 0);
}

void drawSweepSelection(const GameState &gs, int cursorRoom, int selectedGroup) {
    drawGameInternal(gs, cursorRoom, true, selectedGroup);
}

void drawMainMenu() {
    clearScreen();
    const std::string logo =
        std::string(CLR_BOLD CLR_CYAN) +
        "============================================================\n"
        "       C A M E R A   W A T C H  -  H K U   C A M P U S\n"
        "============================================================" +
        CLR_RESET;
    int startY = getCenterY(17);
    drawCenteredArt(startY, logo);

    std::vector<std::string> story = {
        "You are a night security guard at the University of Hong Kong.",
        "An intruder stalks through the campus buildings.",
        "Monitor cameras, use sound lures, close doors,",
        "and survive until 6 AM each night."
    };
    drawTextBlock(startY + 5, 62, story);

    printCentered(startY + 11, std::string(CLR_BOLD CLR_CYAN) + "[1]" + CLR_RESET CLR_BOLD + " New Game");
    printCentered(startY + 12, std::string(CLR_BOLD CLR_CYAN) + "[2]" + CLR_RESET CLR_BOLD + " Load Game");
    printCentered(startY + 13, std::string(CLR_BOLD CLR_CYAN) + "[3]" + CLR_RESET CLR_BOLD + " How to Play");
    printCentered(startY + 14, std::string(CLR_BOLD CLR_CYAN) + "[0]" + CLR_RESET CLR_BOLD + " Quit" + CLR_RESET);
    printCentered(startY + 16, std::string(CLR_BOLD CLR_CYAN) + "============================================================" + CLR_RESET);
    std::cout.flush();
}

void drawDifficultyMenu() {
    clearScreen();
    int startY = getCenterY(9);
    printCentered(startY, std::string(CLR_BOLD CLR_CYAN) + "============================================================" + CLR_RESET);
    printCentered(startY + 1, std::string(CLR_BOLD) + "SELECT DIFFICULTY" + CLR_RESET);
    printCentered(startY + 2, std::string(CLR_BOLD CLR_CYAN) + "============================================================" + CLR_RESET);

    std::vector<std::string> options = {
        std::string(CLR_CYAN) + "[1]" + CLR_RESET + " " + CLR_BOLD + "Easy" + CLR_RESET + "   - 8 buildings, 2 nights, forgiving scans",
        std::string(CLR_CYAN) + "[2]" + CLR_RESET + " " + CLR_BOLD + "Normal" + CLR_RESET + " - 10 buildings, 2 nights, balanced routes",
        std::string(CLR_CYAN) + "[3]" + CLR_RESET + " " + CLR_BOLD + "Hard" + CLR_RESET + "   - 12 active buildings, 2 nights, three gates"
    };
    drawTextBlock(startY + 5, 64, options);
    printCentered(startY + 8, std::string(CLR_BOLD CLR_CYAN) + "============================================================" + CLR_RESET);
    std::cout.flush();
}

void drawEndGame(const GameState &gs) {
    clearScreen();
    int startY = getCenterY(9);
    printCentered(startY, std::string(CLR_BOLD CLR_CYAN) + "============================================================" + CLR_RESET);
    if (gs.status == STATUS_WIN) {
        printCentered(startY + 1, std::string(CLR_GREEN CLR_BOLD) + "Y O U   W I N !" + CLR_RESET);
        printCentered(startY + 3, "You survived all " + std::to_string(gs.totalNights) + " nights at HKU!");
    } else if (gs.status == STATUS_LOSE_ENEMY) {
        printCentered(startY + 1, std::string(CLR_RED CLR_BOLD) + "G A M E   O V E R" + CLR_RESET);
        printCentered(startY + 3, "The intruder reached Main Building on");
        printCentered(startY + 4, "Night " + std::to_string(gs.currentNight) + ", Turn " + std::to_string(gs.turn) + ".");
    } else if (gs.status == STATUS_LOSE_POWER) {
        printCentered(startY + 1, std::string(CLR_RED CLR_BOLD) + "G A M E   O V E R" + CLR_RESET);
        printCentered(startY + 3, "Power ran out on Night " + std::to_string(gs.currentNight) + ".");
    }
    printCentered(startY + 6, gs.statusMessage);
    printCentered(startY + 8, std::string(CLR_BOLD CLR_CYAN) + "============================================================" + CLR_RESET);
    printCentered(startY + 10, "Press Enter to return to main menu...");
    std::cout.flush();
    waitForEnterInput();
}

void drawHelp() {
    clearScreen();
    int startY = getCenterY(23);
    printCentered(startY, std::string(CLR_BOLD CLR_CYAN) + "================================================================" + CLR_RESET);
    printCentered(startY + 1, std::string(CLR_BOLD) + "HOW TO PLAY - HKU CAMPUS" + CLR_RESET);
    printCentered(startY + 2, std::string(CLR_BOLD CLR_CYAN) + "================================================================" + CLR_RESET);

    std::vector<std::string> help = {
        std::string(CLR_BOLD) + "OBJECTIVE:" + CLR_RESET,
        "Survive until 6 AM by tracking the intruder and protecting " + std::string(CLR_GREEN) + "Main Building (MB)" + CLR_RESET + ".",
        "",
        std::string(CLR_BOLD) + "CONTROLS:" + CLR_RESET,
        "Arrow Keys move cursor | Enter deep scans selected building",
        std::string(CLR_CYAN) + "[A]" + CLR_RESET + " sweep | " + CLR_CYAN + "[G]" + CLR_RESET + " gate | " +
            CLR_CYAN + "[L]" + CLR_RESET + " lure | " + CLR_CYAN + "[W]" + CLR_RESET + " wait | " +
            CLR_CYAN + "[H]" + CLR_RESET + " help | " + CLR_CYAN + "[Q]" + CLR_RESET + " save/quit",
        "",
        std::string(CLR_BOLD) + "CAMERA RINGS:" + CLR_RESET,
        "- Quick Sweep reports movement in one cluster only.",
        "- Deep Scan checks the building under the cursor; MB is never clustered.",
        "- Signals decay after a few turns depending on difficulty.",
        "",
        std::string(CLR_BOLD) + "TIPS:" + CLR_RESET,
        "- Use cheap sweeps to narrow down expensive deep scans.",
        "- Gate rooms next to MB control office entrances.",
        "- Closed gates cost energy every turn; lure can redirect movement."
    };
    drawTextBlock(startY + 4, 78, help);
    printCentered(startY + 21, std::string(CLR_BOLD CLR_CYAN) + "================================================================" + CLR_RESET);
    printCentered(startY + 22, "Press Enter to go back...");
    std::cout.flush();
    waitForEnterInput();
}

int promptInt(const std::string &msg, int lo, int hi) {
    int val;
    syncTerminalSize();
    std::string prompt = trimLeft(msg);
    int promptY = std::min(std::max(0, LINES - 3), getCenterY(1) + 12);
    printAt(promptY, getCenterX(visibleLength(prompt) + 2), prompt);
    std::cout.flush();
    std::cin >> val;
    if (std::cin.fail() || val < lo || val > hi) {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        return -1;
    }
    std::cin.ignore(10000, '\n');
    moveCursorToBottom();
    return val;
}

void pause(const std::string &msg) {
    syncTerminalSize();
    std::string prompt = trimLeft(msg);
    printCentered(std::min(std::max(0, LINES - 3), getCenterY(1) + 12), prompt);
    std::cout.flush();
    std::cin.clear();
    std::cin.get();
    moveCursorToBottom();
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