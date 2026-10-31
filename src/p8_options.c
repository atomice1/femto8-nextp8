/**
 * Copyright (C) 2026 Chris January
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "p8_options.h"
#include "p8_audio.h"

/* Global options variable */
p8_options_t g_options = {
    .volume = 0x80  /* Default volume */
};

/* Options file path */
#define OPTIONS_FILE "options.ini"

/**
 * Load options from options.ini.
 */
void p8_options_load(void)
{
    FILE *fp = fopen(OPTIONS_FILE, "r");
    if (!fp)
    {
        /* File doesn't exist, use defaults */
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp))
    {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        /* Parse key=value */
        char *key = line;
        char *value = strchr(line, '=');
        if (!value)
            continue;

        *value = '\0';
        value++;

        /* Remove trailing whitespace/newlines from value */
        char *end = value + strlen(value) - 1;
        while (end > value && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t'))
        {
            *end = '\0';
            end--;
        }

        if (strcmp(key, "volume") == 0)
        {
            g_options.volume = (uint8_t)strtol(value, NULL, 10);
        }
    }

    fclose(fp);
}

/**
 * Save options to options.ini.
 */
void p8_options_save(void)
{
    FILE *fp = fopen(OPTIONS_FILE, "w");
    if (!fp)
    {
        return;
    }

    fprintf(fp, "# femto8-nextp8 options file\n");
    fprintf(fp, "volume=%d\n", g_options.volume);

    fclose(fp);
}

/**
 * Apply current options.
 */
void p8_options_apply(void)
{

    audio_set_volume(g_options.volume);
}

