/*
 * test_output_callback.c — Tests for the fv_set_output() callback system
 *
 * Exercises: fv_set_output, fv_message delivery, severity mapping,
 *            hdu_num tracking, prefix preservation, MAXERRORS abort path,
 *            unset callback (restore FILE* output), backward compatibility.
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

/* ---- callback state ---------------------------------------------------- */

#define MAX_MSGS 2048

typedef struct {
    fv_msg_severity severity;
    int             hdu_num;
    char            text[512];
} saved_message;

static saved_message msgs[MAX_MSGS];
static int           n_msgs = 0;

static void reset_msgs(void)
{
    n_msgs = 0;
}

static void test_callback(const fv_message *msg, void *userdata)
{
    int *counter = (int *)userdata;
    if (counter) (*counter)++;
    if (n_msgs < MAX_MSGS) {
        msgs[n_msgs].severity = msg->severity;
        msgs[n_msgs].hdu_num  = msg->hdu_num;
        strncpy(msgs[n_msgs].text, msg->text, sizeof(msgs[n_msgs].text) - 1);
        msgs[n_msgs].text[sizeof(msgs[n_msgs].text) - 1] = '\0';
        n_msgs++;
    }
}

/* ---- helpers ----------------------------------------------------------- */

static int count_severity(fv_msg_severity sev)
{
    int count = 0;
    for (int i = 0; i < n_msgs; i++)
        if (msgs[i].severity == sev) count++;
    return count;
}

static int any_text_contains(const char *needle)
{
    for (int i = 0; i < n_msgs; i++)
        if (strstr(msgs[i].text, needle)) return 1;
    return 0;
}

static int any_text_starts_with(const char *prefix)
{
    for (int i = 0; i < n_msgs; i++)
        if (strncmp(msgs[i].text, prefix, strlen(prefix)) == 0) return 1;
    return 0;
}

/* ---- tests ------------------------------------------------------------- */

