/*
 * fv_hints.h — fix hints and explanations (static + context-aware)
 *
 * Maps fv_error_code → (fix_hint, explain) strings.
 * fv_get_hint()      — static lookup (no context)
 * fv_generate_hint() — context-aware: uses ctx->hint_keyword, hint_colnum,
 *                       curhdu, curtype to produce rich, educational text.
 */
#ifndef FV_HINTS_H
#define FV_HINTS_H

#include <string.h>
#include "fitsverify.h"

typedef struct {
    const char *fix_hint;
    const char *explain;
} fv_hint;

/* Forward declare */
struct fv_context;

/*
 * Look up the hint entry for a given error code (static strings only).
 * Returns a pointer to a static entry, or NULL if no hint exists.
 */
const fv_hint *fv_get_hint(fv_error_code code);

/*
 * Generate a context-aware hint using fields on ctx (hint_keyword,
 * hint_colnum, curhdu, curtype).  When context is available, writes
 * rich text into ctx->hint_fix_buf / ctx->hint_explain_buf and returns
 * a pointer to a thread-local fv_hint whose pointers reference those
 * buffers.  When no context is set, falls back to fv_get_hint().
 *
 * The returned pointer is valid until the next call to fv_generate_hint()
 * on the same ctx.
 */
const fv_hint *fv_generate_hint(struct fv_context *ctx, fv_error_code code);

/* ---- Context-setting macros for call sites ---- */

#define FV_HINT_SET_KEYWORD(ctx, name) do { \
    strncpy((ctx)->hint_keyword, (name), sizeof((ctx)->hint_keyword) - 1); \
    (ctx)->hint_keyword[sizeof((ctx)->hint_keyword) - 1] = '\0'; \
} while(0)

#define FV_HINT_SET_COLNUM(ctx, col) \
    (ctx)->hint_colnum = (col)

/* Call-site hints: write specific text BEFORE wrterr/wrtwrn.
   fv_generate_hint() will keep it instead of overwriting.
   hint_callsite is a bitmask: bit 0 = fix, bit 1 = explain. */
#define FV_HINT_SET_FIX(ctx, ...) do { \
    snprintf((ctx)->hint_fix_buf, sizeof((ctx)->hint_fix_buf), __VA_ARGS__); \
    (ctx)->hint_callsite |= 1; \
} while(0)

#define FV_HINT_SET_EXPLAIN(ctx, ...) do { \
    snprintf((ctx)->hint_explain_buf, sizeof((ctx)->hint_explain_buf), __VA_ARGS__); \
    (ctx)->hint_callsite |= 2; \
} while(0)

#define FV_HINT_CLEAR(ctx) do { \
    (ctx)->hint_keyword[0] = '\0'; \
    (ctx)->hint_colnum = 0; \
    (ctx)->hint_callsite = 0; \
} while(0)

#endif /* FV_HINTS_H */
