#include "ui.h"
#include <ncurses.h>
static const int NCURSES_KEY_UP = KEY_UP;
static const int NCURSES_KEY_DOWN = KEY_DOWN;
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
#include <utility>
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
static const int GAME_MIN_HEIGHT = 30;
static const int COMPACT_MIN_WIDTH = 58;
static const int COMPACT_MIN_HEIGHT = 22;
static const int COMPACT_MAX_WIDTH = 80;
static const int COMPACT_MAX_HEIGHT = 24;
static const int STARTUP_MIN_WIDTH = 58;
static const int STARTUP_MIN_HEIGHT = 22;
static int activeGameHeight = GAME_MAX_HEIGHT;
static int requiredGameHeight = GAME_MIN_HEIGHT;
static const int GAME_INNER_WIDTH = GAME_WIDTH - 2;

static WINDOW *gameWin = nullptr;
static bool cursesActive = false;
static bool viewportActive = false;
static int terminalRows = 24;
static int terminalCols = 80;
static int gameWinStartY = -1;
static int gameWinStartX = -1;
static int gameWinHeight = 0;
static int gameWinWidth = 0;

enum ViewportColor {
    VP_RED = 1,
    VP_GREEN,
    VP_YELLOW,
    VP_BLUE,
    VP_MAGENTA,
    VP_CYAN,
    VP_WHITE
};

enum StartupColor {
    BOOT_GREEN = 10,
    BOOT_RED = 11,
    BOOT_WHITE = 12
};

static bool readTerminalSizeFromIoctl(int &rows, int &cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        rows = ws.ws_row;
        cols = ws.ws_col;
        return true;
    }
    return false;
}

