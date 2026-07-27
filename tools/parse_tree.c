/*
 * parse_tree.c
 *
 * Display the parse tree of Pico-8 Lua scripts.
 *
 * Uses the Pico-8 cart parser and Lua implementation to parse
 * and display the internal Proto structure (parse tree).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>

#define LUA_CORE
#include "lua.h"
#include "lauxlib.h"
#include "lfunc.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "llex.h"
#include "lobject.h"
#include "lualib.h"

#include "p8_emu.h"
#include "p8_lua_helper.h"
#include "p8_parser.h"

/* We don't need the extern globals from p8_parser.c - we use the functions directly */

/* Provide minimal extern definitions for globals needed by p8_parser.c */
uint8_t *m_file_buffer = NULL;
char    *m_lua_script  = NULL;

/* Constants from p8_parser.c that are static there - we need to define them */
#ifndef MEMORY_TOTAL
#define MEMORY_TOTAL 0x10000
#endif

#ifndef RAW_DATA_LENGTH
#define RAW_DATA_LENGTH 0x4300
#endif

/* Forward declarations */
static void print_proto(Proto *f, int indent, int max_indent, const char *func_name);

/* Print indentation spaces */
static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

/* Print a TValue value */
static void print_tvalue(const TValue *v) {
    switch (ttypenv(v)) {
        case LUA_TNIL:
            printf("nil");
            break;
        case LUA_TNUMBER:
            printf("%f", (double)nvalue(v));
            break;
        case LUA_TBOOLEAN:
            printf("%s", bvalue(v) ? "true" : "false");
            break;
        case LUA_TSTRING: {
            const char *s = svalue(v);
            printf("\"%s\"", s);
            break;
        }
        default:
            printf("<%s>", ttypename(ttypenv(v)));
            break;
    }
}

/* Print source location info */
static void print_lineinfo(Proto *f, int pc, int indent) {
    if (f->lineinfo && pc < f->sizelineinfo) {
        print_indent(indent);
        printf("  ; line %d\n", f->lineinfo[pc]);
    }
}

/* Recursively print a Proto structure */
static void print_proto(Proto *f, int indent, int max_indent, const char *func_name) {
    if (indent >= max_indent && max_indent >= 0) {
        return;
    }

    print_indent(indent);
    printf("Proto {\n");

    /* Function metadata */
    print_indent(indent + 1);
    printf("source: ");
    if (f->source) {
        size_t len = f->source->tsv.len;
        const char *s = getstr(f->source);
        printf("\"%.*s\"\n", (int)len, s);
    } else {
        printf("<none>\n");
    }

    print_indent(indent + 1);
    printf("linedefined: %d, lastlinedefined: %d\n", f->linedefined, f->lastlinedefined);

    print_indent(indent + 1);
    printf("numparams: %d, is_vararg: %d, maxstacksize: %d\n",
           f->numparams, f->is_vararg, f->maxstacksize);

    /* Print constants */
    print_indent(indent + 1);
    printf("constants (%d):\n", f->sizek);
    for (int i = 0; i < f->sizek; i++) {
        print_indent(indent + 2);
        printf("[%d] ", i);
        print_tvalue(&f->k[i]);
        printf("\n");
    }

    /* Print upvalues */
    if (f->sizeupvalues > 0) {
        print_indent(indent + 1);
        printf("upvalues (%d):\n", f->sizeupvalues);
        for (int i = 0; i < f->sizeupvalues; i++) {
            print_indent(indent + 2);
            printf("[%d] ", i);
            if (f->upvalues && f->upvalues[i].name) {
                size_t len = f->upvalues[i].name->tsv.len;
                const char *name = getstr(f->upvalues[i].name);
                printf("name: \"%.*s\", instack: %d, idx: %d\n",
                       (int)len, name, f->upvalues[i].instack, f->upvalues[i].idx);
            } else {
                printf("<unnamed>\n");
            }
        }
    }

    /* Print local variables */
    if (f->sizelocvars > 0) {
        print_indent(indent + 1);
        printf("locals (%d):\n", f->sizelocvars);
        for (int i = 0; i < f->sizelocvars; i++) {
            print_indent(indent + 2);
            printf("[%d] ", i);
            if (f->locvars && f->locvars[i].varname) {
                size_t len = f->locvars[i].varname->tsv.len;
                const char *name = getstr(f->locvars[i].varname);
                printf("name: \"%.*s\", startpc: %d, endpc: %d\n",
                       (int)len, name, f->locvars[i].startpc, f->locvars[i].endpc);
            } else {
                printf("<unnamed>\n");
            }
        }
    }

    /* Print nested functions (always show, even in -s mode) */
    if (f->sizep > 0) {
        print_indent(indent + 1);
        printf("nested functions (%d):\n", f->sizep);
        for (int i = 0; i < f->sizep; i++) {
            if (f->p[i]) {
                print_indent(indent + 2);
                printf("[%d] ", i);
                if (f->p[i]->source) {
                    const char *s = getstr(f->p[i]->source);
                    printf("line %d ", f->p[i]->linedefined);
                    if (func_name == NULL || strstr(s, func_name)) {
                        print_proto(f->p[i], indent + 3, max_indent, func_name);
                    } else {
                        printf("<skipped>\n");
                    }
                } else {
                    printf("<no source>\n");
                }
            }
        }
    }

    /* Print bytecode if requested */
    if (f->sizecode > 0) {
        print_indent(indent + 1);
        printf("bytecode (%d instructions):\n", f->sizecode);
        for (int pc = 0; pc < f->sizecode; pc++) {
            Instruction i = f->code[pc];
            print_indent(indent + 2);
            printf("[%3d] ", pc + 1);

            const char *opname = luaP_opnames[GET_OPCODE(i)];
            printf("%-12s ", opname);

            int a = GETARG_A(i);
            int b = GETARG_B(i);
            int c = GETARG_C(i);
            int bx = GETARG_Bx(i);
            int sbx = GETARG_sBx(i);

            switch (getOpMode(GET_OPCODE(i))) {
                case iABC:
                    printf("A:%-3d B:%-3d C:%-3d", a, b, c);
                    break;
                case iABx:
                    printf("A:%-3d Bx:%-5d", a, bx);
                    break;
                case iAsBx:
                    printf("A:%-3d sBx:%-6d", a, sbx);
                    break;
                case iAx:
                    printf("A:%-3d Ax:%-9d", a, bx);
                    break;
            }

            printf("\n");
            print_lineinfo(f, pc, indent + 2);
        }
    }

    print_indent(indent);
    printf("}\n");
}

