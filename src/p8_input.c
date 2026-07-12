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

#define MAX_KEY_EVENTS 8

struct key_event {
    unsigned scancode;
    unsigned keymod;
    uint8_t ch;
};

static int m_key_queue_write_index = 0;
static int m_key_queue_read_index  = 0;
static struct key_event m_key_queue_buffer[MAX_KEY_EVENTS];
#ifndef NEXTP8
bool m_scancodes[NUM_SCANCODES];
#endif

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
#define KEY_CURSOR_LEFT   0x50
#define KEY_CURSOR_DOWN   0x51
#define KEY_CURSOR_RIGHT  0x4f
#define KEY_CURSOR_UP     0x52
#define KEY_Z             0x1d
#define KEY_X             0x1b
#define KEY_N             0x11
#define KEY_M             0x10
#define KEY_C             0x06
#define KEY_V             0x19
#define KEY_S             0x16
#define KEY_D             0x07
#define KEY_F             0x09
#define KEY_E             0x08
#define KEY_TAB           0x2b
#define KEY_Q             0x14
#define KEY_LEFT_SHIFT    0xe1
#define KEY_A             0x04
#define KEY_RIGHT_SHIFT   0xe5
#define KEY_ENTER         0x28
#define KEY_BREAK         0x29
#define KEY_P             0x13
#define KEY_PAGE_UP       0x4b
#define KEY_PAGE_DOWN     0x4e
#define KEY_LEFT_ALT      0xe2
#define KEY_RIGHT_ALT     0xe6
#define KEY_LEFT_CTRL     0xe0
#define KEY_RIGHT_CTRL    0xe4
#define KEY_LEFT_GUI      0xe3
#define KEY_RIGHT_GUI     0xe7

#define JOY_UP      (1 << 0)
#define JOY_DOWN    (1 << 1)
#define JOY_LEFT    (1 << 2)
#define JOY_RIGHT   (1 << 3)
#define JOY_BUTTON1 (1 << 4)
#define JOY_BUTTON2 (1 << 5)

