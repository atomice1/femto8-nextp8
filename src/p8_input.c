/**
 * Copyright (C) 2026 Chris January, Ben Baker, and contributors
 */

#include <string.h>

#include "p8_emu.h"
#include "p8_input.h"

#if defined(SDL)
#include "SDL.h"
#elif defined(__DA1470x__)
#include "gdi.h"
#elif defined(NEXTP8)
#include "nextp8.h"
#endif

#ifdef SDL
/* Expose window from p8_emu for mouse grab/relative mode changes. */
extern SDL_Window *m_window;
#endif

#define DEFAULT_AUTO_REPEAT_DELAY_MS (DEFAULT_AUTO_REPEAT_DELAY * 1000 / 30)
#define DEFAULT_AUTO_REPEAT_INTERVAL_MS (DEFAULT_AUTO_REPEAT_INTERVAL * 1000 / 30)

int16_t m_mouse_x, m_mouse_y;
int16_t mouse_x4, mouse_y4;
int16_t m_mouse_xrel, m_mouse_yrel;
uint8_t m_mouse_buttons, m_mouse_buttonsp;
int8_t m_mouse_wheel;
unsigned m_mouse_keymod;
static uint8_t m_mouse_click_buttons;
static int16_t m_mouse_click_x;
static int16_t m_mouse_click_y;
static unsigned m_mouse_click_mod;
#ifdef NEXTP8
static int16_t mouse_x_accum_prev = 0;
static int16_t mouse_y_accum_prev = 0;
static int16_t mouse_z_accum_prev = 0;
#endif

static uint8_t m_keypress;
static unsigned m_keymod;
static unsigned m_scancode;
bool m_scancodes[NUM_SCANCODES];

uint16_t m_buttons[PLAYER_COUNT];
uint16_t m_buttonsp[PLAYER_COUNT];
static uint16_t m_button_first_repeat[PLAYER_COUNT];
static unsigned m_button_down_time[PLAYER_COUNT][BUTTON_INTERNAL_COUNT];
#ifdef SDL
static uint16_t m_buttons_latch[PLAYER_COUNT];
#endif

static bool m_prev_pointer_lock;

#ifdef SDL
static void update_buttons(int index, int button, bool state)
{
    uint16_t mask = m_buttons[index];
    mask = state ? mask | (1 << button) : mask & ~(1 << button);
    m_buttons[index] = mask;
    m_memory[MEMORY_BUTTON_STATE + index] = mask & 0xff;
    if (state)
        m_buttons_latch[index] |= (1 << button);
}
#endif

#ifdef NEXTP8
#define KEY_CURSOR_LEFT  235
#define KEY_CURSOR_DOWN  242
#define KEY_CURSOR_RIGHT 244
#define KEY_CURSOR_UP    245
#define KEY_Z             26
#define KEY_X             34
#define KEY_N             49
#define KEY_M             58
#define KEY_C             33
#define KEY_V             42
#define KEY_S             27
#define KEY_D             35
#define KEY_F             43
#define KEY_E             36
#define KEY_TAB           13
#define KEY_Q             21
#define KEY_LEFT_SHIFT    18
#define KEY_A             28
#define KEY_RIGHT_SHIFT   89
#define KEY_ENTER         0x5a
#define KEY_BREAK         0x76
#define KEY_P             0x4d
#define KEY_PAGE_UP       0xfd
#define KEY_PAGE_DOWN     0xfa
#define KEY_LEFT_ALT      0x11
#define KEY_RIGHT_ALT     0x91
#define KEY_LEFT_CTRL     0x14
#define KEY_RIGHT_CTRL    0x94
#define KEY_LEFT_GUI      0x9F
#define KEY_RIGHT_GUI     0xA7

#define JOY_UP      (1 << 0)
#define JOY_DOWN    (1 << 1)
#define JOY_LEFT    (1 << 2)
#define JOY_RIGHT   (1 << 3)
#define JOY_BUTTON1 (1 << 4)
#define JOY_BUTTON2 (1 << 5)

