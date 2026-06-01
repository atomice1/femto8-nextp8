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
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include "strtcpy.h"
#ifdef OS_FREERTOS
#include <FreeRTOS.h>
#include <task.h>
#include "retro_heap.h"
#include "ble_controller.h"
#endif
#include "p8_audio.h"
#include "p8_cache.h"
#include "p8_dialog.h"
#include "p8_emu.h"
#include "p8_input.h"
#include "p8_lua.h"
#include "p8_lua_helper.h"
#include "p8_overlay_helper.h"
#include "p8_parser.h"
#include "p8_pause_menu.h"

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

static int p8_init_lcd(void);
static void p8_main_loop();

uint8_t *m_memory = NULL;
uint8_t *m_cart_memory = NULL;
uint8_t *m_temp_cart_memory = NULL;
uint8_t *m_overlay_memory = NULL;
uint8_t *m_file_buffer = NULL;
uint8_t *m_decompression_buffer = NULL;
char    *m_lua_script  = NULL;
char    *m_temp_lua_script  = NULL;

unsigned m_fps = 30;
unsigned m_actual_fps = 0;
unsigned m_frames = 0;

p8_clock_t m_start_time;

static jmp_buf jmpbuf_restart;
static bool restart;
static bool cart_running = false;
static bool quit_requested = false;

static jmp_buf jmpbuf_load;
bool m_load_available = false;
static bool load_requested = false;
static char load_filename[PATH_MAX];
static char load_param[256];

char m_current_cart_dir[PATH_MAX];
char m_current_cart_file_name[PATH_MAX];
char m_breadcrumb[256];
char m_bbs_cart_id[256];
char m_param_string[256];
char m_clipboard[1024];

static bool skip_main_loop_if_no_callbacks = false;

#ifdef SDL
SDL_Window *m_window = NULL;
SDL_Renderer *m_renderer = NULL;
SDL_Texture *m_texture = NULL;
SDL_Surface *m_output = NULL;
SDL_PixelFormat *m_format = NULL;
#endif
#ifdef OS_FREERTOS
SemaphoreHandle_t m_drawSemaphore;
#endif


static FILE *cartdata = NULL;
static bool cartdata_needs_flush = false;

static int m_initialized = 0;

#ifdef NEXTP8
static int vfrontreq = 0;
#endif

const uint8_t io_icon[32] = {
    0x00, 0x77, 0x77, 0x77,
    0x00, 0x17, 0x11, 0x71,
    0x00, 0x17, 0x77, 0x71,
    0x00, 0x17, 0x77, 0x71,
    0x00, 0x17, 0x11, 0x71,
    0x00, 0x77, 0x77, 0x77,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};

p8_clock_t p8_clock(void)
{
#if defined(OS_FREERTOS)
    return xTaskGetTickCount();
#elif defined(NEXTP8)
    return *(volatile uint64_t *)_UTIMER_1MHZ;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * UINT64_C(1000000) + ts.tv_nsec / UINT64_C(1000);
#endif
}

unsigned p8_clock_ms(p8_clock_t clocks)
{
#if defined(OS_FREERTOS)
    return clocks * portTICK_PERIOD_MS;
#else
    return clocks / UINT64_C(1000);
#endif
}

p8_clock_t p8_clock_delta(p8_clock_t start, p8_clock_t end)
{
    return end - start;
}

static void p8_sleep(unsigned ms)
{
#ifdef OS_FREERTOS
    vTaskDelay(pdMS_TO_TICKS(ms));
#else
    usleep(ms * 1000);
#endif
}

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

    SDL_ShowCursor(SDL_DISABLE);

    /* Create SDL2 window/renderer/texture and an output surface we render into. */
    m_window = SDL_CreateWindow("femto-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!m_window) {
        printf("Error creating SDL window.\n");
        return 1;
    }
    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer) {
        SDL_DestroyWindow(m_window);
        m_window = NULL;
        printf("Error creating SDL renderer.\n");
        return 1;
    }

    /* Texture at native P8 resolution; we'll update it from the output surface. */
    m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, P8_WIDTH, P8_HEIGHT);
    m_output = SDL_CreateRGBSurfaceWithFormat(0, P8_WIDTH, P8_HEIGHT, 32, SDL_PIXELFORMAT_ARGB8888);
    m_format = m_output->format;

    SDL_SetWindowTitle(m_window, "femto-8");
