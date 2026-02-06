/*
 * gen_test_fits.c — Generate test FITS files with known errors using CFITSIO
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fitsio.h"

static void check_status(int status, const char *msg)
{
    if (status) {
        fprintf(stderr, "CFITSIO error in %s: ", msg);
        fits_report_error(stderr, status);
        exit(1);
    }
}

/* Remove file if it exists (CFITSIO won't overwrite) */
static void remove_if_exists(const char *fname)
{
    remove(fname);
}

/* valid_minimal.fits — valid file, primary HDU only */
static void gen_valid_minimal(void)
{
    fitsfile *fptr;
    int status = 0;
    long naxes[2] = {10, 10};
    short data[100];
    int i;

    remove_if_exists("valid_minimal.fits");
    fits_create_file(&fptr, "valid_minimal.fits", &status);
    check_status(status, "create valid_minimal");

    fits_create_img(fptr, SHORT_IMG, 2, naxes, &status);
    check_status(status, "create img");

    for (i = 0; i < 100; i++) data[i] = (short)i;
    fits_write_img(fptr, TSHORT, 1, 100, data, &status);
    check_status(status, "write img");

    fits_close_file(fptr, &status);
    check_status(status, "close valid_minimal");
    printf("  created valid_minimal.fits\n");
}

/* valid_multi_ext.fits — valid file with image + binary table + ASCII table */
static void gen_valid_multi_ext(void)
{
    fitsfile *fptr;
    int status = 0;
    long naxes[2] = {10, 10};
    short data[100];
    int i;
    char *ttype[] = {"X", "Y", "NAME"};
    char *tform[] = {"1E", "1E", "10A"};
    char *tunit[] = {"m", "m", ""};
    char *attype[] = {"COL1", "COL2"};
    char *atform[] = {"F8.3", "I6"};
    char *atunit[] = {"", ""};

    remove_if_exists("valid_multi_ext.fits");
    fits_create_file(&fptr, "valid_multi_ext.fits", &status);
    check_status(status, "create valid_multi_ext");

    /* Primary image */
    fits_create_img(fptr, SHORT_IMG, 2, naxes, &status);
    for (i = 0; i < 100; i++) data[i] = (short)i;
    fits_write_img(fptr, TSHORT, 1, 100, data, &status);
    check_status(status, "write primary img");

    /* Binary table */
    fits_create_tbl(fptr, BINARY_TBL, 0, 3, ttype, tform, tunit,
                    "TEST_BTBL", &status);
    check_status(status, "create bintable");

    /* ASCII table */
    fits_create_tbl(fptr, ASCII_TBL, 0, 2, attype, atform, atunit,
                    "TEST_ATBL", &status);
    check_status(status, "create asctable");

    fits_close_file(fptr, &status);
    check_status(status, "close valid_multi_ext");
    printf("  created valid_multi_ext.fits\n");
}

/* err_bad_bitpix.fits — create a file then corrupt the BITPIX value */
static void gen_err_bad_bitpix(void)
{
    fitsfile *fptr;
    int status = 0;
    long naxes[2] = {10, 10};
    short data[100];
    FILE *fp;
    int i;
    char card[81];

    remove_if_exists("err_bad_bitpix.fits");
    fits_create_file(&fptr, "err_bad_bitpix.fits", &status);
    fits_create_img(fptr, SHORT_IMG, 2, naxes, &status);
    for (i = 0; i < 100; i++) data[i] = (short)i;
    fits_write_img(fptr, TSHORT, 1, 100, data, &status);
    fits_close_file(fptr, &status);
    check_status(status, "create err_bad_bitpix");

    /* Corrupt the BITPIX value by raw byte editing */
    fp = fopen("err_bad_bitpix.fits", "r+b");
    if (fp) {
        /* BITPIX card is at offset 80 (second card) */
        fseek(fp, 80, SEEK_SET);
        memset(card, ' ', 80);
        sprintf(card, "BITPIX  =                   99");
        /* pad with spaces to 80 */
        for (i = (int)strlen(card); i < 80; i++) card[i] = ' ';
        fwrite(card, 1, 80, fp);
        fclose(fp);
    }
    printf("  created err_bad_bitpix.fits\n");
}

