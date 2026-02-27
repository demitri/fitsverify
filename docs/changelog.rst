Changelog
=========

Version 1.1.0 (2026-02-06)
---------------------------

FITS Standard 4.0 compliance updates.  These are low-risk, backward-compatible
improvements — FITS 4.0 is backward-compatible with earlier versions.

**FITS 4.0 Keyword Recognition**

- Recognize column limit keywords ``TLMINn``, ``TLMAXn``, ``TDMINn``, ``TDMAXn``
  as reserved floating-point indexed keywords in tables (no longer flagged
  as unknown in image HDUs)
- Recognize time WCS keywords from WCS Paper IV: ``TIMESYS`` (with allowed-value
  validation), ``MJDREF``, ``JDREF``, ``TSTART``, ``TSTOP``, ``TIMSYER``,
  ``TIMRDER``, ``TIMEDEL``, ``TIMEOFFS``, ``DATEREF``, ``TIMEUNIT``

**New Warnings**

- Warn when Random Groups convention is used (deprecated since FITS Standard
  Version 1) — code ``FV_WARN_RANDOM_GROUPS`` (518)
- Warn on legacy/non-standard XTENSION values (``A3DTABLE``, ``IUEIMAGE``,
  ``FOREIGN``, ``DUMP``) — code ``FV_WARN_LEGACY_XTENSION`` (519)

**Display Improvements**

- Recognize ``SBYTE_IMG`` (8-bit signed byte) and ``ULONGLONG_IMG`` (64-bit
  unsigned integer) pixel types in HDU summary display

**Hints**

- **Data-aware TDISP hints**: when a TDISPn/TFORMn mismatch is detected,
  the hint reads the first data row to confirm the likely cause and
  recommends fixing the root cause (e.g., "Row 1 contains 'T'. Change
  TFORM11 to '1L' to declare the column as logical.")
- **Definitive wrong-type hints**: when a keyword value is entered as a
  quoted string but should be numeric, the hint says "Remove the quotes"
  instead of hedging with "If the value is a string..."
- **Call-site hint infrastructure**: ``FV_HINT_SET_FIX`` and
  ``FV_HINT_SET_EXPLAIN`` macros allow validation code to provide specific
  hints that ``fv_generate_hint()`` preserves instead of overwriting
- Fix hints and explanations for codes 518 and 519
- Keyword purpose and FITS Standard section references for ``TIMESYS``,
  ``MJDREF``, ``DATEREF``, ``TIMEUNIT``

Version 1.0.0 (2026-02-06)
---------------------------

Initial release of libfitsverify.  This is a complete refactoring of NASA/HEASARC's
``fitsverify`` 4.20 into an embeddable, reentrant library with Python bindings.

**C Library (Phase 1)**

- Context-based architecture: all state held in opaque ``fv_context`` struct, no
  globals
- Removed all ``exit()`` calls --- the library never terminates the host process
- Eliminated all global and file-static variables (threaded through context)
- Output callback system: structured ``fv_message`` delivery via function pointer
- In-memory verification via ``fits_open_memfile()``
- Structured error codes: ~70 unique ``fv_error_code`` values covering all
  diagnostics
- Thread-safe design (requires mutex around CFITSIO calls due to CFITSIO's
  global error stack)

**Python Package (Phase 2)**

- ``fitsverify.verify()`` accepts str/Path/bytes/bytearray/memoryview/file-like/astropy
  HDUList
- ``VerificationResult`` with ``.is_valid``, ``.errors``, ``.warnings``,
  ``.issues``, ``.text_report``, ``.to_dict()``, ``.to_json()``
- ``Issue`` objects with severity enum, error code, HDU number, message text
- ``verify_parallel()`` for true parallelism via ``multiprocessing``
- Thread safety via module-level lock
- cffi API/out-of-line mode --- compiles C sources directly into extension module

**CLI**

- Drop-in replacement for original ``fitsverify``
- New flags: ``-s`` (severe only), ``--json`` (JSON output), ``--fix-hints``,
  ``--explain``
- ``@filelist.txt`` support for batch processing

**Bug Fixes and Code Quality (Phase 3)**

- Replaced all 222 ``sprintf`` calls with ``snprintf`` (buffer overflow prevention)
- Removed dead preprocessor conditionals (``ERR2OUT``, ``WEBTOOL``, ``STANDALONE``)
- Fixed ``total_err=1`` initialization bug (dead sentinel, changed to 0)
- Fixed ``cont_fmt`` buffer bounds in ``print_fmt()``
- Replaced ``setjmp``/``longjmp`` abort with flag-based mechanism (fixes memory
  leaks on MAXERRORS path, eliminates undefined behavior)
- Fixed ``close_hdu()`` logic bug: ``&&`` changed to ``||`` (arrays were never
  freed --- memory leak since 1998)

**Fix Hints and Explain Mode (Phase 4)**

- ``fix_hint`` and ``explain`` fields on ``fv_message``
- Static lookup table with hints and explanations for all ~70 error codes
- **Context-aware hint generation**: hints include the specific keyword name,
  HDU number, HDU type, and FITS Standard section reference when available
  (e.g. "Add the keyword 'BITPIX' to the header of HDU 2. The mandatory
  keywords for a binary table in order are: XTENSION, BITPIX, ...")
- Helper functions for keyword purpose descriptions and FITS Standard section
  references covering ~30 keywords
- Automatic fallback to static hints when no runtime context is available
- ``FV_OPT_FIX_HINTS`` and ``FV_OPT_EXPLAIN`` options
- CLI: ``--fix-hints`` and ``--explain`` flags
- Python: ``fix_hints=`` and ``explain=`` kwargs on ``verify()``
- ~107 call sites annotated with ``FV_HINT_SET_KEYWORD`` / ``FV_HINT_SET_COLNUM``
  across ``fvrf_head.c``, ``fvrf_key.c``, and ``fvrf_data.c``

**Test Suite**

- 91 C tests across 5 suites (library API, callbacks, abort, threading, regression)
- 44 Python tests (file, memory, file-like, astropy, hints, parallel, JSON)
- Zero compiler warnings


Prior History (Original fitsverify)
-----------------------------------

``fitsverify`` was originally written in Fortran in 1994 by the HEASARC at
NASA/GSFC.  It was rewritten in C in 1998 by Ning Gan and enhanced through 2019
(version 4.20) by William Pence and others.  libfitsverify preserves all of the
original's ~60 validation checks.
