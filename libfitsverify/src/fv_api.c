/*
 * fv_api.c — public API implementation for libfitsverify
 */
#include <stdlib.h>
#include <string.h>
#include "fitsverify.h"
#include "fv_internal.h"
#include "fv_context.h"

#define LIBFITSVERIFY_VERSION "1.0.0"

/* ---- lifecycle --------------------------------------------------------- */

fv_context *fv_context_new(void)
{
    fv_context *ctx = (fv_context *)calloc(1, sizeof(fv_context));
    if (!ctx) return NULL;

    /* defaults matching original fitsverify behaviour */
    ctx->prhead       = 0;
    ctx->prstat       = 1;
    ctx->testdata     = 1;
    ctx->testcsum     = 1;
    ctx->testfill     = 1;
    ctx->heasarc_conv = 1;
    ctx->testhierarch = 0;
    ctx->err_report   = 0;
    ctx->fix_hints    = 0;
    ctx->explain      = 0;
    ctx->totalhdu     = 0;

    ctx->totalerr     = 0;
    ctx->totalwrn     = 0;
    ctx->nerrs        = 0;
    ctx->nwrns        = 0;

    ctx->hduname      = NULL;
    ctx->file_total_err = 0;
    ctx->file_total_warn = 0;

    ctx->cards        = NULL;
    ctx->ncards       = 0;
    ctx->tmpkwds      = NULL;
    ctx->ttype        = NULL;
    ctx->tform        = NULL;
    ctx->tunit        = NULL;
    ctx->ptemp        = ctx->head_temp;
    ctx->snull[0]     = '\0';
    ctx->curhdu       = 0;
    ctx->curtype      = 0;

    ctx->hint_keyword[0] = '\0';
    ctx->hint_colnum     = 0;
    ctx->hint_fix_buf[0] = '\0';
    ctx->hint_explain_buf[0] = '\0';

    ctx->save_nprompt = 0;
    ctx->cont_fmt[0]  = '\0';
    ctx->hdutitle[0]  = '\0';
    ctx->oldhdu       = 0;

    ctx->maxerrors_reached = 0;

    ctx->output_fn    = NULL;
    ctx->output_udata = NULL;

    return ctx;
}

void fv_context_free(fv_context *ctx)
{
    if (!ctx) return;

    /* Clean up any per-file state that may remain after an abort */
    if (ctx->hduname) {
        int i;
        for (i = 0; i < ctx->totalhdu; i++)
            free(ctx->hduname[i]);
        free(ctx->hduname);
    }
    if (ctx->cards) {
        int i;
        for (i = 0; i < ctx->ncards; i++)
            free(ctx->cards[i]);
        free(ctx->cards);
    }
    free(ctx->tmpkwds);   /* elements not owned */
    free(ctx->ttype);     /* elements not owned */
    free(ctx->tform);     /* elements not owned */
    free(ctx->tunit);     /* elements not owned */

    free(ctx);
}

/* ---- configuration ----------------------------------------------------- */

int fv_set_option(fv_context *ctx, fv_option opt, int value)
{
    if (!ctx) return -1;
    switch (opt) {
        case FV_OPT_PRHEAD:       ctx->prhead       = value; break;
        case FV_OPT_PRSTAT:       ctx->prstat       = value; break;
        case FV_OPT_TESTDATA:     ctx->testdata     = value; break;
        case FV_OPT_TESTCSUM:     ctx->testcsum     = value; break;
        case FV_OPT_TESTFILL:     ctx->testfill     = value; break;
        case FV_OPT_HEASARC_CONV: ctx->heasarc_conv = value; break;
        case FV_OPT_TESTHIERARCH: ctx->testhierarch = value; break;
        case FV_OPT_ERR_REPORT:   ctx->err_report   = value; break;
        case FV_OPT_FIX_HINTS:    ctx->fix_hints    = value; break;
        case FV_OPT_EXPLAIN:       ctx->explain      = value; break;
        default: return -1;
    }
    return 0;
}

