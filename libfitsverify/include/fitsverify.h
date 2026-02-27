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

/* ---- structured error codes -------------------------------------------- */
/*
 * Every diagnostic is assigned a unique code.  Codes are grouped by area
 * with gaps left for future additions.  Use FV_OK (0) when no specific
 * code applies.  The severity (error vs warning) is carried separately
 * in fv_msg_severity; codes in the FV_WARN_* range are conventionally
 * warnings, but severity is authoritative.
 */
typedef enum {
    FV_OK = 0,                          /* no error / default */

    /* ---- File / HDU structure (100–149) ---- */
    FV_ERR_EXTRA_HDUS          = 100,   /* extraneous HDU(s) beyond last */
    FV_ERR_EXTRA_BYTES         = 101,   /* extra bytes after last HDU */
    FV_ERR_BAD_HDU             = 102,   /* bad HDU structure */
    FV_ERR_READ_FAIL           = 103,   /* error reading file data */

    /* ---- Mandatory keyword (150–199) ---- */
    FV_ERR_MISSING_KEYWORD     = 150,   /* mandatory keyword not present */
    FV_ERR_KEYWORD_ORDER       = 151,   /* mandatory keyword out of order */
    FV_ERR_KEYWORD_DUPLICATE   = 152,   /* mandatory keyword duplicated */
    FV_ERR_KEYWORD_VALUE       = 153,   /* mandatory keyword has wrong value */
    FV_ERR_KEYWORD_TYPE        = 154,   /* mandatory keyword has wrong datatype */
    FV_ERR_MISSING_END         = 155,   /* END keyword not present */
    FV_ERR_END_NOT_BLANK       = 156,   /* END not filled with blanks */
    FV_ERR_NOT_FIXED_FORMAT    = 157,   /* mandatory keyword not in fixed format */

    /* ---- Keyword format / value (200–249) ---- */
    FV_ERR_NONASCII_HEADER     = 200,   /* header contains non-ASCII char */
    FV_ERR_ILLEGAL_NAME_CHAR   = 201,   /* keyword name contains illegal char */
    FV_ERR_NAME_NOT_JUSTIFIED  = 202,   /* keyword name not left justified */
    FV_ERR_BAD_VALUE_FORMAT    = 203,   /* keyword value field has illegal format */
    FV_ERR_NO_VALUE_SEPARATOR  = 204,   /* value/comment not separated by / */
    FV_ERR_BAD_STRING          = 205,   /* string value contains non-text chars */
    FV_ERR_MISSING_QUOTE       = 206,   /* closing quote missing */
    FV_ERR_BAD_LOGICAL         = 207,   /* bad logical value */
    FV_ERR_BAD_NUMBER          = 208,   /* bad numerical value */
    FV_ERR_LOWERCASE_EXPONENT  = 209,   /* lowercase exponent (d/e) in number */
    FV_ERR_COMPLEX_FORMAT      = 210,   /* complex value format error */
    FV_ERR_BAD_COMMENT         = 211,   /* comment contains non-text chars */
    FV_ERR_UNKNOWN_TYPE        = 212,   /* unknown keyword value type */
    FV_ERR_WRONG_TYPE          = 213,   /* wrong type (expected str/int/etc.) */
    FV_ERR_NULL_VALUE          = 214,   /* keyword has null value */
    FV_ERR_CARD_TOO_LONG       = 215,   /* card > 80 characters */
    FV_ERR_NONTEXT_CHARS       = 216,   /* string contains non-text characters */
    FV_ERR_LEADING_SPACE       = 217,   /* value contains leading space(s) */
    FV_ERR_RESERVED_VALUE      = 218,   /* reserved keyword has wrong value */

    /* ---- HDU-type keyword placement (250–299) ---- */
    FV_ERR_XTENSION_IN_PRIMARY = 250,   /* XTENSION keyword in primary array */
    FV_ERR_IMAGE_KEY_IN_TABLE  = 251,   /* BSCALE/BZERO/etc. in a table */
    FV_ERR_TABLE_KEY_IN_IMAGE  = 252,   /* column keyword in an image */
    FV_ERR_PRIMARY_KEY_IN_EXT  = 253,   /* SIMPLE/EXTEND/BLOCKED in extension */
    FV_ERR_TABLE_WCS_IN_IMAGE  = 254,   /* table WCS keywords in image */
    FV_ERR_KEYWORD_NOT_ALLOWED = 255,   /* keyword not allowed in this HDU */

    /* ---- Table structure (300–349) ---- */
    FV_ERR_BAD_TFIELDS         = 300,   /* bad TFIELDS value */
    FV_ERR_NAXIS1_MISMATCH     = 301,   /* column widths inconsistent with NAXIS1 */
    FV_ERR_BAD_TFORM           = 302,   /* bad TFORMn value */
    FV_ERR_BAD_TDISP           = 303,   /* TDISPn inconsistent with datatype */
    FV_ERR_INDEX_EXCEEDS_TFIELDS = 304, /* column keyword index > TFIELDS */
    FV_ERR_TSCAL_WRONG_TYPE    = 305,   /* TSCALn/TZEROn on wrong column type */
    FV_ERR_TNULL_WRONG_TYPE    = 306,   /* TNULLn on floating-point column */
    FV_ERR_BLANK_WRONG_TYPE    = 307,   /* BLANK with floating-point image */
    FV_ERR_THEAP_NO_PCOUNT     = 308,   /* THEAP with PCOUNT = 0 */
    FV_ERR_TDIM_IN_ASCII       = 309,   /* TDIMn in ASCII table */
    FV_ERR_TBCOL_IN_BINARY     = 310,   /* TBCOLn in binary table */
    FV_ERR_VAR_FORMAT          = 311,   /* variable-length format error */
    FV_ERR_TBCOL_MISMATCH      = 312,   /* TBCOLn value mismatch */

    /* ---- Data validation (350–399) ---- */
    FV_ERR_VAR_EXCEEDS_MAXLEN  = 350,   /* var length > max from TFORMn */
    FV_ERR_VAR_EXCEEDS_HEAP    = 351,   /* var array address outside heap */
    FV_ERR_BIT_NOT_JUSTIFIED   = 352,   /* bit column not left justified */
    FV_ERR_BAD_LOGICAL_DATA    = 353,   /* logical value not T, F, or 0 */
    FV_ERR_NONASCII_DATA       = 354,   /* string column has non-ASCII */
    FV_ERR_NO_DECIMAL          = 355,   /* ASCII float missing decimal point */
    FV_ERR_EMBEDDED_SPACE      = 356,   /* ASCII numeric has embedded space */
    FV_ERR_NONASCII_TABLE      = 357,   /* ASCII table has non-ASCII chars */
    FV_ERR_DATA_FILL           = 358,   /* data fill bytes incorrect */
    FV_ERR_HEADER_FILL         = 359,   /* header fill bytes not blank */
    FV_ERR_ASCII_GAP           = 360,   /* ASCII table gap has chars > 127 */

    /* ---- WCS (400–419) ---- */
    FV_ERR_WCSAXES_ORDER       = 400,   /* WCSAXES after other WCS keywords */
    FV_ERR_WCS_INDEX           = 401,   /* WCS keyword index > WCSAXES */

    /* ---- CFITSIO / I-O (450–479) ---- */
    FV_ERR_CFITSIO             = 450,   /* CFITSIO library error */
    FV_ERR_CFITSIO_STACK       = 451,   /* CFITSIO error stack dump */

    /* ---- Internal / abort (480–499) ---- */
    FV_ERR_TOO_MANY            = 480,   /* MAXERRORS exceeded, aborted */

    /* ---- Warnings (500–599) ---- */
    FV_WARN_SIMPLE_FALSE       = 500,   /* SIMPLE = F */
    FV_WARN_DEPRECATED         = 501,   /* deprecated keyword (EPOCH, BLOCKED) */
    FV_WARN_DUPLICATE_EXTNAME  = 502,   /* HDUs with same EXTNAME/EXTVER */
    FV_WARN_ZERO_SCALE         = 503,   /* BSCALE or TSCALn = 0 */
    FV_WARN_TNULL_RANGE        = 504,   /* BLANK/TNULLn out of range */
    FV_WARN_RAW_NOT_MULTIPLE   = 505,   /* rAw format: r not multiple of w */
    FV_WARN_Y2K                = 506,   /* DATE yy < 10 (Y2K?) */
    FV_WARN_WCS_INDEX          = 507,   /* WCS index > NAXIS (no WCSAXES) */
    FV_WARN_DUPLICATE_KEYWORD  = 508,   /* duplicated keyword */
    FV_WARN_BAD_COLUMN_NAME    = 509,   /* column name has bad characters */
    FV_WARN_NO_COLUMN_NAME     = 510,   /* column has no name (no TTYPEn) */
    FV_WARN_DUPLICATE_COLUMN   = 511,   /* duplicate column names */
    FV_WARN_BAD_CHECKSUM       = 512,   /* checksum mismatch */
    FV_WARN_MISSING_LONGSTRN   = 513,   /* LONGSTRN keyword missing */
    FV_WARN_VAR_EXCEEDS_32BIT  = 514,   /* var length > 32-bit range */
    FV_WARN_HIERARCH_DUPLICATE = 515,   /* duplicate HIERARCH keyword */
    FV_WARN_PCOUNT_NO_VLA      = 516,   /* PCOUNT != 0, no VLA columns */
    FV_WARN_CONTINUE_CHAR      = 517,   /* column name uses '&' (CONTINUE) */
    FV_WARN_RANDOM_GROUPS      = 518,   /* Random Groups (deprecated since FITS v1) */
    FV_WARN_LEGACY_XTENSION    = 519,   /* non-standard XTENSION value */
    FV_WARN_TIMESYS_VALUE      = 520,   /* TIMESYS has non-standard value */
    FV_WARN_INHERIT_PRIMARY    = 521    /* INHERIT in primary with data */
} fv_error_code;

