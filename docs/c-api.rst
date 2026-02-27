C API Reference
===============

The libfitsverify C library provides an embeddable, reentrant FITS
standards-compliance validator.  All state is held in an opaque ``fv_context``
struct --- there are no global variables.

Include the public header:

.. code-block:: c

   #include "fitsverify.h"


Lifecycle
---------

.. c:function:: fv_context *fv_context_new(void)

   Create a new verification context with default options.  Returns ``NULL`` on
   allocation failure.  The caller must free the context with
   :c:func:`fv_context_free`.

.. c:function:: void fv_context_free(fv_context *ctx)

   Free a verification context and all associated resources.  Safe to call with
   ``NULL``.


Configuration
-------------

.. c:function:: int fv_set_option(fv_context *ctx, fv_option opt, int value)

   Set a configuration option.  Returns 0 on success, -1 on invalid option.

.. c:function:: int fv_get_option(fv_context *ctx, fv_option opt)

   Get the current value of a configuration option.  Returns -1 on invalid
   option.

.. c:type:: fv_option

   Configuration options:

   .. list-table::
      :header-rows: 1
      :widths: 30 10 60

      * - Option
        - Default
        - Description
      * - ``FV_OPT_PRHEAD``
        - 0
        - Print header keyword listing
      * - ``FV_OPT_PRSTAT``
        - 1
        - Print HDU summary statistics
      * - ``FV_OPT_TESTDATA``
        - 1
        - Validate data values
      * - ``FV_OPT_TESTCSUM``
        - 1
        - Verify checksums
      * - ``FV_OPT_TESTFILL``
        - 1
        - Check fill areas
      * - ``FV_OPT_HEASARC_CONV``
        - 1
        - Check HEASARC conventions
      * - ``FV_OPT_TESTHIERARCH``
        - 0
        - Check ESO HIERARCH keywords
      * - ``FV_OPT_ERR_REPORT``
        - 0
        - Severity filter: 0 = all, 1 = errors only, 2 = severe only
      * - ``FV_OPT_FIX_HINTS``
        - 0
        - Attach fix suggestions to messages
      * - ``FV_OPT_EXPLAIN``
        - 0
        - Attach detailed explanations to messages


Verification
------------

.. c:function:: int fv_verify_file(fv_context *ctx, const char *infile, FILE *out, fv_result *result)

   Verify a FITS file on disk.

   :param ctx: Context (must not be ``NULL``)
   :param infile: Path to the FITS file (supports CFITSIO extended syntax)
   :param out: ``FILE*`` for the text report; may be ``NULL`` for quiet mode
   :param result: If non-``NULL``, filled with per-file statistics
   :return: 0 on success, non-zero on fatal I/O error

.. c:function:: int fv_verify_memory(fv_context *ctx, const void *buffer, size_t size, const char *label, FILE *out, fv_result *result)

   Verify FITS data held in a memory buffer.

   :param ctx: Context (must not be ``NULL``)
   :param buffer: Pointer to the FITS data (must not be ``NULL``)
   :param size: Size of the buffer in bytes
   :param label: Display name for reports (e.g. ``"<memory>"``); ``NULL`` uses ``"<memory>"``
   :param out: ``FILE*`` for the text report; may be ``NULL``
   :param result: If non-``NULL``, filled with per-file statistics
   :return: 0 on success, non-zero on fatal I/O error

.. c:type:: fv_result

   Per-file verification result:

   .. code-block:: c

      typedef struct {
          int  num_errors;      /* errors found in this file   */
          int  num_warnings;    /* warnings found in this file */
          int  num_hdus;        /* HDUs processed              */
          int  aborted;         /* 1 if aborted (e.g. >200 errors) */
      } fv_result;


Output Callback
---------------

By default, verification output goes to ``FILE*`` streams.  Register a callback
to receive structured messages instead:

.. c:function:: void fv_set_output(fv_context *ctx, fv_output_fn fn, void *userdata)

   Register an output callback.  When set, all output is delivered through
   ``fn`` instead of ``FILE*`` streams (no word wrapping is applied).
   Pass ``fn=NULL`` to unregister and restore default behavior.

