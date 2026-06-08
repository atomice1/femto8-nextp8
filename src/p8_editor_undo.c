/**
 * p8_editor_undo.c — implementation of the visual editor undo/redo stack.
 *
 * Stores up to P8_UNDO_LIMIT full binary snapshots of the tracked memory
 * region.  On each call to push(), the current region is copied to the undo
 * stack and the redo stack is cleared.  do_undo()/do_redo() swap between the
 * live memory and the saved snapshots.
 */

#include <stdlib.h>
#include <string.h>

#include "p8_editor_undo.h"
#include "p8_emu.h"   /* m_cart_memory */

#define P8_UNDO_LIMIT 8

struct p8_editor_undo_ctx {
    int       addr;
    int       size;
    uint8_t  *undo_stack[P8_UNDO_LIMIT];
    int       undo_depth;
    uint8_t  *redo_stack[P8_UNDO_LIMIT];
    int       redo_depth;
};

/* ── helpers ──────────────────────────────────────────────────────────────── */

static void free_stack(uint8_t **stack, int *depth)
{
    for (int i = 0; i < *depth; i++) { free(stack[i]); stack[i] = NULL; }
    *depth = 0;
}

static void push_to(uint8_t **stack, int *depth,
                    const uint8_t *mem, int size)
{
    if (*depth == P8_UNDO_LIMIT) {
        free(stack[0]);
        memmove(stack, stack + 1, (P8_UNDO_LIMIT - 1) * sizeof(uint8_t *));
        (*depth)--;
    }
    uint8_t *snap = (uint8_t *)malloc((size_t)size);
    if (!snap) return;
    memcpy(snap, mem, (size_t)size);
    stack[(*depth)++] = snap;
}

/* ── public API ───────────────────────────────────────────────────────────── */

p8_editor_undo_ctx_t *p8_editor_undo_create(int mem_addr, int mem_size)
{
    p8_editor_undo_ctx_t *ctx =
        (p8_editor_undo_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->addr = mem_addr;
    ctx->size = mem_size;
    return ctx;
}

void p8_editor_undo_destroy(p8_editor_undo_ctx_t *ctx)
{
    if (!ctx) return;
    free_stack(ctx->undo_stack, &ctx->undo_depth);
    free_stack(ctx->redo_stack, &ctx->redo_depth);
    free(ctx);
}

void p8_editor_undo_push(p8_editor_undo_ctx_t *ctx)
{
    if (!ctx) return;
    push_to(ctx->undo_stack, &ctx->undo_depth,
            m_cart_memory + ctx->addr, ctx->size);
    free_stack(ctx->redo_stack, &ctx->redo_depth);
}

bool p8_editor_undo_do_undo(p8_editor_undo_ctx_t *ctx)
{
    if (!ctx || ctx->undo_depth == 0) return false;
    /* Save current state to redo stack */
    push_to(ctx->redo_stack, &ctx->redo_depth,
            m_cart_memory + ctx->addr, ctx->size);
    /* Restore previous snapshot */
    uint8_t *snap = ctx->undo_stack[--ctx->undo_depth];
    ctx->undo_stack[ctx->undo_depth] = NULL;
    memcpy(m_cart_memory + ctx->addr, snap, (size_t)ctx->size);
    free(snap);
    return true;
}

bool p8_editor_undo_do_redo(p8_editor_undo_ctx_t *ctx)
{
    if (!ctx || ctx->redo_depth == 0) return false;
    /* Save current state to undo stack */
    push_to(ctx->undo_stack, &ctx->undo_depth,
            m_cart_memory + ctx->addr, ctx->size);
    /* Restore redo snapshot */
    uint8_t *snap = ctx->redo_stack[--ctx->redo_depth];
    ctx->redo_stack[ctx->redo_depth] = NULL;
    memcpy(m_cart_memory + ctx->addr, snap, (size_t)ctx->size);
    free(snap);
    return true;
}

void p8_editor_undo_clear(p8_editor_undo_ctx_t *ctx)
{
    if (!ctx) return;
    free_stack(ctx->undo_stack, &ctx->undo_depth);
    free_stack(ctx->redo_stack, &ctx->redo_depth);
}