char ps2_scancode_to_name[2][256] = {
    // Unshifted
    {
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F9, F5, F3, F1, F2, F12
        '\0', '\0', '\0', '\0', '\0', '\t', '`', '\0',     // F10, F8, F6, F4, tab, ` (back tick)
        '\0', '\0', '\0', '\0', '\0', 'q', '1', '\0',      // left alt, left shift, left control, Q, 1
        '\0', '\0', 'z', 's', 'a', 'w', '2', '\0',         // Z, S, A, W, 2
        '\0', 'c', 'x', 'd', 'e', '4', '3', '\0',          // C, X, D, E, 4, 3
        '\0', ' ', 'v', 'f', 't', 'r', '5', '\0',          // space, V, F, T, R, 5
        '\0', 'n', 'b', 'h', 'g', 'y', '6', '\0',          // N, B, H, G, Y, 6
        '\0', '\0', 'm', 'j', 'u', '7', '8', '\0',         // M, J, U, 7, 8
        '\0', ',', 'k', 'i', 'o', '0', '9', '\0',          // ,, K, I, O, 0 (zero), 9
        '\0', '.', '/', 'l', ';', 'p', '-', '\0',          // ., /, L, ;, P, -
        '\0', '\0', '\'', '\0', '[', '=', '\0', '\0',      // ', [, =
        '\0', '\0', '\n', ']', '\0', '\\', '\0', '\0',     // CapsLock, right shift, enter, ],
        '\0', '\0', '\0', '\0', '\0', '\0', '\b', '\0',    // backspace
        '\0', '1', '\0', '4', '7', '\0', '\0', '\0',       // KP 1, KP 4, KP 7
        '0', '.', '2', '\0', '6', '8', '\0', '\0',         // KP 0, KP ., KP 2, KP 5, KP 6, KP 8, escape, NumberLock
        '\0', '\0', '3', '\0', '\0', '9', '\0', '\0',      // F11, KP +, KP 3, KP -, KP *, KP 9, ScrollLock
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // search, right alt, right control, previous track
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // favourites, left GUI
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // refresh, volume down, mute, right GUI
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // stop, calculator, apps
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // forward, volume up, play/pause, power
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // back, home, stop, sleep
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // my computer
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // email, KP /, next track
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // media select
        '\0', '\0', '\n', '\0', '\0', '\0', '\0', '\0',    // KP enter, wake
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // end, cursor left, home
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // insert, delete, cursor down, cursor right, cursor up
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // page down, page up
    },
    // Shift
    {
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F9, F5, F3, F1, F2, F12
        '\0', '\0', '\0', '\0', '\0', '\t', '~', '\0',     // F10, F8, F6, F4, tab, ` (back tick)
        '\0', '\0', '\0', '\0', '\0', 'Q', '!', '\0',      // left alt, left shift, left control, Q, 1
        '\0', '\0', 'Z', 'S', 'A', 'W', '@', '\0',         // Z, S, A, W, 2
        '\0', 'C', 'X', 'D', 'E', '$', '#', '\0',          // C, X, D, E, 4, 3
        '\0', ' ', 'V', 'F', 'T', 'R', '%', '\0',          // space, V, F, T, R, 5
        '\0', 'N', 'B', 'H', 'G', 'Y', '^', '\0',          // N, B, H, G, Y, 6
        '\0', '\0', 'M', 'J', 'U', '&', '*', '\0',         // M, J, U, 7, 8
        '\0', '<', 'K', 'I', 'O', ')', '(', '\0',          // ,, K, I, O, 0 (zero), 9
        '\0', '>', '?', 'L', ':', 'P', '_', '\0',          // ., /, L, ;, P, -
        '\0', '\0', '"', '\0', '{', '+', '\0', '\0',       // ', [, =
        '\0', '\0', '\n', '}', '\0', '|', '\0', '\0',      // CapsLock, right shift, enter, ],
        '\0', '\0', '\0', '\0', '\0', '\0', '\b', '\0',    // backspace
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // KP 1, KP 4, KP 7
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // KP 0, KP ., KP 2, KP 5, KP 6, KP 8, escape, NumberLock
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F11, KP +, KP 3, KP -, KP *, KP 9, ScrollLock
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // search, right alt, right control, previous track
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // favourites, left GUI
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // refresh, volume down, mute, right GUI
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // stop, calculator, apps
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // forward, volume up, play/pause, power
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // back, home, stop, sleep
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // my computer
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // email, KP /, next track
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // media select
        '\0', '\0', '\n', '\0', '\0', '\0', '\0', '\0',    // KP enter, wake
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // end, cursor left, home
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // insert, delete, cursor down, cursor right, cursor up
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // page down, page up
    },
};

