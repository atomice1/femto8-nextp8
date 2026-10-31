/**
 * Copyright (C) 2026 Chris January
 */

#include "p8_options_dialog.h"
#include "p8_dialog.h"
#include "p8_emu.h"
#include "p8_input.h"
#include "p8_options.h"
#include "p8_overlay_helper.h"

#define VOLUME_CONTROL_WIDTH  32
#define VOLUME_CONTROL_HEIGHT 8
#define PADDING               1

/* Draw the volume control (right triangle) */
static void draw_volume_control(const p8_dialog_t *dialog, void *user_data, int x, int y, int width, int height, bool selected)
{
    (void)dialog;
    (void)height;

    uint8_t volume = *(uint8_t *)user_data;

    /* Triangle points:
     *   (x, y+height-1) - bottom left
     *   (x+width-1, y) - top right
     *   (x+width-1, y+height-1) - bottom right
     *
     * Volume determines how much of the triangle is filled (white 7 or yellow 10 if selected)
     * vs empty (dark grey 5)
     * For dx from x to x+width-1:
     *   - Calculate y_at_dx (triangle outline) at position dx
     *   - If dx < volume * width / 255, fill line in white/yellow
     *   - Otherwise, draw outline in dark grey
     */

    int volume_threshold = (volume * width) / 255;
    int fill_color = selected ? 10 : 7;  /* Yellow if selected, white otherwise */
    int outline_color = 5;  /* Dark grey */
    int bottom = y + height - 1;
    int draw_width = width - PADDING * 2;

    for (int dx = 0; dx < draw_width; dx++)
    {
        /* Calculate y coordinates for this x position */
        /* Triangle line goes from (0, height-1) to (width-1, 0) */
        int y_at_dx = MAX(bottom - (dx * height) / draw_width, y);

        if (dx < volume_threshold)
        {
            /* Fill line */
            overlay_draw_vline(x + PADDING + dx, y_at_dx, bottom, fill_color);
        }
        else
        {
            /* Draw outline */
            overlay_draw_vline(x + PADDING + dx, y_at_dx, bottom, outline_color);
        }
    }
}

/* Handle button events for volume control */
static bool handle_volume_buttons(p8_dialog_t *dialog, void *user_data, int button_mask, int buttonp_mask)
{
    (void)dialog;

    uint8_t *volume = (uint8_t *)user_data;

    if ((buttonp_mask & BUTTON_MASK_LEFT)) {
        (*volume) = MAX((*volume) - 32, 0);
        return true;
    }

    if (buttonp_mask & BUTTON_MASK_RIGHT) {
        (*volume) = MIN((*volume) + 32, 255);
        return true;
    }

    /* Event not handled by this control */
    return false;
}

/* Show the options dialog with volume control and autosave checkbox */
void p8_show_options_dialog(void)
{
    uint8_t volume = g_options.volume;
    bool autosave_enabled = g_options.autosave_enabled;
    p8_dialog_control_t controls[] = {
        DIALOG_LABEL("volume"),
        DIALOG_CUSTOM_CONTROL_SELECTABLE(VOLUME_CONTROL_WIDTH, VOLUME_CONTROL_HEIGHT, draw_volume_control, &volume, handle_volume_buttons),
        DIALOG_SPACING(),
        DIALOG_CHECKBOX("autosave", &autosave_enabled),
        DIALOG_SPACING(),
        DIALOG_BUTTONBAR()
    };

    p8_dialog_t dialog;
    p8_dialog_init(&dialog, "options", controls,
                   sizeof(controls) / sizeof(controls[0]), 0);
    p8_dialog_action_t result = p8_dialog_run(&dialog);
    p8_dialog_cleanup(&dialog);

    if (result.type == DIALOG_RESULT_ACCEPTED) {
        g_options.volume = volume;
        g_options.autosave_enabled = autosave_enabled;
        p8_options_apply();
        p8_options_save();
    }
}
