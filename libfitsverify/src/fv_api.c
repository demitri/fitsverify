/*
 * fv_api.c — public API implementation for libfitsverify
 */
#include <stdlib.h>
#include <string.h>
#include "fv_internal.h"
#include "fv_context.h"
#include "fitsverify.h"

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
    ctx->totalhdu     = 0;

    ctx->totalerr     = 0;
    ctx->totalwrn     = 0;
    ctx->nerrs        = 0;
    ctx->nwrns        = 0;

    ctx->hduname      = NULL;
    ctx->file_total_err = 1;   /* matches original init */
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

    ctx->save_nprompt = 0;
    ctx->cont_fmt[0]  = '\0';
    ctx->hdutitle[0]  = '\0';
    ctx->oldhdu       = 0;

    ctx->abort_set    = 0;

    return ctx;
}

void fv_context_free(fv_context *ctx)
{
    if (!ctx) return;
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
        default: return -1;
    }
}

/* ---- verification ------------------------------------------------------ */

int fv_verify_file(fv_context *ctx, const char *infile,
                   FILE *out, fv_result *result)
{
    char buf[FLEN_FILENAME];
    int vfstatus;

    if (!ctx || !infile) return -1;

    /* reset per-file state */
    ctx->file_total_err  = 1;
    ctx->file_total_warn = 0;
    ctx->oldhdu          = 0;

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
            result->aborted      = 0;
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
