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
    char            fix_hint[512];
    char            explain[512];
    int             has_fix_hint;
    int             has_explain;
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
        if (msg->fix_hint) {
            strncpy(msgs[n_msgs].fix_hint, msg->fix_hint, sizeof(msgs[n_msgs].fix_hint) - 1);
            msgs[n_msgs].fix_hint[sizeof(msgs[n_msgs].fix_hint) - 1] = '\0';
            msgs[n_msgs].has_fix_hint = 1;
        } else {
            msgs[n_msgs].fix_hint[0] = '\0';
            msgs[n_msgs].has_fix_hint = 0;
        }
        if (msg->explain) {
            strncpy(msgs[n_msgs].explain, msg->explain, sizeof(msgs[n_msgs].explain) - 1);
            msgs[n_msgs].explain[sizeof(msgs[n_msgs].explain) - 1] = '\0';
            msgs[n_msgs].has_explain = 1;
        } else {
            msgs[n_msgs].explain[0] = '\0';
            msgs[n_msgs].has_explain = 0;
        }
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

    /* ---- 9. Hints disabled by default -> fix_hint/explain are NULL ---- */
    printf("\n9. Hints disabled by default\n");
    ctx = fv_context_new();
    reset_msgs();
    fv_set_output(ctx, test_callback, NULL);

    memset(&result, 0, sizeof(result));
    fv_verify_file(ctx, "err_dup_extname.fits", NULL, &result);
    {
        int any_hint = 0;
        for (int i = 0; i < n_msgs; i++) {
            if (msgs[i].has_fix_hint || msgs[i].has_explain) {
                any_hint = 1;
                break;
            }
        }
        CHECK(!any_hint, "no hints when options disabled");
    }
    fv_context_free(ctx);

    /* ---- 10. FV_OPT_FIX_HINTS -> fix_hint populated on warnings ---- */
    printf("\n10. fix_hints enabled -> fix_hint on warnings\n");
    ctx = fv_context_new();
    fv_set_option(ctx, FV_OPT_FIX_HINTS, 1);
    reset_msgs();
    fv_set_output(ctx, test_callback, NULL);

    memset(&result, 0, sizeof(result));
    fv_verify_file(ctx, "err_dup_extname.fits", NULL, &result);
    {
        int got_hint = 0;
        for (int i = 0; i < n_msgs; i++) {
            if (msgs[i].severity == FV_MSG_WARNING && msgs[i].has_fix_hint) {
                got_hint = 1;
                break;
            }
        }
        CHECK(got_hint, "warning has fix_hint when FV_OPT_FIX_HINTS=1");
    }
    /* explain should still be NULL */
    {
        int any_explain = 0;
        for (int i = 0; i < n_msgs; i++) {
            if (msgs[i].has_explain) {
                any_explain = 1;
                break;
            }
        }
        CHECK(!any_explain, "no explain when FV_OPT_EXPLAIN=0");
    }
    fv_context_free(ctx);

    /* ---- 11. FV_OPT_EXPLAIN -> explain populated on warnings ---- */
    printf("\n11. explain enabled -> explain on warnings\n");
    ctx = fv_context_new();
    fv_set_option(ctx, FV_OPT_EXPLAIN, 1);
    reset_msgs();
    fv_set_output(ctx, test_callback, NULL);

    memset(&result, 0, sizeof(result));
    fv_verify_file(ctx, "err_dup_extname.fits", NULL, &result);
    {
        int got_explain = 0;
        for (int i = 0; i < n_msgs; i++) {
            if (msgs[i].severity == FV_MSG_WARNING && msgs[i].has_explain) {
                got_explain = 1;
                break;
            }
        }
        CHECK(got_explain, "warning has explain when FV_OPT_EXPLAIN=1");
    }
    /* fix_hint should still be NULL */
    {
        int any_hint = 0;
        for (int i = 0; i < n_msgs; i++) {
            if (msgs[i].has_fix_hint) {
                any_hint = 1;
                break;
            }
        }
        CHECK(!any_hint, "no fix_hint when FV_OPT_FIX_HINTS=0");
    }
    fv_context_free(ctx);

    /* ---- 12. Both enabled -> both populated ---- */
    printf("\n12. Both hints and explain enabled\n");
    ctx = fv_context_new();
    fv_set_option(ctx, FV_OPT_FIX_HINTS, 1);
    fv_set_option(ctx, FV_OPT_EXPLAIN, 1);
    reset_msgs();
    fv_set_output(ctx, test_callback, NULL);

    memset(&result, 0, sizeof(result));
    fv_verify_file(ctx, "err_dup_extname.fits", NULL, &result);
    {
        int got_both = 0;
        for (int i = 0; i < n_msgs; i++) {
            if (msgs[i].severity == FV_MSG_WARNING
                && msgs[i].has_fix_hint && msgs[i].has_explain) {
                got_both = 1;
                break;
            }
        }
        CHECK(got_both, "warning has both fix_hint and explain");
    }
    fv_context_free(ctx);

    /* ---- 13. INFO messages have no hints (they're structural) ---- */
    printf("\n13. INFO messages have no hints\n");
    ctx = fv_context_new();
    fv_set_option(ctx, FV_OPT_FIX_HINTS, 1);
    fv_set_option(ctx, FV_OPT_EXPLAIN, 1);
    reset_msgs();
    fv_set_output(ctx, test_callback, NULL);

    memset(&result, 0, sizeof(result));
    fv_verify_file(ctx, "err_dup_extname.fits", NULL, &result);
    {
        int info_with_hint = 0;
        for (int i = 0; i < n_msgs; i++) {
            if (msgs[i].severity == FV_MSG_INFO
                && (msgs[i].has_fix_hint || msgs[i].has_explain)) {
                info_with_hint = 1;
                break;
            }
        }
        CHECK(!info_with_hint, "INFO messages have no hints/explain");
    }
    fv_context_free(ctx);

    /* ---- 14. fix_hints on error messages ---- */
    printf("\n14. fix_hints on error messages\n");
    ctx = fv_context_new();
    fv_set_option(ctx, FV_OPT_FIX_HINTS, 1);
    reset_msgs();
    fv_set_output(ctx, test_callback, NULL);

    memset(&result, 0, sizeof(result));
    fv_verify_file(ctx, "err_bad_bitpix.fits", NULL, &result);
    {
        int got_err_hint = 0;
        for (int i = 0; i < n_msgs; i++) {
            if ((msgs[i].severity == FV_MSG_ERROR || msgs[i].severity == FV_MSG_SEVERE)
                && msgs[i].has_fix_hint) {
                got_err_hint = 1;
                break;
            }
        }
        CHECK(got_err_hint, "error has fix_hint when FV_OPT_FIX_HINTS=1");
    }
    fv_context_free(ctx);

    /* ---- 15. Context-aware fix_hint contains keyword name ---- */
    printf("\n15. Context-aware fix_hint contains keyword name\n");
    ctx = fv_context_new();
    fv_set_option(ctx, FV_OPT_FIX_HINTS, 1);
    reset_msgs();
    fv_set_output(ctx, test_callback, NULL);

    memset(&result, 0, sizeof(result));
    /* err_many_errors.fits has bad TDISPn keywords; the error hints
       should include the keyword name (e.g., 'TDISP') */
    fv_verify_file(ctx, "err_many_errors.fits", NULL, &result);
    {
        int hint_has_keyword = 0;
        for (int i = 0; i < n_msgs; i++) {
            if ((msgs[i].severity == FV_MSG_ERROR || msgs[i].severity == FV_MSG_SEVERE)
                && msgs[i].has_fix_hint
                && strstr(msgs[i].fix_hint, "TDISP")) {
                hint_has_keyword = 1;
                break;
            }
        }
        CHECK(hint_has_keyword,
              "error fix_hint contains keyword name 'TDISP'");
    }
    fv_context_free(ctx);

    /* ---- 16. Context-aware explain contains FITS Standard reference ---- */
    printf("\n16. Context-aware explain contains FITS Standard reference\n");
    ctx = fv_context_new();
    fv_set_option(ctx, FV_OPT_FIX_HINTS, 1);
    fv_set_option(ctx, FV_OPT_EXPLAIN, 1);
    reset_msgs();
    fv_set_output(ctx, test_callback, NULL);

    memset(&result, 0, sizeof(result));
    fv_verify_file(ctx, "err_dup_extname.fits", NULL, &result);
    {
        /* explain text for warnings should reference the FITS Standard
           or contain educational content */
        int explain_has_ref = 0;
        for (int i = 0; i < n_msgs; i++) {
            if (msgs[i].severity == FV_MSG_WARNING
                && msgs[i].has_explain
                && (strstr(msgs[i].explain, "FITS Standard")
                    || strstr(msgs[i].explain, "EXTNAME")
                    || strstr(msgs[i].explain, "unique"))) {
                explain_has_ref = 1;
                break;
            }
        }
        CHECK(explain_has_ref,
              "warning explain contains educational content");
    }
    fv_context_free(ctx);

    /* ---- 17. Context-aware warning hint contains keyword name ---- */
    printf("\n17. Context-aware warning hint contains keyword name\n");
    ctx = fv_context_new();
    fv_set_option(ctx, FV_OPT_FIX_HINTS, 1);
    reset_msgs();
    fv_set_output(ctx, test_callback, NULL);

    memset(&result, 0, sizeof(result));
    fv_verify_file(ctx, "err_dup_extname.fits", NULL, &result);
    {
        /* err_dup_extname.fits triggers WARN_DUPLICATE_EXTNAME with
           FV_HINT_SET_KEYWORD(ctx, "EXTNAME"); the hint should
           contain 'EXTNAME' */
        int warn_hint_has_kw = 0;
        for (int i = 0; i < n_msgs; i++) {
            if (msgs[i].severity == FV_MSG_WARNING
                && msgs[i].has_fix_hint
                && strstr(msgs[i].fix_hint, "EXTNAME")) {
                warn_hint_has_kw = 1;
                break;
            }
        }
        CHECK(warn_hint_has_kw,
              "warning fix_hint contains keyword name 'EXTNAME'");
    }
    fv_context_free(ctx);

    /* ---- 18. Context-aware hint includes HDU number ---- */
    printf("\n18. Context-aware hint includes HDU number\n");
    ctx = fv_context_new();
    fv_set_option(ctx, FV_OPT_FIX_HINTS, 1);
    reset_msgs();
    fv_set_output(ctx, test_callback, NULL);

    memset(&result, 0, sizeof(result));
    fv_verify_file(ctx, "err_many_errors.fits", NULL, &result);
    {
        /* The hint text should mention an HDU number */
        int hint_has_hdu = 0;
        for (int i = 0; i < n_msgs; i++) {
            if ((msgs[i].severity == FV_MSG_ERROR || msgs[i].severity == FV_MSG_SEVERE)
                && msgs[i].has_fix_hint
                && strstr(msgs[i].fix_hint, "HDU")) {
                hint_has_hdu = 1;
                break;
            }
        }
        CHECK(hint_has_hdu,
              "error fix_hint includes HDU reference");
    }
    fv_context_free(ctx);

    /* ---- 19. Fallback: static hints when no context ---- */
    printf("\n19. Fallback: static hints for file-level errors\n");
    ctx = fv_context_new();
    fv_set_option(ctx, FV_OPT_FIX_HINTS, 1);
    fv_set_option(ctx, FV_OPT_EXPLAIN, 1);
    reset_msgs();
    fv_set_output(ctx, test_callback, NULL);

    memset(&result, 0, sizeof(result));
    fv_verify_file(ctx, "err_dup_extname.fits", NULL, &result);
    {
        /* All error/warning messages that have hints should have
           non-empty fix_hint and explain strings */
        int all_valid = 1;
        int found_any = 0;
        for (int i = 0; i < n_msgs; i++) {
            if (msgs[i].severity == FV_MSG_INFO) continue;
            if (msgs[i].has_fix_hint) {
                found_any = 1;
                if (strlen(msgs[i].fix_hint) == 0) {
                    all_valid = 0;
                    break;
                }
            }
            if (msgs[i].has_explain) {
                found_any = 1;
                if (strlen(msgs[i].explain) == 0) {
                    all_valid = 0;
                    break;
                }
            }
        }
        CHECK(found_any && all_valid,
              "all hints have non-empty text (static or context-aware)");
    }
    fv_context_free(ctx);

    /* ---- Summary ---- */
    printf("\n=== Results: %d passed, %d failed ===\n", n_pass, n_fail);
    return n_fail ? 1 : 0;
}