char memb_scancode_to_name[4][256] = {
    // Unshifted
    {
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F9, F5, F3, F1, F2, F12
        '\0', '\0', '\0', '\0', '\0', '\t', '`', '\0',     // F10, F8, F6, F4, tab, ` (back tick)
        '\0', '\0', '\0', '\0', '\0', 'q', '1', '\0',      // left alt, left shift, left control, Q, 1
        '\0', '\0', 'z', 's', 'a', 'w', '2', '\0',         // Z, S, A, W, 2
        '\0', 'c', 'x', 'd', 'e', '4', '3', '\0',          // C, X, D, E, 4, 3
        '\0', ' ', 'v', 'f', 't', 'r', '5', '\0',          // space, V, F, T, R, 5
        '\0', 'n', 'b', 'h', 'g', 'y', '6', '\0',          // N, B, H, G, Y, 6
        '\0', '\0', 'm', 'j', 'u', '7', '8', '\0',         // M, J, U, 7, 8
        '\0', ',', 'k', 'i', 'o', '0', '9', '\0',          // ,, K, I, O, 0 (zero), 9
        '\0', '.', '/', 'l', ';', 'p', '-', '\0',          // ., /, L, ;, P, -
        '\0', '\0', '\'', '\0', '[', '=', '\0', '\0',      // ', [, =
        '\0', '\0', '\n', ']', '\0', '\\', '\0', '\0',     // CapsLock, right shift, enter, ],
        '\0', '\0', '\0', '\0', '\0', '\0', '\b', '\0',    // backspace
        '\0', '1', '\0', '4', '7', '\0', '\0', '\0',       // KP 1, KP 4, KP 7
        '0', '.', '2', '\0', '6', '8', '\0', '\0',         // KP 0, KP ., KP 2, KP 5, KP 6, KP 8, escape, NumberLock
        '\0', '\0', '3', '\0', '\0', '9', '\0', '\0',      // F11, KP +, KP 3, KP -, KP *, KP 9, ScrollLock
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // search, right alt, right control, previous track
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // favourites, left GUI
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // refresh, volume down, mute, right GUI
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // stop, calculator, apps
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // forward, volume up, play/pause, power
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // back, home, stop, sleep
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // my computer
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // email, KP /, next track
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // media select
        '\0', '\0', '\n', '\0', '\0', '\0', '\0', '\0',    // KP enter, wake
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // end, cursor left, home
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // insert, delete, cursor down, cursor right, cursor up
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // page down, page up
    },
    // Shift
    {
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F9, F5, F3, F1, F2, F12
        '\0', '\0', '\0', '\0', '\0', '\t', '~', '\0',     // F10, F8, F6, F4, tab, ` (back tick)
        '\0', '\0', '\0', '\0', '\0', 'Q', '!', '\0',      // left alt, left shift, left control, Q, 1
        '\0', '\0', 'Z', 'S', 'A', 'W', '@', '\0',         // Z, S, A, W, 2
        '\0', 'C', 'X', 'D', 'E', '$', '#', '\0',          // C, X, D, E, 4, 3
        '\0', ' ', 'V', 'F', 'T', 'R', '%', '\0',          // space, V, F, T, R, 5
        '\0', 'N', 'B', 'H', 'G', 'Y', '^', '\0',          // N, B, H, G, Y, 6
        '\0', '\0', 'M', 'J', 'U', '&', '*', '\0',         // M, J, U, 7, 8
        '\0', '<', 'K', 'I', 'O', ')', '(', '\0',          // ,, K, I, O, 0 (zero), 9
        '\0', '>', '?', 'L', ':', 'P', '_', '\0',          // ., /, L, ;, P, -
        '\0', '\0', '"', '\0', '{', '+', '\0', '\0',       // ', [, =
        '\0', '\0', '\n', '}', '\0', '|', '\0', '\0',      // CapsLock, right shift, enter, ],
        '\0', '\0', '\0', '\0', '\0', '\0', '\b', '\0',    // backspace
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // KP 1, KP 4, KP 7
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // KP 0, KP ., KP 2, KP 5, KP 6, KP 8, escape, NumberLock
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F11, KP +, KP 3, KP -, KP *, KP 9, ScrollLock
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // search, right alt, right control, previous track
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // favourites, left GUI
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // refresh, volume down, mute, right GUI
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // stop, calculator, apps
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // forward, volume up, play/pause, power
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // back, home, stop, sleep
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // my computer
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // email, KP /, next track
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // media select
        '\0', '\0', '\n', '\0', '\0', '\0', '\0', '\0',    // KP enter, wake
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // end, cursor left, home
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // insert, delete, cursor down, cursor right, cursor up
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // page down, page up
    },
    // Ctrl
    {
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F9, F5, F3, F1, F2, F12
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F10, F8, F6, F4, tab, ` (back tick)
        '\0', '\0', '\0', '\0', '\0', '\0', '!', '\0',     // left alt, left shift, left control, Q, 1
        '\0', '\0', ':', '\0', '\0', '\0', '@', '\0',      // Z, S, A, W, 2
        '\0', '?', '$', '\0', '\0', '$', '#', '\0',        // C, X, D, E, 4, 3
        '\0', '\0', '/', '\0', '>', '<', '%', '\0',        // space, V, F, T, R, 5
        '\0', ',', '*', '^', '\0', '\0', '&', '\0',        // N, B, H, G, Y, 6
        '\0', '\0', '.', '-', '\0', '\0', '(', '\0',       // M, J, U, 7, 8
        '\0', '\0', '+', '\0', ';', '_', ')', '\0',        // ,, K, I, O, 0 (zero), 9
        '\0', '\0', '\0', '=', '\0', '"', '\0', '\0',      // ., /, L, ;, P, -
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // ', [, =
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // CapsLock, right shift, enter, ],
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // backspace
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // KP 1, KP 4, KP 7
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // KP 0, KP ., KP 2, KP 5, KP 6, KP 8, escape, NumberLock
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F11, KP +, KP 3, KP -, KP *, KP 9, ScrollLock
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // search, right alt, right control, previous track
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // favourites, left GUI
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // refresh, volume down, mute, right GUI
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // stop, calculator, apps
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // forward, volume up, play/pause, power
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // back, home, stop, sleep
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // my computer
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // email, KP /, next track
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // media select
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // KP enter, wake
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // end, cursor left, home
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // insert, delete, cursor down, cursor right, cursor up
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // page down, page up
    },
    // Shift + Ctrl
    {
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F9, F5, F3, F1, F2, F12
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F10, F8, F6, F4, tab, ` (back tick)
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // left alt, left shift, left control, Q, 1
        '\0', '\0', '\0', '|', '~', '\0', '\0', '\0',      // Z, S, A, W, 2
        '\0', '\0', '\0', '\\', '\0', '\0', '\0', '\0',     // C, X, D, E, 4, 3
        '\0', '\0', '\0', '{', '\0', '\0', '\0', '\0',     // space, V, F, T, R, 5
        '\0', '\0', '\0', '\0', '}', '[', '\0', '\0',      // N, B, H, G, Y, 6
        '\0', '\0', '\0', '\0', ']', '\0', '\0', '\0',     // M, J, U, 7, 8
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // ,, K, I, O, 0 (zero), 9
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // ., /, L, ;, P, -
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // ', [, =
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // CapsLock, right shift, enter, ],
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // backspace
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // KP 1, KP 4, KP 7
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // KP 0, KP ., KP 2, KP 5, KP 6, KP 8, escape, NumberLock
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F11, KP +, KP 3, KP -, KP *, KP 9, ScrollLock
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // search, right alt, right control, previous track
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // favourites, left GUI
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // refresh, volume down, mute, right GUI
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // stop, calculator, apps
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // forward, volume up, play/pause, power
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // back, home, stop, sleep
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // my computer
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // email, KP /, next track
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // media select
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // KP enter, wake
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // 
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // end, cursor left, home
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // insert, delete, cursor down, cursor right, cursor up
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // page down, page up
    },
};

unsigned nextp8_scancode_to_sdl_scancode[NUM_SCANCODES] = {
    0, 66, 0, 62, 60, 58, 59, 69,    // F9, F5, F3, F1, F2, F12
    0, 67, 65, 63, 61, 43, 53, 0,    // F10, F8, F6, F4, tab, ` (back tick)
    0, 226, 225, 0, 224, 20, 30, 0,  // left alt, left shift, left control, Q, 1
    0, 0, 29, 22, 4, 26, 31, 0,      // Z, S, A, W, 2
    0, 6, 27, 7, 8, 33, 32, 0,       // C, X, D, E, 4, 3
    0, 44, 25, 9, 23, 21, 34, 0,     // space, V, F, T, R, 5
    0, 17, 5, 11, 10, 28, 35, 0,     // N, B, H, G, Y, 6
    0, 0, 16, 13, 24, 36, 37, 0,     // M, J, U, 7, 8
    0, 54, 14, 12, 18, 39, 38, 0,    // ,, K, I, O, 0 (zero), 9
    0, 55, 56, 15, 51, 19, 45, 0,    // ., /, L, ;, P, -
    0, 0, 52, 0, 47, 46, 0, 0,       // ', [, =
    57, 229, 158, 48, 0, 49, 0, 0,   // CapsLock, right shift, enter, ],
    0, 0, 0, 0, 0, 0, 42, 0,         // backspace
    0, 89, 0, 92, 95, 0, 0, 0,       // KP 1, KP 4, KP 7
    98, 99, 90, 93, 94, 96, 41, 131, // KP 0, KP ., KP 2, KP 5, KP 6, KP 8, escape, NumberLock
    68, 87, 91, 86, 85, 97, 71, 0,   // F11, KP +, KP 3, KP -, KP *, KP 9, ScrollLock
    0, 0, 0, 0, 0, 0, 0, 0,          // 
    0, 0, 0, 0, 0, 0, 0, 0,          // 
    0, 230, 0, 0, 228, 0, 0, 0,      // search, right alt, right control, previous track
    0, 0, 0, 0, 0, 0, 0, 227,        // favourites, left GUI
    0, 0, 0, 0, 0, 0, 0, 231,        // refresh, volume down, mute, right GUI
    0, 0, 0, 0, 0, 0, 0, 0,          // stop, calculator, apps
    0, 0, 0, 0, 0, 0, 0, 0,          // forward, volume up, play/pause, power
    0, 0, 0, 0, 0, 0, 0, 0,          // back, home, stop, sleep
    0, 0, 0, 0, 0, 0, 0, 0,          // my computer
    0, 0, 84, 0, 0, 0, 0, 0,         // email, KP /, next track
    0, 0, 0, 0, 0, 0, 0, 0,          // media select
    0, 0, 88, 0, 0, 0, 0, 0,         // KP enter, wake
    0, 0, 0, 0, 0, 0, 0, 0,          // 
    0, 77, 0, 80, 74, 0, 0, 0,       // end, cursor left, home
    73, 76, 81, 0, 79, 82, 0, 0,     // insert, delete, cursor down, cursor right, cursor up
    0, 0, 78, 0, 0, 75, 0, 0,        // page down, page up
};