/* Print usage information */
static void print_usage(const char *prog_name) {
    printf("Usage: %s [options] <file>\n", prog_name);
    printf("\n");
    printf("Display the parse tree of Pico-8 Lua scripts.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -f <name>    Only show function(s) containing <name> in their source\n");
    printf("  -l <level>   Limit recursion depth (0 = no limit)\n");
    printf("  -p           Parse from Pico-8 cart file (.p8 or .p8.png)\n");
    printf("  -s           Show only source information (no bytecode)\n");
    printf("  -h           Show this help message\n");
    printf("\n");
    printf("If -p is not specified, the file is parsed as raw Lua code.\n");
}

/* Extract Lua code from a .p8 cart file */
static char* extract_lua_from_p8(const char *filename) {
    /* Parse the cart file to extract Lua */
    /* We need to provide the required buffers but can ignore the memory content */
    uint8_t memory[MEMORY_TOTAL];
    uint8_t file_buffer[RAW_DATA_LENGTH];
    uint8_t decompression_buffer[RAW_DATA_LENGTH];
    char lua_script[65536];

    int result = parse_cart_file(filename, memory, file_buffer,
                                  decompression_buffer, lua_script, NULL);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to parse cart file '%s'\n", filename);
        return NULL;
    }

    /* Allocate and copy the Lua script */
    char *lua_code = malloc(strlen(lua_script) + 1);
    if (!lua_code) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }
    strcpy(lua_code, lua_script);
    return lua_code;
}

/* Extract Lua code from a .p8.png cart file */
static char* extract_lua_from_p8png(const char *filename) {
    /* PNG parsing requires the same buffer structure */
    uint8_t memory[MEMORY_TOTAL];
    uint8_t file_buffer[RAW_DATA_LENGTH];
    uint8_t decompression_buffer[RAW_DATA_LENGTH];
    const char *lua_script = NULL;

    /* Get file size */
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Read file into buffer */
    size_t bytes_read = fread(file_buffer, 1, file_size, f);
    fclose(f);

    (void)bytes_read;  /* Suppress unused warning */

    int result = parse_png_ram(filename, file_buffer, file_size, memory,
                                decompression_buffer, &lua_script, NULL);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to parse PNG cart file '%s'\n", filename);
        return NULL;
    }

    /* Allocate and copy the Lua script */
    char *lua_code = malloc(strlen(lua_script) + 1);
    if (!lua_code) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }
    strcpy(lua_code, lua_script);
    return lua_code;
}

