# In-Memory HDU Multiprocessing Proposal

This proposal outlines how to verify multiple HDUs in the same FITS file in parallel when the input is an in-memory buffer and multiple `fitsfile*` handles are effectively free.

## Summary

Parallelizing within a single FITS file is safe and feasible if:
- Each worker thread uses its own `fitsfile*` handle created by `fits_open_memfile()`.
- All per-HDU mutable state is isolated into a new `fv_hdu_context` struct.
- `fv_context` remains shared, read-only during verification (options, totals, output callback).
- File-level checks and aggregation run serially on the main thread.

## Proposed Data Structures

```c
/* libfitsverify/src/fv_hdu_context.h */
typedef struct {
    /* per-HDU counters */
    int nerrs;
    int nwrns;

    /* per-HDU scratch buffers */
    char errmes[256];
    char comm[FLEN_FILENAME + 6];
    char misc_temp[512];

    /* header parsing state (former fvrf_head.c statics) */
    char  **cards;
    int   ncards;
    char  **tmpkwds;
    char  **ttype;
    char  **tform;
    char  **tunit;
    char  head_temp[80];
    char  *ptemp;
    char  snull[1];
    int   curhdu;
    int   curtype;

    /* print_fmt state */
    char  cont_fmt[80];
    int   save_nprompt;

    /* print_title state */
    char  hdutitle[64];
    int   oldhdu;

    /* abort/longjmp (HDU-local) */
    jmp_buf abort_jmp;
    int     abort_set;
} fv_hdu_context;

/* libfitsverify/src/fv_context.h */
struct fv_context {
    /* options, totals, output callback, HDU name tracking, etc. */
    int  prhead, prstat, testdata, testcsum, testfill, heasarc_conv, testhierarch, err_report;
    int  totalhdu;
    long totalerr, totalwrn;

    HduName **hduname;
    int  file_total_err;
    int  file_total_warn;

    fv_output_fn output_fn;
    void *output_udata;
};
```

## Proposed Parallel Flow (In-Memory)

1. Open `N` independent `fitsfile*` handles using `fits_open_memfile()` against the same buffer.
2. Create `N` `fv_hdu_context` instances (one per worker).
3. Each worker thread:
   - `fits_movabs_hdu()` to its assigned HDU
   - `init_hdu()`, `test_hdu()`, `test_data()`, `close_hdu()` using its local `fv_hdu_context`
   - Emits messages into a thread-local vector
4. Main thread merges results by HDU order.
5. File-level checks run serially: `init_report`, duplicate EXTNAME/EXTVER checks, `test_end`, `close_report`.
6. Totals are accumulated in the main thread and written to `fv_context`.

## Exact Function List to Change

The following functions need signature changes to accept both `fv_context *ctx` and `fv_hdu_context *hctx`, or to move HDU-local data fully into `fv_hdu_context`:

### Output and formatting
- `wrtout`
- `wrtwrn`
- `wrterr`
- `wrtferr`
- `wrtserr`
- `wrtsep`
- `print_fmt`
- `print_title`
- `print_header`
- `print_summary`
- `close_err`

### HDU lifecycle
- `init_hdu`
- `test_hdu`
- `test_data`
- `close_hdu`

### Header parsing and helpers (as needed when touching state)
- Any function using `ctx->cards`, `ctx->tmpkwds`, `ctx->ttype`, `ctx->tform`, `ctx->tunit`, `ctx->curhdu`, `ctx->curtype`

### File-level entry points
- `verify_fits_fptr` should become a dispatcher that uses `fv_hdu_context` per HDU (serial path).
- Add a new internal entry point for the parallel in-memory path (e.g., `verify_fits_mem_parallel`).

## Incremental Refactor Order (to avoid breaking everything)

1. **Introduce `fv_hdu_context` and allocation helpers.**
   - Add `fv_hdu_context_new()` / `fv_hdu_context_free()` (internal or public).
   - Initialize all fields to match current `fv_context` defaults.

2. **Split output helpers to accept both contexts.**
   - Update `wrtout`, `wrterr`, `wrtwrn`, `wrtferr`, `wrtserr`, `print_fmt`, `wrtsep` to use `hctx` for per-HDU state.
   - Keep their call sites unchanged by temporarily passing the existing `fv_context*` as both `ctx` and `hctx` (use a typedef or a shim), to keep changes mechanical and safe.

3. **Move HDU-local fields out of `fv_context`.**
   - Remove per-HDU fields from `fv_context` and replace with `fv_hdu_context` usage.
   - Update `init_hdu`, `test_hdu`, `test_data`, `close_hdu` to use `hctx`.

4. **Adjust `verify_fits_fptr` (serial path).**
   - Create one `fv_hdu_context` per HDU iteration (or reuse a single one reset per HDU) and pass it through.
   - Ensure `abort_set` and `setjmp` are on the `hctx`.

5. **Implement in-memory parallel verification.**
   - Add `verify_fits_mem_parallel()` that uses a work queue of HDU indices.
   - Each worker owns its `fitsfile*` and `fv_hdu_context`.
   - Merge message streams by HDU order.

6. **Wire a public option or API for parallelism.**
   - Optional: `fv_set_option(ctx, FV_OPT_PARALLEL_HDU, N)` for in-memory only.
   - Fallback to serial if N <= 1 or if input is file-backed.

## Testing Recommendations

1. **Regression parity (serial path):**
   - Ensure the serial path produces byte-identical output to current behavior.
   - Run existing regression tests (`test_regression.sh`) unchanged.

2. **Parallel vs serial equivalence:**
   - Add tests that run the same in-memory FITS file with `N=1` and `N>1`.
   - Compare totals, HDU counts, and message ordering (or normalized ordering if stable ordering is not guaranteed).

3. **Thread safety smoke test:**
   - Verify parallel HDU execution across multiple threads with different FITS buffers simultaneously.

4. **MAXERRORS abort behavior:**
   - Ensure `longjmp` aborts only the HDU worker and the overall file handling returns cleanly.

5. **Resource cleanup:**
   - Confirm all `fitsfile*` handles are closed and all `fv_hdu_context` allocations are freed on success and failure paths.