char usb_hid_scancode_to_name[2][256] = {
    // Unshifted
    {
        '\0', '\0', '\0', '\0', 'a', 'b', 'c', 'd',        // ErrorRollOver, POSTFail, ErrorUndefined, a and A, b and B, c and C, d and D
        'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',            // e and E, f and F, g and G, h and H, i and I, j and J, k and K, l and L
        'm', 'n', 'o', 'p', 'q', 'r', 's', 't',            // m and M, n and N, o and O, p and P, q and Q, r and R, s and S, t and T
        'u', 'v', 'w', 'x', 'y', 'z', '1', '2',            // u and U, v and V, w and W, x and X, y and Y, z and Z, 1 and !, 2 and @
        '3', '4', '5', '6', '7', '8', '9', '0',            // 3 and #, 4 and $, 5 and %, 6 and ^, 7 and &, 8 and *, 9 and (, 0 and )
        '\n', '\0', '\b', '\t', ' ', '-', '=', '[',        // Return (ENTER), ESCAPE, DELETE (Backspace), Tab, Spacebar, - and (underscore), = and +, [ and {
        ']', '\\', '#', ';', '\'', '`', ',', '.',          // ] and }, \ and |, Non-US # and ˜, ; and :, ‘ and “, Grave Accent and Tilde, , and <, . and >
        '/', '\0', '\0', '\0', '\0', '\0', '\0', '\0',     // / and ?, Caps Lock, F1, F2, F3, F4, F5, F6
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F7, F8, F9, F10, F11, F12, PrintScreen, Scroll Lock
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // Pause, Insert, Home, PageUp, Delete Forward, End, PageDown, RightArrow
        '\0', '\0', '\0', '\0', '/', '*', '-', '+',        // LeftArrow, DownArrow, UpArrow, Keypad Num Lock and Clear, Keypad /, Keypad *, Keypad -, Keypad +
        '\n', '1', '2', '3', '4', '5', '6', '7',           // Keypad ENTER, Keypad 1 and End, Keypad 2 and Down Arrow, Keypad 3 and PageDn, Keypad 4 and Left Arrow, Keypad 5, Keypad 6 and Right Arrow, Keypad 7 and Home
        '8', '9', '0', '.', '\0', '\0', '\0', '=',         // Keypad 8 and Up Arrow, Keypad 9 and PageUp, Keypad 0 and Insert, Keypad . and Delete, Non-US \and |, Application, Power, Keypad =
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F13, F14, F15, F16, F17, F18, F19, F20
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F21, F22, F23, F24, Execute, Help, Menu, Keyboard
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // Stop, Again, Undo, Cut, Copy, Paste, Find, Mute
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // Volume Up, Volume Down, Locking Caps Lock, Locking Num Lock, Locking Scroll Lock, Keypad Comma, Keypad Equal Sign, International1
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // International2, International3, International4, International5, International6, International7, International82, International
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // LANG1, LANG2, LANG3, LANG4, LANG5, LANG6, LANG7, LANG8
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // LANG9, Alternate Erase, SysReq/Attention, Cancel, Clear, Prior, Return, Separator
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // Out, Oper, Clear/Again, CrSel/Props, ExSel, Reserved, Reserved, Reserved
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // Reserved, Reserved, Reserved, Reserved, Reserved, Reserved, Reserved, Reserved
        '\0', '\0', '\0', '\0', '\0', '\0', '(', ')',      // Keypad 00, Keypad 000, Thousands Separator, Decimal Separator, Currency Unit, Currency Sub-unit, Keypad (, Keypad )
        '{', '}', '\0', '\0', 'A', 'B', 'C', 'D',          // Keypad {, Keypad }, Keypad Tab, Keypad Backspace, Keypad A, Keypad B, Keypad C, Keypad D
        'E', 'F', '\0', '^', '%', '<', '>', '&',           // Keypad E, Keypad F, Keypad XOR, Keypad ^, Keypad %, Keypad <, Keypad >, Keypad &
        '\0', '|', '\0', ':', '#', '\0', '@', '!',         // Keypad &&, Keypad |, Keypad ||, Keypad :, Keypad #, Keypad Space, Keypad @, Keypad !
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // Keypad Memory Store, Keypad Memory Recall, Keypad Memory Clear, Keypad Memory Add, Keypad Memory Subtract, Keypad Memory Multiply, Keypad Memory Divide, Keypad +/-
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // Keypad Clear, Keypad Clear Entry, Keypad Binary, Keypad Octal, Keypad Decimal, Keypad Hexadecimal, Reserved, Reserved
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // LeftControl, LeftShift, LeftAlt, Left GUI, RightControl, RightShift, RightAlt, Right GUI
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    //
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    //
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    //
    },
    // Shift
    {
        '\0', '\0', '\0', '\0', 'A', 'B', 'C', 'D',        // ErrorRollOver, POSTFail, ErrorUndefined, a and A, b and B, c and C, d and D
        'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',            // e and E, f and F, g and G, h and H, i and I, j and J, k and K, l and L
        'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',            // m and M, n and N, o and O, p and P, q and Q, r and R, s and S, t and T
        'U', 'V', 'W', 'X', 'Y', 'Z', '!', '@',            // u and U, v and V, w and W, x and X, y and Y, z and Z, 1 and !, 2 and @
        '#', '$', '%', '^', '&', '*', '(', ')',            // 3 and #, 4 and $, 5 and %, 6 and ^, 7 and &, 8 and *, 9 and (, 0 and )
        '\n', '\0', '\b', '\t', ' ', '_', '+', '{',        // Return (ENTER), ESCAPE, DELETE (Backspace), Tab, Spacebar, - and (underscore), = and +, [ and {
        '}', '|', '~', ':', '"', '~', '<', '>',            // ] and }, \ and |, Non-US # and ˜, ; and :, ‘ and “, Grave Accent and Tilde, , and <, . and >
        '?', '\0', '\0', '\0', '\0', '\0', '\0', '\0',     // / and ?, Caps Lock, F1, F2, F3, F4, F5, F6
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F7, F8, F9, F10, F11, F12, PrintScreen, Scroll Lock
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // Pause, Insert, Home, PageUp, Delete Forward, End, PageDown, RightArrow
        '\0', '\0', '\0', '\0', '/', '*', '-', '+',        // LeftArrow, DownArrow, UpArrow, Keypad Num Lock and Clear, Keypad /, Keypad *, Keypad -, Keypad +
        '\n', '\0', '\0', '\0', '\0', '5', '\0', '\0',     // Keypad ENTER, Keypad 1 and End, Keypad 2 and Down Arrow, Keypad 3 and PageDn, Keypad 4 and Left Arrow, Keypad 5, Keypad 6 and Right Arrow, Keypad 7 and Home
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '=',     // Keypad 8 and Up Arrow, Keypad 9 and PageUp, Keypad 0 and Insert, Keypad . and Delete, Non-US \and |, Application, Power, Keypad =
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F13, F14, F15, F16, F17, F18, F19, F20
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // F21, F22, F23, F24, Execute, Help, Menu, Keyboard
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // Stop, Again, Undo, Cut, Copy, Paste, Find, Mute
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // Volume Up, Volume Down, Locking Caps Lock, Locking Num Lock, Locking Scroll Lock, Keypad Comma, Keypad Equal Sign, International1
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // International2, International3, International4, International5, International6, International7, International82, International
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // LANG1, LANG2, LANG3, LANG4, LANG5, LANG6, LANG7, LANG8
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // LANG9, Alternate Erase, SysReq/Attention, Cancel, Clear, Prior, Return, Separator
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // Out, Oper, Clear/Again, CrSel/Props, ExSel, Reserved, Reserved, Reserved
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // Reserved, Reserved, Reserved, Reserved, Reserved, Reserved, Reserved, Reserved
        '\0', '\0', '\0', '\0', '\0', '\0', '(', ')',      // Keypad 00, Keypad 000, Thousands Separator, Decimal Separator, Currency Unit, Currency Sub-unit, Keypad (, Keypad )
        '{', '}', '\0', '\0', 'A', 'B', 'C', 'D',          // Keypad {, Keypad }, Keypad Tab, Keypad Backspace, Keypad A, Keypad B, Keypad C, Keypad D
        'E', 'F', '\0', '^', '%', '<', '>', '&',           // Keypad E, Keypad F, Keypad XOR, Keypad ^, Keypad %, Keypad <, Keypad >, Keypad &
        '\0', '|', '\0', ':', '#', '\0', '@', '!',         // Keypad &&, Keypad |, Keypad ||, Keypad :, Keypad #, Keypad Space, Keypad @, Keypad !
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // Keypad Memory Store, Keypad Memory Recall, Keypad Memory Clear, Keypad Memory Add, Keypad Memory Subtract, Keypad Memory Multiply, Keypad Memory Divide, Keypad +/-
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // Keypad Clear, Keypad Clear Entry, Keypad Binary, Keypad Octal, Keypad Decimal, Keypad Hexadecimal, Reserved, Reserved
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    // LeftControl, LeftShift, LeftAlt, Left GUI, RightControl, RightShift, RightAlt, Right GUI
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    //
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    //
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',    //
    },
};