static uint32_t keyboard_matrix_prev[8];

static unsigned is_down(volatile uint8_t *base, unsigned index)
{
    return base[index >> 3] & (1 << (index & 0x7));
}

static bool is_nextp8_modifier(unsigned index)
{
    switch (index) {
        case KEY_LEFT_SHIFT:
        case KEY_RIGHT_SHIFT:
        case KEY_LEFT_CTRL:
        case KEY_RIGHT_CTRL:
        case KEY_LEFT_ALT:
        case KEY_RIGHT_ALT:
        case KEY_LEFT_GUI:
        case KEY_RIGHT_GUI:
            return true;
        default:
            return false;
    }
}

static uint16_t player0_mask(volatile uint8_t *keyboard_matrix, uint8_t joy0)
{
    uint16_t mask = 0;
    if (is_down(keyboard_matrix, KEY_CURSOR_LEFT) ||
        (joy0 & JOY_LEFT))
        mask |= BUTTON_MASK_LEFT;
    if (is_down(keyboard_matrix, KEY_CURSOR_RIGHT) ||
        (joy0 & JOY_RIGHT))
        mask |= BUTTON_MASK_RIGHT;
    if (is_down(keyboard_matrix, KEY_CURSOR_UP) ||
        (joy0 & JOY_UP))
        mask |= BUTTON_MASK_UP;
    if (is_down(keyboard_matrix, KEY_CURSOR_DOWN) ||
        (joy0 & JOY_DOWN))
        mask |= BUTTON_MASK_DOWN;
    if (is_down(keyboard_matrix, KEY_Z) ||
        is_down(keyboard_matrix, KEY_N) ||
        is_down(keyboard_matrix, KEY_C) ||
        is_down(keyboard_matrix, KEY_ENTER) ||
        (joy0 & JOY_BUTTON1))
        mask |= BUTTON_MASK_ACTION1;
    if (is_down(keyboard_matrix, KEY_X) ||
        is_down(keyboard_matrix, KEY_M) ||
        is_down(keyboard_matrix, KEY_V) ||
        (joy0 & JOY_BUTTON2))
        mask |= BUTTON_MASK_ACTION2;
    if (is_down(keyboard_matrix, KEY_ENTER))
        mask |= BUTTON_MASK_PAUSE | BUTTON_MASK_RETURN;
    if (is_down(keyboard_matrix, KEY_P))
        mask |= BUTTON_MASK_PAUSE;
    if (is_down(keyboard_matrix, KEY_BREAK))
        mask |= BUTTON_MASK_ESCAPE;
    if (is_down(keyboard_matrix, KEY_PAGE_UP))
        mask |= BUTTON_MASK_PAGE_UP;
    if (is_down(keyboard_matrix, KEY_PAGE_DOWN))
        mask |= BUTTON_MASK_PAGE_DOWN;
    return mask;
}

static uint16_t player1_mask(volatile uint8_t *keyboard_matrix, uint8_t joy1)
{
    uint16_t mask = 0;
    if (is_down(keyboard_matrix, KEY_S) ||
        (joy1 & JOY_LEFT))
        mask |= BUTTON_MASK_LEFT;
    if (is_down(keyboard_matrix, KEY_D) ||
        (joy1 & JOY_RIGHT))
        mask |= BUTTON_MASK_RIGHT;
    if (is_down(keyboard_matrix, KEY_F) ||
        (joy1 & JOY_UP))
        mask |= BUTTON_MASK_UP;
    if (is_down(keyboard_matrix, KEY_E) ||
        (joy1 & JOY_DOWN))
        mask |= BUTTON_MASK_DOWN;
    if (is_down(keyboard_matrix, KEY_TAB) ||
        is_down(keyboard_matrix, KEY_LEFT_SHIFT) ||
        (joy1 & JOY_BUTTON1))
        mask |= BUTTON_MASK_ACTION1;
    if (is_down(keyboard_matrix, KEY_Q) ||
        is_down(keyboard_matrix, KEY_A) ||
        (joy1 & JOY_BUTTON2))
        mask |= BUTTON_MASK_ACTION2;
    return mask;
}
#endif

static bool is_modifier(unsigned sdl2_sc)
{
    switch (sdl2_sc) {
        case 225:
        case 229:
        case 224:
        case 228:
        case 226:
        case 230:
        case 227:
        case 231:
            return true;
        default:
            return false;
    }
}

static void clear_input_queue(void)
{
    m_keypress = 0;
    m_keymod = 0;
    m_scancode = 0;
    m_mouse_click_buttons = 0;
    m_mouse_click_x = 0;
    m_mouse_click_y = 0;
    m_mouse_click_mod = 0;
}

static void queue_keypress(unsigned scancode, uint8_t keychar, unsigned mod)
{
    if (is_modifier(scancode) && m_scancode != 0)
        return;
    m_scancode = scancode;
    m_keypress = keychar;
    m_keymod = mod;
}

static void queue_mouse_click(int buttons, int x, int y, unsigned mod)
{
    m_mouse_click_buttons = buttons;
    m_mouse_click_x = x;
    m_mouse_click_y = y;
    m_mouse_click_mod = mod;
}

bool p8_get_next_keypress(unsigned *scancode, uint8_t *keychar, unsigned *mod)
{
    bool has_keypress = (m_keypress != 0) || (m_scancode != 0);

    if (scancode) *scancode = m_scancode;
    if (keychar) *keychar = m_keypress;
    if (mod) *mod = m_keymod;

    m_scancode = 0;
    m_keypress = 0;
    m_keymod = 0;

    return has_keypress;
}

