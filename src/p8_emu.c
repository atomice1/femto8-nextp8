/*
 * p8_emu.c
 *
 *  Created on: Dec 13, 2023
 *      Author: bbaker
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#ifdef OS_FREERTOS
#include <FreeRTOS.h>
#include <task.h>
#include "retro_heap.h"
#include "ble_controller.h"
#endif
#include "p8_audio.h"
#include "p8_compat.h"
#include "p8_emu.h"
#include "p8_lua.h"
#include "p8_lua_helper.h"
#include "p8_parser.h"

#if defined(SDL)
#include "SDL.h"
#elif defined(__DA1470x__)
#include "gdi.h"
#elif defined(NEXTP8)
#include "nextp8.h"
#include "postcodes.h"
#endif

#if defined(SDL)
// ARGB
uint32_t m_colors[32] = {
    0x00000000, 0x001d2b53, 0x007e2553, 0x00008751, 0x00ab5236, 0x005f574f, 0x00c2c3c7, 0x00fff1e8,
    0x00ff004d, 0x00ffa300, 0x00ffec27, 0x0000e436, 0x0029adff, 0x0083769c, 0x00ff77a8, 0x00ffccaa,
    0x00291814, 0x00111D35, 0x00422136, 0x00125359, 0x00742F29, 0x0049333B, 0x00A28879, 0x00F3EF7D,
    0x00BE1250, 0x00FF6C24, 0x00A8E72E, 0x0000B54E, 0x00065AB5, 0x00754665, 0x00FF6E59, 0x00FF9D81};
#else
// RGB565
uint16_t m_colors[32] = {
    0x0000, 0x194a, 0x792a, 0x042a, 0xaa86, 0x5aa9, 0xc618, 0xff9d, 0xf809, 0xfd00, 0xff64, 0x0726, 0x2d7f, 0x83b3, 0xfbb5, 0xfe75,
    0x28c2, 0x10e6, 0x4106, 0x128b, 0x7165, 0x4987, 0xa44f, 0xf76f, 0xb88a, 0xfb64, 0xaf25, 0x05a9, 0x02d6, 0x722c, 0xfb6b, 0xfcf0};
#endif

// Convert 8-bit screen palette value to 5-bit color index.
static inline int color_index(uint8_t c)
{
    return ((c >> 3) & 0x10) | (c & 0xf);
}

static void p8_abort();
static int p8_init_lcd(void);
static void p8_main_loop();
static void p8_show_compatibility_error(int severity);

uint8_t *m_memory = NULL;
uint8_t *m_cart_memory = NULL;

unsigned m_fps = 30;
unsigned m_actual_fps = 0;
unsigned m_frames = 0;

clock_t m_start_time;

jmp_buf jmpbuf;
static bool restart;

#ifdef SDL
SDL_Surface *m_screen = NULL;
SDL_Surface *m_output = NULL;
SDL_PixelFormat *m_format = NULL;
#endif
#ifdef OS_FREERTOS
SemaphoreHandle_t m_drawSemaphore;
#endif

int16_t m_mouse_x, m_mouse_y;
int16_t m_mouse_xrel, m_mouse_yrel;
uint8_t m_mouse_buttons;
uint8_t m_keypress;

uint8_t m_buttons[2];
uint8_t m_buttonsp[2];
uint8_t m_button_first_repeat[2];
unsigned m_button_down_time[2][6];

static bool m_prev_pointer_lock;

static FILE *cartdata = NULL;
static bool cartdata_needs_flush = false;

static int m_initialized = 0;

#ifdef NEXTP8
static int vfrontreq = 0;
#endif

int p8_init()
{
    assert(!m_initialized);

    srand((unsigned int)time(NULL));

#ifdef SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
    {
        printf("Error on SDL_Init().\n");
        return 1;
    }

    SDL_ShowCursor(0);
    SDL_EnableKeyRepeat(0, 0);

    m_screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_HWSURFACE);
    m_format = m_screen->format;

    m_output = SDL_CreateRGBSurface(0, P8_WIDTH, P8_HEIGHT, 32, m_format->Rmask, m_format->Gmask, m_format->Bmask, m_format->Amask);

    SDL_WM_SetCaption("femto-8", NULL);
#endif
#ifdef OS_FREERTOS
    m_drawSemaphore = xSemaphoreCreateBinary();

    xSemaphoreGive(m_drawSemaphore);
#endif
#ifndef OS_FREERTOS
    m_memory = (uint8_t *)malloc(MEMORY_SIZE);
    m_cart_memory = (uint8_t *)malloc(CART_MEMORY_SIZE);
#else
    m_memory = (uint8_t *)rh_malloc(MEMORY_SIZE);
    m_cart_memory = (uint8_t *)rh_malloc(CART_MEMORY_SIZE);
#endif

    memset(m_memory, 0, MEMORY_SIZE);
    memset(m_cart_memory, 0, CART_MEMORY_SIZE);

#ifdef ENABLE_AUDIO
    audio_init();
#endif

    p8_init_lcd();

    m_initialized = 1;

    return 0;
}

static int p8_init_lcd(void)
{
#if defined(NEXTP8)
    vfrontreq = *(volatile uint8_t *)_VFRONT;
#elif defined(__DA1470x__)
    gdi_set_layer_start(HW_LCDC_LAYER_0, 0, 0);

    gdi_set_layer_enable(HW_LCDC_LAYER_0, true);

    uint16_t *fb = (uint16_t *)gdi_get_frame_buffer_addr(HW_LCDC_LAYER_0);

    gdi_set_layer_src(HW_LCDC_LAYER_0, fb, SCREEN_WIDTH, SCREEN_HEIGHT, GDI_FORMAT_RGB565);
#endif

    return 0;
}

static void p8_init_common(const char *file_name, const char *lua_script)
{
    if (lua_script == NULL) {
        if (file_name) fprintf(stderr, "%s: ", file_name);
        fprintf(stderr, "invalid cart\n");
        exit(1);
    }

    lua_load_api();

    if (setjmp(jmpbuf) && !restart)
        return;
    if (!restart) {
        int ret = check_compatibility(file_name, lua_script);
        if (ret != COMPAT_OK)
            p8_show_compatibility_error(ret);
        if (ret == COMPAT_NONE)
            return;
    }
    restart = false;

    memcpy(m_memory, m_cart_memory, CART_MEMORY_SIZE);

    clear_screen(0);

    p8_reset();
    p8_update_input();

    lua_init_script(lua_script);

    clear_screen(0);

    lua_init();

    p8_init_lcd();

    p8_main_loop();
}

int p8_init_file(const char *file_name)
{
    if (!m_initialized)
        p8_init();

    const char *lua_script = NULL;
    uint8_t *file_buffer = NULL;

    parse_cart_file(file_name, m_cart_memory, &lua_script, &file_buffer);

    p8_init_common(file_name, lua_script);

#ifdef OS_FREERTOS
    rh_free(file_buffer);
#else
    free(file_buffer);
#endif

    return 0;
}

int p8_init_ram(uint8_t *buffer, int size)
{
    if (!m_initialized)
        p8_init();

    p8_init();

    const char *lua_script = NULL;
    uint8_t *decompression_buffer = NULL;

    parse_cart_ram(buffer, size, m_cart_memory, &lua_script, &decompression_buffer);

    p8_init_common(NULL, lua_script);

#ifdef OS_FREERTOS
    rh_free(decompression_buffer);
#else
    free(decompression_buffer);
#endif

    return 0;
}

int p8_shutdown()
{
#ifdef ENABLE_AUDIO
    audio_close();
#endif

    lua_shutdown_api();

    p8_close_cartdata();

#ifdef SDL
    SDL_FreeSurface(m_output);
    SDL_FreeSurface(m_screen);
    SDL_Quit();
#endif
#ifdef OS_FREERTOS
    rh_free(m_cart_memory);
    rh_free(m_memory);
#else
    free(m_cart_memory);
    free(m_memory);
#endif

    m_initialized = 0;

    return 0;
}

#ifdef SDL
void p8_render()
{
    uint32_t *output = m_output->pixels;

    for (int y = 0; y < P8_HEIGHT; y++)
    {
        for (int x = 0; x < P8_WIDTH; x++)
        {
            int screen_offset = MEMORY_SCREEN + (x >> 1) + y * 64;
            uint8_t value = m_memory[screen_offset];
            uint8_t index = color_get(PALTYPE_SCREEN, IS_EVEN(x) ? value & 0xF : value >> 4);
            uint32_t color = m_colors[color_index(index)];

            output[x + (y * P8_WIDTH)] = color;
        }
    }

    SDL_Rect rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    // SDL_BlitSurface(m_output, NULL, m_screen, &rect);
    SDL_SoftStretch(m_output, NULL, m_screen, &rect);
    SDL_Flip(m_screen);
}
#elif defined(__DA1470x__)

#ifdef OS_FREERTOS
void draw_complete(bool underflow, void *user_data)
{
    xSemaphoreGive(m_drawSemaphore);
}
#endif

void p8_render()
{
#ifdef OS_FREERTOS
    if (xSemaphoreTake(m_drawSemaphore, portMAX_DELAY) != pdTRUE)
        return;
#endif

    sprintf(m_str_buffer, "%d", (int)m_actual_fps);
    draw_text(m_str_buffer, 0, 0, 1);

#if defined(__DA1470x__)
    uint16_t *output = gdi_get_frame_buffer_addr(HW_LCDC_LAYER_0);
#endif
    uint8_t *screen_mem = &m_memory[MEMORY_SCREEN];
    uint8_t *pal = &m_memory[MEMORY_PALETTES + PALTYPE_SCREEN * 16];

    for (int y = 1; y <= 128; y++)
    {
        if (y & 0x7)
        {
            uint16_t *top = output;
            uint16_t *bottom = output + 240;

            for (int x = 0; x < 128; x += 8)
            {

                uint8_t left = (*screen_mem) & 0xF;
                uint8_t right = (*screen_mem) >> 4;

                uint8_t index_left = pal[left];
                uint8_t index_right = color_pal[right];

                uint16_t c_left = m_colors[color_index(index_left)];
                uint16_t c_right = m_colors[color_index(index_right)];

                *top++ = c_left;
                *top++ = c_left;
                *top++ = c_right;
                *top++ = c_right;

                *bottom++ = c_left;
                *bottom++ = c_left;
                *bottom++ = c_right;
                *bottom++ = c_right;

                screen_mem++;

                left = (*screen_mem) & 0xF;
                right = (*screen_mem) >> 4;

                index_left = pal[left];
                index_right = pal[right];

                c_left = m_colors[color_index(index_left)];
                c_right = m_colors[color_index(index_right)];

                *top++ = c_left;
                *top++ = c_left;
                *top++ = c_right;
                *top++ = c_right;

                *bottom++ = c_left;
                *bottom++ = c_left;
                *bottom++ = c_right;
                *bottom++ = c_right;

                screen_mem++;

                left = (*screen_mem) & 0xF;
                right = (*screen_mem) >> 4;

                index_left = pal[left];
                index_right = pal[right];

                c_left = m_colors[color_index(index_left)];
                c_right = m_colors[color_index(index_right)];

                *top++ = c_left;
                *top++ = c_left;
                *top++ = c_right;
                *top++ = c_right;

                *bottom++ = c_left;
                *bottom++ = c_left;
                *bottom++ = c_right;
                *bottom++ = c_right;

                screen_mem++;

                left = (*screen_mem) & 0xF;
                right = (*screen_mem) >> 4;

                index_left = pal[left];
                index_right = pal[right];

                c_left = m_colors[color_index(index_left)];
                c_right = m_colors[color_index(index_right)];

                *top++ = c_left;
                *top++ = c_left;
                *top++ = c_right;

                *bottom++ = c_left;
                *bottom++ = c_right;
                *bottom++ = c_right;

                screen_mem++;
            }

            output += 480;
        }
        else
        {
            uint16_t *top = output;

            for (int x = 0; x < 128; x += 8)
            {

                uint8_t left = (*screen_mem) & 0xF;
                uint8_t right = (*screen_mem) >> 4;

                uint8_t index_left = pal[left];
                uint8_t index_right = pal[right];

                uint16_t c_left = m_colors[color_index(index_left)];
                uint16_t c_right = m_colors[color_index(index_right)];

                *top++ = c_left;
                *top++ = c_left;
                *top++ = c_right;
                *top++ = c_right;

                screen_mem++;

                left = (*screen_mem) & 0xF;
                right = (*screen_mem) >> 4;

                index_left = pal[left];
                index_right = pal[right];

                c_left = m_colors[color_index(index_left)];
                c_right = m_colors[color_index(index_right)];

                *top++ = c_left;
                *top++ = c_left;
                *top++ = c_right;
                *top++ = c_right;

                screen_mem++;

                left = (*screen_mem) & 0xF;
                right = (*screen_mem) >> 4;

                index_left = pal[left];
                index_right = pal[right];

                c_left = m_colors[color_index(index_left)];
                c_right = m_colors[color_index(index_right)];

                *top++ = c_left;
                *top++ = c_left;
                *top++ = c_right;
                *top++ = c_right;

                screen_mem++;

                left = (*screen_mem) & 0xF;
                right = (*screen_mem) >> 4;

                index_left = pal[left];
                index_right = pal[right];

                c_left = m_colors[color_index(index_left)];
                c_right = m_colors[color_index(index_right)];

                *top++ = c_left;
                *top++ = c_left;
                *top++ = c_right;

                screen_mem++;
            }

            output += 240;
        }
    }

    gdi_display_update_async(draw_complete, NULL);
}
#elif defined(NEXTP8)
void p8_render()
{
    while (*(volatile uint8_t *) _VFRONT != vfrontreq) {
        // wait for previous flip to complete
    }
    int vback = 1 - vfrontreq;
    uint8_t *screen_mem = &m_memory[MEMORY_SCREEN];
    uint8_t *pal = &m_memory[MEMORY_PALETTES + PALTYPE_SCREEN * 16];
    memcpy((uint8_t *)_PALETTE_BASE, pal, _PALETTE_SIZE);
    memcpy((uint8_t *)_BACK_BUFFER_BASE, screen_mem, _FRAME_BUFFER_SIZE);
    *(volatile uint8_t *) _VFRONTREQ = vfrontreq = vback;
}
#endif

#if defined(NEXTP8)
char scancode_to_name[2][256] = {
    {
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\t', '`', '\0',
        '\0', '\0', '\0', '\0', '\0', 'q', '1', '\0', '\0', '\0', 'z', 's', 'a', 'w', '2', '\0',
        '\0', 'c', 'x', 'd', 'e', '4', '3', '\0', '\0', '\0', 'v', 'f', 't', 'r', '5', '\0',
        '\0', 'n', 'b', 'h', 'g', 'y', '6', '\0', '\0', '\0', 'm', 'j', 'u', '7', '8', '\0',
        '\0', ',', 'k', 'i', 'o', '0', '9', '\0', '\0', '.', '/', 'l', ';', 'p', '-', '\0',
        '\0', '\0', '\'', '\0', '[', '=', '\0', '\0', '\0', '\0', '\r', ']', '\0', '\\', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\b', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\x1b', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\x7f', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    },
    {
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\t', '~', '\0',
        '\0', '\0', '\0', '\0', '\0', 'Q', '!', '\0', '\0', '\0', 'Z', 'S', 'A', 'W', '@', '\0',
        '\0', 'C', 'X', 'D', 'E', '$', '#', '\0', '\0', '\0', 'V', 'F', 'T', 'R', '%', '\0',
        '\0', 'N', 'B', 'H', 'G', 'Y', '&', '\0', '\0', '\0', 'M', 'J', 'U', '\'', '(', '\0',
        '\0', '<', 'K', 'I', 'O', '-', ')', '\0', '\0', '>', '?', 'L', ':', 'P', '_', '\0',
        '\0', '\0', '"', '\0', '{', '+', '\0', '\0', '\0', '\0', '\r', '}', '\0', '|', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\b', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\x1b', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\x7f', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'
    },
};

#define KEY_CURSOR_LEFT 235
#define KEY_CURSOR_DOWN 242
#define KEY_CURSOR_RIGHT 244
#define KEY_CURSOR_UP 245
#define KEY_Z 26
#define KEY_X 34
#define KEY_N 49
#define KEY_M 58
#define KEY_C 33
#define KEY_V 42
#define KEY_S 27
#define KEY_D 35
#define KEY_F 43
#define KEY_E 36
#define KEY_TAB 13
#define KEY_Q 21
#define KEY_LEFT_SHIFT 18
#define KEY_A 28
#define KEY_RIGHT_SHIFT 89
#define KEY_ENTER 0x5a
#define KEY_BREAK 0x76

#define JOY_UP      (1 << 0)
#define JOY_DOWN    (1 << 1)
#define JOY_LEFT    (1 << 2)
#define JOY_RIGHT   (1 << 3)
#define JOY_BUTTON1 (1 << 4)
#define JOY_BUTTON2 (1 << 5)

static unsigned is_down(uint8_t *base, unsigned index)
{
    return base[index >> 3] & (1 << (index & 0x7));
}
#endif

void p8_update_input()
{
    bool pointer_lock = (m_memory[MEMORY_DEVKIT_MODE] & 0x4) != 0;
    if (pointer_lock != m_prev_pointer_lock) {
        m_prev_pointer_lock  = pointer_lock;
#ifdef SDL
        SDL_WM_GrabInput(pointer_lock ? SDL_GRAB_ON : SDL_GRAB_OFF);
#endif
    }

    bool escape = false;

#ifdef SDL
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_MOUSEMOTION:
            m_mouse_x = event.motion.x;
            m_mouse_y = event.motion.y;
            m_mouse_xrel += event.motion.xrel;
            m_mouse_yrel += event.motion.yrel;
            break;
        case SDL_MOUSEBUTTONDOWN:
            m_mouse_buttons |= 1 << event.button.button;
            break;
        case SDL_MOUSEBUTTONUP:
            m_mouse_buttons &= ~(1 << event.button.button);
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym)
            {
            case INPUT_LEFT:
                update_buttons(0, 0, true);
                break;
            case INPUT_RIGHT:
                update_buttons(0, 1, true);
                break;
            case INPUT_UP:
                update_buttons(0, 2, true);
                break;
            case INPUT_DOWN:
                update_buttons(0, 3, true);
                break;
            case INPUT_ACTION1:
                update_buttons(0, 4, true);
                break;
            case INPUT_ACTION2:
                update_buttons(0, 5, true);
                break;
            case INPUT_ESCAPE:
                escape = true;
                break;
            default:
                break;
            }
            m_keypress = (event.key.keysym.sym < 256) ? event.key.keysym.sym : 0;
            break;
        case SDL_KEYUP:
            switch (event.key.keysym.sym)
            {
            case INPUT_LEFT:
                update_buttons(0, 0, false);
                break;
            case INPUT_RIGHT:
                update_buttons(0, 1, false);
                break;
            case INPUT_UP:
                update_buttons(0, 2, false);
                break;
            case INPUT_DOWN:
                update_buttons(0, 3, false);
                break;
            case INPUT_ACTION1:
                update_buttons(0, 4, false);
                break;
            case INPUT_ACTION2:
                update_buttons(0, 5, false);
                break;
            default:
                break;
            }
            break;
        case SDL_QUIT:
            p8_abort();
            break;
        default:
            break;
        }
    }
#elif defined(OS_FREERTOS)
    uint8_t mask = 0;

    if (gamepad & AXIS_L_LEFT)
        mask |= BUTTON_LEFT;
    if (gamepad & AXIS_L_RIGHT)
        mask |= BUTTON_RIGHT;
    if (gamepad & AXIS_L_UP)
        mask |= BUTTON_UP;
    if (gamepad & AXIS_L_DOWN)
        mask |= BUTTON_DOWN;
    if (gamepad & AXIS_L_TRIGGER)
        mask |= BUTTON_ACTION1;
    if (gamepad & AXIS_R_LEFT)
        mask |= BUTTON_LEFT;
    if (gamepad & AXIS_R_RIGHT)
        mask |= BUTTON_RIGHT;
    if (gamepad & AXIS_R_UP)
        mask |= BUTTON_UP;
    if (gamepad & AXIS_R_DOWN)
        mask |= BUTTON_DOWN;
    if (gamepad & AXIS_R_TRIGGER)
        mask |= BUTTON_ACTION2;
    if (gamepad & DPAD_UP)
        mask |= BUTTON_UP;
    if (gamepad & DPAD_RIGHT)
        mask |= BUTTON_RIGHT;
    if (gamepad & DPAD_DOWN)
        mask |= BUTTON_DOWN;
    if (gamepad & DPAD_LEFT)
        mask |= BUTTON_LEFT;
    if (gamepad & BUTTON_1)
        mask |= BUTTON_ACTION1;
    if (gamepad & BUTTON_2)
        mask |= BUTTON_ACTION2;

    m_buttons[0] = mask;

#elif defined(NEXTP8)
    uint8_t *keyboard_matrix = (uint8_t *) _KEYBOARD_MATRIX;
    uint8_t joy0 = *(volatile uint8_t *) _JOYSTICK0;
    uint8_t mask = 0;
    if (is_down(keyboard_matrix, KEY_CURSOR_LEFT) ||
        (joy0 & JOY_LEFT))
        mask |= BUTTON_LEFT;
    if (is_down(keyboard_matrix, KEY_CURSOR_RIGHT) ||
        (joy0 & JOY_RIGHT))
        mask |= BUTTON_RIGHT;
    if (is_down(keyboard_matrix, KEY_CURSOR_UP) ||
        (joy0 & JOY_UP))
        mask |= BUTTON_UP;
    if (is_down(keyboard_matrix, KEY_CURSOR_DOWN) ||
        (joy0 & JOY_DOWN))
        mask |= BUTTON_DOWN;
    if (is_down(keyboard_matrix, KEY_Z) ||
        is_down(keyboard_matrix, KEY_N) ||
        is_down(keyboard_matrix, KEY_C) ||
        is_down(keyboard_matrix, KEY_ENTER) ||
        (joy0 & JOY_BUTTON1))
        mask |= BUTTON_ACTION1;
    if (is_down(keyboard_matrix, KEY_X) ||
        is_down(keyboard_matrix, KEY_M) ||
        is_down(keyboard_matrix, KEY_V) ||
        (joy0 & JOY_BUTTON2))
        mask |= BUTTON_ACTION2;
    m_buttons[0] = mask;

    uint8_t joy1 = *(volatile uint8_t *) _JOYSTICK1;
    mask = 0;
    if (is_down(keyboard_matrix, KEY_S) ||
        (joy1 & JOY_LEFT))
        mask |= BUTTON_LEFT;
    if (is_down(keyboard_matrix, KEY_D) ||
        (joy1 & JOY_RIGHT))
        mask |= BUTTON_RIGHT;
    if (is_down(keyboard_matrix, KEY_F) ||
        (joy1 & JOY_UP))
        mask |= BUTTON_UP;
    if (is_down(keyboard_matrix, KEY_E) ||
        (joy1 & JOY_DOWN))
        mask |= BUTTON_DOWN;
    if (is_down(keyboard_matrix, KEY_TAB) ||
        is_down(keyboard_matrix, KEY_LEFT_SHIFT) ||
        (joy1 & JOY_BUTTON1))
        mask |= BUTTON_ACTION1;
    if (is_down(keyboard_matrix, KEY_Q) ||
        is_down(keyboard_matrix, KEY_A) ||
        (joy1 & JOY_BUTTON2))
        mask |= BUTTON_ACTION2;
    m_buttons[1] = mask;

    escape = is_down(keyboard_matrix, KEY_BREAK);

    bool shifted = is_down(keyboard_matrix, KEY_LEFT_SHIFT) ||
                   is_down(keyboard_matrix, KEY_RIGHT_SHIFT);
    for (unsigned i=0;i<256;++i) {
        if (is_down(keyboard_matrix, i))
            m_keypress = scancode_to_name[shifted?1:0][i];
    }
#endif

    if (m_memory[MEMORY_DEVKIT_MODE] & 0x2)
        m_buttons[0] |= (((m_mouse_buttons >> 0) & 1) << 4) | (((m_mouse_buttons >> 1) & 1) << 5) | (((m_mouse_buttons >> 2) & 1) << 6);

    uint8_t delay = m_memory[MEMORY_AUTO_REPEAT_DELAY];
    if (delay == 0)
        delay = DEFAULT_AUTO_REPEAT_DELAY;
    uint8_t interval = m_memory[MEMORY_AUTO_REPEAT_INTERVAL];
    if (interval == 0)
        interval = DEFAULT_AUTO_REPEAT_INTERVAL;
    for (unsigned p=0;p<2;++p) {
        m_buttonsp[p] = 0;
        for (unsigned i=0;i<6;++i) {
            if (m_buttons[p] & (1 << i)) {
                if (!m_button_down_time[p][i]) {
                    m_button_down_time[p][i] = m_frames;
                    m_buttonsp[p] |= 1 << i;
                } else if (delay != 255 && !(m_button_first_repeat[p] & (1 << i)) && m_frames - m_button_down_time[p][i] >= delay) {
                    m_button_down_time[p][i] = m_frames;
                    m_button_first_repeat[p] |= 1 << i;
                    m_buttonsp[p] |= 1 << i;
                } else if ((m_button_first_repeat[p] & (1 << i)) && m_frames - m_button_down_time[p][i] >= interval) {
                    m_button_down_time[p][i] = m_frames;
                    m_buttonsp[p] |= 1 << i;
                }
            } else  {
                if (m_button_down_time[p][i]) {
                    m_button_down_time[p][i] = 0;
                    m_button_first_repeat[p] &= ~(1 << i);
                }
            }
        }
    }

    static bool prev_escape = false;
    bool old_prev_escape = prev_escape;
    prev_escape = escape;
    if (escape && !old_prev_escape)
        p8_abort();
}

void p8_flip()
{
    p8_update_input();

    p8_render();

    p8_flush_cartdata();

    unsigned elapsed_time = p8_elapsed_time();
    const unsigned target_frame_time = 1000 / m_fps;
    int sleep_time = target_frame_time - elapsed_time;
    if (sleep_time < 0)
        sleep_time = 0;
    m_actual_fps = 1000 / (elapsed_time + sleep_time);
    m_frames++;

    if (sleep_time > 0)
    {
#ifdef OS_FREERTOS
        vTaskDelay(pdMS_TO_TICKS(sleep_time));
#else
        usleep(sleep_time * 1000);
#endif
    }

#ifdef OS_FREERTOS
    m_start_time = xTaskGetTickCount();
#else
    m_start_time = clock();
#endif
}

static void p8_main_loop()
{
    for (;;)
    {
        lua_update();
        lua_draw();

        p8_flip();
    }
}

unsigned p8_elapsed_time(void)
{
    unsigned elapsed_time;
#ifdef OS_FREERTOS
    long now = xTaskGetTickCount();
    if (start_time == 0)
        elapsed_time = 0;
    else
        elapsed_time = (now - m_start_time) * portTICK_PERIOD_MS;
#else
    clock_t now = clock();
    if (m_start_time == 0)
        elapsed_time = 0;
    else
        elapsed_time = ((now - m_start_time) + ((now < m_start_time) ? CLOCKS_PER_CLOCK_T : 0)) * (clock_t)1000 / CLOCKS_PER_SEC;
#endif
    return elapsed_time;
}

void p8_reset(void)
{
    memset(m_memory + MEMORY_DRAWSTATE, 0, MEMORY_DRAWSTATE_SIZE);
    memset(m_memory + MEMORY_HARDWARESTATE, 0, MEMORY_HARDWARESTATE_SIZE);
    pencolor_set(6);
    reset_color();
    clip_set(0, 0, P8_WIDTH, P8_HEIGHT);
}

static void __attribute__ ((noreturn)) p8_abort()
{
    longjmp(jmpbuf, 1);
}

void __attribute__ ((noreturn)) p8_restart()
{
    restart = true;
    p8_abort();
}

bool p8_open_cartdata(const char *id)
{
    if (cartdata)
        return false;
    int ret = mkdir(CARTDATA_PATH, 0777);
    if (ret == 0 && errno != EEXIST) {
        return false;
    } else {
        char *path = alloca(strlen(CARTDATA_PATH) + 1 + strlen(id) + 1);
        sprintf(path, "%s/%s", CARTDATA_PATH, id);
        cartdata = fopen(path, "w+");
        if (cartdata == NULL) {
            return false;
        } else {
            fread(m_memory + MEMORY_CARTDATA, 0x100, 1, cartdata);
            return true;
        }
    }
}

void p8_flush_cartdata(void)
{
    if (cartdata && cartdata_needs_flush) {
        cartdata_needs_flush = false;
        fseek(cartdata, 0, SEEK_SET);
        fwrite(m_memory + MEMORY_CARTDATA, 0x100, 1, cartdata);
        fflush(cartdata);
    }
}

void p8_delayed_flush_cartdata(void)
{
    cartdata_needs_flush = true;
}

void p8_close_cartdata(void)
{
    if (cartdata) {
        p8_flush_cartdata();
        fclose(cartdata);
        cartdata = NULL;
    }
}

static void p8_show_compatibility_error(int severity)
{
    p8_reset();
    clear_screen(0);
    draw_rect(10, 51, 118, 78, 7, 0);
    if (severity <= COMPAT_SOME) {
        draw_text("this cart may not be", 24, 55, 7);
        draw_text("fully compatible with", 22, 62, 7);
        draw_text(PROGNAME, 64-strlen(PROGNAME)*GLYPH_WIDTH/2, 69, 7);
    } else {
        draw_text("this cart is not", 32, 55, 7);
        draw_text("compatible with", 34, 62, 7);
        draw_text(PROGNAME, 64-strlen(PROGNAME)*GLYPH_WIDTH/2, 69, 7);
    }
    p8_flip();
    p8_update_input();
    while ((m_buttons[0] & BUTTON_ACTION1)) { p8_update_input(); }
    while (!(m_buttons[0] & BUTTON_ACTION1)) { p8_update_input(); }
    while ((m_buttons[0] & BUTTON_ACTION1)) { p8_update_input(); }
    clear_screen(0);
}