#endif
#ifdef OS_FREERTOS
    m_drawSemaphore = xSemaphoreCreateBinary();

    xSemaphoreGive(m_drawSemaphore);
#endif

    m_memory = (uint8_t *)malloc(MEMORY_SIZE);
    m_cart_memory = (uint8_t *)malloc(CART_MEMORY_SIZE);
    m_temp_cart_memory = (uint8_t *)malloc(CART_MEMORY_SIZE);
    m_overlay_memory = (uint8_t *)malloc(MEMORY_SCREEN_SIZE);
    m_file_buffer = (uint8_t *)malloc(FILE_BUFFER_SIZE);
    m_decompression_buffer = (uint8_t *)malloc(DECOMPRESSION_BUFFER_SIZE);
    m_lua_script  = (char *)malloc(LUA_SCRIPT_SIZE);
    m_temp_lua_script  = (char *)malloc(LUA_SCRIPT_SIZE);

    if (!m_memory || !m_cart_memory || !m_overlay_memory || !m_file_buffer || !m_decompression_buffer || !m_lua_script || !m_temp_lua_script) {
        fputs("Out of memory\n", stderr);
        free(m_memory);
        free(m_cart_memory);
        free(m_temp_cart_memory);
        free(m_overlay_memory);
        free(m_file_buffer);
        free(m_decompression_buffer);
        free(m_lua_script);
        free(m_temp_lua_script);
        return 1;
    }

    memset(m_memory, 0, MEMORY_SIZE);
    memset(m_cart_memory, 0, CART_MEMORY_SIZE);
    memset(m_overlay_memory, (OVERLAY_TRANSPARENT_COLOR << 4) | OVERLAY_TRANSPARENT_COLOR, MEMORY_SCREEN_SIZE);
    m_lua_script[0] = '\0';

#ifdef ENABLE_AUDIO
    audio_init();
#endif

    p8_init_lcd();
    p8_init_input();

    m_initialized = 1;

    return 0;
}

static int p8_init_lcd(void)
{
#if defined(NEXTP8)
    uint8_t overlay_ctrl_reg = _OVERLAY_ENABLE_BIT | (OVERLAY_TRANSPARENT_COLOR & _OVERLAY_TRANSPARENT_MASK);
    *(volatile uint8_t *)_OVERLAY_CONTROL = overlay_ctrl_reg;
    vfrontreq = *(volatile uint8_t *)_VFRONT;
#elif defined(__DA1470x__)
    gdi_set_layer_start(HW_LCDC_LAYER_0, 0, 0);

    gdi_set_layer_enable(HW_LCDC_LAYER_0, true);

    uint16_t *fb = (uint16_t *)gdi_get_frame_buffer_addr(HW_LCDC_LAYER_0);

    gdi_set_layer_src(HW_LCDC_LAYER_0, fb, SCREEN_WIDTH, SCREEN_HEIGHT, GDI_FORMAT_RGB565);
#endif

    return 0;
}

static void p8_wait_for_any_key(void)
{
    int x, y;
    cursor_get(&x, &y);
    y = scroll(y, GLYPH_HEIGHT);
    draw_simple_text("press any key...", 0, y, 7);
    p8_flip();
    while (!p8_is_quit_requested()) {
        p8_update_input();
        unsigned scancode = 0, keymod = 0;
        uint8_t keypress = 0;
        if (p8_get_next_keypress(&scancode, &keypress, &keymod))
            break;
    }
}

static int p8_init_common(const char *file_name, const char *lua_script)
{
    p8_show_io_icon(false);

    if (lua_script == NULL) {
        if (file_name) fprintf(stderr, "%s: ", file_name);
        fprintf(stderr, "invalid cart\n");
        return -1;
    }

    cart_running = true;
    if (setjmp(jmpbuf_restart)) {
        if (!restart) {
            cart_running = false;
            return 0;
        }
        lua_shutdown_api();
        lua_load_api();
        printf("restarted cart\n");
    }

    restart = false;

    memcpy(m_memory, m_cart_memory, CART_MEMORY_SIZE);

    m_frames = 0;
    p8_init_input();

    p8_reset();
    clear_screen(0);
    p8_update_input();

    lua_init_script(file_name, lua_script);

    lua_init();

    if (lua_has_main_loop_callbacks())
        p8_main_loop();
    else if (!skip_main_loop_if_no_callbacks)
        p8_wait_for_any_key();

    cart_running = false;

    return 0;
}