bool p8_get_next_mouse_click(int *x, int *y, int *button, unsigned *mod)
{
    bool has_click = (m_mouse_click_buttons != 0);

    if (x) *x = m_mouse_click_x;
    if (y) *y = m_mouse_click_y;
    if (button) *button = m_mouse_click_buttons;

    m_mouse_click_x = 0;
    m_mouse_click_y = 0;
    m_mouse_click_buttons = 0;

    return has_click;
}

bool p8_has_pending_keypress(void)
{
    return (m_keypress != 0);
}

void p8_init_input(void)
{
#if defined(NEXTP8)
    memset((void *)_KEYBOARD_MATRIX_LATCHED, 0xff, 32);
    *(volatile uint8_t *)_JOYSTICK0_LATCHED = 0xff;
    *(volatile uint8_t *)_JOYSTICK1_LATCHED = 0xff;
    *(volatile uint8_t *)_MOUSE_BUTTONS_LATCHED = 0xff;
#endif

    p8_reset_input();
}

void p8_pump_events(void)
{
#if defined(SDL)
    SDL_PumpEvents();

    // SDL_QUIT must be consumed and acted on immediately.
    SDL_Event event;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_QUIT, SDL_QUIT) > 0)
        p8_abort();

    // Consume all pending keydown/keyup events.  Handle pause/escape immediately,
    // and re-queue everything else so p8_update_input processes it normally.
    SDL_Event keyevents[64];
    int n = SDL_PeepEvents(keyevents, 64, SDL_GETEVENT, SDL_KEYDOWN, SDL_KEYUP);
    for (int i = 0; i < n; i++) {
        SDL_Keycode sym = keyevents[i].key.keysym.sym;
        bool is_pause = (sym == SDLK_RETURN || sym == SDLK_p);
        bool is_escape = (sym == INPUT_ESCAPE);
        if (is_pause || is_escape) {
            if (sym == SDLK_RETURN) {
                if (keyevents[i].type == SDL_KEYDOWN) {
                    if ((m_buttons[0] & BUTTON_MASK_PAUSE) == 0)
                        m_buttonsp[0] |= BUTTON_MASK_PAUSE;
                    if ((m_buttons[0] & BUTTON_MASK_RETURN) == 0)
                        m_buttonsp[0] |= BUTTON_MASK_RETURN;
                    m_buttons[0] |= BUTTON_MASK_PAUSE | BUTTON_MASK_RETURN;
                    m_button_down_time[0][BUTTON_PAUSE] = UINT_MAX;
                    m_button_down_time[0][BUTTON_RETURN] = UINT_MAX;
                } else {
                    m_buttons[0] &= ~(BUTTON_MASK_PAUSE | BUTTON_MASK_RETURN);
                }
            } else if (sym == SDLK_p) {
                if (keyevents[i].type == SDL_KEYDOWN) {
                    if ((m_buttons[0] & BUTTON_MASK_PAUSE) == 0)
                        m_buttonsp[0] |= BUTTON_MASK_PAUSE;
                    m_buttons[0] |= BUTTON_MASK_PAUSE;
                    m_button_down_time[0][BUTTON_PAUSE] = UINT_MAX;
                } else {
                    m_buttons[0] &= ~BUTTON_MASK_PAUSE;
                }
            } else if (sym == INPUT_ESCAPE) {
                if (keyevents[i].type == SDL_KEYDOWN) {
                    if ((m_buttons[0] & BUTTON_MASK_ESCAPE) == 0)
                        m_buttonsp[0] |= BUTTON_MASK_ESCAPE;
                    m_buttons[0] |= BUTTON_MASK_ESCAPE;
                    m_button_down_time[0][BUTTON_ESCAPE] = UINT_MAX;
                } else {
                    m_buttons[0] &= ~BUTTON_MASK_ESCAPE;
                }
            }
        } else {
            SDL_PushEvent(&keyevents[i]);
        }
    }
#elif defined(NEXTP8)
    volatile uint8_t *keyboard_matrix = (volatile uint8_t *) _KEYBOARD_MATRIX;
    bool escape_down = is_down(keyboard_matrix, KEY_BREAK);
    if (escape_down) {
        if ((m_buttons[0] & BUTTON_MASK_ESCAPE) == 0)
            m_buttonsp[0] |= BUTTON_MASK_ESCAPE;
        m_buttons[0] |= BUTTON_MASK_ESCAPE;
        m_button_down_time[0][BUTTON_ESCAPE] = UINT_MAX;
    } else {
        m_buttons[0] &= ~BUTTON_MASK_ESCAPE;
    }
    bool p_down = is_down(keyboard_matrix, KEY_P);
    if (p_down) {
        if ((m_buttons[0] & BUTTON_MASK_PAUSE) == 0)
            m_buttonsp[0] |= BUTTON_MASK_PAUSE;
        m_buttons[0] |= BUTTON_MASK_PAUSE;
        m_button_down_time[0][BUTTON_PAUSE] = UINT_MAX;
    } else {
        m_buttons[0] &= ~BUTTON_MASK_PAUSE;
    }
    bool enter_down = is_down(keyboard_matrix, KEY_ENTER);
    if (enter_down) {
        if ((m_buttons[0] & BUTTON_MASK_PAUSE) == 0)
            m_buttonsp[0] |= BUTTON_MASK_PAUSE;
        if ((m_buttons[0] & BUTTON_MASK_RETURN) == 0)
            m_buttonsp[0] |= BUTTON_MASK_RETURN;
        m_buttons[0] |= BUTTON_MASK_PAUSE | BUTTON_MASK_RETURN;
        m_button_down_time[0][BUTTON_PAUSE] = UINT_MAX;
       m_button_down_time[0][BUTTON_RETURN] = UINT_MAX;
    } else {
        m_buttons[0] &= ~(BUTTON_MASK_PAUSE | BUTTON_MASK_RETURN);
    }
#endif
}

void p8_reset_input()
{
    for (unsigned p=0;p<2;++p) {
        for (unsigned i=0;i<BUTTON_INTERNAL_COUNT;++i) {
            m_button_down_time[p][i] = UINT_MAX;
        }
        m_button_first_repeat[p] = 1;
    }
}

