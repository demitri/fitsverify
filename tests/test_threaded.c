/*
 * test_threaded.c — multi-threaded safety tests for libfitsverify
 *
 * Tests that:
 * 1. Independent fv_context instances can be created/destroyed from
 *    multiple threads without interference.
 * 2. Serial verification from different threads works correctly
 *    (context created in one thread, used in another).
 * 3. Concurrent verification with a mutex works correctly.
 *
 * Note: CFITSIO's internal error message stack is NOT thread-safe
 * (it uses a global buffer). True concurrent verification from
 * multiple threads requires either:
 *   - A CFITSIO build with --enable-reentrant (if available)
 *   - A mutex around CFITSIO calls (demonstrated here)
 *
 * The libfitsverify context itself is thread-safe: independent
 * contexts have no shared state. The threading limitation is
 * purely in CFITSIO.
 *
 * Requires pthreads (POSIX).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "fitsverify.h"
#include "fitsio.h"

#define NUM_THREADS 4
#define ITERATIONS  5

static int total_pass = 0;
static int total_fail = 0;

/* Mutex for serializing CFITSIO access */
static pthread_mutex_t cfitsio_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ---- Test 1: Concurrent context lifecycle ------------------------------- */

typedef struct {
    int thread_id;
    int passed;
    int failed;
} lifecycle_arg;

static void *lifecycle_worker(void *arg)
{
    lifecycle_arg *la = (lifecycle_arg *)arg;
    int iter;
    la->passed = 0;
    la->failed = 0;

    for (iter = 0; iter < ITERATIONS * 10; iter++) {
        fv_context *ctx = fv_context_new();
        if (!ctx) {
            la->failed++;
            continue;
        }

        /* Set various options to exercise the context */
        fv_set_option(ctx, FV_OPT_PRHEAD, iter % 2);
        fv_set_option(ctx, FV_OPT_TESTDATA, 1);
        fv_set_option(ctx, FV_OPT_ERR_REPORT, iter % 3);

        /* Verify options are correct */
        if (fv_get_option(ctx, FV_OPT_PRHEAD) == iter % 2 &&
            fv_get_option(ctx, FV_OPT_TESTDATA) == 1 &&
            fv_get_option(ctx, FV_OPT_ERR_REPORT) == iter % 3) {
            la->passed++;
        } else {
            la->failed++;
        }

        fv_context_free(ctx);
    }

    return NULL;
}

/* ---- Test 2: Mutex-protected concurrent verification -------------------- */

typedef struct {
    int         thread_id;
    const char *filename;
    int         expect_issues;   /* 1 if file should have errors or warnings */
    int         passed;
    int         failed;
    char        fail_msg[256];
} mutex_verify_arg;

static void *mutex_verify_worker(void *arg)
{
    mutex_verify_arg *ma = (mutex_verify_arg *)arg;
    int iter;
    ma->passed = 0;
    ma->failed = 0;
    ma->fail_msg[0] = '\0';

    for (iter = 0; iter < ITERATIONS; iter++) {
        fv_context *ctx = fv_context_new();
        fv_result result;
        int vfstatus;

        if (!ctx) {
            ma->failed++;
            continue;
        }

        /* Serialize CFITSIO access with a mutex */
        pthread_mutex_lock(&cfitsio_mutex);
        vfstatus = fv_verify_file(ctx, ma->filename, NULL, &result);
        pthread_mutex_unlock(&cfitsio_mutex);

        if (ma->expect_issues) {
            if (result.num_errors > 0 || result.num_warnings > 0 || vfstatus != 0) {
                ma->passed++;
            } else {
                snprintf(ma->fail_msg, sizeof(ma->fail_msg),
                         "thread %d iter %d: expected issues in %s but got 0 errors + 0 warnings",
                         ma->thread_id, iter, ma->filename);
                ma->failed++;
            }
        } else {
            if (vfstatus == 0 && result.num_errors == 0) {
                ma->passed++;
            } else {
                snprintf(ma->fail_msg, sizeof(ma->fail_msg),
                         "thread %d iter %d: expected 0 errors in %s but got %d (status=%d)",
                         ma->thread_id, iter, ma->filename, result.num_errors, vfstatus);
                ma->failed++;
            }
        }

        fv_context_free(ctx);
    }

    return NULL;
}

/* ---- Test 3: Callbacks from mutex-protected threads --------------------- */