int p8_init_file_with_param(const char *file_name, const char *param)
{
    if (!m_initialized)
        p8_init();

    strtcpy(m_param_string, param ? param : "", sizeof(m_param_string));
    m_load_available = true;

    if (setjmp(jmpbuf_load)) {
        load_requested = false;

        lua_shutdown_api();

        file_name = load_filename;
    }

    m_current_cart_dir[0] = '\0';
    m_current_cart_file_name[0] = '\0';
    if (!m_bbs_cart_id[0])
        strtcpy(m_current_cart_file_name, file_name, sizeof(m_current_cart_file_name));

    /* For BBS carts, set m_current_cart_dir to DEFAULT_CARTS_PATH */
    if (m_bbs_cart_id[0]) {
        if (access(DEFAULT_CARTS_PATH, F_OK) != -1)
            strtcpy(m_current_cart_dir, DEFAULT_CARTS_PATH, sizeof(m_current_cart_dir));
        else
            strtcpy(m_current_cart_dir, ".", sizeof(m_current_cart_dir));
    } else {
        const char *last_slash = strrchr(file_name, '/');
        if (last_slash) {
            size_t dir_len = last_slash - file_name;
            if (dir_len >= sizeof(m_current_cart_dir))
                dir_len = sizeof(m_current_cart_dir) - 1;
            memcpy(m_current_cart_dir, file_name, dir_len);
            m_current_cart_dir[dir_len] = '\0';
        } else {
            strtcpy(m_current_cart_dir, ".", sizeof(m_current_cart_dir));
        }
    }

    p8_show_io_icon(true);
    lua_load_api();

    printf("Loading %s\n", file_name);
    if (parse_cart_file(file_name, m_cart_memory, m_file_buffer, m_decompression_buffer, m_lua_script, NULL) != 0)
        return -1;

    int ret = p8_init_common(file_name, m_lua_script);

    return ret;
}

int p8_init_ram(uint8_t *buffer, int size)
{
    if (!m_initialized)
        p8_init();

    p8_show_io_icon(true);
    lua_load_api();

    parse_cart_ram(buffer, size, m_cart_memory, m_decompression_buffer, m_lua_script, NULL);

    int init_ret = p8_init_common(NULL, m_lua_script);

    return init_ret;
}

int p8_shutdown()
{
#ifdef ENABLE_AUDIO
    audio_close();
#endif

    lua_shutdown_api();

    p8_close_cartdata();

#ifdef SDL
    if (m_texture) { SDL_DestroyTexture(m_texture); m_texture = NULL; }
    if (m_renderer) { SDL_DestroyRenderer(m_renderer); m_renderer = NULL; }
    if (m_window) { SDL_DestroyWindow(m_window); m_window = NULL; }
    if (m_output) { SDL_FreeSurface(m_output); m_output = NULL; }
    SDL_Quit();
#endif

    free(m_cart_memory);
    free(m_temp_cart_memory);
    free(m_memory);
    free(m_overlay_memory);
    free(m_file_buffer);
    free(m_decompression_buffer);
    free(m_lua_script);
    free(m_temp_lua_script);
    m_cart_memory = NULL;
    m_memory = NULL;
    m_overlay_memory = NULL;
    m_file_buffer = NULL;
    m_decompression_buffer = NULL;
    m_lua_script = NULL;
    m_temp_lua_script = NULL;

    m_initialized = 0;

    return 0;
}

