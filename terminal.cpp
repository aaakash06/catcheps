#include "terminal.h"
#include <termios.h>
#include <unistd.h>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <poll.h>

static struct termios origTermios;
static bool termiosActive = false;
static bool handlersActive = false;
static volatile sig_atomic_t interruptSignal = 0;
static struct sigaction oldSigint;
static struct sigaction oldSigterm;
static struct sigaction oldSighup;

static void handleInterruptSignal(int sig) {
    interruptSignal = sig;
}

static void installInterruptHandlers() {
    if (handlersActive)
        return;

    struct sigaction action;
    sigemptyset(&action.sa_mask);
    action.sa_handler = handleInterruptSignal;
    action.sa_flags = 0;

    sigaction(SIGINT, &action, &oldSigint);
    sigaction(SIGTERM, &action, &oldSigterm);
    sigaction(SIGHUP, &action, &oldSighup);
    handlersActive = true;
}

static void restoreInterruptHandlers() {
    if (!handlersActive)
        return;

    sigaction(SIGINT, &oldSigint, nullptr);
    sigaction(SIGTERM, &oldSigterm, nullptr);
    sigaction(SIGHUP, &oldSighup, nullptr);
    handlersActive = false;
}

static int readByteWithTimeout(int timeoutMs) {
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int ready;
    do {
        ready = poll(&pfd, 1, timeoutMs);
    } while (ready < 0 && errno == EINTR && !terminalInterruptRequested());

    if (terminalInterruptRequested())
        return -2;
    if (ready <= 0 || !(pfd.revents & POLLIN))
        return -1;

    unsigned char c;
    ssize_t n;
    do {
        n = read(STDIN_FILENO, &c, 1);
    } while (n < 0 && errno == EINTR && !terminalInterruptRequested());

    if (terminalInterruptRequested())
        return -2;
    if (n != 1)
        return -1;
    return c;
}

void initTerminal() {
    if (!termiosActive) {
        if (tcgetattr(STDIN_FILENO, &origTermios) != 0) {
            installInterruptHandlers();
            return;
        }
        termiosActive = true;
    }
    interruptSignal = 0;
    struct termios raw = origTermios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    installInterruptHandlers();
}

void restoreTerminal() {
    if (termiosActive) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);
        termiosActive = false;
    }
    restoreInterruptHandlers();
}

bool terminalInterruptRequested() {
    return interruptSignal != 0;
}

Key getKey() {
    unsigned char c;
    ssize_t n;
    do {
        n = read(STDIN_FILENO, &c, 1);
    } while (n < 0 && errno == EINTR && !terminalInterruptRequested());

    if (terminalInterruptRequested()) return KEY_INTERRUPT;
    if (n != 1) return KEY_UNKNOWN;
    if (c == 3) return KEY_INTERRUPT;

    if (c == '\n' || c == '\r') return KEY_ENTER;
    if (c == ' ') return KEY_SPACE;
    if (c == 27) { // ESC or arrow key
        int first = readByteWithTimeout(75);
        if (first == -2) return KEY_INTERRUPT;
        if (first < 0) return KEY_ESCAPE;
        int second = readByteWithTimeout(75);
        if (second == -2) return KEY_INTERRUPT;
        if (second < 0) return KEY_ESCAPE;
        char seq[2] = {static_cast<char>(first), static_cast<char>(second)};
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
            }
        }
        return KEY_ESCAPE;
    }
    if (c == 'w' || c == 'W') return KEY_W;
    if (c == 'a' || c == 'A') return KEY_A;
    if (c == 'g' || c == 'G') return KEY_G;
    if (c == 'l' || c == 'L') return KEY_L;
    if (c == 'q' || c == 'Q') return KEY_Q;
    if (c == 'h' || c == 'H') return KEY_H;
    if (c == '0') return KEY_0;
    if (c == '1') return KEY_1;
    if (c == '2') return KEY_2;
    if (c == '3') return KEY_3;
    if (c == '4') return KEY_4;
    if (c == '5') return KEY_5;
    if (c == '?') return KEY_QUESTION;

    return KEY_UNKNOWN;
}