int fv_get_option(fv_context *ctx, fv_option opt)
{
    if (!ctx) return -1;
    switch (opt) {
        case FV_OPT_PRHEAD:       return ctx->prhead;
        case FV_OPT_PRSTAT:       return ctx->prstat;
        case FV_OPT_TESTDATA:     return ctx->testdata;
        case FV_OPT_TESTCSUM:     return ctx->testcsum;
        case FV_OPT_TESTFILL:     return ctx->testfill;
        case FV_OPT_HEASARC_CONV: return ctx->heasarc_conv;
        case FV_OPT_TESTHIERARCH: return ctx->testhierarch;
        case FV_OPT_ERR_REPORT:   return ctx->err_report;
        case FV_OPT_FIX_HINTS:    return ctx->fix_hints;
        case FV_OPT_EXPLAIN:       return ctx->explain;
        default: return -1;
    }
}

/* ---- output callback --------------------------------------------------- */

void fv_set_output(fv_context *ctx, fv_output_fn fn, void *userdata)
{
    if (!ctx) return;
    ctx->output_fn    = fn;
    ctx->output_udata = userdata;
}

/* ---- verification ------------------------------------------------------ */

int fv_verify_file(fv_context *ctx, const char *infile,
                   FILE *out, fv_result *result)
{
    char buf[FLEN_FILENAME];
    int vfstatus;

    if (!ctx || !infile) return -1;

    /* reset per-file state */
    ctx->file_total_err    = 0;
    ctx->file_total_warn   = 0;
    ctx->oldhdu            = 0;
    ctx->maxerrors_reached = 0;

    /* make a mutable copy of the filename (verify_fits trims whitespace) */
    strncpy(buf, infile, FLEN_FILENAME - 1);
    buf[FLEN_FILENAME - 1] = '\0';

    vfstatus = verify_fits(ctx, buf, out);

    if (result) {
        if (vfstatus) {
            result->num_errors   = 1;
            result->num_warnings = 0;
            result->aborted      = 1;
        } else {
            result->num_errors   = get_total_err(ctx);
            result->num_warnings = get_total_warn(ctx);
            result->aborted      = ctx->maxerrors_reached;
        }
        result->num_hdus = ctx->totalhdu;
    }

    return vfstatus;
}

/* ---- in-memory verification -------------------------------------------- */

int fv_verify_memory(fv_context *ctx, const void *buffer, size_t size,
                     const char *label, FILE *out, fv_result *result)
{
    fitsfile *infits = NULL;
    int status = 0;
    int vfstatus;
    void *membuf;
    size_t memsize;
    const char *display_label;

    if (!ctx || !buffer || size == 0) return -1;

    display_label = label ? label : "<memory>";

    /* reset per-file state */
    ctx->file_total_err    = 0;
    ctx->file_total_warn   = 0;
    ctx->oldhdu            = 0;
    ctx->totalhdu          = 0;
    ctx->maxerrors_reached = 0;

    /* Print the File: header to match verify_fits() behavior */
    wrtout(ctx, out, " ");
    snprintf(ctx->comm, sizeof(ctx->comm), "File: %s", display_label);
    wrtout(ctx, out, ctx->comm);

    /*
     * fits_open_memfile takes void** for the buffer and size_t* for size.
     * In READONLY mode CFITSIO will not modify the buffer, but the API
     * signature requires non-const pointers. Cast accordingly.
     */
    membuf  = (void *)buffer;
    memsize = size;

    if (fits_open_memfile(&infits, display_label, READONLY,
                          &membuf, &memsize, 0, NULL, &status)) {
        wrtserr(ctx, out, "", &status, 2, FV_ERR_CFITSIO_STACK);
        leave_early(ctx, out);
        if (result) {
            result->num_errors   = 1;
            result->num_warnings = 0;
            result->num_hdus     = 0;
            result->aborted      = 1;
        }
        return 1;
    }

    vfstatus = verify_fits_fptr(ctx, infits, out);

    if (result) {
        if (vfstatus) {
            result->num_errors   = 1;
            result->num_warnings = 0;
            result->aborted      = 1;
        } else {
            result->num_errors   = get_total_err(ctx);
            result->num_warnings = get_total_warn(ctx);
            result->aborted      = ctx->maxerrors_reached;
        }
        result->num_hdus = ctx->totalhdu;
    }

    return vfstatus;
}

/* ---- accumulated totals ------------------------------------------------ */

void fv_get_totals(const fv_context *ctx,
                   long *total_errors, long *total_warnings)
{
    if (!ctx) return;
    if (total_errors)   *total_errors   = ctx->totalerr;
    if (total_warnings) *total_warnings = ctx->totalwrn;
}

/* ---- version ----------------------------------------------------------- */

const char *fv_version(void)
{
    return LIBFITSVERIFY_VERSION;
}
