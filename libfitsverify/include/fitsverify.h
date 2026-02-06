/*
 * libfitsverify — public API
 *
 * Embeddable, reentrant FITS standards-compliance validator.
 * All state is held in an opaque fv_context; no globals.
 */
#ifndef LIBFITSVERIFY_H
#define LIBFITSVERIFY_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- opaque context ---------------------------------------------------- */
typedef struct fv_context fv_context;

/* ---- options ----------------------------------------------------------- */
typedef enum {
    FV_OPT_PRHEAD       = 0,   /* print header keyword listing (int 0/1) */
    FV_OPT_PRSTAT       = 1,   /* print HDU summary (int 0/1)            */
    FV_OPT_TESTDATA     = 2,   /* test data values  (int 0/1)            */
    FV_OPT_TESTCSUM     = 3,   /* test checksum     (int 0/1)            */
    FV_OPT_TESTFILL     = 4,   /* test fill areas   (int 0/1)            */
    FV_OPT_HEASARC_CONV = 5,   /* check HEASARC conventions (int 0/1)    */
    FV_OPT_TESTHIERARCH = 6,   /* test ESO HIERARCH keywords (int 0/1)   */
    FV_OPT_ERR_REPORT   = 7    /* 0=all, 1=errors only, 2=severe only    */
} fv_option;

/* ---- per-file result --------------------------------------------------- */
typedef struct {
    int  num_errors;      /* errors found in this file   */
    int  num_warnings;    /* warnings found in this file */
    int  num_hdus;        /* HDUs processed              */
    int  aborted;         /* 1 if verification was aborted (e.g. >MAXERRORS) */
} fv_result;

/* ---- lifecycle --------------------------------------------------------- */
fv_context *fv_context_new(void);
void        fv_context_free(fv_context *ctx);

/* ---- configuration ----------------------------------------------------- */
int  fv_set_option(fv_context *ctx, fv_option opt, int value);
int  fv_get_option(fv_context *ctx, fv_option opt);

/* ---- verification ------------------------------------------------------ */
/*
 * Verify a single FITS file.
 *
 *   ctx    – context (must not be NULL)
 *   infile – path to the FITS file (may contain CFITSIO extended syntax)
 *   out    – FILE* for the report; may be NULL (quiet mode)
 *   result – if non-NULL, filled with per-file stats
 *
 * Returns 0 on success, non-zero on fatal/I-O error.
 * Errors/warnings accumulate in ctx across calls.
 */
int fv_verify_file(fv_context *ctx, const char *infile,
                   FILE *out, fv_result *result);

/* ---- accumulated totals ------------------------------------------------ */
void fv_get_totals(const fv_context *ctx,
                   long *total_errors, long *total_warnings);

/* ---- version ----------------------------------------------------------- */
const char *fv_version(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBFITSVERIFY_H */
