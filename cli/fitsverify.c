/*
 * fitsverify — thin CLI wrapper around libfitsverify
 *
 * Supports all original flags: -l -H -q -e -h
 * New flags: -s (severe only), --json (JSON output)
 * Supports @filelist.txt syntax for file lists.
 * No globals, no stubs, no HEADAS/PIL/WEBTOOL code.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fitsverify.h"
#include "fitsio.h"

/* ---- JSON output callback ----------------------------------------------- */

typedef struct {
    int          msg_count;
    int          first_file;   /* for comma before file objects */
    int          first_msg;    /* for comma before messages in current file */
    int          in_file;      /* currently inside a file object */
    FILE        *out;
} json_state;

static const char *severity_str(fv_msg_severity sev)
{
    switch (sev) {
        case FV_MSG_INFO:    return "info";
        case FV_MSG_WARNING: return "warning";
        case FV_MSG_ERROR:   return "error";
        case FV_MSG_SEVERE:  return "severe";
        default:             return "unknown";
    }
}

/* Escape a string for JSON output: handle \, ", and control characters */
static void json_write_escaped(FILE *out, const char *s)
{
    fputc('"', out);
    for (; *s; s++) {
        switch (*s) {
            case '"':  fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            case '\t': fputs("\\t", out); break;
            default:
                if ((unsigned char)*s < 0x20)
                    fprintf(out, "\\u%04x", (unsigned char)*s);
                else
                    fputc(*s, out);
                break;
        }
    }
    fputc('"', out);
}

static void json_callback(const fv_message *msg, void *userdata)
{
    json_state *js = (json_state *)userdata;
    FILE *out = js->out;

    if (!js->first_msg) fprintf(out, ",\n");
    js->first_msg = 0;

    fprintf(out, "      {\"severity\": \"%s\", \"code\": %d, \"hdu\": %d, \"text\": ",
            severity_str(msg->severity), (int)msg->code, msg->hdu_num);
    json_write_escaped(out, msg->text);
    if (msg->fix_hint) {
        fprintf(out, ", \"fix_hint\": ");
        json_write_escaped(out, msg->fix_hint);
    }
    if (msg->explain) {
        fprintf(out, ", \"explain\": ");
        json_write_escaped(out, msg->explain);
    }
    fprintf(out, "}");
    js->msg_count++;
}

static void json_begin_file(json_state *js, const char *filename)
{
    FILE *out = js->out;

    if (!js->first_file) fprintf(out, ",\n");
    js->first_file = 0;
    js->first_msg = 1;
    js->msg_count = 0;
    js->in_file = 1;

    fprintf(out, "    {\n      \"file\": ");
    json_write_escaped(out, filename);
    fprintf(out, ",\n      \"messages\": [\n");
}

static void json_end_file(json_state *js, const fv_result *result, int vfstatus)
{
    FILE *out = js->out;

    fprintf(out, "\n      ],\n");
    fprintf(out, "      \"num_errors\": %d,\n", vfstatus ? 1 : result->num_errors);
    fprintf(out, "      \"num_warnings\": %d,\n", result->num_warnings);
    fprintf(out, "      \"num_hdus\": %d,\n", result->num_hdus);
    fprintf(out, "      \"aborted\": %s\n", result->aborted ? "true" : "false");
    fprintf(out, "    }");
    js->in_file = 0;
}

/* ---- @filelist support -------------------------------------------------- */

/*
 * Read filenames from a text file, one per line.
 * Returns a dynamically allocated array of strings (caller frees).
 * Sets *count to the number of filenames read.
 * Returns NULL on error.
 */
static char **read_filelist(const char *listpath, int *count)
{
    FILE *fp;
    char line[FLEN_FILENAME];
    int capacity = 64;
    int n = 0;
    char **files;

    fp = fopen(listpath, "r");
    if (!fp) {
        fprintf(stderr, "Cannot open the list file: %s\n", listpath);
        return NULL;
    }

    files = (char **)malloc(capacity * sizeof(char *));
    if (!files) { fclose(fp); return NULL; }

    while (fgets(line, sizeof(line), fp)) {
        /* strip trailing whitespace/newline */
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'
                        || line[len-1] == ' '  || line[len-1] == '\t'))
            line[--len] = '\0';

        /* skip blank lines */
        if (len == 0) continue;

        if (n >= capacity) {
            capacity *= 2;
            files = (char **)realloc(files, capacity * sizeof(char *));
            if (!files) { fclose(fp); return NULL; }
        }

        files[n] = (char *)malloc(len + 1);
        if (!files[n]) { fclose(fp); return NULL; }
        strcpy(files[n], line);
        n++;
    }

    fclose(fp);
    *count = n;
    return files;
}