.. c:type:: fv_output_fn

   .. code-block:: c

      typedef void (*fv_output_fn)(const fv_message *msg, void *userdata);

.. c:type:: fv_message

   Structured message delivered to callbacks:

   .. code-block:: c

      typedef struct {
          fv_msg_severity severity;   /* INFO, WARNING, ERROR, or SEVERE */
          fv_error_code   code;       /* unique error code (0 = no code) */
          int             hdu_num;    /* HDU number (0 = file-level)     */
          const char     *text;       /* message text                    */
          const char     *fix_hint;   /* fix suggestion (NULL if disabled) */
          const char     *explain;    /* explanation (NULL if disabled)   */
      } fv_message;

   .. warning::

      ``text``, ``fix_hint``, and ``explain`` point to internal buffers and are
      only valid for the duration of the callback invocation.  Copy the strings
      if you need to keep them.

.. c:type:: fv_msg_severity

   Message severity levels:

   - ``FV_MSG_INFO`` (0) --- informational output (headers, summaries)
   - ``FV_MSG_WARNING`` (1) --- standards compliance warning
   - ``FV_MSG_ERROR`` (2) --- standards compliance error
   - ``FV_MSG_SEVERE`` (3) --- severe error (structural/fatal)


Accumulated Totals
------------------

.. c:function:: void fv_get_totals(const fv_context *ctx, long *total_errors, long *total_warnings)

   Get accumulated error and warning counts across all files verified with this
   context.  Either pointer may be ``NULL`` if that count is not needed.

.. c:function:: const char *fv_version(void)

   Return the libfitsverify version string (e.g. ``"1.0.0"``).


Thread Safety
-------------

Each ``fv_context`` is fully independent --- no shared state between contexts.
However, **CFITSIO's internal error message stack is a process-global resource
and is NOT thread-safe**.

Concurrent calls to :c:func:`fv_verify_file` or :c:func:`fv_verify_memory`
from different threads will corrupt CFITSIO's error state.  Options:

1. **Serialize with a mutex** (simplest; eliminates parallelism)
2. **Build CFITSIO with** ``--enable-reentrant`` (if supported)
3. **Process-level parallelism** (fork/multiprocessing; most practical for batch)

The Python bindings use option 1 (module-level lock) by default, with option 3
available via :func:`fitsverify.verify_parallel`.


Complete Example
----------------

.. code-block:: c

   #include <stdio.h>
   #include "fitsverify.h"

   /* Callback: collect messages with context-aware hints */
   static void my_callback(const fv_message *msg, void *userdata)
   {
       if (msg->severity >= FV_MSG_WARNING) {
           printf("[%s] HDU %d: %s\n",
                  msg->severity == FV_MSG_WARNING ? "WARN" : "ERR",
                  msg->hdu_num, msg->text);
           if (msg->fix_hint)
               printf("  Fix: %s\n", msg->fix_hint);
           if (msg->explain)
               printf("  Why: %s\n", msg->explain);
       }
   }

   int main(void)
   {
       fv_context *ctx = fv_context_new();
       fv_result result;

       /* Enable context-aware fix hints and explanations */
       fv_set_option(ctx, FV_OPT_FIX_HINTS, 1);
       fv_set_option(ctx, FV_OPT_EXPLAIN, 1);

       /* Use callback instead of FILE* output */
       fv_set_output(ctx, my_callback, NULL);

       fv_verify_file(ctx, "myfile.fits", NULL, &result);

       /* Hints include the keyword name, HDU number, and
          FITS Standard section reference when context is
          available --- e.g.:
            Fix: Add the keyword 'BITPIX' to the header of
                 HDU 2. The mandatory keywords for a binary
                 table in order are: XTENSION, BITPIX, ...
            Why: 'BITPIX' specifies the number of bits per
                 data element... See FITS Standard Section
                 4.4.1.1.
       */

       printf("\n%d errors, %d warnings\n",
              result.num_errors, result.num_warnings);

       fv_context_free(ctx);
       return 0;
   }
