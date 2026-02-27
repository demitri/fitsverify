"""
fitsverify — Python interface to libfitsverify

Verify FITS files for standards compliance.

    >>> import fitsverify
    >>> result = fitsverify.verify("myfile.fits")
    >>> result.is_valid
    True
    >>> result.num_errors
    0

Thread safety: Each verification call uses an independent C context
with no shared state. However, CFITSIO's internal error message stack
is a process-global resource and is NOT thread-safe. Concurrent calls
from multiple threads require a mutex (provided automatically by this
module). For true parallelism, use multiprocessing.
"""

from fitsverify._core import (
    verify,
    verify_all,
    verify_parallel,
    VerificationResult,
    Issue,
    Severity,
    version,
)

__all__ = [
    'verify',
    'verify_all',
    'verify_parallel',
    'VerificationResult',
    'Issue',
    'Severity',
    'version',
]