void p8_update_input()
{
    bool pointer_lock = (m_memory[MEMORY_DEVKIT_MODE] & 0x4) != 0;
    if (pointer_lock != m_prev_pointer_lock) {
        m_prev_pointer_lock  = pointer_lock;
#ifdef SDL
        SDL_SetRelativeMouseMode(pointer_lock ? SDL_TRUE : SDL_FALSE);
        if (m_window)
            SDL_SetWindowGrab(m_window, pointer_lock ? SDL_TRUE : SDL_FALSE);
#endif
    }

    clear_input_queue();

#ifdef SDL
    m_mouse_xrel = 0;
    m_mouse_yrel = 0;
    m_mouse_wheel = 0;
    m_mouse_buttonsp = 0;

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_MOUSEMOTION:
            m_mouse_x = event.motion.x * P8_WIDTH / SCREEN_WIDTH;
            m_mouse_y = event.motion.y * P8_HEIGHT / SCREEN_HEIGHT;
            m_mouse_xrel += event.motion.xrel * P8_WIDTH / SCREEN_WIDTH;
            m_mouse_yrel += event.motion.yrel * P8_HEIGHT / SCREEN_HEIGHT;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == 1) {
                if (!m_mouse_buttons & 0x1)
                    m_mouse_buttonsp |= 0x1;
                m_mouse_buttons |= 0x1;
                if (m_memory[MEMORY_DEVKIT_MODE] & 0x2)
                    update_buttons(0, BUTTON_ACTION1, true);
            } else if (event.button.button == 3) {
                if (!(m_mouse_buttons & 0x2))
                    m_mouse_buttonsp |= 0x2;
                m_mouse_buttons |= 0x2;
                if (m_memory[MEMORY_DEVKIT_MODE] & 0x2)
                    update_buttons(0, BUTTON_ACTION2, true);
            } else if (event.button.button == 2) {
                if (!(m_mouse_buttons & 0x4))
                    m_mouse_buttonsp |= 0x4;
                m_mouse_buttons |= 0x4;
                if (m_memory[MEMORY_DEVKIT_MODE] & 0x2)
                    update_buttons(0, BUTTON_PAUSE, true);
            } else if (event.button.button == 4) {
                m_mouse_wheel += 1;
            } else if (event.button.button == 5) {
                m_mouse_wheel -= 1;
            }
            queue_mouse_click(event.button.button, event.button.x, event.button.y, m_mouse_keymod);
            break;
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == 1) {
                m_mouse_buttons &= ~0x1;
                if (m_memory[MEMORY_DEVKIT_MODE] & 0x2)
                    update_buttons(0, BUTTON_ACTION1, false);
            } else if (event.button.button == 3) {
                m_mouse_buttons &= ~0x2;
                if (m_memory[MEMORY_DEVKIT_MODE] & 0x2)
                    update_buttons(0, BUTTON_ACTION2, false);
            } else if (event.button.button == 2) {
                m_mouse_buttons &= ~0x4;
                if (m_memory[MEMORY_DEVKIT_MODE] & 0x2)
                    update_buttons(0, BUTTON_PAUSE, false);
            }
            break;
        case SDL_TEXTINPUT:
            if (event.text.text[0] != '\0')
                m_keypress = (uint8_t)event.text.text[0];
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym)
            {
            case INPUT_LEFT:
                update_buttons(0, BUTTON_LEFT, true);
                break;
            case INPUT_RIGHT:
                update_buttons(0, BUTTON_RIGHT, true);
                break;
            case INPUT_UP:
                update_buttons(0, BUTTON_UP, true);
                break;
            case INPUT_DOWN:
                update_buttons(0, BUTTON_DOWN, true);
                break;
            case INPUT_ACTION1:
                update_buttons(0, BUTTON_ACTION1, true);
                break;
            case INPUT_ACTION2:
                update_buttons(0, BUTTON_ACTION2, true);
                break;
            case INPUT_ESCAPE:
                update_buttons(0, BUTTON_ESCAPE, true);
                break;
            case SDLK_RETURN:
                update_buttons(0, BUTTON_PAUSE, true);
                update_buttons(0, BUTTON_RETURN, true);
                break;
            case SDLK_p:
                update_buttons(0, BUTTON_PAUSE, true);
                break;
            case SDLK_SPACE:
                update_buttons(0, BUTTON_SPACE, true);
                break;
            case SDLK_PAGEUP:
                update_buttons(0, BUTTON_PAGE_UP, true);
                break;
            case SDLK_PAGEDOWN:
                update_buttons(0, BUTTON_PAGE_DOWN, true);
                break;
            default:
                break;
            }
            {
                if (event.key.keysym.scancode > 0 && event.key.keysym.scancode < NUM_SCANCODES)
                    m_scancodes[event.key.keysym.scancode] = true;
                queue_keypress(event.key.keysym.scancode, event.key.keysym.sym < 32 ? event.key.keysym.sym : 0, event.key.keysym.mod);
                m_mouse_keymod = event.key.keysym.mod;
            }
            break;
        case SDL_KEYUP:
            switch (event.key.keysym.sym)
            {
            case INPUT_LEFT:
                update_buttons(0, BUTTON_LEFT, false);
                break;
            case INPUT_RIGHT:
                update_buttons(0, BUTTON_RIGHT, false);
                break;
            case INPUT_UP:
                update_buttons(0, BUTTON_UP, false);
                break;
            case INPUT_DOWN:
                update_buttons(0, BUTTON_DOWN, false);
                break;
            case INPUT_ACTION1:
                update_buttons(0, BUTTON_ACTION1, false);
                break;
            case INPUT_ACTION2:
                update_buttons(0, BUTTON_ACTION2, false);
                break;
            case SDLK_RETURN:
                update_buttons(0, BUTTON_PAUSE, false);
                update_buttons(0, BUTTON_RETURN, false);
                break;
            case SDLK_p:
                update_buttons(0, BUTTON_PAUSE, false);
                break;
            case SDLK_SPACE:
                update_buttons(0, BUTTON_SPACE, false);
                break;
            case SDLK_PAGEUP:
                update_buttons(0, BUTTON_PAGE_UP, false);
                break;
            case SDLK_PAGEDOWN:
                update_buttons(0, BUTTON_PAGE_DOWN, false);
                break;
            case INPUT_ESCAPE:
                update_buttons(0, BUTTON_ESCAPE, false);
                break;
            default:
                break;
            }
            {
                if (event.key.keysym.scancode > 0 && event.key.keysym.scancode < NUM_SCANCODES)
                    m_scancodes[event.key.keysym.scancode] = false;
                m_mouse_keymod = event.key.keysym.mod;
            }
            break;
        case SDL_QUIT:
            p8_quit();
            break;
        default:
            break;
        }
    }
