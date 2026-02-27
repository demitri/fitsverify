# libfitsverify

**Embeddable, reentrant FITS standards-compliance validator with Python bindings.**

libfitsverify takes NASA/HEASARC's [`fitsverify`](https://heasarc.gsfc.nasa.gov/docs/software/ftools/fitsverify/) — the standard tool for verifying FITS files since 1994 — and refactors it into a modern library that can be embedded in pipelines, called from Python, and run on data in memory. All ~60 original validation checks are preserved exactly.

## Quick Start

### Python

```bash
cd python/
pip install .
```

```python
import fitsverify

result = fitsverify.verify("myfile.fits")
print(result)
# VerificationResult(VALID, errors=0, warnings=0, hdus=2)

if not result.is_valid:
    for err in result.errors:
        print(err.message)
```

`verify()` accepts filenames, `bytes`, file-like objects, and [astropy](https://www.astropy.org/) `HDUList` objects.

### Fix Hints

Every error and warning can carry a context-aware fix suggestion that names the specific keyword, HDU, and FITS Standard section involved. Hints inspect the actual data to confirm the likely cause:

```python
result = fitsverify.verify("myfile.fits", fix_hints=True, explain=True)

for issue in result.errors:
    print(issue.message)
    if issue.fix_hint:
        print(f"  Fix: {issue.fix_hint}")
    if issue.explain:
        print(f"  Why: {issue.explain}")

# Example output:
# *** Error:   Keyword #75, TDISP11: Format L1 cannot be used for TFORM "A1".
#   Fix: Row 1 contains 'T'. Change TFORM11 to '1L' to declare the column
#        as logical.
#   Why: Column 11 is declared as character data (TFORM 'A1') but has a
#        logical display format ('L1'). Row 1 contains 'T', confirming this
#        holds logical values stored as text. See FITS Standard Section 7.3.3.
```

The hint reads the first data row, sees `'T'`, and confirms the column holds logical values stored as text -- so it recommends fixing the TFORM rather than the TDISP. Without data inspection, it would have to hedge with "either change X or change Y".

Also available from the CLI:

```bash
fitsverify --fix-hints --explain myfile.fits   # text output
fitsverify --json --fix-hints myfile.fits       # JSON output with fix_hint fields
```

### CLI

```bash
fitsverify myfile.fits                     # standard verification
fitsverify -q *.fits                       # quiet: one-line pass/fail per file
fitsverify --json myfile.fits              # JSON output
fitsverify --fix-hints --explain myfile.fits  # with fix hints
```

### C Library

```c
#include "fitsverify.h"

fv_context *ctx = fv_context_new();
fv_result result;

fv_verify_file(ctx, "myfile.fits", stdout, &result);
printf("errors: %d, warnings: %d\n", result.num_errors, result.num_warnings);

fv_context_free(ctx);
```

## Features

- **Python package**: `fitsverify.verify()` — one line, structured results
- **Accepts anything**: filenames, bytes in memory, open file objects, astropy HDULists
- **Machine-readable output**: every error has a unique code, severity level, and HDU number
- **Context-aware fix hints**: actionable suggestions that name the specific keyword, HDU, and FITS Standard section involved
- **Parallel verification**: `verify_parallel()` across multiple CPU cores via multiprocessing
- **In-memory verification**: validate FITS data from web uploads or generated data without writing to disk
- **Severity filtering**: filter by error severity (the feature STScI had to fork the project to get)
- **Safe for embedding**: never calls `exit()`, never writes to global state, fully reentrant
- **JSON output**: machine-readable JSON from the CLI
- **FITS Standard 4.0 compliance**: time WCS keywords, column limit keywords, deprecation warnings for Random Groups and legacy XTENSION values
- **135 automated tests** (91 C, 44 Python), zero compiler warnings

## Installation

### Prerequisites

[CFITSIO](https://heasarc.gsfc.nasa.gov/fitsio/) 4.x must be installed. See the [installation guide](docs/installation.rst) for platform-specific instructions.

### Python Package

```bash
cd python/
pip install .
```

The build compiles the C sources directly into the Python extension module — no separate C library installation needed. CFITSIO is found via `CFITSIO_DIR` environment variable, `pkg-config`, or common system paths.

### C Library and CLI

```bash
mkdir build && cd build
cmake ..
make
```

This produces `libfitsverify.a` (static library) and `fitsverify` (CLI executable).

## Documentation

Full documentation is available in the [`docs/`](docs/) directory:

- [Installation](docs/installation.rst)
- [Quick Start](docs/quickstart.rst)
- [Python API Reference](docs/python-api.rst)
- [C API Reference](docs/c-api.rst)
- [CLI Reference](docs/cli.rst)
- [Error Codes Reference](docs/error-codes.rst) — all ~70 error codes with fix hints
- [Changelog](docs/changelog.rst)

## Background

`fitsverify` is the de facto standard for verifying that FITS files conform to the [FITS Standard](https://fits.gsfc.nasa.gov/fits_standard.html). Maintained by the HEASARC at NASA/GSFC, it has been in continuous use since 1994. However, it could only be run as a standalone command-line program — it couldn't be embedded as a library.

This limitation forced institutions like STScI to [fork the project](https://ssb.stsci.edu/cdbs/temp/gcc_test/opus/hlp/opus_fitsverify.html) to add even simple features like severity filtering for the Hubble Space Telescope pipeline.

libfitsverify solves this by refactoring the scaffolding (state management, error handling, output) while preserving all the original validation logic. The result is an embeddable library with a modern API that eliminates the need for forks.

## What Changed from the Original

| Aspect | Original fitsverify | libfitsverify |
|--------|-------------------|---------------|
| State management | Global/static variables | Context struct per verification |
| Error handling | `exit(1)` on threshold | Return code, never terminates |
| Output | `FILE*` streams only | Callbacks with structured messages |
| Diagnostics | Text strings | Error code + severity + HDU + text + hints |
| File access | Disk files only | Disk files + memory buffers |
| Concurrency | Impossible (global state) | Thread-safe (independent contexts) |
| Extensibility | Fork required | Callbacks, configurable options |

## Bug Fixes

During the refactoring, several bugs in the original code were discovered and fixed:

- **Memory leak in `close_hdu()`**: `&&` where `||` was needed — arrays were never freed (present since ~1998)
- **Memory leaks on abort**: `setjmp`/`longjmp` skipped cleanup; replaced with flag-based mechanism
- **222 buffer overflow risks**: all `sprintf` replaced with `snprintf`
- **Dead code**: removed `ERR2OUT`, `WEBTOOL`, `STANDALONE` preprocessor conditionals

## License

BSD 3-Clause. See [LICENSE](LICENSE).

This project is derived from [fitsverify](https://heasarc.gsfc.nasa.gov/docs/software/ftools/fitsverify/) by the HEASARC at NASA/GSFC. The original code was written by William Pence and Ning Gan.
