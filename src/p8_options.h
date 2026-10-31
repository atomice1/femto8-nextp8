/**
 * Copyright (C) 2026 Chris January
 */

#ifndef P8_OPTIONS_H
#define P8_OPTIONS_H

#include <stdint.h>

/**
 * Options structure for femto8-nextp8.
 */
typedef struct
{
    /** Global volume level (0-255, top 4 bits stored in hardware) */
    uint8_t volume;
} p8_options_t;

/**
 * Global options variable.
 */
extern p8_options_t g_options;

/**
 * Load options from disk (options.ini).
 */
void p8_options_load(void);

/**
 * Save options to disk (options.ini).
 */
void p8_options_save(void);

/**
 * Apply current options (called at startup and after options dialog).
 */
void p8_options_apply(void);

/**
 * Set volume level (0-255).
 */
void p8_options_set_volume(uint8_t volume);

#endif /* P8_OPTIONS_H */
