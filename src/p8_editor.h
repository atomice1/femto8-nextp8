/**
 * Copyright (C) 2026 Chris January
 */

#ifndef P8_EDITOR_H
#define P8_EDITOR_H

#include "p8_screen.h"

typedef enum {
    P8_TAB_CODE   = 0,  /* F1 */
    P8_TAB_SPRITE = 1,  /* F2 */
    P8_TAB_MAP    = 2,  /* F3 */
    P8_TAB_SFX    = 3,  /* F4 */
    P8_TAB_MUSIC  = 4,  /* F5 */
    P8_TAB_COUNT
} p8_tab_t;

extern p8_screen_t p8_screen_edit;

/** Invalidate the editors after cart load or reset */
extern void p8_editor_invalidate(void);

/** Mark the cart as modified */
extern void p8_editor_mark_modified(void);

/** Synchronize all editor tabs with the current cart state */
extern void p8_editor_sync(void);

#endif