#ifdef SDL
// Map output pixel (ox, oy) to source framebuffer pixel (sx, sy) based on
// the screen transform mode at 0x5f2c.
static void screen_transform_pixel(uint8_t mode, int ox, int oy, int *sx, int *sy)
{
    switch (mode) {
    default:
    case 0: // normal
        *sx = ox; *sy = oy;
        break;
    case 1: // horizontal stretch: 64x128 → 128x128
        *sx = ox >> 1; *sy = oy;
        break;
    case 2: // vertical stretch: 128x64 → 128x128
        *sx = ox; *sy = oy >> 1;
        break;
    case 3: // both stretch: 64x64 → 128x128
        *sx = ox >> 1; *sy = oy >> 1;
        break;
    case 5: // horizontal mirror: left half mirrored to right
        *sx = (ox < 64) ? ox : (127 - ox); *sy = oy;
        break;
    case 6: // vertical mirror: top half mirrored to bottom
        *sx = ox; *sy = (oy < 64) ? oy : (127 - oy);
        break;
    case 7: // both mirror: top-left quarter mirrored to all
        *sx = (ox < 64) ? ox : (127 - ox);
        *sy = (oy < 64) ? oy : (127 - oy);
        break;
    case 129: // horizontal flip
        *sx = 127 - ox; *sy = oy;
        break;
    case 130: // vertical flip
        *sx = ox; *sy = 127 - oy;
        break;
    case 131: // both flip (180 degree rotation)
        *sx = 127 - ox; *sy = 127 - oy;
        break;
    case 133: // clockwise 90 degree rotation
        *sx = 127 - oy; *sy = ox;
        break;
    case 134: // 180 degree rotation
        *sx = 127 - ox; *sy = 127 - oy;
        break;
    case 135: // counterclockwise 90 degree rotation
        *sx = oy; *sy = 127 - ox;
        break;
    }
}

// Resolve palette for a framebuffer pixel given the high-color mode.
// pix_index is the raw 4-bit pixel value, sy is the source scanline,
// sx is the source x coordinate.
static uint8_t high_color_resolve(uint8_t hc_mode, uint8_t pix_index, int sx, int sy)
{
    if (hc_mode == 0x10) {
        // Per-line palette swap: use secondary palette if bit set in bitfield
        uint8_t bf = m_memory[0x5f70 + (sy >> 3)];
        if (bf & (1 << (sy & 7)))
            return color_get(PALTYPE_SECONDARY, pix_index);
        return color_get(PALTYPE_SCREEN, pix_index);
    }
    if (hc_mode == 0x20) {
        // 5-bitplane mode: requires 0x5f2c == 1 (horizontal stretch).
        // If the corresponding pixel in the hidden right half (sx+64) is
        // non-zero, use secondary palette.
        int hidden_offset = (m_memory[MEMORY_SCREEN_PHYS] << 8) + ((sx + 64) >> 1) + sy * 64;
        uint8_t hidden_val = m_memory[hidden_offset];
        uint8_t hidden_pix = IS_EVEN(sx + 64) ? (hidden_val & 0xF) : (hidden_val >> 4);
        if (hidden_pix != 0)
            return color_get(PALTYPE_SECONDARY, pix_index);
        return color_get(PALTYPE_SCREEN, pix_index);
    }
    if ((hc_mode & 0xf0) == 0x30) {
        // Gradient fill: replace color n with per-section gradient
        uint8_t replace_color = hc_mode & 0x0f;
        uint8_t screen_index = color_get(PALTYPE_SCREEN, pix_index);
        if ((screen_index & 0x0f) == replace_color) {
            int section = sy >> 3;
            uint8_t bf = m_memory[0x5f70 + (sy >> 3)];
            if (bf & (1 << (sy & 7)))
                section = (section + 1) & 0x0f;
            return color_get(PALTYPE_SECONDARY, section);
        }
        return screen_index;
    }
    return color_get(PALTYPE_SCREEN, pix_index);
}

