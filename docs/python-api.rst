Python API Reference
====================

.. module:: fitsverify

The ``fitsverify`` Python package provides a high-level API for verifying FITS
files.  It wraps the libfitsverify C library via `cffi
<https://cffi.readthedocs.io/>`_.


Verification Functions
----------------------

.. autofunction:: verify

.. autofunction:: verify_all

.. autofunction:: verify_parallel


Result Objects
--------------

.. autoclass:: VerificationResult
   :members:
   :undoc-members:

.. autoclass:: Issue
   :members:
   :undoc-members:


Enumerations
------------

.. autoclass:: Severity
   :members:
   :undoc-members:


Utility Functions
-----------------

.. autofunction:: version


Thread Safety
-------------

Each call to :func:`verify` creates an independent C context with no shared
state.  However, **CFITSIO's internal error message stack is a process-global
resource and is NOT thread-safe**.

The Python module handles this automatically:

- All verification calls are serialized with a module-level ``threading.Lock``.
- For true parallelism, use :func:`verify_parallel`, which uses
  ``multiprocessing.Pool`` to run each file in a separate process.

If you need to call :func:`verify` from multiple threads, the lock ensures
safety but eliminates parallelism.  If you need parallel verification, use
:func:`verify_parallel` instead.


Input Types
-----------

The :func:`verify` function accepts several input types:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Input type
     - Behavior
   * - ``str`` or ``pathlib.Path``
     - Treated as a file path.  Passed to CFITSIO's file opener, which supports
       `extended filename syntax <https://heasarc.gsfc.nasa.gov/docs/software/fitsio/c/c_user/node79.html>`_.
   * - ``bytes`` or ``bytearray``
     - Verified in memory via ``fits_open_memfile()``.  No disk I/O.
   * - ``memoryview``
     - Converted to ``bytes``, then verified in memory.
   * - File-like object (``.read()``)
     - Read into bytes, then verified in memory.  Must be opened in binary mode.
   * - ``astropy.io.fits.HDUList``
     - Serialized to an in-memory buffer (with ``output_verify='ignore'`` to
       pass through invalid data), then verified.
