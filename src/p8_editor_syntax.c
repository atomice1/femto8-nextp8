/**
 * PICO-8 / Lua syntax highlighting implementation.
 *
 * Token classes:
 *   KEYWORD  – Lua control keywords (if, then, else, end, while, …)
 *   SPECIAL  – nil, true, false
 *   BUILTIN  – PICO-8 API functions (btn, spr, cls, …)
 *   STRING   – double-quoted, single-quoted, and long [[ … ]] strings
 *   COMMENT  – single-line  --  and block  --[[ … ]]  comments
 *   NUMBER   – decimal and 0x hex literals
 *   NORMAL   – everything else
 *
 * Multi-line state (block comments, long strings) is tracked via the
 * p8_syn_context_t struct; callers must preserve it between lines.
 */

#include <string.h>
#include "p8_editor_syntax.h"

/* -------------------------------------------------------------------------
 * Keyword / built-in tables
 * Kept sorted so a binary search can replace the linear scan if speed ever
 * matters, but with PICO-8's short lines a linear search is fine.
 * ------------------------------------------------------------------------- */

/* Lua control keywords → P8_SYN_COLOR_KEYWORD */
static const char * const lua_keywords[] = {
    "and", "break", "do", "else", "elseif", "end",
    "for", "function", "goto", "if", "in",
    "local", "not", "or",
    "repeat", "return", "then", "until", "while",
    NULL
};

/* Lua/PICO-8 special literals → P8_SYN_COLOR_SPECIAL */
static const char * const lua_specials[] = {
    "false", "nil", "true",
    NULL
};

/* PICO-8 built-in functions → P8_SYN_COLOR_BUILTIN
 * (from syntax-highlighting.txt; "function" is omitted – it is a keyword) */
static const char * const p8_builtins[] = {
    "abs", "add", "all", "assert", "atan",
    "band", "bnot", "bor", "btn", "btnp", "bxor",
    "camera", "cartdata", "ceil", "chr", "circ", "circfill", "clip", "cls",
    "cocreate", "color", "coresume", "cos", "costatus", "count", "cstore",
    "cursor",
    "del", "deli", "dget", "dset",
    "extcmd",
    "fget", "fillp", "flip", "flr", "foreach", "fset",
    "getmetatable",
    "info", "ipairs",
    "line", "load", "ls", "lshr",
    "map", "max", "memcpy", "memset", "menuitem", "mget", "mid", "min",
    "mset", "music",
    "ord", "oval", "ovalfill",
    "pairs", "pal", "palt", "peek", "pget", "poke", "print", "printh",
    "pset",
    "rawequal", "rawget", "rawlen", "rawset",
    "rect", "rectfill", "reload", "reset", "rnd", "rotl", "rotr",
    "rrect", "rrectfill", "run",
    "save", "select", "setmetatable", "sfx", "sget", "sgn", "shl", "shr",
    "sin", "split", "spr", "sqrt", "srand", "sset", "sspr", "stat", "stop",
    "sub",
    "time", "tline", "tonum", "tostr", "type",
    "yield",
    NULL
};

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static int table_contains(const char * const *table, const char *word, int len)
{
    for (int i = 0; table[i]; i++) {
        if ((int)strlen(table[i]) == len &&
            memcmp(table[i], word, (size_t)len) == 0)
            return 1;
    }
    return 0;
}