/* Parse Lua code and print the parse tree */
static int print_parse_tree(const char *lua_code,
                            int max_indent, const char *func_name, int show_bytecode) {
    /* Create a new Lua state just for parsing */
    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "Error: Failed to create Lua state\n");
        return 1;
    }

    /* Load the Lua code */
    int status = luaL_loadbuffer(L, lua_code, strlen(lua_code), "=stdin");
    if (status != 0) {
        fprintf(stderr, "Parse error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        lua_close(L);
        return 1;
    }

    /* Get the closure from the stack */
    if (!lua_isfunction(L, -1)) {
        fprintf(stderr, "Error: Loaded chunk is not a function\n");
        lua_close(L);
        return 1;
    }

    /* Access the Proto from the Lua closure */
    /* We need to use the getproto() macro which is defined in lfunc.h */
    /* The function is at the top of the stack */

    /* Use lua_pushvalue to keep the function on the stack */
    lua_pushvalue(L, -1);

    /* Get the Proto using getproto macro */
    /* getproto() takes a TValue*, which is what's on the stack */
    Proto *f = getproto(L->top - 1);

    if (!f) {
        fprintf(stderr, "Error: Could not get Proto from function\n");
        lua_pop(L, 1);
        lua_close(L);
        return 1;
    }

    /* Print the parse tree */
    if (show_bytecode) {
        print_proto(f, 0, max_indent, func_name);
    } else {
        /* Print without bytecode */
        print_indent(0);
        printf("Proto {\n");
        print_indent(1);
        printf("source: ");
        if (f->source) {
            size_t len = f->source->tsv.len;
            const char *s = getstr(f->source);
            printf("\"%.*s\"\n", (int)len, s);
        } else {
            printf("<none>\n");
        }
        print_indent(1);
        printf("linedefined: %d, lastlinedefined: %d\n", f->linedefined, f->lastlinedefined);
        print_indent(1);
        printf("numparams: %d, is_vararg: %d, maxstacksize: %d\n",
               f->numparams, f->is_vararg, f->maxstacksize);
        print_indent(1);
        printf("constants: %d\n", f->sizek);
        print_indent(1);
        printf("upvalues: %d\n", f->sizeupvalues);
        print_indent(1);
        printf("locals: %d\n", f->sizelocvars);
        print_indent(1);
        printf("nested functions: %d\n", f->sizep);
        print_indent(0);
        printf("}\n");
    }

    lua_pop(L, 1);
    lua_close(L);
    return 0;
}

int main(int argc, char *argv[]) {
    lua_State *L = NULL;
    const char *filename = NULL;
    const char *func_filter = NULL;
    int max_indent = -1;  /* No limit by default */
    int parse_p8 = 0;
    int show_bytecode = 1;
    int i;

    /* Initialize Lua (needed for string operations in getstr) */
    L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "Error: Failed to create Lua state\n");
        return 1;
    }
    /* Only load the base library which is always available */
    luaL_requiref(L, "_G", luaopen_base, 1);
    lua_pop(L, 1);

    /* Parse command line arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            lua_close(L);
            return 0;
        } else if (strcmp(argv[i], "-f") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -f requires a function name argument\n");
                lua_close(L);
                return 1;
            }
            func_filter = argv[++i];
        } else if (strcmp(argv[i], "-l") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -l requires a depth argument\n");
                lua_close(L);
                return 1;
            }
            max_indent = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-p") == 0) {
            parse_p8 = 1;
        } else if (strcmp(argv[i], "-s") == 0) {
            show_bytecode = 0;
        } else if (argv[i][0] != '-') {
            if (filename) {
                fprintf(stderr, "Error: Multiple files specified\n");
                lua_close(L);
                return 1;
            }
            filename = argv[i];
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            lua_close(L);
            return 1;
        }
    }

    if (!filename) {
        print_usage(argv[0]);
        lua_close(L);
        return 1;
    }

    /* Load the cart parser globals */
    m_file_buffer = malloc(RAW_DATA_LENGTH);
    m_lua_script = malloc(65536);

    /* Extract Lua code from file */
    char *lua_code = NULL;
    if (parse_p8) {
        /* Check if it's a PNG file */
        FILE *f = fopen(filename, "rb");
        if (f) {
            uint8_t header[8];
            size_t read = fread(header, 1, 8, f);
            fclose(f);

            if (read == 8 && header[0] == 0x89 &&
                header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47) {
                lua_code = extract_lua_from_p8png(filename);
            } else {
                lua_code = extract_lua_from_p8(filename);
            }
        } else {
            fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
            lua_close(L);
            return 1;
        }
    } else {
        /* Read raw Lua file */
        FILE *f = fopen(filename, "rb");
        if (!f) {
            fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
            lua_close(L);
            return 1;
        }

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        lua_code = malloc(size + 1);
        if (!lua_code) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            fclose(f);
            lua_close(L);
            return 1;
        }

        size_t bytes_read = fread(lua_code, 1, size, f);
        lua_code[bytes_read] = '\0';
        fclose(f);
    }

    if (!lua_code) {
        lua_close(L);
        return 1;
    }

    /* Print the parse tree */
    int result = print_parse_tree(lua_code, max_indent, func_filter, show_bytecode);

    /* Cleanup */
    free(lua_code);
    free(m_file_buffer);
    free(m_lua_script);
    lua_close(L);

    return result;
}