#elif defined(OS_FREERTOS)
    uint16_t mask = 0;

    if (gamepad & AXIS_L_LEFT)
        mask |= BUTTON_MASK_LEFT;
    if (gamepad & AXIS_L_RIGHT)
        mask |= BUTTON_MASK_RIGHT;
    if (gamepad & AXIS_L_UP)
        mask |= BUTTON_MASK_UP;
    if (gamepad & AXIS_L_DOWN)
        mask |= BUTTON_MASK_DOWN;
    if (gamepad & AXIS_L_TRIGGER)
        mask |= BUTTON_MASK_ACTION1;
    if (gamepad & AXIS_R_LEFT)
        mask |= BUTTON_MASK_LEFT;
    if (gamepad & AXIS_R_RIGHT)
        mask |= BUTTON_MASK_RIGHT;
    if (gamepad & AXIS_R_UP)
        mask |= BUTTON_MASK_UP;
    if (gamepad & AXIS_R_DOWN)
        mask |= BUTTON_MASK_DOWN;
    if (gamepad & AXIS_R_TRIGGER)
        mask |= BUTTON_MASK_ACTION2;
    if (gamepad & DPAD_UP)
        mask |= BUTTON_MASK_UP;
    if (gamepad & DPAD_RIGHT)
        mask |= BUTTON_MASK_RIGHT;
    if (gamepad & DPAD_DOWN)
        mask |= BUTTON_MASK_DOWN;
    if (gamepad & DPAD_LEFT)
        mask |= BUTTON_MASK_LEFT;
    if (gamepad & BUTTON_1)
        mask |= BUTTON_MASK_ACTION1;
    if (gamepad & BUTTON_2)
        mask |= BUTTON_MASK_ACTION2;

    m_buttons[0] = mask;