char memb_scancode_to_name[4][256] = {
    // Unshifted
    {
        '\0', '\0', '\0', '\0', 'a', 'b', 'c', 'd',
        'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
        'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
        'u', 'v', 'w', 'x', 'y', 'z', '1', '2',
        '3', '4', '5', '6', '7', '8', '9', '0',
        '\n', '\0', '\b', '\t', ' ', '-', '=', '[',
        ']', '\\', '#', ';', '\'', '`', ',', '.',
        '/', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '/', '*', '-', '+',
        '\n', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', '0', '.', '\0', '\0', '\0', '=',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '(', ')',
        '{', '}', '\0', '\0', 'A', 'B', 'C', 'D',
        'E', 'F', '\0', '^', '%', '<', '>', '&',
        '\0', '|', '\0', ':', '#', '\0', '@', '!',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    },
    // Shift
    {
        '\0', '\0', '\0', '\0', 'A', 'B', 'C', 'D',
        'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
        'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
        'U', 'V', 'W', 'X', 'Y', 'Z', '!', '@',
        '#', '$', '%', '^', '&', '*', '(', ')',
        '\n', '\0', '\b', '\t', ' ', '_', '+', '{',
        '}', '|', '~', ':', '"', '~', '<', '>',
        '?', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '/', '*', '-', '+',
        '\n', '\0', '\0', '\0', '\0', '5', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '=',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '(', ')',
        '{', '}', '\0', '\0', 'A', 'B', 'C', 'D',
        'E', 'F', '\0', '^', '%', '<', '>', '&',
        '\0', '|', '\0', ':', '#', '\0', '@', '!',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    },
    // Ctrl
    {
        '\0', '\0', '\0', '\0', '\0', '*', '?', '\0',
        '\0', '\0', '\0', '^', '\0', '-', '+', '=',
        '.', ',', ';', '"', '\0', '<', '\0', '>',
        '\0', '/', '\0', '$', '\0', ':', '!', '@',
        '#', '$', '%', '&', '\0', '(', ')', '_',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    },
    // Shift + Ctrl
    {
        '\0', '\0', '\0', '\0', '~', '\0', '\0', '\\',
        '\0', '{', '}', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '|', '\0',
        ']', '\0', '\0', '\0', '[', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    },
};