static int is_ident_start(unsigned char c)
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static int is_ident_cont(unsigned char c)
{
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static int is_digit(unsigned char c)
{
    return c >= '0' && c <= '9';
}

static int is_hex_digit(unsigned char c)
{
    return is_digit(c) ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/* -------------------------------------------------------------------------
 * Main highlight function
 * ------------------------------------------------------------------------- */

void p8_syn_highlight_line(const char *line, int len,
                           uint8_t *colors,
                           p8_syn_context_t *ctx)
{
    int i = 0;

    while (i < len) {

        /* ---------------------------------------------------------------
         * Continuation of a multi-line --[[ block comment
         * --------------------------------------------------------------- */
        if (ctx->in_block_comment) {
            while (i < len) {
                if (line[i] == ']' && i + 1 < len && line[i + 1] == ']') {
                    colors[i]     = P8_SYN_COLOR_COMMENT;
                    colors[i + 1] = P8_SYN_COLOR_COMMENT;
                    i += 2;
                    ctx->in_block_comment = 0;
                    break;
                }
                colors[i++] = P8_SYN_COLOR_COMMENT;
            }
            /* If we hit end-of-line still inside, the outer loop will exit
             * naturally because i == len. */
            continue;
        }

        /* ---------------------------------------------------------------
         * Continuation of a multi-line [[ long string
         * --------------------------------------------------------------- */
        if (ctx->in_long_string) {
            while (i < len) {
                if (line[i] == ']' && i + 1 < len && line[i + 1] == ']') {
                    colors[i]     = P8_SYN_COLOR_STRING;
                    colors[i + 1] = P8_SYN_COLOR_STRING;
                    i += 2;
                    ctx->in_long_string = 0;
                    break;
                }
                colors[i++] = P8_SYN_COLOR_STRING;
            }
            continue;
        }

        unsigned char c = (unsigned char)line[i];

        /* ---------------------------------------------------------------
         * Comment:  --  or  --[[
         * --------------------------------------------------------------- */
        if (c == '-' && i + 1 < len && (unsigned char)line[i + 1] == '-') {
            if (i + 3 < len &&
                (unsigned char)line[i + 2] == '[' &&
                (unsigned char)line[i + 3] == '[') {
                /* Block comment opening */
                colors[i]     = P8_SYN_COLOR_COMMENT;
                colors[i + 1] = P8_SYN_COLOR_COMMENT;
                colors[i + 2] = P8_SYN_COLOR_COMMENT;
                colors[i + 3] = P8_SYN_COLOR_COMMENT;
                i += 4;
                ctx->in_block_comment = 1;
                /* Rest of the line is consumed by the continuation branch
                 * on the next iteration of the outer while loop. */
                continue;
            }
            /* Single-line comment: colour remainder of line */
            while (i < len)
                colors[i++] = P8_SYN_COLOR_COMMENT;
            break;
        }

        /* ---------------------------------------------------------------
         * Long string:  [[
         * --------------------------------------------------------------- */
        if (c == '[' && i + 1 < len && (unsigned char)line[i + 1] == '[') {
            colors[i]     = P8_SYN_COLOR_STRING;
            colors[i + 1] = P8_SYN_COLOR_STRING;
            i += 2;
            ctx->in_long_string = 1;
            continue;
        }

        /* ---------------------------------------------------------------
         * Quoted string:  "…"  or  '…'
         * --------------------------------------------------------------- */
        if (c == '"' || c == '\'') {
            unsigned char delim = c;
            colors[i++] = P8_SYN_COLOR_STRING;
            while (i < len) {
                if ((unsigned char)line[i] == '\\' && i + 1 < len) {
                    /* Escape sequence: colour both characters */
                    colors[i]     = P8_SYN_COLOR_STRING;
                    colors[i + 1] = P8_SYN_COLOR_STRING;
                    i += 2;
                } else if ((unsigned char)line[i] == delim) {
                    colors[i++] = P8_SYN_COLOR_STRING;
                    break;
                } else {
                    colors[i++] = P8_SYN_COLOR_STRING;
                }
            }
            continue;
        }

        /* ---------------------------------------------------------------
         * Numeric literal:  digit  or  0x…
         * --------------------------------------------------------------- */
        if (is_digit(c)) {
            if (c == '0' && i + 1 < len &&
                ((unsigned char)line[i + 1] == 'x' ||
                 (unsigned char)line[i + 1] == 'X')) {
                /* Hex literal */
                colors[i]     = P8_SYN_COLOR_NUMBER;
                colors[i + 1] = P8_SYN_COLOR_NUMBER;
                i += 2;
                while (i < len && is_hex_digit((unsigned char)line[i]))
                    colors[i++] = P8_SYN_COLOR_NUMBER;
            } else {
                /* Decimal / fixed-point literal */
                while (i < len &&
                       (is_digit((unsigned char)line[i]) ||
                        (unsigned char)line[i] == '.'))
                    colors[i++] = P8_SYN_COLOR_NUMBER;
            }
            continue;
        }

        /* ---------------------------------------------------------------
         * Identifier, keyword, special value, or built-in function
         * --------------------------------------------------------------- */
        if (is_ident_start(c)) {
            int start = i;
            while (i < len && is_ident_cont((unsigned char)line[i]))
                i++;
            int word_len = i - start;

            uint8_t col;
            if (table_contains(lua_keywords, line + start, word_len))
                col = P8_SYN_COLOR_KEYWORD;
            else if (table_contains(lua_specials, line + start, word_len))
                col = P8_SYN_COLOR_SPECIAL;
            else if (table_contains(p8_builtins, line + start, word_len))
                col = P8_SYN_COLOR_BUILTIN;
            else
                col = P8_SYN_COLOR_NORMAL;

            for (int j = start; j < i; j++)
                colors[j] = col;
            continue;
        }

        /* ---------------------------------------------------------------
         * Everything else  (operators, punctuation, whitespace, …)
         * --------------------------------------------------------------- */
        colors[i++] = P8_SYN_COLOR_NORMAL;
    }
}
