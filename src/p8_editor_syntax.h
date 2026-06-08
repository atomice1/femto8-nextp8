/**
 * PICO-8 / Lua syntax highlighting for the femto8 code editor.
 *
 * Tokenises a single source line and produces a per-character PICO-8
 * colour-index array.  Multi-line constructs (block comments, long strings)
 * are tracked through a caller-maintained p8_syn_context_t value that is
 * updated after each line and passed in at the start of the next.
 */

#ifndef P8_EDITOR_SYNTAX_H
#define P8_EDITOR_SYNTAX_H

#include <stdint.h>

/* PICO-8 palette colour indices used for each token class.
 * All values are standard PICO-8 colour numbers (0-15). */
#define P8_SYN_COLOR_NORMAL   7   /* white  – default code                  */
#define P8_SYN_COLOR_KEYWORD  12  /* blue   – control keywords (if/end/…)   */
#define P8_SYN_COLOR_BUILTIN  11  /* green  – PICO-8 API functions           */
#define P8_SYN_COLOR_STRING   9   /* orange – string literals                */
#define P8_SYN_COLOR_COMMENT  5   /* grey   – single-line & block comments   */
#define P8_SYN_COLOR_NUMBER   10  /* yellow – numeric literals               */
#define P8_SYN_COLOR_SPECIAL  14  /* pink   – nil / true / false             */

/**
 * State carried across lines to handle multi-line constructs.
 * Initialise to {0, 0} before processing the first line of a file.
 */
typedef struct {
    int in_block_comment; /* non-zero while inside a  --[[ … ]]  comment  */
    int in_long_string;   /* non-zero while inside a  [[  … ]]  string    */
} p8_syn_context_t;

/**
 * Highlight a single source line.
 *
 * @param line    Null-terminated source text for the line.
 * @param len     strlen(line) – number of characters to process.
 * @param colors  Output buffer of at least @p len uint8_t entries; each
 *                entry receives the PICO-8 colour index for that character.
 * @param ctx     In/out multi-line state.  On return it reflects any
 *                block-comment or long-string that was opened (or closed)
 *                on this line and should be passed unchanged to the next call.
 */
void p8_syn_highlight_line(const char *line, int len,
                           uint8_t *colors,
                           p8_syn_context_t *ctx);

#endif /* P8_EDITOR_SYNTAX_H */
