#ifndef P8_EDITOR_CODE_H
#define P8_EDITOR_CODE_H

#include <stdbool.h>
#include "p8_editor_tab.h"

extern p8_editor_tab_t p8_subeditor_code;

/** Invalidate the code editor line cache */
void p8_editor_invalidate(void);

/** Set current line number */
void p8_editor_code_set_line(int line);

/** Sync the global script to the code editor contents */
void p8_editor_code_sync(void);

#endif
