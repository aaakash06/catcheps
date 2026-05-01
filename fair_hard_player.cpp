#include "game.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

static const int MB = 0;
static const int LIB = 2;
static const int KAD = 3;
static const int KNOW = 4;

static bool isGate(int room) {
    return room == KAD || room == LIB || room == KNOW;
}

static bool roomInGroupPublic(int room, int group) {
    if (group == 0) return room == 8 || room == 10 || room == 12;
    if (group == 1) return room == 7 || room == 3;
    if (group == 2) return room == 5 || room == 6 || room == 2;
    if (group == 3) return room == 11 || room == 9 || room == 4;
    if (group == 4) return room == 3 || room == 4 || room == 2;
    return false;
}

static std::string roomCode(int room) {
    static const char* names[] = {
        "MB", "KKL", "LIB", "KAD", "KNOW", "CYM", "HC",
        "HW", "MW", "RM", "RHS", "RR", "JL"
    };
    if (room < 0 || room >= 13) return "?";
    return names[room];
}

static int parseDetectedRoom(const std::vector<std::string>& lines) {
    for (const std::string& line : lines) {
        if (line.find("Enemy detected") == std::string::npos)
            continue;
        for (int room = 0; room < 13; room++) {
            std::string needle = "at " + roomCode(room);
            if (line.find(needle) != std::string::npos)
                return room;
        }
    }
    return -1;
}

static bool scanSaysMovement(const GameState& gs) {
    for (const std::string& line : gs.lastScanOutput) {
        if (line.find("Enemy detected") != std::string::npos ||
            line.find("Movement detected") != std::string::npos)
            return true;
    }
    return false;
}

static bool gateClosed(const GameState& gs, int room) {
    if (room == KAD) return gs.rightGateClosed;
    if (room == LIB) return gs.centerGateClosed;
    if (room == KNOW) return gs.leftGateClosed;
    return false;
}

static bool edgeBlocked(const GameState& gs, int from, int to) {
    if ((from == MB && to == KAD) || (from == KAD && to == MB)) return gs.rightGateClosed;
    if ((from == MB && to == LIB) || (from == LIB && to == MB)) return gs.centerGateClosed;
    if ((from == MB && to == KNOW) || (from == KNOW && to == MB)) return gs.leftGateClosed;
    return false;
}

static std::set<int> advanceBelief(const GameState& gs, const std::set<int>& beforeMove) {
    std::set<int> after;
    for (int room : beforeMove) {
        const std::vector<int>& nbs = gs.gameMap.rooms[room].neighbors;
        bool moved = false;
        for (int nb : nbs) {
            if (!edgeBlocked(gs, room, nb)) {
                after.insert(nb);
                moved = true;
            }
        }
        if (!moved)
            after.insert(room);
    }
    after.erase(MB);
    return after;
}

static void printBelief(const std::set<int>& belief) {
    std::cout << "{";
    bool first = true;
    for (int room : belief) {
        if (!first) std::cout << ",";
        std::cout << roomCode(room);
        first = false;
    }
    std::cout << "}";
}

static int chooseScanTarget(const std::set<int>& belief) {
    const int priority[] = {KAD, LIB, KNOW, 6, 9, 7, 5, 11, 10, 8, 12};
    for (int room : priority) {
        if (belief.count(room))
            return room;
    }
    return KAD;
}

static int chooseGateToClose(const GameState& gs, const std::set<int>& belief) {
    for (int gate : {KAD, LIB, KNOW}) {
        if (belief.count(gate) && !gateClosed(gs, gate))
            return gate;
    }
    if (belief.count(6)) {
        if (!gateClosed(gs, KAD)) return KAD;
        if (!gateClosed(gs, LIB)) return LIB;
    }
    if (belief.count(9) && !gateClosed(gs, KNOW))
        return KNOW;
    if (belief.count(7) && !gateClosed(gs, KAD))
        return KAD;
    return -1;
}

static int chooseGateToOpen(const GameState& gs, const std::set<int>& belief) {
    bool nearKAD = belief.count(KAD) || belief.count(6) || belief.count(7);
    bool nearLIB = belief.count(LIB) || belief.count(6);
    bool nearKNOW = belief.count(KNOW) || belief.count(9);
    for (int gate : {KAD, LIB, KNOW}) {
        bool near = (gate == KAD && nearKAD) ||
                    (gate == LIB && nearLIB) ||
                    (gate == KNOW && nearKNOW);
        if (gateClosed(gs, gate) && !near)
            return gate;
    }
    return -1;
}

int main(int argc, char** argv) {
    unsigned seed = argc > 1 ? static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10))
                             : static_cast<unsigned>(std::time(nullptr));
    std::srand(seed);

    GameState gs;
    gs.init(HARD);

    std::set<int> belief;
    int initial = parseDetectedRoom(gs.eventLog);
    if (initial >= 0)
        belief.insert(initial);

    std::cout << "seed=" << seed << " initial=" << roomCode(initial) << "\n";

    while (gs.status == STATUS_PLAYING) {
        int action = 5;
        int param = -1;
        std::string label = "wait";

        int closeGate = chooseGateToClose(gs, belief);
        int openGate = chooseGateToOpen(gs, belief);
        if (closeGate >= 0) {
            action = 3;
            param = closeGate;
            label = "close " + roomCode(closeGate);
        } else if (belief.size() > 1 && gs.power > 20) {
            action = 2;
            param = chooseScanTarget(belief);
            label = "scan " + roomCode(param);
        } else if (openGate >= 0) {
            action = 3;
            param = openGate;
            label = "open " + roomCode(openGate);
        }

        std::cout << "night=" << gs.currentNight
                  << " turn=" << (gs.turn + 1)
                  << " power=" << gs.power
                  << " belief=";
        printBelief(belief);
        std::cout << " action=" << label << "\n";

        std::set<int> beforeMove = belief;
        gs.doTurn(action, param);

        if (action == 2) {
            bool hit = scanSaysMovement(gs);
            if (hit) {
                beforeMove.clear();
                beforeMove.insert(param);
            } else {
                beforeMove.erase(param);
            }
        } else if (action == 1) {
            bool hit = scanSaysMovement(gs);
            std::set<int> filtered;
            for (int room : beforeMove) {
                if (roomInGroupPublic(room, param) == hit)
                    filtered.insert(room);
            }
            beforeMove = filtered;
        }

        int confirmed = parseDetectedRoom(gs.lastScanOutput);
        if (confirmed >= 0) {
            beforeMove.clear();
            beforeMove.insert(confirmed);
        }

        belief = advanceBelief(gs, beforeMove);
        if (belief.empty()) {
            for (int room = 1; room < 13; room++)
                belief.insert(room);
        }

        if (gs.turn == 0 && gs.status == STATUS_PLAYING) {
            belief.clear();
            int detected = parseDetectedRoom(gs.eventLog);
            if (detected >= 0)
                belief.insert(detected);
            else
                for (int room = 1; room < 13; room++)
                    belief.insert(room);
        }
    }

    std::cout << "status=" << gs.status
              << " night=" << gs.currentNight
              << " turn=" << gs.turn
              << " power=" << gs.power
              << " message=" << gs.statusMessage << "\n";
    return gs.status == STATUS_WIN ? 0 : 1;
}
