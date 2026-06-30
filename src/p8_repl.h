#ifndef P8_REPL_H
#define P8_REPL_H

#include "p8_screen.h"

extern p8_screen_t p8_screen_repl;

/** Render a directory listing into the REPL overlay with pagination.
 * `arg` is the optional path argument (may be NULL or empty).
 */
void repl_handle_ls(const char *arg);

#endif