#elif defined(NEXTP8)
    volatile uint8_t *keyboard_matrix = (volatile uint8_t *) _KEYBOARD_MATRIX;
    volatile uint8_t *keyboard_matrix_latched = (volatile uint8_t *) _KEYBOARD_MATRIX_LATCHED;
    volatile uint8_t *joy0_latched_ptr = (volatile uint8_t *) _JOYSTICK0_LATCHED;
    volatile uint8_t *joy1_latched_ptr = (volatile uint8_t *) _JOYSTICK1_LATCHED;

    uint8_t joy0 = *(volatile uint8_t *) _JOYSTICK0;
    uint8_t joy1 = *(volatile uint8_t *) _JOYSTICK1;
    uint8_t joy0_latched = *joy0_latched_ptr;
    uint8_t joy1_latched = *joy1_latched_ptr;

    // Set current button state from unlatched inputs
    m_buttons[0] = player0_mask(keyboard_matrix, joy0);
    m_memory[MEMORY_BUTTON_STATE] = m_buttons[0] & 0xff;

    // Set button press state from latched inputs
    m_buttonsp[0] = player0_mask(keyboard_matrix_latched, joy0_latched);

    // Clear joystick latched bits for player 0
    *joy0_latched_ptr = 255;

    // Set current button state from unlatched inputs
    m_buttons[1] = player1_mask(keyboard_matrix, joy1);
    m_memory[MEMORY_BUTTON_STATE + 1] = m_buttons[1] & 0xff;

    // Set button press state from latched inputs
    m_buttonsp[1] = player1_mask(keyboard_matrix_latched, joy1_latched);

    // Clear joystick latched bits for player 1
    *joy1_latched_ptr = 255;

    // Clear all keyboard latched bits
    memset((void *)keyboard_matrix_latched, 0xff, 32);

    bool need_update = false;
    volatile uint32_t *keyboard_matrix32 = (volatile  uint32_t *) keyboard_matrix;
    for (unsigned i = 0; i < 8; ++i) {
        if (keyboard_matrix32[i] != keyboard_matrix_prev[i])
            need_update = true;
    }
    // Repeat state for software key-repeat (nextp8 has no hardware-level repeat)
    static unsigned repeat_sc = 0;
    static uint8_t repeat_char = 0;
    static unsigned repeat_mod = 0;
    static uint64_t repeat_down_time = 0;
    static bool repeat_first = false;
    static unsigned keymod = 0;
    if (need_update) {
        keymod = 0;
        if (is_down(keyboard_matrix, KEY_LEFT_SHIFT))
            keymod |= KMOD_LSHIFT;
        if (is_down(keyboard_matrix, KEY_RIGHT_SHIFT))
            keymod |= KMOD_RSHIFT;
        if (is_down(keyboard_matrix, KEY_LEFT_CTRL))
            keymod |= KMOD_LCTRL;
        if (is_down(keyboard_matrix, KEY_RIGHT_CTRL))
            keymod |= KMOD_RCTRL;
        if (is_down(keyboard_matrix, KEY_LEFT_ALT))
            keymod |= KMOD_LALT;
        if (is_down(keyboard_matrix, KEY_RIGHT_ALT))
            keymod |= KMOD_RALT;
        bool membrane_mode = is_down(keyboard_matrix, 0);
        char *keymap_page;
        if (membrane_mode) {
            unsigned page_index;
            if (keymod & (KMOD_SHIFT | KMOD_CTRL))
                page_index = 3;
            else if (keymod & (KMOD_CTRL))
                page_index = 2;
            else if (keymod & (KMOD_SHIFT))
                page_index = 1;
            else
                page_index = 0;
            keymap_page = memb_scancode_to_name[page_index];
        } else  {
            if (keymod & KMOD_CTRL)
                keymap_page = NULL;
            else
                keymap_page = ps2_scancode_to_name[(keymod & KMOD_SHIFT) ? 1 : 0];
        }
        if (keymap_page){
            for (unsigned i=0;i<256;++i) {
                bool down = is_down(keyboard_matrix, i);
                bool prev_down = is_down((uint8_t *)keyboard_matrix_prev, i);
                unsigned sdl2_sc = nextp8_scancode_to_sdl_scancode[i];
                if (down && !prev_down) {
                    // Key press event: fire immediately and arm repeat
                    char ch = keymap_page[i];
                    queue_keypress(sdl2_sc, ch, keymod);
                    if (!is_nextp8_modifier(sdl2_sc)) {
                        repeat_sc = sdl2_sc;
                        repeat_char = ch;
                        repeat_mod = keymod;
                        repeat_down_time = p8_clock();
                        repeat_first = false;
                    }
                } else if (!down && prev_down && nextp8_scancode_to_sdl_scancode[i] == repeat_sc) {
                    repeat_sc = 0;
                }
                m_scancodes[sdl2_sc] = down;
            }
        }
        for (unsigned i = 0; i < 8; ++i)
            keyboard_matrix_prev[i] = keyboard_matrix32[i];
    }
    // Software key repeat: re-fire queue_keypress while a key is held
    if (repeat_sc != 0 && !m_scancodes[repeat_sc]) {
        repeat_sc = 0;
    } else if (repeat_sc != 0) {
        uint64_t now = p8_clock();
        if (!repeat_first && (now - repeat_down_time) / 1000 >= DEFAULT_AUTO_REPEAT_DELAY_MS) {
            repeat_down_time = now;
            repeat_first = true;
            queue_keypress(repeat_sc, repeat_char, repeat_mod);
        } else if (repeat_first && (now - repeat_down_time) / 1000 >= DEFAULT_AUTO_REPEAT_INTERVAL_MS) {
            repeat_down_time = now;
            queue_keypress(repeat_sc, repeat_char, repeat_mod);
        }
    }

    int16_t mouse_x_accum = *(volatile int16_t *) _MOUSE_X;
    int16_t mouse_y_accum = *(volatile int16_t *) _MOUSE_Y;
    int16_t mouse_z_accum = *(volatile int16_t *) _MOUSE_Z;
    uint8_t prev_mouse_buttons = m_mouse_buttons;
    m_mouse_buttons = *(volatile uint8_t *) _MOUSE_BUTTONS;
    m_mouse_buttonsp = m_mouse_buttons & ~prev_mouse_buttons;

    // Handle wrap-around of signed 16-bit accumulators
    m_mouse_xrel = (int16_t)(mouse_x_accum - mouse_x_accum_prev);
    m_mouse_yrel = (int16_t)(mouse_y_accum - mouse_y_accum_prev);
    m_mouse_wheel = (int16_t)(mouse_z_accum - mouse_z_accum_prev);

    mouse_x4 += m_mouse_xrel;
    mouse_y4 += m_mouse_yrel;
    if (mouse_x4 < 0) mouse_x4 = 0;
    if (mouse_x4 >= P8_WIDTH * 4) mouse_x4 = P8_WIDTH * 4 - 1;
    if (mouse_y4 < 0) mouse_y4 = 0;
    if (mouse_y4 >= P8_HEIGHT * 4) mouse_y4 = P8_HEIGHT * 4 - 1;
    m_mouse_x = mouse_x4 / 4;
    m_mouse_y = mouse_y4 / 4;
    m_mouse_keymod = keymod;

    mouse_x_accum_prev = mouse_x_accum;
    mouse_y_accum_prev = mouse_y_accum;
    mouse_z_accum_prev = mouse_z_accum;

    if (m_memory[MEMORY_DEVKIT_MODE] & 0x2) {
        m_buttons[0] |= (m_mouse_buttons & 0x7) << 4;

        volatile uint8_t mouse_buttons_latched = *(volatile uint8_t *) _MOUSE_BUTTONS_LATCHED;
        m_buttonsp[0] |= (mouse_buttons_latched & 0x7) << 4;
        *(volatile uint8_t *) _MOUSE_BUTTONS_LATCHED = 0xff;
    }
    uint8_t new_buttons = m_mouse_buttons & ~prev_mouse_buttons;
    if (new_buttons != 0)
        queue_mouse_click(new_buttons, m_mouse_x, m_mouse_y, m_mouse_keymod);
#endif

    uint8_t delay = m_memory[MEMORY_AUTO_REPEAT_DELAY];
    if (delay == 0)
        delay = DEFAULT_AUTO_REPEAT_DELAY;
    uint8_t interval = m_memory[MEMORY_AUTO_REPEAT_INTERVAL];
    if (interval == 0)
        interval = DEFAULT_AUTO_REPEAT_INTERVAL;
    for (unsigned p=0;p<PLAYER_COUNT;++p) {
#ifndef NEXTP8
        m_buttonsp[p] = 0;
#endif
        for (unsigned i=0;i<BUTTON_INTERNAL_COUNT;++i) {
            if (m_buttons[p] & (1 << i)) {
                if (m_button_down_time[p][i] == UINT_MAX) {
                    // ignore buttons pressed at startup
                } else if (!m_button_down_time[p][i]) {
                    m_button_down_time[p][i] = m_frames;
#ifndef NEXTP8
                    m_buttonsp[p] |= 1 << i;
#endif
                } else if (i < BUTTON_REPEAT_COUNT) {
                    if (delay != 255 && !(m_button_first_repeat[p] & (1 << i)) && m_frames - m_button_down_time[p][i] >= delay) {
                        m_button_down_time[p][i] = m_frames;
                        m_button_first_repeat[p] |= 1 << i;
                        m_buttonsp[p] |= 1 << i;
                    } else if ((m_button_first_repeat[p] & (1 << i)) && m_frames - m_button_down_time[p][i] >= interval) {
                        m_button_down_time[p][i] = m_frames;
                        m_buttonsp[p] |= 1 << i;
                    }
                }
            } else  {
#ifdef SDL
                // Catch a press-and-release within one frame via latch
                if ((m_buttons_latch[p] & (1 << i)) && !m_button_down_time[p][i]) {
                    m_buttonsp[p] |= 1 << i;
                }
#endif
                if (m_button_down_time[p][i]) {
                    m_button_down_time[p][i] = 0;
                    m_button_first_repeat[p] &= ~(1 << i);
                }
            }
        }
#ifdef SDL
        m_buttons_latch[p] = 0;
#endif
    }
}
