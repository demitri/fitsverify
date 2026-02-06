"""
Core implementation of the fitsverify Python API.

Uses cffi to call libfitsverify's C functions. All verification is
serialized with a module-level lock because CFITSIO's error message
stack is not thread-safe.
"""
import enum
import os
import threading
from pathlib import Path

from fitsverify._fitsverify_cffi import ffi, lib

# Module-level lock for CFITSIO thread safety
_cfitsio_lock = threading.Lock()


class Severity(enum.IntEnum):
    """Message severity level."""
    INFO = 0
    WARNING = 1
    ERROR = 2
    SEVERE = 3


class Issue:
    """A single diagnostic message from verification."""

    __slots__ = ('severity', 'code', 'hdu', 'message')

    def __init__(self, severity, code, hdu, message):
        self.severity = Severity(severity)
        self.code = code
        self.hdu = hdu
        self.message = message

    def __repr__(self):
        return (f"Issue(severity={self.severity.name}, code={self.code}, "
                f"hdu={self.hdu}, message={self.message!r})")

    @property
    def is_error(self):
        """True if this is an error (ERROR or SEVERE)."""
        return self.severity >= Severity.ERROR

    @property
    def is_warning(self):
        """True if this is a warning."""
        return self.severity == Severity.WARNING


class VerificationResult:
    """Result of verifying a FITS file or memory buffer.

    Attributes:
        is_valid: True if no errors or warnings were found.
        num_errors: Number of errors found.
        num_warnings: Number of warnings found.
        num_hdus: Number of HDUs processed.
        aborted: True if verification was aborted (e.g., >200 errors).
        issues: List of all Issue objects (errors + warnings + info).
        errors: List of Issue objects with severity ERROR or SEVERE.
        warnings: List of Issue objects with severity WARNING.
    """

    def __init__(self, num_errors, num_warnings, num_hdus, aborted, issues):
        self.num_errors = num_errors
        self.num_warnings = num_warnings
        self.num_hdus = num_hdus
        self.aborted = aborted
        self.issues = issues

    @property
    def is_valid(self):
        """True if no errors or warnings were found."""
        return self.num_errors == 0 and self.num_warnings == 0

    @property
    def errors(self):
        """List of error issues (severity ERROR or SEVERE)."""
        return [i for i in self.issues if i.is_error]

    @property
    def warnings(self):
        """List of warning issues."""
        return [i for i in self.issues if i.is_warning]

    @property
    def text_report(self):
        """Full text report (all messages joined with newlines)."""
        return '\n'.join(i.message for i in self.issues)

    def __repr__(self):
        status = "VALID" if self.is_valid else "INVALID"
        return (f"VerificationResult({status}, "
                f"errors={self.num_errors}, warnings={self.num_warnings}, "
                f"hdus={self.num_hdus})")

    def __bool__(self):
        """True if the file is valid (no errors or warnings)."""
        return self.is_valid


def _collect_messages(ctx):
    """Set up a callback on ctx that collects messages into a list.

    Returns the list that will be populated during verification.
    """
    messages = []

    @ffi.def_extern()
    def _python_output_callback(msg, userdata):
        messages.append(Issue(
            severity=msg.severity,
            code=msg.code,
            hdu=msg.hdu_num,
            message=ffi.string(msg.text).decode('utf-8', errors='replace'),
        ))

    lib.fv_set_output(ctx, lib._python_output_callback, ffi.NULL)
    return messages


def _make_result(result_struct, vfstatus, messages):
    """Build a VerificationResult from C result and collected messages."""
    if vfstatus:
        num_errors = 1
        num_warnings = 0
        aborted = True
    else:
        num_errors = result_struct.num_errors
        num_warnings = result_struct.num_warnings
        aborted = bool(result_struct.aborted)

    return VerificationResult(
        num_errors=num_errors,
        num_warnings=num_warnings,
        num_hdus=result_struct.num_hdus,
        aborted=aborted,
        issues=messages,
    )


def verify(input, *, testdata=True, testcsum=True, testfill=True,
           heasarc=True, hierarch=False, err_report=0):
    """Verify a FITS file or memory buffer for standards compliance.

    Parameters
    ----------
    input : str, Path, or bytes
        Path to a FITS file, or bytes containing FITS data.
    testdata : bool
        Test data values (default True).
    testcsum : bool
        Test checksum (default True).
    testfill : bool
        Test fill areas (default True).
    heasarc : bool
        Check HEASARC conventions (default True).
    hierarch : bool
        Test ESO HIERARCH keywords (default False).
    err_report : int
        0 = all messages, 1 = errors only, 2 = severe only.

    Returns
    -------
    VerificationResult
        Object with .is_valid, .num_errors, .num_warnings, .errors,
        .warnings, .issues, .text_report, etc.

    Examples
    --------
    >>> result = fitsverify.verify("myfile.fits")
    >>> if not result.is_valid:
    ...     for err in result.errors:
    ...         print(err.message)
    """
    ctx = lib.fv_context_new()
    if ctx == ffi.NULL:
        raise MemoryError("Failed to allocate fv_context")

    try:
        # Configure options
        lib.fv_set_option(ctx, lib.FV_OPT_TESTDATA, int(testdata))
        lib.fv_set_option(ctx, lib.FV_OPT_TESTCSUM, int(testcsum))
        lib.fv_set_option(ctx, lib.FV_OPT_TESTFILL, int(testfill))
        lib.fv_set_option(ctx, lib.FV_OPT_HEASARC_CONV, int(heasarc))
        lib.fv_set_option(ctx, lib.FV_OPT_TESTHIERARCH, int(hierarch))
        lib.fv_set_option(ctx, lib.FV_OPT_ERR_REPORT, int(err_report))
        lib.fv_set_option(ctx, lib.FV_OPT_PRSTAT, 1)
        lib.fv_set_option(ctx, lib.FV_OPT_PRHEAD, 0)

        # Set up message collection
        messages = _collect_messages(ctx)

        result = ffi.new("fv_result *")

        if isinstance(input, (str, Path)):
            # File path
            path = str(input)
            path_bytes = path.encode('utf-8')

            with _cfitsio_lock:
                vfstatus = lib.fv_verify_file(
                    ctx, path_bytes, ffi.NULL, result)

        elif isinstance(input, (bytes, bytearray, memoryview)):
            # Memory buffer
            if isinstance(input, memoryview):
                input = bytes(input)
            buf = ffi.from_buffer(input)

            with _cfitsio_lock:
                vfstatus = lib.fv_verify_memory(
                    ctx, buf, len(input), ffi.NULL, ffi.NULL, result)

        else:
            raise TypeError(
                f"input must be str, Path, bytes, or bytearray, "
                f"got {type(input).__name__}")

        return _make_result(result, vfstatus, messages)

    finally:
        lib.fv_context_free(ctx)


def verify_all(inputs, **kwargs):
    """Verify multiple FITS files.

    Parameters
    ----------
    inputs : iterable of str, Path, or bytes
        Paths to FITS files or bytes containing FITS data.
    **kwargs
        Options passed to verify().

    Returns
    -------
    list of VerificationResult
        One result per input, in the same order.

    Notes
    -----
    Due to CFITSIO's thread-safety limitations, files are verified
    sequentially. For true parallelism, use multiprocessing.
    """
    return [verify(inp, **kwargs) for inp in inputs]


def version():
    """Return the libfitsverify version string."""
    return ffi.string(lib.fv_version()).decode('ascii')
