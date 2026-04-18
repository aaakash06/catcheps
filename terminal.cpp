#include "terminal.h"
#include <termios.h>
#include <unistd.h>
#include <cstdio>

static struct termios origTermios;

void initTerminal() {
    tcgetattr(STDIN_FILENO, &origTermios);
    struct termios raw = origTermios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void restoreTerminal() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);
}

Key getKey() {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return KEY_UNKNOWN;

    if (c == '\n' || c == '\r') return KEY_ENTER;
    if (c == ' ') return KEY_SPACE;
    if (c == 27) { // ESC or arrow key
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_ESCAPE;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_ESCAPE;
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
    if (c == 's' || c == 'S') return KEY_S;
    if (c == 'd' || c == 'D') return KEY_D;
    if (c == 'c' || c == 'C') return KEY_C;
    if (c == 'l' || c == 'L') return KEY_L;
    if (c == 'r' || c == 'R') return KEY_R;
    if (c == 'e' || c == 'E') return KEY_E;
    if (c == 'q' || c == 'Q') return KEY_Q;
    if (c == 'h' || c == 'H') return KEY_H;
    if (c == '0') return KEY_0;
    if (c == '1') return KEY_1;
    if (c == '2') return KEY_2;
    if (c == '3') return KEY_3;
    if (c == '4') return KEY_4;
    if (c == '.') return KEY_DOT;
    if (c == '?') return KEY_QUESTION;

    return KEY_UNKNOWN;
}