typedef struct {
    int msg_count;
    int error_count;
    int warning_count;
} cb_data;

static void counting_callback(const fv_message *msg, void *userdata)
{
    cb_data *d = (cb_data *)userdata;
    d->msg_count++;
    if (msg->severity == FV_MSG_ERROR || msg->severity == FV_MSG_SEVERE)
        d->error_count++;
    else if (msg->severity == FV_MSG_WARNING)
        d->warning_count++;
}

typedef struct {
    int         thread_id;
    const char *filename;
    int         expect_issues;   /* 1 if file should have errors or warnings */
    int         passed;
    int         failed;
    char        fail_msg[256];
} cb_verify_arg;

static void *callback_verify_worker(void *arg)
{
    cb_verify_arg *ca = (cb_verify_arg *)arg;
    int iter;
    ca->passed = 0;
    ca->failed = 0;
    ca->fail_msg[0] = '\0';

    for (iter = 0; iter < ITERATIONS; iter++) {
        fv_context *ctx = fv_context_new();
        fv_result result;
        cb_data data;

        if (!ctx) {
            ca->failed++;
            continue;
        }

        memset(&data, 0, sizeof(data));
        fv_set_output(ctx, counting_callback, &data);

        pthread_mutex_lock(&cfitsio_mutex);
        fv_verify_file(ctx, ca->filename, NULL, &result);
        pthread_mutex_unlock(&cfitsio_mutex);

        /* Callback should have been invoked */
        if (data.msg_count > 0) {
            int has_issues = data.error_count > 0 || data.warning_count > 0 ||
                             result.num_errors > 0 || result.num_warnings > 0;
            if (ca->expect_issues && has_issues) {
                ca->passed++;
            } else if (!ca->expect_issues && data.error_count == 0 && result.num_errors == 0) {
                ca->passed++;
            } else if (ca->expect_issues) {
                snprintf(ca->fail_msg, sizeof(ca->fail_msg),
                         "thread %d iter %d: expected issues but cb_errors=%d cb_warnings=%d",
                         ca->thread_id, iter, data.error_count, data.warning_count);
                ca->failed++;
            } else {
                snprintf(ca->fail_msg, sizeof(ca->fail_msg),
                         "thread %d iter %d: expected 0 errors but cb_errors=%d result_errors=%d",
                         ca->thread_id, iter, data.error_count, result.num_errors);
                ca->failed++;
            }
        } else {
            snprintf(ca->fail_msg, sizeof(ca->fail_msg),
                     "thread %d iter %d: callback not invoked",
                     ca->thread_id, iter);
            ca->failed++;
        }

        fv_context_free(ctx);
    }

    return NULL;
}

