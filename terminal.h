#ifndef TERMINAL_H
#define TERMINAL_H

// Key codes returned by getKey()
enum Key {
    KEY_UNKNOWN = 0,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_ENTER, KEY_SPACE, KEY_ESCAPE,
    KEY_W, KEY_A,
    KEY_G, KEY_L, KEY_Q, KEY_H,
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5,
    KEY_QUESTION,
};

// Enter raw terminal mode (no echo, no line buffering)
void initTerminal();

// Restore terminal to original settings
void restoreTerminal();

// Read a single keypress. Blocks until a key is pressed.
Key getKey();

#endif
