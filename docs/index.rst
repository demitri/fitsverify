libfitsverify
=============

**Embeddable, reentrant FITS standards-compliance validator.**

libfitsverify takes NASA/HEASARC's ``fitsverify`` --- the standard tool for
verifying that FITS files conform to the `FITS Standard
<https://fits.gsfc.nasa.gov/fits_standard.html>`_ --- and makes it work as an
embeddable library with a Python API, structured output, and modern safety
guarantees.  The included CLI is a drop-in replacement for the original
``fitsverify`` command.

All ~60 original validation checks are preserved exactly, plus new checks for
FITS Standard 4.0 compliance (time coordinates, column limits, deprecated
conventions).  No science logic was changed.

**CLI**

::

   $ fitsverify myfile.fits
   $ fitsverify --fix-hints --explain myfile.fits

**Python**

.. code-block:: python

   import fitsverify

   result = fitsverify.verify("myfile.fits")

   if not result.is_valid:
       for err in result.errors:
           print(err.message)

   # With context-aware fix hints
   result = fitsverify.verify("myfile.fits", fix_hints=True)
   for issue in result.errors:
       print(f"{issue.message}")
       if issue.fix_hint:
           print(f"  Fix: {issue.fix_hint}")

**C Library**

.. code-block:: c

   #include "fitsverify.h"

   fv_context *ctx = fv_context_new();
   fv_result result;

   fv_set_option(ctx, FV_OPT_FIX_HINTS, 1);
   fv_verify_file(ctx, "myfile.fits", stdout, &result);

   printf("errors: %d, warnings: %d\n",
          result.num_errors, result.num_warnings);
   fv_context_free(ctx);


Improvements Over the Original
------------------------------

- **Safe for embedding**: the original calls ``exit(1)`` when errors exceed a threshold --- libfitsverify never terminates the host process
- **No global state**: all state is held in per-verification context structs, making the library reentrant and thread-safe
- **Structured diagnostics**: every error has a unique code, severity level, and HDU number --- no more parsing text output
- **Configurable severity filtering**: the feature STScI had to `fork the project <https://ssb.stsci.edu/cdbs/temp/gcc_test/opus/hlp/opus_fitsverify.html>`_ to get is now a single option
- **Buffer overflow fixes**: all 222 ``sprintf`` calls replaced with ``snprintf``
- **Memory leak fixes**: fixed ``close_hdu()`` logic bug (arrays never freed since ~1998) and leaks on the error-abort path
- **Dead code removed**: ``WEBTOOL``, ``STANDALONE``, ``ERR2OUT`` preprocessor conditionals and HEADAS/PIL stubs eliminated

New Features
------------

- **Python package**: ``result = fitsverify.verify("myfile.fits")`` --- one line, structured results
- **Accepts anything**: filenames, bytes in memory, open file objects, astropy HDULists
- **Data-aware fix hints**: actionable suggestions that inspect the actual data to confirm the root cause --- e.g., reading row 1 to determine whether a TFORM or TDISPn mismatch should be fixed by changing the data type or the display format
- **Parallel verification**: verify thousands of files across multiple CPU cores via ``verify_parallel()``
- **In-memory verification**: validate data from web uploads or generated data without writing to disk
- **Output callbacks**: register a function to receive structured messages instead of ``FILE*`` output
- **C library**: embed validation directly in C/C++ programs via ``libfitsverify``
- **CLI enhancements**: drop-in replacement with ``--json`` output, ``--fix-hints``, ``--explain``, and ``@filelist.txt`` batch processing
- **FITS Standard 4.0 compliance**: time WCS keywords (``TIMESYS``, ``MJDREF``, etc.), column limit keywords (``TLMINn``, ``TLMAXn``, etc.), deprecation warnings for Random Groups and legacy XTENSION values


.. toctree::
   :maxdepth: 2
   :caption: Getting Started

   installation
   quickstart

.. toctree::
   :maxdepth: 2
   :caption: API Reference

   python-api
   c-api
   cli

.. toctree::
   :maxdepth: 2
   :caption: Reference

   error-codes
   changelog


Indices and tables
==================

* :ref:`genindex`
* :ref:`search`