static void syncTerminalSize() {
    int rows = 0;
    int cols = 0;
    if (readTerminalSizeFromIoctl(rows, cols)) {
        if (cursesActive && (rows != terminalRows || cols != terminalCols))
            resizeterm(rows, cols);
        terminalRows = rows;
        terminalCols = cols;
        return;
    }

    if (cursesActive) {
        getmaxyx(stdscr, rows, cols);
        if (rows > 0 && cols > 0) {
            terminalRows = rows;
            terminalCols = cols;
            return;
        }
    }

    terminalCols = 80;
    terminalRows = 24;
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

static void padLinesToSameVisibleWidth(std::vector<std::string> &lines) {
    int width = 0;
    for (const std::string &line : lines)
        width = std::max(width, visibleLength(line));

    for (std::string &line : lines) {
        int pad = width - visibleLength(line);
        if (pad > 0)
            line.append(pad, ' ');
    }
}

int getCenterY(int totalRows) {
    syncTerminalSize();
    return std::max(0, (terminalRows - totalRows) / 2);
}

int getCenterX(int textLength) {
    syncTerminalSize();
    return std::max(0, (terminalCols - textLength) / 2);
}

void printAt(int y, int x, const std::string &text) {
    syncTerminalSize();
    y = std::max(0, y);
    x = std::max(0, x);
    if (cursesActive) {
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
    if (!cursesActive)
        std::cout << "\033[" << terminalRows << ";1H";
}

static void waitForEnterInput() {
    if (cursesActive) {
        nodelay(stdscr, FALSE);
        int ch;
        do {
            ch = getch();
        } while (ch != '\n' && ch != '\r' && ch != 3 && !terminalInterruptRequested());
        flushinp();
        return;
    }

    std::cin.clear();
    std::cin.get();
}

void clearScreen() {
    if (cursesActive) {
        clear();
        refresh();
    } else {
        std::cout << "\033[2J\033[1;1H";
    }
}

bool initializeCurses() {
    if (cursesActive)
        return true;
    WINDOW *screen = initscr();
    if (screen == nullptr)
        return false;

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        init_pair(VP_RED, COLOR_RED, COLOR_BLACK);
        init_pair(VP_GREEN, COLOR_GREEN, COLOR_BLACK);
        init_pair(VP_YELLOW, COLOR_YELLOW, COLOR_BLACK);
        init_pair(VP_BLUE, COLOR_BLUE, COLOR_BLACK);
        init_pair(VP_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(VP_CYAN, COLOR_CYAN, COLOR_BLACK);
        init_pair(VP_WHITE, COLOR_WHITE, COLOR_BLACK);
        init_pair(BOOT_GREEN, COLOR_GREEN, COLOR_BLACK);
        init_pair(BOOT_RED, COLOR_RED, COLOR_BLACK);
        init_pair(BOOT_WHITE, COLOR_WHITE, COLOR_BLACK);
        bkgd(COLOR_PAIR(BOOT_WHITE));
    }

    clear();
    refresh();
    cursesActive = true;
    syncTerminalSize();
    return true;
}

void shutdownCurses() {
    if (gameWin != nullptr) {
        delwin(gameWin);
        gameWin = nullptr;
    }
    viewportActive = false;
    gameWinStartY = -1;
    gameWinStartX = -1;
    gameWinHeight = 0;
    gameWinWidth = 0;

    if (cursesActive) {
        clear();
        refresh();
        endwin();
        cursesActive = false;
    }
}

void startGameViewport() {
    if (viewportActive)
        return;
    if (!cursesActive && !initializeCurses())
        return;

    keypad(stdscr, FALSE);
    nodelay(stdscr, FALSE);
    cbreak();
    noecho();
    curs_set(0);
    clear();
    refresh();
    flushinp();
    viewportActive = true;
}

void stopGameViewport() {
    if (gameWin != nullptr) {
        delwin(gameWin);
        gameWin = nullptr;
    }
    gameWinStartY = -1;
    gameWinStartX = -1;
    gameWinHeight = 0;
    gameWinWidth = 0;
    if (viewportActive) {
        viewportActive = false;
        keypad(stdscr, TRUE);
        nodelay(stdscr, FALSE);
        clear();
        refresh();
        flushinp();
    }
}

static void attrOnPair(int colorPair, int attrs = 0) {
    attron((colorPair > 0 ? COLOR_PAIR(colorPair) : 0) | attrs);
}

static void attrOffPair(int colorPair, int attrs = 0) {
    attroff((colorPair > 0 ? COLOR_PAIR(colorPair) : 0) | attrs);
}

static void mvaddCenteredCurses(int y, const std::string &text, int colorPair, int attrs = 0) {
    syncTerminalSize();
    int x = std::max(0, (terminalCols - static_cast<int>(text.size())) / 2);
    attrOnPair(colorPair, attrs);
    mvaddnstr(y, x, text.c_str(), std::max(0, terminalCols - x));
    attrOffPair(colorPair, attrs);
}

static void mvaddTextCurses(int y, int x, const std::string &text, int colorPair, int attrs = 0) {
    if (y < 0 || y >= terminalRows || x >= terminalCols)
        return;
    x = std::max(0, x);
    attrOnPair(colorPair, attrs);
    mvaddnstr(y, x, text.c_str(), std::max(0, terminalCols - x));
    attrOffPair(colorPair, attrs);
}

static std::vector<std::string> wrapText(const std::string &text, int width) {
    std::vector<std::string> lines;
    if (text.empty()) {
        lines.push_back("");
        return lines;
    }

    std::istringstream words(text);
    std::string word;
    std::string line;
    while (words >> word) {
        if (!line.empty() && static_cast<int>(line.size() + 1 + word.size()) > width) {
            lines.push_back(line);
            line.clear();
        }
        if (!line.empty())
            line += " ";
        line += word;
    }
    if (!line.empty())
        lines.push_back(line);
    return lines;
}

bool ensureStartupTerminalSize() {
    if (!cursesActive && !initializeCurses())
        return false;

    int maxY = 0;
    int maxX = 0;
    getmaxyx(stdscr, maxY, maxX);
    terminalRows = maxY;
    terminalCols = maxX;

    if (maxY >= STARTUP_MIN_HEIGHT && maxX >= STARTUP_MIN_WIDTH)
        return true;

    clear();
    mvaddCenteredCurses(std::max(0, maxY / 2 - 2), "Terminal too small for PROTOCOL 1911.", BOOT_RED, A_BOLD);
    mvaddCenteredCurses(std::max(0, maxY / 2), "Minimum size: 58 x 22", BOOT_WHITE);
    mvaddCenteredCurses(std::max(0, maxY / 2 + 1),
                        "Current size: " + std::to_string(maxX) + " x " + std::to_string(maxY),
                        BOOT_WHITE);
    mvaddCenteredCurses(std::max(0, maxY / 2 + 3), "Press any key to exit...", BOOT_GREEN);
    refresh();
    nodelay(stdscr, FALSE);
    getch();
    flushinp();
    return false;
}

void showTitleScreen() {
    clear();
    refresh();
    syncTerminalSize();

    const std::vector<std::pair<std::string, int> > diagnostics = {
        {"MEM CHECK................ OK", BOOT_GREEN},
        {"MOUNTING /dev/sda1....... FAILED", BOOT_RED},
        {"ROUTING SECURITY CAMERAS. DEGRADED", BOOT_RED},
        {"BACKUP BATTERY........... ONLINE", BOOT_GREEN}
    };

    int y = std::max(1, terminalRows / 2 - 3);
    for (size_t i = 0; i < diagnostics.size(); i++) {
        mvaddCenteredCurses(y + static_cast<int>(i), diagnostics[i].first, diagnostics[i].second, A_BOLD);
        refresh();
        napms(260);
    }
    napms(650);
    flushinp();

    clear();
    const std::vector<std::string> logo = {
        " ______  ______   ______  _______  ______  ______  ______  __       ",
        "|   __ \\|   __ \\ |   __ \\|_     _||   __ \\|      ||   __ \\|  |      ",
        "|    __/|      < |  |  | | |   |  |  |  | |   ---||  |  | |  |      ",
        "|___|   |___|__| |______/  |___|  |______/|______||______/|__|      ",
        "                                                                    ",
        "                         P R O T O C O L                           ",
        "                              1 9 1 1                               "
    };
    int logoStart = std::max(1, (terminalRows - static_cast<int>(logo.size())) / 2 - 1);
    for (size_t i = 0; i < logo.size(); i++)
        mvaddCenteredCurses(logoStart + static_cast<int>(i), logo[i], BOOT_GREEN, A_BOLD);
    mvaddCenteredCurses(terminalRows - 3,
                        "[ SYSTEM BOOT SEQUENCE INITIATED ] - Press [ENTER] to boot...",
                        BOOT_WHITE, A_BOLD);
    refresh();

    nodelay(stdscr, FALSE);
    int ch;
    do {
        ch = getch();
    } while (ch != '\n' && ch != '\r' && ch != 3 && !terminalInterruptRequested());
    flushinp();
}

static bool slowPrintLine(int y, int x, const std::string &text, int colorPair, int delayMs) {
    nodelay(stdscr, TRUE);
    for (size_t i = 0; i < text.size(); i++) {
        int ch = getch();
        if (ch == ' ' || ch == '\n' || ch == '\r') {
            nodelay(stdscr, FALSE);
            return true;
        }
        if (ch == 3 || terminalInterruptRequested()) {
            nodelay(stdscr, FALSE);
            return true;
        }

        int attrs = 0;
        bool glitch = (colorPair == BOOT_RED && (std::rand() % 26) == 0);
        if (glitch)
            attrs |= A_REVERSE;
        attrOnPair(colorPair, attrs);
        mvaddch(y, x + static_cast<int>(i), text[i]);
        attrOffPair(colorPair, attrs);
        refresh();
        if (glitch) {
            napms(24);
            attrOnPair(colorPair);
            mvaddch(y, x + static_cast<int>(i), text[i]);
            attrOffPair(colorPair);
            refresh();
        }
        napms(delayMs);
    }
    nodelay(stdscr, FALSE);
    return false;
}

static void slowPrintWrapped(int &y, const std::string &text, int colorPair, bool &skipAll) {
    int maxWidth = std::max(20, terminalCols - 8);
    std::vector<std::string> lines = wrapText(text, maxWidth);
    for (size_t i = 0; i < lines.size(); i++) {
        const std::string &line = lines[i];
        if (y >= terminalRows - 3) {
            scrl(1);
            y = terminalRows - 4;
            refresh();
        }
        int x = std::max(0, (terminalCols - static_cast<int>(line.size())) / 2);
        if (skipAll) {
            mvaddTextCurses(y, x, line, colorPair);
        } else if (slowPrintLine(y, x, line, colorPair, 18)) {
            skipAll = true;
            mvaddTextCurses(y, x, line, colorPair);
        }
        y++;
    }
}

void showStoryline() {
    clear();
    refresh();
    syncTerminalSize();
    scrollok(stdscr, TRUE);

    bool skipAll = false;
    int y = 1;
    const std::vector<std::string> greenLines = {
        "HKU MAINFRAME [Version 4.2.1]",
        "LOGIN SUCCESSFUL.",
        "USER: STUDENT_ADMIN",
        "LOCATION: MAIN BUILDING (MB)"
    };
    const std::vector<std::string> redLines = {
        "WARNING: CAMPUS WIDE POWER FAILURE DETECTED.",
        "WARNING: MULTIPLE UNAUTHORIZED INTRUDERS DETECTED ON CAMPUS.",
        "THREAT BEHAVIOR: HOSTILE."
    };
    const std::vector<std::string> whiteLines = {
        "You are trapped in the Main Building (MB) Server Room.",
        "Dangerous intruders have sneaked into the campus.",
        "They are hunting for you, moving room-by-room across the campus.",
        "Main power is dead. You are surviving on a backup battery.",
        "You must use this terminal to track the intruders using security cameras.",
        "You can close blast doors to block their path, but keeping them closed drains your battery fast.",
        "You can use Audio Lure to trick them into another hallway.",
        "Do not let your battery hit 0%.",
        "Do not let the intruders reach the Main Building.",
        "Survive until dawn."
    };

    for (size_t i = 0; i < greenLines.size(); i++)
        slowPrintWrapped(y, greenLines[i], BOOT_GREEN, skipAll);
    y++;
    for (size_t i = 0; i < redLines.size(); i++)
        slowPrintWrapped(y, redLines[i], BOOT_RED, skipAll);
    y++;
    for (size_t i = 0; i < whiteLines.size(); i++)
        slowPrintWrapped(y, whiteLines[i], BOOT_WHITE, skipAll);

    mvaddCenteredCurses(terminalRows - 2, "PRESS [ENTER] TO ACCESS MAIN MENU...", BOOT_GREEN, A_BOLD);
    refresh();
    nodelay(stdscr, FALSE);
    int ch;
    do {
        ch = getch();
    } while (ch != '\n' && ch != '\r' && ch != 3 && !terminalInterruptRequested());
    scrollok(stdscr, FALSE);
    flushinp();
}

MainMenuChoice showMainMenu() {
    keypad(stdscr, TRUE);
    nodelay(stdscr, FALSE);
    flushinp();

    const std::vector<std::string> options = {
        "Start Game",
        "Load Game",
        "How to Play",
        "Quit"
    };
    int selected = 0;

    while (!terminalInterruptRequested()) {
        clear();
        syncTerminalSize();
        mvaddCenteredCurses(2, "P R O T O C O L  1 9 1 1", BOOT_RED, A_BOLD);
        mvaddCenteredCurses(4, "HKU MAINFRAME ACCESS TERMINAL", BOOT_GREEN);

        int startY = std::max(7, terminalRows / 2 - 2);
        for (size_t i = 0; i < options.size(); i++) {
            int attrs = (static_cast<int>(i) == selected) ? A_REVERSE | A_BOLD : A_NORMAL;
            mvaddCenteredCurses(startY + static_cast<int>(i) * 2, options[i], BOOT_WHITE, attrs);
        }
        refresh();

        int ch = getch();
        if (ch == 3 || terminalInterruptRequested())
            return MENU_QUIT;
        if (ch == NCURSES_KEY_UP || ch == 'w' || ch == 'W') {
            selected = (selected + static_cast<int>(options.size()) - 1) % static_cast<int>(options.size());
        } else if (ch == NCURSES_KEY_DOWN || ch == 's' || ch == 'S') {
            selected = (selected + 1) % static_cast<int>(options.size());
        } else if (ch >= '1' && ch <= '4') {
            selected = ch - '1';
            flushinp();
            clear();
            refresh();
            return static_cast<MainMenuChoice>(selected);
        } else if (ch == '0' || ch == 27) {
            flushinp();
            clear();
            refresh();
            return MENU_QUIT;
        } else if (ch == '\n' || ch == '\r') {
            flushinp();
            clear();
            refresh();
            return static_cast<MainMenuChoice>(selected);
        }
    }

    return MENU_QUIT;
}

bool showModeMenu(Difficulty &difficulty) {
    keypad(stdscr, TRUE);
    nodelay(stdscr, FALSE);
    flushinp();

    const std::vector<std::string> options = {
        "Easy Mode",
        "Normal Mode",
        "Hard Mode",
        "Back"
    };
    int selected = 1;

    while (!terminalInterruptRequested()) {
        clear();
        syncTerminalSize();
        mvaddCenteredCurses(2, "SELECT PLAY MODE", BOOT_RED, A_BOLD);
        mvaddCenteredCurses(4, "Choose the campus layout and pressure level.", BOOT_GREEN);

        int startY = std::max(7, terminalRows / 2 - 3);
        for (size_t i = 0; i < options.size(); i++) {
            int attrs = (static_cast<int>(i) == selected) ? A_REVERSE | A_BOLD : A_NORMAL;
            mvaddCenteredCurses(startY + static_cast<int>(i) * 2, options[i], BOOT_WHITE, attrs);
        }
        refresh();

        int ch = getch();
        if (ch == 3 || terminalInterruptRequested())
            return false;
        if (ch == NCURSES_KEY_UP || ch == 'w' || ch == 'W') {
            selected = (selected + static_cast<int>(options.size()) - 1) % static_cast<int>(options.size());
        } else if (ch == NCURSES_KEY_DOWN || ch == 's' || ch == 'S') {
            selected = (selected + 1) % static_cast<int>(options.size());
        } else if (ch >= '1' && ch <= '3') {
            selected = ch - '1';
            flushinp();
            clear();
            refresh();
            difficulty = static_cast<Difficulty>(selected);
            return true;
        } else if (ch == '4' || ch == '0') {
            flushinp();
            clear();
            refresh();
            return false;
        } else if (ch == '\n' || ch == '\r') {
            flushinp();
            clear();
            refresh();
            if (selected == 3)
                return false;
            difficulty = static_cast<Difficulty>(selected);
            return true;
        } else if (ch == 27 || ch == 'q' || ch == 'Q') {
            flushinp();
            clear();
            refresh();
            return false;
        }
    }

    return false;
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

static void printWindowAnsiInBox(WINDOW *win, int boxHeight, int boxWidth,
                                 int y, int x, const std::string &text, int maxWidth) {
    if (win == nullptr || y <= 0 || y >= boxHeight - 1 || x <= 0 || x >= boxWidth - 1)
        return;

    int attrs = 0;
    int colorPair = 0;
    int col = x;
    int printed = 0;
    wattrset(win, A_NORMAL);

    for (size_t i = 0; i < text.size() && printed < maxWidth && col < boxWidth - 1; i++) {
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

static void printWindowAnsi(WINDOW *win, int y, int x, const std::string &text, int maxWidth = GAME_INNER_WIDTH) {
    printWindowAnsiInBox(win, activeGameHeight, GAME_WIDTH, y, x, text, maxWidth);
}

static void printWindowCentered(WINDOW *win, int y, const std::string &text) {
    printWindowAnsi(win, y, centeredXForLine(text), text);
}

static void printWindowCenteredInBox(WINDOW *win, int boxHeight, int boxWidth,
                                     int y, const std::string &text) {
    int innerWidth = std::max(1, boxWidth - 2);
    int x = 1 + std::max(0, (innerWidth - visibleLength(text)) / 2);
    printWindowAnsiInBox(win, boxHeight, boxWidth, y, x, text, innerWidth);
}

static void printWindowDividerInBox(WINDOW *win, int boxHeight, int boxWidth, int y, char ch = '=') {
    if (win == nullptr || y <= 0 || y >= boxHeight - 1)
        return;
    std::string divider(std::max(1, boxWidth - 2), ch);
    mvwprintw(win, y, 1, "%s", divider.c_str());
}

static void printWindowDivider(WINDOW *win, int y, char ch = '=') {
    if (win == nullptr || y <= 0 || y >= activeGameHeight - 1)
        return;
    std::string divider(GAME_INNER_WIDTH, ch);
    mvwprintw(win, y, 1, "%s", divider.c_str());
}

static bool ensureGameWindow(int height, int width, int startY, int startX) {
    const bool geometryChanged =
        gameWin == nullptr ||
        gameWinHeight != height ||
        gameWinWidth != width ||
        gameWinStartY != startY ||
        gameWinStartX != startX;

    if (geometryChanged) {
        erase();
        if (gameWin != nullptr)
            delwin(gameWin);
        gameWin = newwin(height, width, startY, startX);
        if (gameWin == nullptr) {
            gameWinStartY = -1;
            gameWinStartX = -1;
            gameWinHeight = 0;
            gameWinWidth = 0;
            return false;
        }
        leaveok(gameWin, TRUE);
        gameWinStartY = startY;
        gameWinStartX = startX;
        gameWinHeight = height;
        gameWinWidth = width;
    }

    return gameWin != nullptr;
}

static bool terminalTooSmallForGameViewport() {
    return terminalRows < activeGameHeight || terminalCols < GAME_WIDTH;
}

static bool terminalCanUseCompactViewport() {
    return terminalRows >= COMPACT_MIN_HEIGHT && terminalCols >= COMPACT_MIN_WIDTH;
}

static void drawViewportTooSmallMessage() {
    clear();
    syncTerminalSize();
    std::vector<std::string> lines = {
        "Terminal too small.",
        "Need at least " + std::to_string(GAME_WIDTH) + " x " +
            std::to_string(requiredGameHeight) + " for the full HUD.",
        "Current size: " + std::to_string(terminalCols) + " x " + std::to_string(terminalRows)
    };
    int startY = std::max(0, (terminalRows - static_cast<int>(lines.size())) / 2);
    for (size_t i = 0; i < lines.size(); i++) {
        std::string line = lines[i];
        if (terminalCols > 0 && static_cast<int>(line.size()) > terminalCols)
            line = line.substr(0, terminalCols);
        int x = std::max(0, (terminalCols - static_cast<int>(line.size())) / 2);
        mvprintw(startY + static_cast<int>(i), x, "%s", line.c_str());
    }
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
            {6, 16, 2},  // HOC
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
            {6, 16, 4},  // HOC
            {2, 30, 4},  // LIB
            {1, 46, 4},  // KKL
            {0,  0, 6},  // MB
            {4,  0, 8},  // KNOW
        };
    }

    return {
        {8,   7,  2},  // MW
        {10, 32,  2},  // RHT
        {12, 57,  2},  // CYC
        {7,   7,  7},  // HW
        {5,  32,  7},  // CYM
        {11, 57,  7},  // RR
        {3,   7, 12},  // KAD
        {6,  32, 12},  // HOC
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

static std::vector<std::string> wrapPlainText(const std::string &text, int width) {
    std::vector<std::string> lines;
    if (width <= 0) {
        lines.push_back("");
        return lines;
    }

    std::stringstream words(text);
    std::string word;
    std::string line;
    while (words >> word) {
        if (line.empty()) {
            line = word;
        } else if (static_cast<int>(line.size() + 1 + word.size()) <= width) {
            line += " " + word;
        } else {
            lines.push_back(line);
            line = word;
        }

        while (static_cast<int>(line.size()) > width) {
            lines.push_back(line.substr(0, width));
            line = line.substr(width);
        }
    }

    if (!line.empty())
        lines.push_back(line);
    if (lines.empty())
        lines.push_back("");
    return lines;
}

static std::vector<std::string> eventLogDisplayLines(const std::vector<std::string> &events,
                                                     int maxEvents, int maxRows, int width) {
    std::vector<std::vector<std::string>> wrappedEvents;
    int totalRows = 0;

    for (int i = static_cast<int>(events.size()) - 1;
         i >= 0 && static_cast<int>(wrappedEvents.size()) < maxEvents; i--) {
        std::vector<std::string> wrapped = wrapPlainText(events[i], std::max(1, width - 2));
        if (totalRows + static_cast<int>(wrapped.size()) > maxRows && !wrappedEvents.empty())
            break;

        if (totalRows + static_cast<int>(wrapped.size()) > maxRows)
            wrapped.resize(maxRows - totalRows);

        totalRows += static_cast<int>(wrapped.size());
        wrappedEvents.push_back(wrapped);
        if (totalRows >= maxRows)
            break;
    }

    std::vector<std::string> lines;
    for (auto it = wrappedEvents.rbegin(); it != wrappedEvents.rend(); ++it) {
        for (size_t i = 0; i < it->size(); i++)
            lines.push_back((i == 0 ? "> " : "  ") + (*it)[i]);
    }
    return lines;
}

static std::vector<std::string> scanOutputDisplayLines(const GameState &gs, int maxRows, int width) {
    std::vector<std::string> lines;
    if (maxRows <= 0 || width <= 0 || gs.lastScanOutput.empty())
        return lines;

    for (const std::string &entry : gs.lastScanOutput) {
        std::vector<std::string> wrapped = wrapPlainText(entry, width);
        for (const std::string &line : wrapped) {
            if (static_cast<int>(lines.size()) >= maxRows)
                return lines;
            lines.push_back(line);
        }
    }
    return lines;
}

static std::string compactRoomToken(const GameState &gs, int roomId, int cursorRoom) {
    if (roomId < 0 || roomId >= gs.gameMap.totalRooms)
        return "";

    const Room &room = gs.gameMap.rooms[roomId];
    return renderRoom(gs, room, room.id == cursorRoom, 4);
}

static std::string compactGateToken(const GameState &gs, int roomId) {
    if (roomId == 3)
        return renderGateMarker(gs.rightGateClosed);
    if (roomId == 2 && roomConnectsToOffice(gs, 2))
        return renderGateMarker(gs.centerGateClosed);
    if (roomId == 4)
        return renderGateMarker(gs.leftGateClosed);
    return std::string(CLR_DIM) + "----" + CLR_RESET;
}

static std::vector<std::string> compactMapLines(const GameState &gs, int cursorRoom) {
    auto r = [&](int id) { return compactRoomToken(gs, id, cursorRoom); };
    auto g = [&](int id) { return compactGateToken(gs, id); };
    std::vector<std::string> lines;

    if (gs.gameMap.totalRooms <= 8) {
        lines.push_back("      " + r(1) + "        " + r(7) + "====" + r(5));
        lines.push_back(std::string(CLR_DIM) + "        |             \\      /" + CLR_RESET);
        lines.push_back("      " + r(2) + "===========" + r(6));
        lines.push_back(std::string(CLR_DIM) + "        |               |" + CLR_RESET);
        lines.push_back("      " + r(4) + "--" + g(4) + "--" + r(0) + "--" + g(3) + "--" + r(3));
        padLinesToSameVisibleWidth(lines);
        return lines;
    }

    if (gs.gameMap.totalRooms <= 10) {
        lines.push_back("                     " + r(8));
        lines.push_back(std::string(CLR_DIM) + "                       |" + CLR_RESET);
        lines.push_back("      " + r(1) + "--" + r(2) + "--" + r(9) + "--" + r(5) + "--" + r(7));
        lines.push_back(std::string(CLR_DIM) + "        |              \\      /       |" + CLR_RESET);
        lines.push_back("      " + r(4) + "--" + g(4) + "--" + r(0) + "--" + g(3) + "--" + r(3) + "--" + r(6));
        padLinesToSameVisibleWidth(lines);
        return lines;
    }

    lines.push_back("      " + r(8) + "-----" + r(10) + "-----" + r(12));
    lines.push_back(std::string(CLR_DIM) + "        |         |         |" + CLR_RESET);
    lines.push_back("      " + r(7) + "     " + r(5) + "     " + r(11));
    lines.push_back(std::string(CLR_DIM) + "        |         |         |" + CLR_RESET);
    lines.push_back("      " + r(3) + "-----" + r(6) + "-----" + r(9));
    lines.push_back(std::string(CLR_DIM) + "        |         |         |" + CLR_RESET);
    lines.push_back(std::string(CLR_DIM) + "        |" + CLR_RESET + "        " + r(2) + "     " + r(4));
    lines.push_back("       " + g(3) + "       " + g(2) + "       " + g(4));
    lines.push_back(std::string(CLR_DIM) + "        \\         |         /" + CLR_RESET);
    lines.push_back("                 " + r(0));
    padLinesToSameVisibleWidth(lines);
    return lines;
}

static std::string gateSummaryLine(const GameState &gs) {
    std::stringstream ss;
    ss << "Gates KAD:" << (gs.rightGateClosed ? CLR_RED "Closed" : CLR_GREEN "Open");
    if (roomConnectsToOffice(gs, 2))
        ss << CLR_RESET << " LIB:" << (gs.centerGateClosed ? CLR_RED "Closed" : CLR_GREEN "Open");
    ss << CLR_RESET << " KNOW:" << (gs.leftGateClosed ? CLR_RED "Closed" : CLR_GREEN "Open") << CLR_RESET;
    return ss.str();
}

static void drawCompactGameInternal(const GameState &gs, int cursorRoom, bool selectingSweep, int selectedGroup) {
    int compactHeight = std::min(COMPACT_MAX_HEIGHT, terminalRows);
    int compactWidth = std::min(COMPACT_MAX_WIDTH, terminalCols);
    compactHeight = std::max(COMPACT_MIN_HEIGHT, compactHeight);
    compactWidth = std::max(COMPACT_MIN_WIDTH, compactWidth);
    activeGameHeight = compactHeight;

    int startY = std::max(0, (terminalRows - compactHeight) / 2);
    int startX = std::max(0, (terminalCols - compactWidth) / 2);
    if (!ensureGameWindow(compactHeight, compactWidth, startY, startX)) {
        drawViewportTooSmallMessage();
        return;
    }

    werase(gameWin);
    box(gameWin, 0, 0);

    const int innerWidth = compactWidth - 2;
    int y = 1;
    std::stringstream header;
    header << CLR_BOLD << "PROTOCOL 1911"
           << CLR_RESET << "  Night " << gs.currentNight << "/" << gs.totalNights
           << "  Turn " << gs.turn << "/" << gs.maxTurns;
    printWindowCenteredInBox(gameWin, compactHeight, compactWidth, y++, header.str());

    std::stringstream energy;
    energy << "Energy " << (gs.maxPower > 0 ? (gs.power * 100 / gs.maxPower) : 0)
           << "% " << powerBar(gs.power, gs.maxPower);
    printWindowCenteredInBox(gameWin, compactHeight, compactWidth, y++, energy.str());
    printWindowCenteredInBox(gameWin, compactHeight, compactWidth, y++, gateSummaryLine(gs));

    std::stringstream lure;
    lure << "Lure: " << lureStatusText(gs)
         << " | Next: " << lureCooldownText(gs)
         << " | Signal: " << getSignalDisplay(gs);
    printWindowCenteredInBox(gameWin, compactHeight, compactWidth, y++, lure.str());
    printWindowDividerInBox(gameWin, compactHeight, compactWidth, y++, '-');

    int commandRows = selectingSweep ? 5 : 4;
    int commandTop = compactHeight - commandRows;
    std::vector<std::string> mapLines = compactMapLines(gs, cursorRoom);
    for (const std::string &line : mapLines) {
        if (y >= commandTop - 4)
            break;
        printWindowCenteredInBox(gameWin, compactHeight, compactWidth, y++, line);
    }

    if (cursorRoom >= 0 && cursorRoom < gs.gameMap.totalRooms && y < commandTop - 3) {
        const Room &curRoom = gs.gameMap.rooms[cursorRoom];
        printWindowCenteredInBox(gameWin, compactHeight, compactWidth, y++,
                                 std::string("Cursor: ") + CLR_CYAN + curRoom.abbrev + CLR_RESET +
                                 " - " + curRoom.name);
    }

    if (y < commandTop - 2) {
        std::vector<std::string> scanLines = scanOutputDisplayLines(gs, 1, innerWidth - 4);
        if (!scanLines.empty()) {
            printWindowAnsiInBox(gameWin, compactHeight, compactWidth, y++, 2,
                                 std::string("> ") + CLR_CYAN + scanLines.front() + CLR_RESET,
                                 innerWidth);
        } else if (!gs.eventLog.empty()) {
            std::vector<std::string> logLines = eventLogDisplayLines(gs.eventLog, 1, 1, innerWidth - 2);
            if (!logLines.empty())
                printWindowAnsiInBox(gameWin, compactHeight, compactWidth, y++, 2,
                                     std::string(CLR_DIM) + logLines.front() + CLR_RESET,
                                     innerWidth);
        }
    }

    std::vector<std::string> costs = commandCostLines(gs);
    for (const std::string &line : costs) {
        if (y >= commandTop)
            break;
        printWindowCenteredInBox(gameWin, compactHeight, compactWidth, y++,
                                 std::string(CLR_DIM) + line + CLR_RESET);
    }

    printWindowDividerInBox(gameWin, compactHeight, compactWidth, commandTop, '=');
    if (selectingSweep) {
        int groupCount = effectiveCameraGroupCount(gs.gameMap);
        if (groupCount > 0) {
            selectedGroup = std::max(0, std::min(selectedGroup, groupCount - 1));
            printWindowCenteredInBox(gameWin, compactHeight, compactWidth, commandTop + 1,
                                     sweepOptionLine(gs, selectedGroup));
            printWindowCenteredInBox(gameWin, compactHeight, compactWidth, commandTop + 2,
                                     cameraGroupRoomsText(gs, selectedGroup, cursorRoom));
            printWindowCenteredInBox(gameWin, compactHeight, compactWidth, commandTop + 3,
                                     std::string(CLR_CYAN) + "Enter" + CLR_RESET + " Confirm  " +
                                     CLR_CYAN + "0/Esc" + CLR_RESET + " Cancel  Arrows/1-5");
        } else {
            printWindowCenteredInBox(gameWin, compactHeight, compactWidth, commandTop + 2,
                                     std::string(CLR_RED) + "No camera clusters available." + CLR_RESET);
        }
    } else {
        printWindowCenteredInBox(gameWin, compactHeight, compactWidth, commandTop + 1,
                                 std::string(CLR_CYAN) + "A" + CLR_RESET + " Sweep  " +
                                 CLR_CYAN + "Enter" + CLR_RESET + " Scan  " +
                                 CLR_CYAN + "G" + CLR_RESET + " Gate  " +
                                 CLR_CYAN + "L" + CLR_RESET + " Lure");
        printWindowCenteredInBox(gameWin, compactHeight, compactWidth, commandTop + 2,
                                 std::string(CLR_CYAN) + "W" + CLR_RESET + " Wait  " +
                                 CLR_CYAN + "Q" + CLR_RESET + " Save/Quit  " +
                                 CLR_CYAN + "H" + CLR_RESET + " Help  Move: Arrows");
    }

    wnoutrefresh(stdscr);
    wnoutrefresh(gameWin);
    doupdate();
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
    syncTerminalSize();
    std::vector<std::string> mapLines = renderMapLines(gs, cursorRoom);
    const int mapStartY = 8;
    const int maxLogEvents = 2;
    const int maxLogRows = 3;
    const int commandPanelRows = 5;
    int desiredLogRows = std::max(2, std::min(maxLogRows,
                                              static_cast<int>(gs.eventLog.size()) + 1));

    requiredGameHeight = std::max(GAME_MIN_HEIGHT,
                                  mapStartY + static_cast<int>(mapLines.size()) +
                                  1 + 2 + commandPanelRows);
    activeGameHeight = requiredGameHeight;

    if (terminalTooSmallForGameViewport()) {
        if (terminalCanUseCompactViewport()) {
            drawCompactGameInternal(gs, cursorRoom, selectingSweep, selectedGroup);
            return;
        }
        drawViewportTooSmallMessage();
        return;
    }

    int maxViewportHeight = std::min(GAME_MAX_HEIGHT, terminalRows);
    int maxFittingLogRows = std::max(0, maxViewportHeight - requiredGameHeight);
    int logContentRows = std::min(desiredLogRows, maxFittingLogRows);
    int compactCommandPanelTop = mapStartY + static_cast<int>(mapLines.size()) + 1 + 2 + logContentRows;
    activeGameHeight = std::min(maxViewportHeight,
                                std::max(requiredGameHeight, compactCommandPanelTop + commandPanelRows));

    int startY = getCenterY(activeGameHeight);
    int startX = getCenterX(GAME_WIDTH);
    if (!ensureGameWindow(activeGameHeight, GAME_WIDTH, startY, startX)) {
        drawViewportTooSmallMessage();
        return;
    }
    werase(gameWin);
    box(gameWin, 0, 0);

    int y = 1;
    std::stringstream header;
    header << CLR_BOLD << "PROTOCOL 1911 - HKU CAMPUS"
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
                        std::string(CLR_BOLD) +
                        (gs.lastScanOutput.empty() ? "[ SYSTEM EVENT LOG ]" : "[ SCAN RESULT ]") +
                        CLR_RESET,
                        leftWidth);
        printWindowAnsi(gameWin, panelTop, rightX,
                        std::string(CLR_BOLD) + "[ ENERGY COST ]" + CLR_RESET,
                        rightWidth);

        y = panelTop + 1;
        int availableRows = std::max(0, logPanelBottom - y);
        std::vector<std::string> infoLines =
            gs.lastScanOutput.empty()
                ? eventLogDisplayLines(gs.eventLog, maxLogEvents, availableRows, leftWidth)
                : scanOutputDisplayLines(gs, availableRows, leftWidth - 2);
        for (const std::string &line : infoLines) {
            if (y >= logPanelBottom)
                break;
            std::string prefix = gs.lastScanOutput.empty() ? std::string(CLR_DIM) : std::string(CLR_CYAN);
            std::string renderedLine = gs.lastScanOutput.empty() ? line : "> " + line;
            printWindowAnsi(gameWin, y++, leftX, prefix + renderedLine + CLR_RESET, leftWidth);
        }

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

    wnoutrefresh(stdscr);
    wnoutrefresh(gameWin);
    doupdate();
}

void drawGame(const GameState &gs, int cursorRoom) {
    drawGameInternal(gs, cursorRoom, false, 0);
}

void drawSweepSelection(const GameState &gs, int cursorRoom, int selectedGroup) {
    drawGameInternal(gs, cursorRoom, true, selectedGroup);
}

void drawEndGame(const GameState &gs) {
    clear();
    syncTerminalSize();
    int startY = std::max(1, (terminalRows - 11) / 2);
    mvaddCenteredCurses(startY, "============================================================", BOOT_GREEN, A_BOLD);
    if (gs.status == STATUS_WIN) {
        mvaddCenteredCurses(startY + 1, "Y O U   W I N !", BOOT_GREEN, A_BOLD);
        mvaddCenteredCurses(startY + 3, "You survived all " + std::to_string(gs.totalNights) + " nights at HKU!", BOOT_WHITE);
    } else if (gs.status == STATUS_LOSE_ENEMY) {
        mvaddCenteredCurses(startY + 1, "G A M E   O V E R", BOOT_RED, A_BOLD);
        mvaddCenteredCurses(startY + 3, "The intruder reached Main Building on", BOOT_WHITE);
        mvaddCenteredCurses(startY + 4, "Night " + std::to_string(gs.currentNight) + ", Turn " + std::to_string(gs.turn) + ".", BOOT_WHITE);
    } else if (gs.status == STATUS_LOSE_POWER) {
        mvaddCenteredCurses(startY + 1, "G A M E   O V E R", BOOT_RED, A_BOLD);
        mvaddCenteredCurses(startY + 3, "Power ran out on Night " + std::to_string(gs.currentNight) + ".", BOOT_WHITE);
    }
    mvaddCenteredCurses(startY + 6, gs.statusMessage, BOOT_WHITE);
    mvaddCenteredCurses(startY + 8, "============================================================", BOOT_GREEN, A_BOLD);
    mvaddCenteredCurses(startY + 10, "Press Enter to return to main menu...", BOOT_WHITE);
    refresh();
    waitForEnterInput();
}

void drawHelp() {
    clear();
    syncTerminalSize();
    int startY = std::max(0, (terminalRows - 20) / 2);
    mvaddCenteredCurses(startY, "================================================================", BOOT_GREEN, A_BOLD);
    mvaddCenteredCurses(startY + 1, "HOW TO PLAY - HKU CAMPUS", BOOT_WHITE, A_BOLD);
    mvaddCenteredCurses(startY + 2, "================================================================", BOOT_GREEN, A_BOLD);

    std::vector<std::string> help = {
        "OBJECTIVE:",
        "Survive until 6 AM by tracking intruders and protecting Main Building (MB).",
        "",
        "CONTROLS:",
        "Arrow Keys move cursor | Enter deep scans selected building",
        "[A] sweep | [G] gate | [L] lure | [W] wait | [H] help | [Q] save/quit",
        "",
        "CAMERA RINGS:",
        "- Quick Sweep reports movement in one cluster only.",
        "- Deep Scan checks the building under the cursor; MB is never clustered.",
        "- Signals decay after a few turns depending on difficulty.",
        "",
        "TIPS:",
        "- Use cheap sweeps to narrow down expensive deep scans.",
        "- Gate rooms next to MB control office entrances.",
        "- Closed gates cost energy every turn; lure can redirect movement."
    };

    int y = startY + 4;
    for (size_t i = 0; i < help.size() && y < terminalRows - 3; i++, y++) {
        int attrs = (!help[i].empty() && help[i][help[i].size() - 1] == ':') ? A_BOLD : A_NORMAL;
        int pair = attrs == A_BOLD ? BOOT_GREEN : BOOT_WHITE;
        mvaddCenteredCurses(y, help[i], pair, attrs);
    }
    mvaddCenteredCurses(terminalRows - 3, "================================================================", BOOT_GREEN, A_BOLD);
    mvaddCenteredCurses(terminalRows - 2, "Press Enter to go back...", BOOT_WHITE);
    refresh();
    waitForEnterInput();
}

void pause(const std::string &msg) {
    syncTerminalSize();
    std::string prompt = trimLeft(msg);
    printCentered(std::min(std::max(0, terminalRows - 3), getCenterY(1) + 12), prompt);
    if (cursesActive) {
        refresh();
        nodelay(stdscr, FALSE);
        int ch;
        do {
            ch = getch();
        } while (ch != '\n' && ch != '\r' && ch != 3 && !terminalInterruptRequested());
        flushinp();
        return;
    }

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
