Quick Start
===========

This page shows the most common operations.  For full details, see the
:doc:`python-api`, :doc:`c-api`, and :doc:`cli` reference pages.


Python: Verify a File
---------------------

.. code-block:: python

   import fitsverify

   result = fitsverify.verify("myfile.fits")
   print(result)
   # VerificationResult(VALID, errors=0, warnings=0, hdus=2)

The :func:`~fitsverify.verify` function returns a
:class:`~fitsverify.VerificationResult` with all the details:

.. code-block:: python

   result.is_valid      # True if no errors or warnings
   result.num_errors    # int
   result.num_warnings  # int
   result.num_hdus      # number of HDUs processed
   result.errors        # list of Issue objects (errors only)
   result.warnings      # list of Issue objects (warnings only)
   result.issues        # list of all Issue objects
   result.text_report   # full text report as a string


Python: Inspect Errors
----------------------

Each diagnostic is an :class:`~fitsverify.Issue` object with a unique error
code, severity level, HDU number, and message text:

.. code-block:: python

   result = fitsverify.verify("bad_file.fits")

   for err in result.errors:
       print(f"HDU {err.hdu}: [{err.code}] {err.message}")
       # HDU 1: [154] *** Error:   Keyword BITPIX has wrong type ...


Python: Fix Hints
-----------------

Enable fix hints to get context-aware suggestions that name the specific keyword,
HDU, and FITS Standard section involved:

.. code-block:: python

   result = fitsverify.verify("bad_file.fits", fix_hints=True, explain=True)

   for err in result.errors:
       print(err.message)
       if err.fix_hint:
           print(f"  Fix: {err.fix_hint}")
       if err.explain:
           print(f"  Why: {err.explain}")

   # Example output:
   # *** Error:   Mandatory keyword BITPIX is not present.
   #   Fix: Add the keyword 'BITPIX' to the header of HDU 2. The mandatory
   #        keywords for a binary table in order are: XTENSION, BITPIX, NAXIS,
   #        NAXIS1, NAXIS2, PCOUNT, GCOUNT, TFIELDS, END.
   #   Why: 'BITPIX' specifies the number of bits per data element (e.g., 8
   #        for bytes, 16 for short integers, -32 for single-precision floats).
   #        Without it, FITS readers cannot interpret the binary table. See
   #        FITS Standard Section 4.4.1.1.

For some errors, hints inspect the actual data to confirm the likely cause.
For example, when a column's display format (TDISPn) doesn't match its data
type (TFORMn), the hint reads the first row to determine which keyword is
wrong:

.. code-block:: python

   # *** Error:   Keyword #75, TDISP11: Format L1 cannot be used for TFORM "A1".
   #   Fix: Row 1 contains 'T'. Change TFORM11 to '1L' to declare the column
   #        as logical.
   #   Why: Column 11 is declared as character data (TFORM 'A1') but has a
   #        logical display format ('L1'). Row 1 contains 'T', confirming this
   #        holds logical values stored as text. See FITS Standard Section 7.3.3.

Here, the hint sees that ``TDISP11 = 'L1'`` (logical display) and ``TFORM11 = 'A1'``
(character data) are inconsistent.  Rather than guessing, it reads row 1 and finds
``'T'`` --- confirming the column holds logical values stored as text.  The fix
recommends changing the TFORM (the root cause), not the TDISP.

See :doc:`error-codes` for the complete list of error codes, hints, and
explanations.


Python: Verify from Memory
---------------------------

Verify FITS data without writing to disk --- useful for web services and
generated data:

.. code-block:: python

   # From bytes
   with open("myfile.fits", "rb") as f:
       data = f.read()
   result = fitsverify.verify(data)

   # From a BytesIO object
   import io
   result = fitsverify.verify(io.BytesIO(data))

   # From an astropy HDUList
   from astropy.io import fits
   hdulist = fits.open("myfile.fits")
   result = fitsverify.verify(hdulist)


Python: Parallel Verification
------------------------------

Verify many files across CPU cores using
:func:`~fitsverify.verify_parallel`:

.. code-block:: python

   import glob
   import fitsverify

   files = glob.glob("/data/survey/*.fits")
   results = fitsverify.verify_parallel(files, max_workers=8)

   for path, result in zip(files, results):
       if not result.is_valid:
           print(f"FAIL: {path} ({result.num_errors} errors)")

This uses ``multiprocessing`` to bypass CFITSIO's thread-safety limitation.


Python: JSON Output
-------------------

.. code-block:: python

   result = fitsverify.verify("myfile.fits")

   # As a Python dict
   d = result.to_dict()

   # As a JSON string
   print(result.to_json(indent=2))


CLI: Basic Usage
----------------

::

    fitsverify myfile.fits
    fitsverify *.fits
    fitsverify @filelist.txt

**Common flags:**

::

    fitsverify -q *.fits              # quiet: one-line pass/fail per file
    fitsverify -l myfile.fits         # list all header keywords
    fitsverify -e myfile.fits         # errors only (no warnings)
    fitsverify -s myfile.fits         # severe errors only
    fitsverify --json myfile.fits     # JSON output
    fitsverify --fix-hints myfile.fits   # show fix suggestions
    fitsverify --explain myfile.fits     # show detailed explanations


C: Basic Usage
--------------

.. code-block:: c

   #include "fitsverify.h"

   int main(void)
   {
       fv_context *ctx = fv_context_new();
       fv_result result;

       fv_verify_file(ctx, "myfile.fits", stdout, &result);

       printf("errors: %d, warnings: %d\n",
              result.num_errors, result.num_warnings);

       fv_context_free(ctx);
       return result.num_errors > 0 ? 1 : 0;
   }

Compile and link::

    gcc -o myverifier myverifier.c -lfitsverify -lcfitsio -lm

See :doc:`c-api` for the complete C API reference.