/* err_dup_extname.fits — two extensions with same EXTNAME/EXTVER */
static void gen_err_dup_extname(void)
{
    fitsfile *fptr;
    int status = 0;
    long naxes[1] = {0};
    char *ttype[] = {"COL1"};
    char *tform[] = {"1E"};
    char *tunit[] = {""};

    remove_if_exists("err_dup_extname.fits");
    fits_create_file(&fptr, "err_dup_extname.fits", &status);

    /* Empty primary */
    fits_create_img(fptr, SHORT_IMG, 0, naxes, &status);

    /* First extension */
    fits_create_tbl(fptr, BINARY_TBL, 0, 1, ttype, tform, tunit,
                    "DUPLICATE", &status);
    fits_write_key(fptr, TINT, "EXTVER", &(int){1}, NULL, &status);

    /* Second extension with same name/version */
    fits_create_tbl(fptr, BINARY_TBL, 0, 1, ttype, tform, tunit,
                    "DUPLICATE", &status);
    fits_write_key(fptr, TINT, "EXTVER", &(int){1}, NULL, &status);

    fits_close_file(fptr, &status);
    check_status(status, "close err_dup_extname");
    printf("  created err_dup_extname.fits\n");
}

/* err_missing_end.fits — corrupt the END keyword */
static void gen_err_missing_end(void)
{
    fitsfile *fptr;
    int status = 0;
    long naxes[2] = {10, 10};
    short data[100];
    FILE *fp;
    long filesize;
    char *buf;
    int i;

    remove_if_exists("err_missing_end.fits");
    fits_create_file(&fptr, "err_missing_end.fits", &status);
    fits_create_img(fptr, SHORT_IMG, 2, naxes, &status);
    for (i = 0; i < 100; i++) data[i] = (short)i;
    fits_write_img(fptr, TSHORT, 1, 100, data, &status);
    fits_close_file(fptr, &status);
    check_status(status, "create err_missing_end base");

    /* Find and corrupt the END card */
    fp = fopen("err_missing_end.fits", "r+b");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        filesize = ftell(fp);
        rewind(fp);
        buf = (char *)malloc(filesize);
        fread(buf, 1, filesize, fp);

        /* Search for "END     " in the header */
        for (i = 0; i < filesize - 80; i += 80) {
            if (!strncmp(buf + i, "END     ", 8)) {
                /* Replace END with spaces */
                memset(buf + i, ' ', 80);
                break;
            }
        }
        rewind(fp);
        fwrite(buf, 1, filesize, fp);
        fclose(fp);
        free(buf);
    }
    printf("  created err_missing_end.fits\n");
}

/* err_many_errors.fits — file triggering >200 errors (tests abort path) */
static void gen_err_many_errors(void)
{
    fitsfile *fptr;
    int status = 0;
    long naxes[1] = {0};
    char *ttype[220];
    char *tform[220];
    char *tunit[220];
    char names[220][20];
    int i;

    remove_if_exists("err_many_errors.fits");
    fits_create_file(&fptr, "err_many_errors.fits", &status);

    /* Empty primary */
    fits_create_img(fptr, SHORT_IMG, 0, naxes, &status);

    /* Binary table with 220 columns — we'll add bad TDISPn for each */
    for (i = 0; i < 220; i++) {
        sprintf(names[i], "COL%d", i + 1);
        ttype[i] = names[i];
        tform[i] = "1J";
        tunit[i] = "";
    }

    fits_create_tbl(fptr, BINARY_TBL, 10, 220, ttype, tform, tunit,
                    "ERRORS", &status);
    check_status(status, "create many errors table");

    /* Write bad TDISP values that will each trigger an error */
    for (i = 0; i < 220; i++) {
        char keyname[20], badval[20];
        sprintf(keyname, "TDISP%d", i + 1);
        sprintf(badval, "Q%d", i);  /* 'Q' is not a valid TDISP format */
        fits_write_key(fptr, TSTRING, keyname, badval, NULL, &status);
        status = 0; /* clear any error from cfitsio */
    }

    fits_close_file(fptr, &status);
    check_status(status, "close err_many_errors");
    printf("  created err_many_errors.fits\n");
}

int main(void)
{
    printf("Generating test FITS files...\n");
    gen_valid_minimal();
    gen_valid_multi_ext();
    gen_err_bad_bitpix();
    gen_err_dup_extname();
    gen_err_missing_end();
    gen_err_many_errors();
    printf("Done.\n");
    return 0;
}
