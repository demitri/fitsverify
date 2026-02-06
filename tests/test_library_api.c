/*
 * test_library_api.c — Tests for the libfitsverify public API
 *
 * Exercises: fv_context_new, fv_set_option, fv_get_option,
 *            fv_verify_file, fv_get_totals, fv_context_free
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fitsverify.h"

static int n_pass = 0;
static int n_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { n_pass++; printf("  PASS: %s\n", msg); } \
    else      { n_fail++; printf("  FAIL: %s\n", msg); } \
} while(0)

int main(void)
{
    fv_context *ctx;
    fv_result result;
    long toterr, totwrn;
    int rc;

    printf("=== test_library_api ===\n\n");

    /* ---- 1. Context creation ---- */
    printf("1. Context lifecycle\n");
    ctx = fv_context_new();
    CHECK(ctx != NULL, "fv_context_new returns non-NULL");

    /* ---- 2. Option round-trip ---- */
    printf("\n2. Option get/set\n");
    /* defaults */
    CHECK(fv_get_option(ctx, FV_OPT_PRHEAD) == 0, "default PRHEAD == 0");
    CHECK(fv_get_option(ctx, FV_OPT_PRSTAT) == 1, "default PRSTAT == 1");
    CHECK(fv_get_option(ctx, FV_OPT_TESTDATA) == 1, "default TESTDATA == 1");
    CHECK(fv_get_option(ctx, FV_OPT_TESTCSUM) == 1, "default TESTCSUM == 1");
    CHECK(fv_get_option(ctx, FV_OPT_TESTFILL) == 1, "default TESTFILL == 1");
    CHECK(fv_get_option(ctx, FV_OPT_HEASARC_CONV) == 1, "default HEASARC_CONV == 1");
    CHECK(fv_get_option(ctx, FV_OPT_TESTHIERARCH) == 0, "default TESTHIERARCH == 0");
    CHECK(fv_get_option(ctx, FV_OPT_ERR_REPORT) == 0, "default ERR_REPORT == 0");

    /* set and read back */
    fv_set_option(ctx, FV_OPT_PRHEAD, 1);
    CHECK(fv_get_option(ctx, FV_OPT_PRHEAD) == 1, "set PRHEAD -> 1");
    fv_set_option(ctx, FV_OPT_PRHEAD, 0);
    CHECK(fv_get_option(ctx, FV_OPT_PRHEAD) == 0, "set PRHEAD -> 0");

    fv_set_option(ctx, FV_OPT_ERR_REPORT, 2);
    CHECK(fv_get_option(ctx, FV_OPT_ERR_REPORT) == 2, "set ERR_REPORT -> 2");
    fv_set_option(ctx, FV_OPT_ERR_REPORT, 0);

    /* ---- 3. Version string ---- */
    printf("\n3. Version\n");
    CHECK(fv_version() != NULL, "fv_version returns non-NULL");
    CHECK(strlen(fv_version()) > 0, "fv_version returns non-empty string");

    /* ---- 4. Verify a valid file ---- */
    printf("\n4. Verify valid_minimal.fits\n");
    memset(&result, 0, sizeof(result));
    rc = fv_verify_file(ctx, "valid_minimal.fits", NULL, &result);
    CHECK(rc == 0, "fv_verify_file returns 0 for valid file");
    CHECK(result.num_errors == 0, "valid file has 0 errors");
    CHECK(result.num_warnings == 0, "valid file has 0 warnings");
    CHECK(result.num_hdus >= 1, "valid file has >= 1 HDU");
    CHECK(result.aborted == 0, "valid file not aborted");

    /* ---- 5. Verify a valid multi-extension file ---- */
    printf("\n5. Verify valid_multi_ext.fits\n");
    memset(&result, 0, sizeof(result));
    rc = fv_verify_file(ctx, "valid_multi_ext.fits", NULL, &result);
    CHECK(rc == 0, "fv_verify_file returns 0 for multi-ext file");
    CHECK(result.num_hdus >= 3, "multi-ext file has >= 3 HDUs");

    /* ---- 6. Verify a known-bad file ---- */
    printf("\n6. Verify err_bad_bitpix.fits\n");
    memset(&result, 0, sizeof(result));
    rc = fv_verify_file(ctx, "err_bad_bitpix.fits", NULL, &result);
    CHECK(result.num_errors > 0, "bad bitpix file has > 0 errors");

    /* ---- 7. Verify duplicate extname ---- */
    printf("\n7. Verify err_dup_extname.fits\n");
    memset(&result, 0, sizeof(result));
    rc = fv_verify_file(ctx, "err_dup_extname.fits", NULL, &result);
    CHECK(result.num_errors > 0 || result.num_warnings > 0,
          "dup extname file has errors or warnings");

    /* ---- 8. Accumulated totals ---- */
    printf("\n8. Accumulated totals\n");
    fv_get_totals(ctx, &toterr, &totwrn);
    CHECK(toterr >= 0, "total errors >= 0");
    CHECK(totwrn >= 0, "total warnings >= 0");
    printf("   (totals: %ld errors, %ld warnings)\n", toterr, totwrn);

    /* ---- 9. Context free ---- */
    printf("\n9. Context free\n");
    fv_context_free(ctx);
    printf("  PASS: fv_context_free did not crash\n");
    n_pass++;

    /* ---- Summary ---- */
    printf("\n=== Results: %d passed, %d failed ===\n", n_pass, n_fail);
    return n_fail ? 1 : 0;
}