/* ---- verify_one_file ---------------------------------------------------- */

static int verify_one_file(fv_context *ctx, const char *filename,
                           int quiet, int json_mode, json_state *js)
{
    fv_result result;
    FILE *out;
    int vfstatus;

    if (json_mode) {
        json_begin_file(js, filename);
        out = NULL;  /* suppress FILE* output; callback handles it */
    } else {
        out = quiet ? NULL : stdout;
    }

    vfstatus = fv_verify_file(ctx, filename, out, &result);

    if (json_mode) {
        json_end_file(js, &result, vfstatus);
    }

    if (quiet && !json_mode) {
        int nerrs  = vfstatus ? 1 : result.num_errors;
        int nwarns = result.num_warnings;
        int filestatus = ((nerrs + nwarns) > 0) ? 1 : 0;

        if (filestatus) {
            if (fv_get_option(ctx, FV_OPT_ERR_REPORT))
                printf("verification FAILED: %-20s, %d errors\n",
                       filename, nerrs);
            else
                printf("verification FAILED: %-20s, %d warnings and %d errors\n",
                       filename, nwarns, nerrs);
        } else {
            printf("verification OK: %-20s\n", filename);
        }
    }

    return vfstatus;
}

/* ---- help and usage ----------------------------------------------------- */

static void print_help(void)
{
printf("fitsverify -- Verify that the input files conform to the FITS Standard.\n");
printf("\n");
printf("USAGE:   fitsverify filename ...  - verify one or more FITS files\n");
printf("                                    (may use wildcard characters)\n");
printf("   or    fitsverify @filelist.txt - verify a list of FITS files\n");
printf("      \n");
printf("   Optional flags:\n");
printf("          -H  test ESO HIERARCH keywords\n");
printf("          -l  list all header keywords\n");
printf("          -q  quiet; print one-line pass/fail summary per file\n");
printf("          -e  only test for error conditions (ignore warnings)\n");
printf("          -s  only test for severe error conditions\n");
printf("       --json output results as JSON\n");
printf("  --fix-hints show actionable fix suggestions for each error/warning\n");
printf("    --explain show detailed explanations for each error/warning\n");
printf(" \n");
printf("   fitsverify exits with a status equal to the number of errors + warnings.\n");
printf("        \n");
printf("EXAMPLES:\n");
printf("     fitsverify -l m101.fits    - produce a detailed verification report of\n");
printf("                                  a single file, including a keyword listing\n");
printf("     fitsverify -q *.fits *.fit - verify all files with .fits or .fit\n");
printf("                                  extensions, writing a 1-line pass/fail\n");
printf("                                  message for each file\n");
printf("     fitsverify --json *.fits   - output JSON verification results\n");
printf(" \n");
printf("DESCRIPTION:\n");
printf("    \n");
printf("    This task reads one or more input FITS files and verifies that the\n");
printf("    files conform to the specifications of the FITS Standard, Definition\n");
printf("    of the Flexible Image Transport System (FITS), Version 3.0, available\n");
printf("    online  at http://fits.gsfc.nasa.gov/.  The input filename template may\n");
printf("    contain wildcard characters, in which case all matching files will be \n");
printf("    tested.  Alternatively, the name of an ASCII text file containing a list\n");
printf("    of file names, one per line, may be entered preceded by an '@' character.\n");
printf("    The following error or warning conditions will be reported:\n");
printf("    \n");
printf("    ERROR CONDITIONS\n");
printf("    \n");
printf("     - Mandatory keyword not present or out of order\n");
printf("     - Mandatory keyword has wrong datatype or illegal value\n");
printf("     - END header keyword is not present\n");
printf("     - Sum of table column widths is inconsistent with NAXIS1 value\n");
printf("     - BLANK keyword present in image with floating-point datatype\n");
printf("     - TNULLn keyword present for floating-point binary table column\n");
printf("     - Bit column has non-zero fill bits or is not left adjusted \n");
printf("     - ASCII TABLE column contains illegal value inconsistent with TFORMn\n");
printf("     - Address to a variable length array not within the data heap \n");
printf("     - Extraneous bytes in the FITS file following the last HDU    \n");
printf("     - Mandatory keyword values not expressed in fixed format\n");
printf("     - Mandatory keyword duplicated elsewhere in the header\n");
printf("     - Header contains illegal ASCII character (not ASCII 32 - 126)\n");
printf("     - Keyword name contains illegal character\n");
printf("     - Keyword value field has illegal format\n");
printf("     - Value and comment fields not separated by a slash character\n");
printf("     - END keyword not filled with blanks in columns 9 - 80\n");
printf("     - Reserved keyword with wrong datatype or illegal value\n");
printf("     - XTENSION keyword in the primary array\n");
printf("     - Column related keyword (TFIELDS, TTYPEn,TFORMn, etc.) in an image\n");
printf("     - SIMPLE, EXTEND, or BLOCKED keyword in any extension\n");
printf("     - BSCALE, BZERO, BUNIT, BLANK, DATAMAX, DATAMIN keywords in a table\n");
printf("     - Table WCS keywords (TCTYPn, TCRPXn, TCRVLn, etc.) in an image\n");
printf("     - TDIMn or THEAP keyword in an ASCII table \n");
printf("     - TBCOLn keyword in a Binary table\n");
printf("     - THEAP keyword in a binary table that has PCOUNT = 0 \n");
printf("     - XTENSION, TFORMn, TDISPn or TDIMn value contains leading space(s)\n");
printf("     - WCSAXES keyword appears after other WCS keywords\n");
printf("     - Index of any WCS keyword (CRPIXn, CRVALn, etc.) greater than \n");
printf("       value of WCSAXES\n");
printf("     - Index of any table column descriptor keyword (TTYPEn, TFORMn,\n");
printf("       etc.) greater than value of TFIELDS\n");
printf("     - TSCALn or TZEROn present for an ASCII, logical, or Bit column\n");
printf("     - TDISPn value is inconsistent with the column datatype \n");
printf("     - Length of a variable length array greater than the maximum \n");
printf("       length as given by the TFORMn keyword\n");
printf("     - ASCII table floating-point column value does not have decimal point(*)\n");
printf("     - ASCII table numeric column value has embedded space character\n");
printf("     - Logical column contains illegal value not equal to 'T', 'F', or 0\n");
printf("     - Character string column contains non-ASCII text character\n");
printf("     - Header fill bytes not all blanks\n");
printf("     - Data fill bytes not all blanks in ASCII tables or all zeros \n");
printf("       in any other type of HDU \n");
printf("     - Gaps between defined ASCII table columns contain characters with\n");
printf("       ASCII value > 127\n");
printf("    \n");
printf("    WARNING CONDITIONS\n");
printf("    \n");
printf("     - SIMPLE = F\n");
printf("     - Presence of deprecated keywords BLOCKED or EPOCH\n");
printf("     - 2 HDUs have identical EXTNAME, EXTVER, and EXTLEVEL values\n");
printf("     - BSCALE or TSCALn value = 0.\n");
printf("     - BLANK OR TNULLn value exceeds the legal range\n");
printf("     - TFORMn has 'rAw' format and r is not a multiple of w\n");
printf("     - DATE = 'dd/mm/yy' and yy is less than 10 (Y2K problem?)\n");
printf("     - Index of any WCS keyword (CRPIXn, CRVALn, etc.) greater than\n");
printf("       value of NAXIS, if the WCSAXES keyword is not present\n");
printf("     - Duplicated keyword (except COMMENT, HISTORY, blank, etc.)\n");
printf("     - Column name (TTYPEn) does not exist or contains characters \n");
printf("       other than letter, digit and underscore\n");
printf("     - Calculated checksum inconsistent with CHECKSUM or DATASUM keyword\n");
printf("        \n");
printf("    This is the stand alone version of the FTOOLS 'fverify' program.  It is\n");
printf("    maintained by the HEASARC at NASA/GSFC.  Any comments about this program\n");
printf("    should be submitted to http://heasarc.gsfc.nasa.gov/cgi-bin/ftoolshelp\n");
}