void p8_render()
{
    uint32_t *output = m_output->pixels;
    uint8_t transform = m_memory[MEMORY_SCREEN_TRANSFORM];
    uint8_t hc_mode = m_memory[MEMORY_HIGH_COLOUR_MODE];

    for (int y = 0; y < P8_HEIGHT; y++)
    {
        for (int x = 0; x < P8_WIDTH; x++)
        {
            int sx, sy;
            screen_transform_pixel(transform, x, y, &sx, &sy);
            int screen_offset = (m_memory[MEMORY_SCREEN_PHYS] << 8) + (sx >> 1) + sy * 64;
            uint8_t value = m_memory[screen_offset];
            uint8_t pix = IS_EVEN(sx) ? value & 0xF : value >> 4;
            uint8_t index;
            if (hc_mode != 0)
                index = high_color_resolve(hc_mode, pix, sx, sy);
            else
                index = color_get(PALTYPE_SCREEN, pix);
            uint32_t color = m_colors[color_index(index)];

            output[x + (y * P8_WIDTH)] = color;
        }
    }

    uint8_t *overlay_mem = m_overlay_memory;

    for (int y = 0; y < P8_HEIGHT; y++)
    {
        for (int x = 0; x < P8_WIDTH; x++)
        {
            int overlay_offset = (x >> 1) + y * 64;
            uint8_t value = overlay_mem[overlay_offset];
            uint8_t pixel_color = IS_EVEN(x) ? (value & 0xF) : (value >> 4);

            if (pixel_color != OVERLAY_TRANSPARENT_COLOR)
            {
                uint32_t color = m_colors[color_index(pixel_color)];
                output[x + (y * P8_WIDTH)] = color;
            }
        }
    }

    SDL_Rect rectDest = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
#ifdef SDL
    if (m_texture && m_renderer) {
        SDL_UpdateTexture(m_texture, NULL, m_output->pixels, m_output->pitch);
        SDL_RenderClear(m_renderer);
        SDL_RenderCopy(m_renderer, m_texture, NULL, &rectDest);
        SDL_RenderPresent(m_renderer);
    }
#else
    (void)rectDest;
#endif
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
    draw_text(m_str_buffer, strlen(m_str_buffer), 0, 0, 1, 0, false, NULL, NULL, NULL);

    uint16_t *output = gdi_get_frame_buffer_addr(HW_LCDC_LAYER_0);
    uint8_t *screen_mem = &m_memory[(m_memory[MEMORY_SCREEN_PHYS] << 8)];
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

    output = gdi_get_frame_buffer_addr(HW_LCDC_LAYER_0);

    for (int y = 1; y <= P8_HEIGHT; y++)
    {
        if (y & 0x7)
        {
            uint16_t *top = output;

            for (int x = 0; x < 128; x += 2)
            {
                int overlay_offset = (x >> 1) + (y - 1) * 64;
                uint8_t value = m_overlay_memory[overlay_offset];
                uint8_t left = value & 0xF;
                uint8_t right = value >> 4;

                if (left != OVERLAY_TRANSPARENT_COLOR)
                {
                    uint16_t c_left = m_colors[color_index(left)];
                    *top = c_left;
                    *(top + 1) = c_left;
                }
                top += 2;

                if (right != OVERLAY_TRANSPARENT_COLOR)
                {
                    uint16_t c_right = m_colors[color_index(right)];
                    *top = c_right;
                    *(top + 1) = c_right;
                }
                top += 2;
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
    uint8_t *screen_mem = &m_memory[(m_memory[MEMORY_SCREEN_PHYS] << 8)];
    uint8_t *pal = &m_memory[MEMORY_PALETTES + PALTYPE_SCREEN * 16];
    memcpy((uint8_t *)_PALETTE_BASE, pal, _PALETTE_SIZE);
    memcpy((uint8_t *)_BACK_BUFFER_BASE, screen_mem, _FRAME_BUFFER_SIZE);
    *(volatile uint8_t *)_HIGH_COLOUR_MODE = m_memory[MEMORY_HIGH_COLOUR_MODE];
    *(volatile uint8_t *)_SCREEN_TRANSFORM = m_memory[MEMORY_SCREEN_TRANSFORM];
    if (m_memory[MEMORY_HIGH_COLOUR_MODE] != 0) {
        uint8_t *secondary_palette = &m_memory[MEMORY_PALETTE_SECONDARY];
        memcpy((uint8_t *)_SECONDARY_PALETTE_BASE, secondary_palette, 32);
    }
    static int dialog_showing_clear_n = 0;
    if (m_dialog_showing || dialog_showing_clear_n > 0)
        memcpy((uint8_t *)_OVERLAY_BACK_BUFFER_BASE, m_overlay_memory, MEMORY_SCREEN_SIZE);
    else
        memcpy((uint8_t *)_OVERLAY_BACK_BUFFER_BASE, m_overlay_memory, 512); // optimization: only copy first 8 rows if pause menu not showing
    if (m_dialog_showing)
        dialog_showing_clear_n = 2;
    else if (dialog_showing_clear_n > 0)
        dialog_showing_clear_n--;
    *(volatile uint8_t *) _VFRONTREQ = vfrontreq = vback;
}
#endif

static void p8_post_flip(void)
{
    p8_flush_cartdata();
    p8_update_input();
    p8_check_for_pause();
    m_frames++;
}

void p8_flip()
{
    p8_render();

    unsigned elapsed_time = p8_elapsed_time();
    const unsigned target_frame_time = 1000 / m_fps;
    int sleep_time = target_frame_time - elapsed_time;
    if (sleep_time < 0)
        sleep_time = 0;
    m_actual_fps = 1000 / (elapsed_time + sleep_time);

    if (sleep_time > 0)
        p8_sleep(sleep_time);

    m_start_time = p8_clock();

    p8_post_flip();
}

static void p8_main_loop()
{
    int time_debt = 0;
    unsigned updates_since_last_flip = 0;

    for (;;)
    {
        const int target_frame_time = 1000 / m_fps;

        lua_update();
        updates_since_last_flip++;

        unsigned elapsed = p8_elapsed_time();

        time_debt += elapsed;

        if (time_debt < target_frame_time || updates_since_last_flip >= m_fps) {
            lua_draw();
            time_debt += p8_elapsed_time() - elapsed;

            p8_flip();

            if (updates_since_last_flip >= m_fps) {
                time_debt = 0;
            } else {
                time_debt -= target_frame_time;
                if (time_debt < -target_frame_time) time_debt = -target_frame_time;
            }

            updates_since_last_flip = 0;
        } else {
            p8_post_flip();

            time_debt -= target_frame_time;
        }
    }
}

unsigned p8_elapsed_time(void)
{
    p8_clock_t now = p8_clock();
    unsigned elapsed_time;
    if (m_start_time == 0)
        elapsed_time = 0;
    else
        elapsed_time = p8_clock_ms(p8_clock_delta(m_start_time, now));
    return elapsed_time;
}

void p8_check_for_pause(void)
{
    if (!m_dialog_showing) {
        if ((m_buttonsp[0] & BUTTON_MASK_ESCAPE) != 0)
            p8_abort();

        if ((m_buttonsp[0] & BUTTON_MASK_PAUSE) != 0)
            p8_show_pause_menu();
    }
}

void p8_seed_rng_state(uint32_t seed)
{
    uint32_t hi, lo;

    if (seed == 0) {
        hi = 0x60009755;
        lo = 0xdeadbeef;
    } else {
        seed &= 0x7fffffff;

        // seed is already a full 32-bit fix32 value; use it directly.
        hi = seed ^ 0xbead29ba;
        lo = seed;
    }

    for (int i = 0; i < 32; i++) {
        hi = (hi << 16) | (hi >> 16);
        hi += lo;
        lo += hi;
    }

    m_memory[MEMORY_RNG_STATE] = hi & 0xFF;
    m_memory[MEMORY_RNG_STATE + 1] = (hi >> 8) & 0xFF;
    m_memory[MEMORY_RNG_STATE + 2] = (hi >> 16) & 0xFF;
    m_memory[MEMORY_RNG_STATE + 3] = (hi >> 24) & 0xFF;
    m_memory[MEMORY_RNG_STATE + 4] = lo & 0xFF;
    m_memory[MEMORY_RNG_STATE + 5] = (lo >> 8) & 0xFF;
    m_memory[MEMORY_RNG_STATE + 6] = (lo >> 16) & 0xFF;
    m_memory[MEMORY_RNG_STATE + 7] = (lo >> 24) & 0xFF;
}

void p8_reset(void)
{
    memset(m_memory + MEMORY_DRAWSTATE, 0, MEMORY_DRAWSTATE_SIZE);
    memset(m_memory + MEMORY_HARDWARESTATE, 0, MEMORY_HARDWARESTATE_SIZE);
    m_memory[MEMORY_SCREEN_PHYS] = 0x60;
    m_memory[MEMORY_MAP_START] = 0x20;
    m_memory[MEMORY_MAP_WIDTH] = 128;
    m_memory[MEMORY_RW_MASK] = 0xff;
    pencolor_set(6);
    reset_color();
    clip_set(0, 0, P8_WIDTH, P8_HEIGHT);
    // time(NULL) is a plain integer; shift left 16 to make it a fix32 integer.
    p8_seed_rng_state((uint32_t)time(NULL) << 16);
}

void __attribute__ ((noreturn)) p8_abort()
{
    assert(cart_running);
    longjmp(jmpbuf_restart, 1);
}

void __attribute__ ((noreturn)) p8_restart()
{
    assert(cart_running);
    restart = true;
    p8_abort();
}

void p8_quit()
{
    quit_requested = true;
    if (cart_running)
        p8_abort();
}

bool p8_is_cart_running(void)
{
    return cart_running;
}

bool p8_is_quit_requested(void)
{
    return quit_requested;
}

int p8_resolve_relative_path(char *dest_filename, const char *src_filename, size_t dest_size, bool for_cstore)
{
    if (src_filename[0] == '/' || src_filename[1] == ':')
        return strtcpy(dest_filename, src_filename, dest_size);

    if (!m_current_cart_dir[0])
        return strtcpy(dest_filename, src_filename, dest_size);

    for (int pass=1;pass<=(for_cstore?1:2);pass++) {
        const char *cart_dir = (pass == 1) ? DEFAULT_CARTS_PATH : m_current_cart_dir;
        int ret = snprintf(dest_filename, dest_size, "%s/%s", cart_dir, src_filename);
        if (pass == (for_cstore ? 1 : 2) || access(dest_filename, F_OK) == 0)
            return (ret > 0 && (size_t)ret < dest_size) ? ret : -1;
    }

    return -1;
}

void __attribute__ ((noreturn)) p8_load_new(const char *filename, const char *param)
{
    assert(m_load_available);

    strtcpy(load_filename, filename, sizeof(load_filename));
    if (param)
        strtcpy(load_param, param, sizeof(load_param));
    else
        load_param[0] = '\0';
    load_requested = true;

    longjmp(jmpbuf_load, 1);
}

void p8_set_skip_main_loop_if_no_callbacks(bool skip)
{
    skip_main_loop_if_no_callbacks = skip;
}

bool p8_open_cartdata(const char *id)
{
    if (cartdata)
        return false;
    int ret = MKDIR(CARTDATA_PATH);
    if (ret == -1 && errno != EEXIST) {
        return false;
    }
    char *path = alloca(strlen(CARTDATA_PATH) + 1 + strlen(id) + 1);
    sprintf(path, "%s/%s", CARTDATA_PATH, id);
    cartdata = fopen(path, "r+b");
    if (!cartdata) {
        cartdata = fopen(path, "w+b");
        if (!cartdata) {
            return false;
        }
    }
    fseek(cartdata, 0, SEEK_SET);
    uint8_t *dst = m_memory + MEMORY_CARTDATA;
    size_t n = fread(dst, 1, 0x100, cartdata);
    if (n < 0x100) {
        memset(dst + n, 0, 0x100 - n);
    }
    return true;
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

void p8_show_io_icon(bool show)
{
    if (show) {
        overlay_draw_icon(io_icon, P8_WIDTH - 8, 0);
    } else {
        overlay_draw_rectfill(P8_WIDTH - 8, 0, P8_WIDTH-1, 7, OVERLAY_TRANSPARENT_COLOR);
        p8_dialog_draw_stack();
    }
    p8_render();
}

void p8_show_error_dialog(const char **lines, int line_count, p8_error_severity_t severity)
{
    assert(line_count <= 4);

    int control_count = line_count + 2;
    p8_dialog_control_t controls[6];

    /* Add label controls for each line */
    for (int i = 0; i < line_count; i++) {
        controls[i] = (p8_dialog_control_t)DIALOG_LABEL(lines[i]);
    }

    /* Add spacing and button bar */
    controls[line_count] = (p8_dialog_control_t)DIALOG_SPACING();
    controls[line_count + 1] = (p8_dialog_control_t)DIALOG_BUTTONBAR_OK_ONLY();

    p8_dialog_t error_dialog;
    const char *title = (severity == P8_ERROR_WARNING) ? "warning" : "error";
    p8_dialog_init(&error_dialog, title, controls, control_count, 120);
    p8_dialog_run(&error_dialog);
    p8_dialog_cleanup(&error_dialog);
}

#ifdef ENABLE_BBS_DOWNLOAD
int p8_download_bbs_cart(const char *cart_id, char *cached_filename, size_t cached_filename_size)
{
    /* Download cart from BBS */
    p8_show_io_icon(true);
    printf("Downloading cart %s from BBS...\n", cart_id);
    cached_filename[0] = '\0';
    int ret = cache_download(cart_id, cached_filename, cached_filename_size);
    p8_show_io_icon(false);
    if (ret < 0) {
        printf("Failed to download cart %s from BBS, error %d\n", cart_id, errno);
        /* Show error dialog */
        const char *error_lines[] = {
            "failed to download cart",
            "from bbs."
        };
        p8_show_error_dialog(error_lines, 2, P8_ERROR_ERROR);
        return -1;
    } else {
        /* Successfully downloaded - set BBS cart ID */
        strtcpy(m_bbs_cart_id, cart_id, sizeof(m_bbs_cart_id));
        printf("Downloaded cart to %s\n", cached_filename);
        return 0;
    }
}
#endif

void p8_show_version_dialog(void)
{
#ifdef NEXTP8
    uint32_t hw_timestamp = *(uint32_t *)_BUILD_TIMESTAMP_HI;
    uint32_t hw_version = *(uint32_t *)_HW_VERSION_HI;
    uint32_t loader_version = _loader_data->loader_version;
    uint32_t loader_timestamp = _loader_data->loader_timestamp;
    extern uint32_t nextp8_version;
    extern uint32_t nextp8_timestamp;
    extern const char *femto8_version;

    char ram_string[12];
    char hw_version_string[50];
    char bsp_version_string[50];
    char loader_version_string[50];
    char nextp8_version_string[50];
    char femto8_version_string[50];
    unsigned ram_mb = _loader_data->memtop >> 20;
    strcpy(ram_string, "ram    0 mb");
    ram_string[7] = ram_mb + '0';
    _format_version(nextp8_version_string, sizeof(nextp8_version_string), "nextp8", nextp8_version, nextp8_timestamp);
    _format_version(hw_version_string, sizeof(hw_version_string), "hw    ", hw_version, hw_timestamp);
    _format_version(bsp_version_string, sizeof(bsp_version_string), "bsp   ", _bsp_version, _bsp_timestamp);
    _format_version(loader_version_string, sizeof(loader_version_string), "loader", loader_version, loader_timestamp);
    sprintf(femto8_version_string, "femto8 %s", femto8_version);

    p8_dialog_control_t controls[] = {
        DIALOG_LABEL(nextp8_version_string),
        DIALOG_LABEL(femto8_version_string),
        DIALOG_LABEL(hw_version_string),
        DIALOG_LABEL(bsp_version_string),
        DIALOG_LABEL(loader_version_string),
        DIALOG_LABEL(ram_string),
        DIALOG_SPACING(),
        DIALOG_BUTTONBAR_OK_ONLY()
    };

    p8_dialog_t dialog;
    p8_dialog_init(&dialog, "nextp8 version", controls, sizeof(controls) / sizeof(controls[0]), 128);
    p8_dialog_run(&dialog);
    p8_dialog_cleanup(&dialog);
#else
    extern const char *femto8_version;
    char femto8_version_string[50];
    sprintf(femto8_version_string, "femto8 %s", femto8_version);

    p8_dialog_control_t controls[] = {
        DIALOG_LABEL(femto8_version_string),
        DIALOG_SPACING(),
        DIALOG_BUTTONBAR_OK_ONLY()
    };

    p8_dialog_t dialog;
    p8_dialog_init(&dialog, "femto8 version", controls, sizeof(controls) / sizeof(controls[0]), 0);
    p8_dialog_run(&dialog);
    p8_dialog_cleanup(&dialog);
#endif
}
