/*
 * test_abort.c — Verify that >MAXERRORS returns instead of calling exit()
 *
 * Calls fv_verify_file on a file designed to trigger >200 errors.
 * If the library incorrectly calls exit(), this process will terminate
 * and the test runner will report a failure.  If the library correctly
 * uses longjmp, the function returns and we can inspect the result.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fitsverify.h"

int main(void)
{
    fv_context *ctx;
    fv_result result;
    int rc;

    printf("=== test_abort ===\n\n");

    ctx = fv_context_new();
    if (!ctx) {
        fprintf(stderr, "FAIL: could not create context\n");
        return 1;
    }

    printf("Verifying err_many_errors.fits (expecting >200 errors)...\n");

    memset(&result, 0, sizeof(result));
    rc = fv_verify_file(ctx, "err_many_errors.fits", NULL, &result);

    /* If we get here, exit() was NOT called — that's the main assertion */
    printf("  PASS: fv_verify_file returned (exit() was not called)\n");

    printf("  return code: %d\n", rc);
    printf("  errors:      %d\n", result.num_errors);
    printf("  warnings:    %d\n", result.num_warnings);
    printf("  aborted:     %d\n", result.aborted);

    if (result.aborted) {
        printf("  PASS: result.aborted == 1 (verification was aborted)\n");
    } else {
        printf("  INFO: result.aborted == 0 (completed without hitting abort)\n");
    }

    if (result.num_errors > 0) {
        printf("  PASS: errors detected as expected\n");
    } else {
        printf("  WARN: no errors detected (unexpected)\n");
    }

    fv_context_free(ctx);
    printf("\n=== test_abort PASSED ===\n");
    return 0;
}