static void print_usage(void)
{
    printf("\n");
    printf("fitsverify - test if the input file(s) conform to the FITS format.\n");
    printf("\n");
    printf("Usage:  fitsverify filename ...   or   fitsverify @filelist.txt\n");
    printf("\n");
    printf("  where 'filename' is a filename template (with optional wildcards), and\n");
    printf("        'filelist.txt' is an ASCII text file with a list of\n");
    printf("         FITS file names, one per line.\n");
    printf("\n");
    printf("   Optional flags:\n");
    printf("          -H  test ESO HIERARCH keywords\n");
    printf("          -l  list all header keywords\n");
    printf("          -q  quiet; print one-line pass/fail summary per file\n");
    printf("          -e  only test for error conditions; don't issue warnings\n");
    printf("          -s  only test for severe error conditions\n");
    printf("       --json output results as JSON\n");
    printf("  --fix-hints show actionable fix suggestions for each error/warning\n");
    printf("    --explain show detailed explanations for each error/warning\n");
    printf("\n");
    printf("Help:   fitsverify -h\n");
}

/* ---- main --------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    fv_context *ctx;
    int ii, file1 = 0, invalid = 0;
    int quiet = 0, json_mode = 0;
    float fversion;
    char banner[256];
    long toterr, totwrn;
    json_state js;

    if (argc == 2 && !strcmp(argv[1], "-h")) {
        print_help();
        return 0;
    }

    ctx = fv_context_new();
    if (!ctx) {
        fprintf(stderr, "Failed to allocate fv_context\n");
        return 1;
    }

    /* Match original fitsverify CLI behavior: HEASARC conventions off by default */
    fv_set_option(ctx, FV_OPT_HEASARC_CONV, 0);

    /* check for flags on the command line (flags can appear anywhere) */
    for (ii = 1; ii < argc; ii++) {
        /* long options */
        if (!strcmp(argv[ii], "--json")) {
            json_mode = 1;
            continue;
        }
        if (!strcmp(argv[ii], "--fix-hints")) {
            fv_set_option(ctx, FV_OPT_FIX_HINTS, 1);
            continue;
        }
        if (!strcmp(argv[ii], "--explain")) {
            fv_set_option(ctx, FV_OPT_EXPLAIN, 1);
            continue;
        }

        if ((*argv[ii] != '-') || !strcmp(argv[ii], "-") || argv[ii][0] == '@') {
            if (!file1) file1 = ii;
            continue;
        }

        if (!strcmp(argv[ii], "-l")) {
            fv_set_option(ctx, FV_OPT_PRHEAD, 1);
        } else if (!strcmp(argv[ii], "-H")) {
            fv_set_option(ctx, FV_OPT_TESTHIERARCH, 1);
        } else if (!strcmp(argv[ii], "-e")) {
            fv_set_option(ctx, FV_OPT_ERR_REPORT, 1);
        } else if (!strcmp(argv[ii], "-s")) {
            fv_set_option(ctx, FV_OPT_ERR_REPORT, 2);
        } else if (!strcmp(argv[ii], "-q")) {
            fv_set_option(ctx, FV_OPT_PRSTAT, 0);
            quiet = 1;
        } else {
            invalid = 1;
        }
    }

    if (invalid || argc == 1 || file1 == 0) {
        print_usage();
        fv_context_free(ctx);
        return 0;
    }

    /* JSON mode: set up callback and suppress FILE* output */
    if (json_mode) {
        memset(&js, 0, sizeof(js));
        js.first_file = 1;
        js.out = stdout;
        fv_set_output(ctx, json_callback, &js);
        fprintf(stdout, "{\n  \"fitsverify_version\": \"%s\",\n", fv_version());
        fits_get_version(&fversion);
        fprintf(stdout, "  \"cfitsio_version\": \"%.3f\",\n", fversion);
        fprintf(stdout, "  \"files\": [\n");
    }

    /* print banner (text mode only) */
    if (!quiet && !json_mode) {
        FILE *out = stdout;

        fprintf(out, " \n");
        fits_get_version(&fversion);
        snprintf(banner, sizeof(banner), "fitsverify %s (CFITSIO V%.3f)", fv_version(), fversion);
        {
            int blen = (int)strlen(banner);
            int pad = (60 - blen) / 2;
            int i;
            if (pad < 0) pad = 0;
            fprintf(out, "%*s%s%*s\n", pad, "", banner, 60 - blen - pad, "");
            for (i = 0; banner[i] != '\0'; i++) banner[i] = '-';
            fprintf(out, "%*s%s%*s\n", pad, "", banner, 60 - blen - pad, "");
        }
        fprintf(out, " \n");
        fprintf(out, " \n");

        if (fv_get_option(ctx, FV_OPT_ERR_REPORT) == 2) {
            fprintf(out, "Caution: Only checking for the most severe FITS format errors.\n");
        }
        if (fv_get_option(ctx, FV_OPT_HEASARC_CONV)) {
            fprintf(out, "HEASARC conventions are being checked.\n");
        }
        if (fv_get_option(ctx, FV_OPT_TESTHIERARCH)) {
            fprintf(out, "ESO HIERARCH keywords are being checked.\n");
        }
    }

    /* process files (skip flags that were already parsed) */
    for (ii = file1; ii < argc; ii++) {
        const char *arg = argv[ii];

        /* skip flags intermixed with filenames */
        if (!strcmp(arg, "--json") || !strcmp(arg, "--fix-hints") ||
            !strcmp(arg, "--explain") ||
            (!strcmp(arg, "-l") || !strcmp(arg, "-H") ||
             !strcmp(arg, "-e") || !strcmp(arg, "-s") ||
             !strcmp(arg, "-q")))
            continue;

        if (arg[0] == '@') {
            /* @filelist: read filenames from text file */
            int nfiles = 0, jj;
            char **files = read_filelist(arg + 1, &nfiles);
            if (!files) {
                fv_context_free(ctx);
                return 1;
            }
            for (jj = 0; jj < nfiles; jj++) {
                int vfstatus = verify_one_file(ctx, files[jj],
                                               quiet, json_mode, &js);
                free(files[jj]);
                if (vfstatus) {
                    /* free remaining filenames */
                    int kk;
                    for (kk = jj + 1; kk < nfiles; kk++) free(files[kk]);
                    free(files);
                    if (json_mode) {
                        fprintf(stdout, "\n  ],\n");
                        fv_get_totals(ctx, &toterr, &totwrn);
                        fprintf(stdout, "  \"total_errors\": %ld,\n", toterr);
                        fprintf(stdout, "  \"total_warnings\": %ld\n", totwrn);
                        fprintf(stdout, "}\n");
                    }
                    fv_context_free(ctx);
                    return vfstatus;
                }
            }
            free(files);
        } else {
            /* regular filename */
            int vfstatus = verify_one_file(ctx, arg, quiet, json_mode, &js);
            if (vfstatus) {
                if (json_mode) {
                    fprintf(stdout, "\n  ],\n");
                    fv_get_totals(ctx, &toterr, &totwrn);
                    fprintf(stdout, "  \"total_errors\": %ld,\n", toterr);
                    fprintf(stdout, "  \"total_warnings\": %ld\n", totwrn);
                    fprintf(stdout, "}\n");
                }
                fv_context_free(ctx);
                return vfstatus;
            }
        }
    }

    fv_get_totals(ctx, &toterr, &totwrn);

    if (json_mode) {
        fprintf(stdout, "\n  ],\n");
        fprintf(stdout, "  \"total_errors\": %ld,\n", toterr);
        fprintf(stdout, "  \"total_warnings\": %ld\n", totwrn);
        fprintf(stdout, "}\n");
    }

    fv_context_free(ctx);

    if ((toterr + totwrn) > 255)
        return 255;
    else
        return (int)(toterr + totwrn);
}