/* ---- output callback --------------------------------------------------- */
typedef enum {
    FV_MSG_INFO    = 0,
    FV_MSG_WARNING = 1,
    FV_MSG_ERROR   = 2,
    FV_MSG_SEVERE  = 3
} fv_msg_severity;

typedef struct {
    fv_msg_severity severity;
    fv_error_code   code;
    int             hdu_num;
    const char     *text;
    const char     *fix_hint;  /* NULL unless FV_OPT_FIX_HINTS enabled */
    const char     *explain;   /* NULL unless FV_OPT_EXPLAIN enabled   */
} fv_message;

/*
 * Output callback type.  When registered via fv_set_output(), every output
 * message is delivered through this function instead of being written to
 * FILE* streams.  msg->text is only valid for the duration of the call;
 * the callback must copy it if needed.
 */
typedef void (*fv_output_fn)(const fv_message *msg, void *userdata);

/* ---- options ----------------------------------------------------------- */
typedef enum {
    FV_OPT_PRHEAD       = 0,   /* print header keyword listing (int 0/1) */
    FV_OPT_PRSTAT       = 1,   /* print HDU summary (int 0/1)            */
    FV_OPT_TESTDATA     = 2,   /* test data values  (int 0/1)            */
    FV_OPT_TESTCSUM     = 3,   /* test checksum     (int 0/1)            */
    FV_OPT_TESTFILL     = 4,   /* test fill areas   (int 0/1)            */
    FV_OPT_HEASARC_CONV = 5,   /* check HEASARC conventions (int 0/1)    */
    FV_OPT_TESTHIERARCH = 6,   /* test ESO HIERARCH keywords (int 0/1)   */
    FV_OPT_ERR_REPORT   = 7,   /* 0=all, 1=errors only, 2=severe only    */
    FV_OPT_FIX_HINTS    = 8,   /* attach fix hints to messages (int 0/1) */
    FV_OPT_EXPLAIN       = 9    /* attach explanations to messages (0/1)  */
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

/* ---- output callback --------------------------------------------------- */
/*
 * Register an output callback.  When set, all output is delivered through
 * fn instead of FILE* streams (no word wrapping is applied).
 * Pass fn=NULL to unregister and restore default FILE*-based output.
 */
void fv_set_output(fv_context *ctx, fv_output_fn fn, void *userdata);

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
 *
 * Thread safety: Each fv_context is independent and contains no shared
 * state. However, CFITSIO's internal error message stack is a process-
 * global resource and is NOT thread-safe. Concurrent calls to
 * fv_verify_file() or fv_verify_memory() from different threads will
 * corrupt CFITSIO's error state and may cause crashes or incorrect
 * results. To use libfitsverify from multiple threads, either:
 *   - Serialize all verification calls with a mutex, or
 *   - Build CFITSIO with --enable-reentrant (if supported by your
 *     CFITSIO version)
 */
int fv_verify_file(fv_context *ctx, const char *infile,
                   FILE *out, fv_result *result);

/*
 * Verify FITS data held in a memory buffer.
 *
 *   ctx    – context (must not be NULL)
 *   buffer – pointer to the FITS data (must not be NULL)
 *   size   – size of the buffer in bytes
 *   label  – display name for reports (e.g. "<memory>"); NULL → "<memory>"
 *   out    – FILE* for the report; may be NULL (quiet mode)
 *   result – if non-NULL, filled with per-file stats
 *
 * Returns 0 on success, non-zero on fatal/I-O error.
 * Errors/warnings accumulate in ctx across calls.
 */
int fv_verify_memory(fv_context *ctx, const void *buffer, size_t size,
                     const char *label, FILE *out, fv_result *result);

/* ---- accumulated totals ------------------------------------------------ */
void fv_get_totals(const fv_context *ctx,
                   long *total_errors, long *total_warnings);

/* ---- version ----------------------------------------------------------- */
const char *fv_version(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBFITSVERIFY_H */
