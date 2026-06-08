/**
 * p8_editor_undo.h — simple per-editor undo/redo using binary memory snapshots.
 *
 * Each visual editor creates one context for the memory region it modifies.
 * Before any write, call p8_editor_undo_push(); CTRL-Z/Y call do_undo/do_redo.
 */

#ifndef P8_EDITOR_UNDO_H
#define P8_EDITOR_UNDO_H

#include <stdbool.h>
#include <stdint.h>

typedef struct p8_editor_undo_ctx p8_editor_undo_ctx_t;

/* Allocate a context for a memory region [mem_addr, mem_addr+mem_size). */
p8_editor_undo_ctx_t *p8_editor_undo_create(int mem_addr, int mem_size);

/* Destroy a context and free its resources. */
void p8_editor_undo_destroy(p8_editor_undo_ctx_t *ctx);

/* Push a snapshot of the tracked region onto the undo stack and clear redo. */
void p8_editor_undo_push(p8_editor_undo_ctx_t *ctx);

/* Undo: restore previous snapshot; return true if anything was undone. */
bool p8_editor_undo_do_undo(p8_editor_undo_ctx_t *ctx);

/* Redo: re-apply an undone snapshot; return true if anything was redone. */
bool p8_editor_undo_do_redo(p8_editor_undo_ctx_t *ctx);

/* Discard all undo/redo history (e.g. on cart reload). */
void p8_editor_undo_clear(p8_editor_undo_ctx_t *ctx);

#endif /* P8_EDITOR_UNDO_H */