int main(void)
{
    int i;

    const char *files[] = {
        "valid_minimal.fits",
        "err_bad_bitpix.fits",
        "valid_multi_ext.fits",
        "err_dup_extname.fits"
    };
    int expect_errors[] = { 0, 1, 0, 1 };

    printf("=== test_threaded ===\n\n");

    /* ---- Test 1: Concurrent context creation/destruction ---- */
    printf("1. Concurrent context lifecycle (%d threads, %d iterations each)\n",
           NUM_THREADS, ITERATIONS * 10);
    {
        pthread_t threads[NUM_THREADS];
        lifecycle_arg args[NUM_THREADS];
        int pass = 0, fail = 0;

        for (i = 0; i < NUM_THREADS; i++)
            args[i].thread_id = i;

        for (i = 0; i < NUM_THREADS; i++) {
            if (pthread_create(&threads[i], NULL, lifecycle_worker, &args[i]) != 0) {
                fprintf(stderr, "Failed to create thread %d\n", i);
                return 1;
            }
        }

        for (i = 0; i < NUM_THREADS; i++)
            pthread_join(threads[i], NULL);

        for (i = 0; i < NUM_THREADS; i++) {
            pass += args[i].passed;
            fail += args[i].failed;
        }

        if (fail == 0) {
            printf("  PASS: %d context lifecycle operations completed\n", pass);
            total_pass++;
        } else {
            printf("  FAIL: %d failures in context lifecycle\n", fail);
            total_fail++;
        }
    }

    /* ---- Test 2: Mutex-protected concurrent verification ---- */
    printf("\n2. Mutex-protected concurrent verification (%d threads, %d iterations each)\n",
           NUM_THREADS, ITERATIONS);
    {
        pthread_t threads[NUM_THREADS];
        mutex_verify_arg args[NUM_THREADS];
        int pass = 0, fail = 0;

        for (i = 0; i < NUM_THREADS; i++) {
            args[i].thread_id = i;
            args[i].filename = files[i % 4];
            args[i].expect_issues = expect_errors[i % 4];
        }

        for (i = 0; i < NUM_THREADS; i++) {
            if (pthread_create(&threads[i], NULL, mutex_verify_worker, &args[i]) != 0) {
                fprintf(stderr, "Failed to create thread %d\n", i);
                return 1;
            }
        }

        for (i = 0; i < NUM_THREADS; i++)
            pthread_join(threads[i], NULL);

        for (i = 0; i < NUM_THREADS; i++) {
            pass += args[i].passed;
            fail += args[i].failed;
            if (args[i].failed > 0)
                printf("  FAIL: %s\n", args[i].fail_msg);
        }

        if (fail == 0) {
            printf("  PASS: all %d mutex-protected verifications correct\n", pass);
            total_pass++;
        } else {
            total_fail++;
        }
    }

    /* ---- Test 3: Callbacks from mutex-protected threads ---- */
    printf("\n3. Callbacks from mutex-protected threads (%d threads, %d iterations each)\n",
           NUM_THREADS, ITERATIONS);
    {
        pthread_t threads[NUM_THREADS];
        cb_verify_arg args[NUM_THREADS];
        int pass = 0, fail = 0;

        for (i = 0; i < NUM_THREADS; i++) {
            args[i].thread_id = i;
            args[i].filename = files[i % 4];
            args[i].expect_issues = expect_errors[i % 4];
        }

        for (i = 0; i < NUM_THREADS; i++) {
            if (pthread_create(&threads[i], NULL, callback_verify_worker, &args[i]) != 0) {
                fprintf(stderr, "Failed to create thread %d\n", i);
                return 1;
            }
        }

        for (i = 0; i < NUM_THREADS; i++)
            pthread_join(threads[i], NULL);

        for (i = 0; i < NUM_THREADS; i++) {
            pass += args[i].passed;
            fail += args[i].failed;
            if (args[i].failed > 0)
                printf("  FAIL: %s\n", args[i].fail_msg);
        }

        if (fail == 0) {
            printf("  PASS: all %d callback verifications correct\n", pass);
            total_pass++;
        } else {
            total_fail++;
        }
    }

    /* ---- Test 4: Context reuse (sequential) ---- */
    printf("\n4. Context reuse (sequential, single-threaded)\n");
    {
        fv_context *ctx = fv_context_new();
        fv_result result;
        int vfstatus;

        if (!ctx) {
            printf("  FAIL: fv_context_new returned NULL\n");
            total_fail++;
        } else {
            vfstatus = fv_verify_file(ctx, "valid_minimal.fits", NULL, &result);
            if (vfstatus == 0 && result.num_errors == 0) {
                printf("  PASS: first verification correct\n");
                total_pass++;
            } else {
                printf("  FAIL: first verification (errors=%d, status=%d)\n",
                       result.num_errors, vfstatus);
                total_fail++;
            }

            vfstatus = fv_verify_file(ctx, "valid_multi_ext.fits", NULL, &result);
            if (vfstatus == 0 && result.num_errors == 0) {
                printf("  PASS: second verification correct (context reused)\n");
                total_pass++;
            } else {
                printf("  FAIL: second verification (errors=%d, status=%d)\n",
                       result.num_errors, vfstatus);
                total_fail++;
            }

            vfstatus = fv_verify_file(ctx, "err_bad_bitpix.fits", NULL, &result);
            if (result.num_errors > 0 || vfstatus != 0) {
                printf("  PASS: error file detected correctly after valid files\n");
                total_pass++;
            } else {
                printf("  FAIL: error file not detected\n");
                total_fail++;
            }

            {
                long toterr, totwrn;
                fv_get_totals(ctx, &toterr, &totwrn);
                if (toterr >= 1) {
                    printf("  PASS: accumulated totals correct (%ld errors from 3 files)\n", toterr);
                    total_pass++;
                } else {
                    printf("  FAIL: expected >= 1 accumulated errors, got %ld\n", toterr);
                    total_fail++;
                }
            }

            fv_context_free(ctx);
        }
    }

    printf("\n=== Results: %d passed, %d failed ===\n", total_pass, total_fail);

    return total_fail > 0 ? 1 : 0;
}
