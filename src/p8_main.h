/**
 * Copyright (C) 2026 Chris January
 */

#ifndef P8_MAIN_H
#define P8_MAIN_H

typedef enum {
    P8_SCREEN_BROWSE = 0,
    P8_SCREEN_EDIT,
} p8_screen_index_t;

extern void p8_main(void);
extern void p8_set_active_screen(p8_screen_index_t screen);

#endif
