/*
 * fv_context.h — full fv_context struct definition (internal only)
 *
 * This header is included only by the library's .c files, never by
 * public consumers (they see the opaque typedef in fitsverify.h).
 */
#ifndef FV_CONTEXT_H
#define FV_CONTEXT_H

#include "fitsio.h"
#include "fv_internal.h"

struct fv_context {

    /* ---- configuration (former globals from ftverify.c) -------------- */
    int  prhead;           /* print header keyword listing                */
    int  prstat;           /* print HDU summary                           */
    int  testdata;         /* test data values                            */
    int  testcsum;         /* test checksum                               */
    int  testfill;         /* test fill areas                             */
    int  heasarc_conv;     /* check HEASARC conventions                   */
    int  testhierarch;     /* test ESO HIERARCH keywords                  */
    int  err_report;       /* 0 = all, 1 = errors only, 2 = severe only  */
    int  fix_hints;        /* attach fix hints to callback messages       */
    int  explain;          /* attach explanations to callback messages    */
    int  totalhdu;         /* total number of HDUs in current file        */

    /* ---- session accumulators (former globals from ftverify.c) ------- */
    long totalerr;
    long totalwrn;

    /* ---- per-HDU counters (former statics in fvrf_misc.c) ----------- */
    int  nerrs;
    int  nwrns;

    /* ---- scratch buffers -------------------------------------------- */
    char errmes[256];                  /* was static in fverify.h        */
    char comm[FLEN_FILENAME + 6];     /* was static in fverify.h        */
    char misc_temp[512];               /* was static temp[512] in fvrf_misc.c */

    /* ---- HDU name tracking (former statics in fvrf_file.c) ---------- */
    HduName **hduname;
    int  file_total_err;   /* total_err in fvrf_file.c */
    int  file_total_warn;  /* total_warn in fvrf_file.c */

    /* ---- header parsing state (former statics in fvrf_head.c) ------- */
    char  **cards;         /* array of keyword cards                     */
    int   ncards;          /* total keywords                             */
    char  **tmpkwds;       /* sorted keyword name array                  */
    char  **ttype;
    char  **tform;
    char  **tunit;
    char  head_temp[80];   /* was static temp[80] in fvrf_head.c        */
    char  *ptemp;          /* always points to head_temp                 */
    char  snull[1];        /* static "" string                           */
    int   curhdu;          /* current HDU index                          */
    int   curtype;         /* current HDU type                           */

    /* ---- hint context for context-aware hints ------------------------ */
    char  hint_keyword[FLEN_KEYWORD]; /* keyword name for hints          */
    int   hint_colnum;                /* column number (1-based); 0=none */
    int   hint_callsite;              /* 1 = call site wrote hint_fix_buf */
    char  hint_fix_buf[1024];         /* buffer for generated fix_hint   */
    char  hint_explain_buf[1024];     /* buffer for generated explain    */

    /* ---- print_fmt state (former statics in print_fmt) -------------- */
    char  cont_fmt[80];
    int   save_nprompt;

    /* ---- print_title state (former statics in print_title) ---------- */
    char  hdutitle[64];
    int   oldhdu;

    /* ---- abort state ------------------------------------------------ */
    int     maxerrors_reached;  /* set when nerrs > MAXERRORS           */

    /* ---- output callback (NULL = use FILE* streams) ----------------- */
    fv_output_fn output_fn;
    void        *output_udata;
};

#endif /* FV_CONTEXT_H */