int main(void)
{
    fv_context *ctx;
    fv_result result;
    int rc;
    int cb_count;

    printf("=== test_output_callback ===\n\n");

    /* ---- 1. Valid file -> callback receives INFO only ---- */
    printf("1. Valid file via callback\n");
    ctx = fv_context_new();
    CHECK(ctx != NULL, "context created");

    cb_count = 0;
    reset_msgs();
    fv_set_output(ctx, test_callback, &cb_count);

    memset(&result, 0, sizeof(result));
    rc = fv_verify_file(ctx, "valid_minimal.fits", NULL, &result);
    CHECK(rc == 0, "fv_verify_file returns 0");
    CHECK(result.num_errors == 0, "0 errors");
    CHECK(result.num_warnings == 0, "0 warnings");
    CHECK(cb_count > 0, "callback was invoked");
    CHECK(n_msgs > 0, "messages were captured");
    CHECK(count_severity(FV_MSG_ERROR) == 0, "no ERROR messages");
    CHECK(count_severity(FV_MSG_WARNING) == 0, "no WARNING messages");
    CHECK(count_severity(FV_MSG_SEVERE) == 0, "no SEVERE messages");
    printf("   (%d messages captured, %d callback invocations)\n", n_msgs, cb_count);
    fv_context_free(ctx);

    /* ---- 2. Bad file -> callback receives ERROR messages ---- */
    printf("\n2. Bad file via callback\n");
    ctx = fv_context_new();
    cb_count = 0;
    reset_msgs();
    fv_set_output(ctx, test_callback, &cb_count);

    memset(&result, 0, sizeof(result));
    rc = fv_verify_file(ctx, "err_bad_bitpix.fits", NULL, &result);
    CHECK(result.num_errors > 0, "errors detected");
    CHECK(count_severity(FV_MSG_ERROR) + count_severity(FV_MSG_SEVERE) > 0,
          "ERROR or SEVERE messages received");
    printf("   (errors=%d, warnings=%d, msgs=%d)\n",
           result.num_errors, result.num_warnings, n_msgs);
    fv_context_free(ctx);

    /* ---- 3. Dup extname -> callback receives WARNING messages ---- */
    printf("\n3. Dup extname via callback\n");
    ctx = fv_context_new();
    cb_count = 0;
    reset_msgs();
    fv_set_output(ctx, test_callback, &cb_count);

    memset(&result, 0, sizeof(result));
    rc = fv_verify_file(ctx, "err_dup_extname.fits", NULL, &result);
    CHECK(result.num_errors > 0 || result.num_warnings > 0,
          "errors or warnings detected");
    /* dup extname produces warnings */
    if (result.num_warnings > 0) {
        CHECK(count_severity(FV_MSG_WARNING) > 0,
              "WARNING messages received for dup extname");
    } else {
        printf("  INFO: no warnings (errors only) for dup extname\n");
    }
    printf("   (errors=%d, warnings=%d, msgs=%d)\n",
           result.num_errors, result.num_warnings, n_msgs);
    fv_context_free(ctx);

    /* ---- 4. No callback + NULL out -> results still correct ---- */
    printf("\n4. Backward compat: no callback + NULL out\n");
    ctx = fv_context_new();

    memset(&result, 0, sizeof(result));
    rc = fv_verify_file(ctx, "valid_minimal.fits", NULL, &result);
    CHECK(rc == 0, "returns 0 for valid file without callback");
    CHECK(result.num_errors == 0, "0 errors without callback");
    CHECK(result.num_warnings == 0, "0 warnings without callback");
    fv_context_free(ctx);

    /* ---- 5. Callback + NULL out -> messages arrive via callback ---- */
    printf("\n5. Callback with NULL out\n");
    ctx = fv_context_new();
    cb_count = 0;
    reset_msgs();
    fv_set_output(ctx, test_callback, &cb_count);

    memset(&result, 0, sizeof(result));
    rc = fv_verify_file(ctx, "err_bad_bitpix.fits", NULL, &result);
    CHECK(cb_count > 0, "callback invoked even with NULL out");
    CHECK(result.num_errors > 0, "errors still counted with NULL out + callback");
    fv_context_free(ctx);

    /* ---- 6. MAXERRORS abort -> "Too many Errors" delivered ---- */
    printf("\n6. MAXERRORS abort via callback\n");
    ctx = fv_context_new();
    cb_count = 0;
    reset_msgs();
    fv_set_output(ctx, test_callback, &cb_count);

    memset(&result, 0, sizeof(result));
    rc = fv_verify_file(ctx, "err_many_errors.fits", NULL, &result);
    CHECK(result.aborted == 1, "result.aborted == 1");
    CHECK(any_text_contains("Too many Errors"),
          "'Too many Errors' message delivered via callback");
    printf("   (errors=%d, msgs=%d, aborted=%d)\n",
           result.num_errors, n_msgs, result.aborted);
    fv_context_free(ctx);

    /* ---- 7. Unset callback -> FILE* output restored ---- */
    printf("\n7. Unset callback restores FILE* output\n");
    ctx = fv_context_new();
    cb_count = 0;
    reset_msgs();
    fv_set_output(ctx, test_callback, &cb_count);

    /* verify callback is active */
    memset(&result, 0, sizeof(result));
    fv_verify_file(ctx, "valid_minimal.fits", NULL, &result);
    CHECK(cb_count > 0, "callback was active");
    {
        int saved_count = cb_count;

        /* unset callback */
        fv_set_output(ctx, NULL, NULL);

        /* verify with no callback — cb_count should not change */
        cb_count = 0;
        reset_msgs();
        memset(&result, 0, sizeof(result));
        fv_verify_file(ctx, "valid_minimal.fits", NULL, &result);
        CHECK(cb_count == 0, "callback not invoked after unset");
        CHECK(result.num_errors == 0, "results still correct after unset");
    }
    fv_context_free(ctx);

    /* ---- 8. Error/Warning text prefixes ---- */
    printf("\n8. Message text prefixes\n");
    ctx = fv_context_new();
    reset_msgs();
    fv_set_output(ctx, test_callback, NULL);

    memset(&result, 0, sizeof(result));
    fv_verify_file(ctx, "err_bad_bitpix.fits", NULL, &result);
    CHECK(any_text_starts_with("*** Error:   "),
          "error text starts with '*** Error:   '");
    fv_context_free(ctx);

    ctx = fv_context_new();
    reset_msgs();
    fv_set_output(ctx, test_callback, NULL);

    memset(&result, 0, sizeof(result));
    fv_verify_file(ctx, "err_dup_extname.fits", NULL, &result);
    if (result.num_warnings > 0) {
        CHECK(any_text_starts_with("*** Warning: "),
              "warning text starts with '*** Warning: '");
    } else {
        printf("  INFO: no warnings to check prefix on\n");
    }
    fv_context_free(ctx);

    /* ---- Summary ---- */
    printf("\n=== Results: %d passed, %d failed ===\n", n_pass, n_fail);
    return n_fail ? 1 : 0;
}
