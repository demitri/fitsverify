CLI Reference
=============

The ``fitsverify`` command-line tool is a thin wrapper around libfitsverify.
It is a drop-in replacement for the original HEASARC ``fitsverify`` with
additional features.


Synopsis
--------

::

    fitsverify [options] filename ...
    fitsverify [options] @filelist.txt


Options
-------

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Flag
     - Description
   * - ``-l``
     - List all header keywords in each HDU
   * - ``-H``
     - Check ESO HIERARCH keywords
   * - ``-q``
     - Quiet mode: print one-line pass/fail summary per file
   * - ``-e``
     - Only report error conditions (suppress warnings)
   * - ``-s``
     - Only report severe error conditions
   * - ``--json``
     - Output results as JSON
   * - ``--fix-hints``
     - Show context-aware fix suggestions (names keyword, HDU, mandatory keyword list)
   * - ``--explain``
     - Show detailed explanations with FITS Standard section references
   * - ``-h``
     - Print detailed help text


Exit Code
---------

``fitsverify`` exits with a status equal to ``min(errors + warnings, 255)``.
An exit code of 0 means the file(s) passed all checks.


File Lists
----------

Prefix a filename with ``@`` to read a list of FITS file paths from a text
file, one per line::

    fitsverify @my_files.txt

Blank lines are skipped.  This is compatible with the original ``fitsverify``
behavior.


Examples
--------

**Basic verification:**

::

    $ fitsverify myfile.fits

**Quiet mode for batch checking:**

::

    $ fitsverify -q *.fits
    verification OK: good_file.fits
    verification FAILED: bad_file.fits      , 1 warnings and 2 errors

Fix Hints and Explanations
--------------------------

The ``--fix-hints`` flag adds context-aware fix suggestions after each error or
warning.  Hints name the specific keyword, HDU number, and mandatory keyword
list involved --- not just generic advice.

::

    $ fitsverify --fix-hints bad_file.fits
    ...
    *** Warning: The HDU 3 and 2 have identical type/name/version
        Fix: Keyword 'EXTNAME' in HDU 3: Give each HDU a unique combination
             of EXTNAME, EXTVER, and EXTLEVEL.
    ...
    *** Error:   Mandatory keyword BITPIX is not present.
        Fix: Add the keyword 'BITPIX' to the header of HDU 2. The mandatory
             keywords for a binary table in order are: XTENSION, BITPIX, NAXIS,
             NAXIS1, NAXIS2, PCOUNT, GCOUNT, TFIELDS, END.
    ...

Add ``--explain`` to also show detailed explanations with FITS Standard section
references:

::

    $ fitsverify --fix-hints --explain bad_file.fits
    ...
    *** Error:   Mandatory keyword BITPIX is not present.
        Fix: Add the keyword 'BITPIX' to the header of HDU 2. The mandatory
             keywords for a binary table in order are: XTENSION, BITPIX, NAXIS,
             NAXIS1, NAXIS2, PCOUNT, GCOUNT, TFIELDS, END.
        Explanation: 'BITPIX' specifies the number of bits per data element
             (e.g., 8 for bytes, 16 for short integers, -32 for single-precision
             floats). Without it, FITS readers cannot interpret the binary table.
             See FITS Standard Section 4.4.1.1.
    ...

Both flags also work with ``--json``.  When enabled, each message object
includes ``fix_hint`` and/or ``explain`` fields:

::

    $ fitsverify --json --fix-hints --explain myfile.fits

.. code-block:: json

   {
     "fitsverify_version": "1.0.0",
     "cfitsio_version": "4.060",
     "files": [
       {
         "file": "myfile.fits",
         "messages": [
           {
             "severity": "warning",
             "code": 502,
             "hdu": 3,
             "text": "*** Warning: The HDU 3 and 2 have identical type/name/version",
             "fix_hint": "Keyword 'EXTNAME' in HDU 3: Give each HDU a unique combination of EXTNAME, EXTVER, and EXTLEVEL.",
             "explain": "Multiple HDUs share the same EXTNAME, EXTVER, and EXTLEVEL values. Each HDU should be uniquely addressable by name and version. See FITS Standard Section 4.4.1.1."
           }
         ],
         "num_errors": 0,
         "num_warnings": 1,
         "num_hdus": 3,
         "aborted": false
       }
     ],
     "total_errors": 0,
     "total_warnings": 1
   }


JSON Output
-----------

The ``--json`` flag produces machine-readable output.  Combine with
``--fix-hints`` and ``--explain`` to include those fields (see above).

Each message object contains:

- ``severity``: ``"info"``, ``"warning"``, ``"error"``, or ``"severe"``
- ``code``: integer error code (see :doc:`error-codes`)
- ``hdu``: HDU number (0 = file-level)
- ``text``: the message text
- ``fix_hint``: fix suggestion (only present if ``--fix-hints`` is used)
- ``explain``: explanation (only present if ``--explain`` is used)


Severity Filtering
------------------

The ``-e`` and ``-s`` flags control which diagnostics are reported:

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Flag
     - Effect
   * - (none)
     - Report all errors and warnings
   * - ``-e``
     - Report errors only; suppress warnings
   * - ``-s``
     - Report only severe errors (structural/fatal problems)

This is the feature that STScI's OPUS pipeline had to fork ``fitsverify`` to
implement.  It is now a single flag.
