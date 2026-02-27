"""
Core implementation of the fitsverify Python API.

Uses cffi to call libfitsverify's C functions. All verification is
serialized with a module-level lock because CFITSIO's error message
stack is not thread-safe.
"""
import enum
import json
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

    __slots__ = ('severity', 'code', 'hdu', 'message', 'fix_hint', 'explain')

    def __init__(self, severity, code, hdu, message, fix_hint=None, explain=None):
        self.severity = Severity(severity)
        self.code = code
        self.hdu = hdu
        self.message = message
        self.fix_hint = fix_hint
        self.explain = explain

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

    def to_dict(self):
        """Return a dict representation of this issue."""
        d = {
            'severity': self.severity.name,
            'code': self.code,
            'hdu': self.hdu,
            'message': self.message,
        }
        if self.fix_hint is not None:
            d['fix_hint'] = self.fix_hint
        if self.explain is not None:
            d['explain'] = self.explain
        return d


class VerificationResult:
    """Result of verifying a FITS file or memory buffer.

    Attributes:
        num_errors: Number of errors found.
        num_warnings: Number of warnings found.
        num_hdus: Number of HDUs processed.
        aborted: True if verification was aborted (e.g., >200 errors).
        issues: List of all Issue objects (errors + warnings + info).
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

    def to_dict(self):
        """Return a dict representation of this result."""
        return {
            'is_valid': self.is_valid,
            'num_errors': self.num_errors,
            'num_warnings': self.num_warnings,
            'num_hdus': self.num_hdus,
            'aborted': self.aborted,
            'messages': [i.to_dict() for i in self.issues],
        }

    def to_json(self, **kwargs):
        """Return a JSON string representation of this result.

        Parameters
        ----------
        **kwargs
            Passed to json.dumps (e.g. indent=2).
        """
        return json.dumps(self.to_dict(), **kwargs)


def _collect_messages(ctx):
    """Set up a callback on ctx that collects messages into a list.

    Returns the list that will be populated during verification.
    """
    messages = []

    @ffi.def_extern()
    def _python_output_callback(msg, userdata):
        fix_hint = None
        explain = None
        if msg.fix_hint != ffi.NULL:
            fix_hint = ffi.string(msg.fix_hint).decode('utf-8', errors='replace')
        if msg.explain != ffi.NULL:
            explain = ffi.string(msg.explain).decode('utf-8', errors='replace')
        messages.append(Issue(
            severity=msg.severity,
            code=msg.code,
            hdu=msg.hdu_num,
            message=ffi.string(msg.text).decode('utf-8', errors='replace'),
            fix_hint=fix_hint,
            explain=explain,
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


def _read_input_to_bytes(input):
    """Convert input to a (mode, data) tuple.

    Returns
    -------
    ('file', str) for file paths, ('memory', bytes) for buffers.
    """
    # str or Path → file path
    if isinstance(input, (str, Path)):
        return ('file', str(input))

    # bytes / bytearray / memoryview → memory buffer
    if isinstance(input, (bytes, bytearray, memoryview)):
        if isinstance(input, memoryview):
            input = bytes(input)
        return ('memory', input)

    # astropy HDUList → serialize to memory
    try:
        from astropy.io.fits import HDUList
        if isinstance(input, HDUList):
            import io
            buf = io.BytesIO()
            input.writeto(buf, output_verify='ignore')
            return ('memory', buf.getvalue())
    except ImportError:
        pass

    # file-like object with .read()
    if hasattr(input, 'read'):
        data = input.read()
        if isinstance(data, str):
            raise TypeError(
                "file-like object must be opened in binary mode")
        return ('memory', bytes(data))

    raise TypeError(
        f"input must be str, Path, bytes, bytearray, file-like, "
        f"or astropy HDUList, got {type(input).__name__}")


def verify(input, *, testdata=True, testcsum=True, testfill=True,
           heasarc=True, hierarch=False, err_report=0,
           fix_hints=False, explain=False):
    """Verify a FITS file or memory buffer for standards compliance.

    Parameters
    ----------
    input : str, Path, bytes, file-like, or astropy HDUList
        Path to a FITS file, bytes/bytearray/memoryview containing
        FITS data, a binary file-like object with .read(), or an
        astropy HDUList (serialized to memory automatically).
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
    fix_hints : bool
        Attach short fix suggestions to each error/warning (default False).
    explain : bool
        Attach detailed explanations to each error/warning (default False).

    Returns
    -------
    VerificationResult
        Object with .is_valid, .num_errors, .num_warnings, .errors,
        .warnings, .issues, .text_report, .to_dict(), .to_json(), etc.
        When fix_hints or explain are enabled, Issue objects will have
        .fix_hint and/or .explain attributes populated.

    Examples
    --------
    >>> result = fitsverify.verify("myfile.fits")
    >>> if not result.is_valid:
    ...     for err in result.errors:
    ...         print(err.message)

    >>> result = fitsverify.verify("myfile.fits", fix_hints=True)
    >>> for err in result.errors:
    ...     if err.fix_hint:
    ...         print(f"{err.message} -> {err.fix_hint}")
    """
    mode, data = _read_input_to_bytes(input)

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
        lib.fv_set_option(ctx, lib.FV_OPT_FIX_HINTS, int(fix_hints))
        lib.fv_set_option(ctx, lib.FV_OPT_EXPLAIN, int(explain))
        lib.fv_set_option(ctx, lib.FV_OPT_PRSTAT, 1)
        lib.fv_set_option(ctx, lib.FV_OPT_PRHEAD, 0)

        # Set up message collection
        messages = _collect_messages(ctx)

        result = ffi.new("fv_result *")

        if mode == 'file':
            path_bytes = data.encode('utf-8')
            with _cfitsio_lock:
                vfstatus = lib.fv_verify_file(
                    ctx, path_bytes, ffi.NULL, result)
        else:
            buf = ffi.from_buffer(data)
            with _cfitsio_lock:
                vfstatus = lib.fv_verify_memory(
                    ctx, buf, len(data), ffi.NULL, ffi.NULL, result)

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


class _VerifyWorker:
    """Picklable callable for multiprocessing pool workers."""

    def __init__(self, kwargs):
        self.kwargs = kwargs

    def __call__(self, path):
        return verify(path, **self.kwargs)


def verify_parallel(inputs, *, max_workers=None, **kwargs):
    """Verify multiple FITS files in parallel using separate processes.

    Uses multiprocessing to bypass CFITSIO's thread-safety limitation.
    Each file is verified in a separate process with its own CFITSIO
    instance.

    Parameters
    ----------
    inputs : iterable of str or Path
        Paths to FITS files. Memory buffers are not supported
        (they cannot be efficiently serialized across processes).
    max_workers : int, optional
        Maximum number of worker processes. Defaults to the number
        of CPUs.
    **kwargs
        Options passed to verify() (testdata, testcsum, etc.).

    Returns
    -------
    list of VerificationResult
        One result per input, in the same order.
    """
    from multiprocessing import Pool

    inputs = [str(p) for p in inputs]
    worker = _VerifyWorker(kwargs)

    with Pool(processes=max_workers) as pool:
        return pool.map(worker, inputs)


def version():
    """Return the libfitsverify version string."""
    return ffi.string(lib.fv_version()).decode('ascii')