static unsigned is_down(volatile uint8_t *base, unsigned index)
{
    return base[index >> 3] & (1 << (index & 0x7));
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

bool p8_is_key_down(unsigned scancode)
{
    if (scancode >= NUM_SCANCODES)
        return false;
#ifdef NEXTP8
    return is_down((volatile uint8_t *) _KEYBOARD_MATRIX, scancode);
#else
    return m_scancodes[scancode];
#endif
}

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
	m_key_queue_read_index = m_key_queue_write_index;
    m_mouse_click_buttons = 0;
    m_mouse_click_x = 0;
    m_mouse_click_y = 0;
    m_mouse_click_mod = 0;
}

static void queue_keypress(unsigned scancode, uint8_t keychar, unsigned mod)
{
    if (is_modifier(scancode))
        return;
    if ((m_key_queue_write_index + 1) % MAX_KEY_EVENTS == m_key_queue_read_index)
    {
       // buffer is full, avoid overflow
       return;
    }
    m_key_queue_buffer[m_key_queue_write_index].scancode = scancode;
    m_key_queue_buffer[m_key_queue_write_index].ch = keychar;
    m_key_queue_buffer[m_key_queue_write_index].keymod = mod;
    m_key_queue_write_index = (m_key_queue_write_index + 1) % MAX_KEY_EVENTS;
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
    if (m_key_queue_read_index == m_key_queue_write_index) {
       // buffer is empty
       return false;
    }

    *scancode = m_key_queue_buffer[m_key_queue_read_index].scancode;
    *keychar = m_key_queue_buffer[m_key_queue_read_index].ch;
    *mod = m_key_queue_buffer[m_key_queue_read_index].keymod;
    m_key_queue_read_index = (m_key_queue_read_index + 1) % MAX_KEY_EVENTS;
    return true;
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
    return m_key_queue_read_index != m_key_queue_write_index;
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
                if (!(m_mouse_buttons & 0x1))
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
            if (event.text.text[0] != '\0') {
                queue_keypress(0, (uint8_t)event.text.text[0], 0);
            }
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

    // Repeat state for software key-repeat (nextp8 has no hardware-level repeat)
    static unsigned repeat_sc = 0;
    static uint8_t repeat_char = 0;
    static unsigned repeat_mod = 0;
    static uint64_t repeat_down_time = 0;
    static bool repeat_first = false;
    static unsigned keymod = 0;
    uint32_t key_event;
    while ((key_event = *(volatile uint32_t *)_KEYBOARD_EVENT_QUEUE_HI) != 0) {
        bool press = (key_event & 0x80000000) != 0;
        uint16_t scancode = key_event & 0xffff;
        uint16_t keymod = ((key_event >> 16) & 3) |
                          ((key_event >> 12) & 0xc3f0);
        bool membrane_mode = (key_event & 0x40000000);
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
                keymap_page = usb_hid_scancode_to_name[(keymod & KMOD_SHIFT) ? 1 : 0];
        }
        if (keymap_page) {
            if (press) {
                // Key press event: fire immediately and arm repeat
                char ch = keymap_page[scancode];
                queue_keypress(scancode, ch, keymod);
                if (!is_modifier(scancode)) {
                    repeat_sc = scancode;
                    repeat_char = ch;
                    repeat_mod = keymod;
                    repeat_down_time = p8_clock();
                    repeat_first = false;
                }
            } else if (!press && scancode == repeat_sc) {
                repeat_sc = 0;
            }
        }
    }
    // Software key repeat: re-fire queue_keypress while a key is held
    if (repeat_sc != 0 && !is_down(keyboard_matrix, repeat_sc)) {
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
