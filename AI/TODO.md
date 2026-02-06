# fitsverify Refactoring — Task Tracker

> **Instructions for agents**: Keep this file current. Mark items `[x]` when done, add new items as discovered, note blockers. Date significant updates.

## Phase 1: C Library (`libfitsverify`)

### 1.1 Foundation: Context Struct and Build System
- [ ] Design the `fv_context` struct that will hold all state
  - Configuration flags: `prhead`, `prstat`, `testdata`, `testcsum`, `testfill`, `heasarc_conv`, `testhierarch`, `err_report`
  - Error/warning counters (replacing `static int nerrs/nwrns` in fvrf_misc.c)
  - HDU tracking array (replacing `static HduName **hduname` in fvrf_file.c)
  - Header parsing state (replacing statics in fvrf_head.c: `cards`, `ncards`, `tmpkwds`, `ttype`, `tform`, `tunit`, `curhdu`, `curtype`)
  - Data validation state (replacing statics in fvrf_data.c: `UserIter` fields, `find_bad*` flags)
  - Scratch buffers (replacing `static char errmes[256]`, `static char comm[...]`, `static char temp[...]`)
  - Session-level accumulators (replacing `long totalerr, totalwrn`)
  - Output callback function pointer and userdata
  - Result accumulator (structured errors/warnings list)
- [ ] Create CMakeLists.txt for building the library
  - Find CFITSIO 4.x via `find_package` or `pkg-config`
  - Build `libfitsverify` as both static and shared library
  - Build `fitsverify` CLI executable linking against the library
  - Install targets for library, headers, and CLI
- [ ] Create the public header `fitsverify.h` (new, distinct from original `fverify.h`)
  - `fv_context` opaque type
  - `fv_context_new()` / `fv_context_free()`
  - `fv_set_option()` / `fv_get_option()`
  - `fv_verify_file()` / `fv_verify_memory()`
  - `fv_result` struct (error/warning counts, per-HDU breakdown)
  - `fv_message` struct (error code, severity, HDU number, text)
  - Callback typedefs
  - Error code enum (`FV_ERR_MISSING_END`, `FV_WARN_DUPLICATE_EXTNAME`, etc.)
- [ ] Set up initial directory structure: `libfitsverify/`, `cli/`, copy original source as starting point

### 1.2 Remove `exit()` Calls
- [ ] Replace `exit(1)` in `wrterr()` (fvrf_misc.c:86) with error return/longjmp
- [ ] Replace `exit(1)` in `wrtferr()` (fvrf_misc.c:129) with error return/longjmp
- [ ] Replace `exit(1)` in `wrtserr()` (fvrf_misc.c:182) with error return/longjmp
- [ ] Design the abort-propagation mechanism: either `setjmp`/`longjmp` (simpler, one-shot) or return-code propagation through all callers (more invasive but cleaner)
- [ ] Audit all callers of `wrterr`/`wrtferr`/`wrtserr` to handle the "abort" return
- [ ] Ensure `close_report()` and memory cleanup still run on abort path

### 1.3 Eliminate Global State
- [ ] Move the 8 `extern` config variables into `fv_context`
  - `prhead`, `prstat`, `testdata`, `testcsum`, `testfill`, `heasarc_conv`, `testhierarch`, `err_report`
- [ ] Move `long totalerr, totalwrn` into `fv_context`
- [ ] Move `static int nerrs, nwrns` (fvrf_misc.c) into `fv_context`
- [ ] Move `static HduName **hduname`, `total_err`, `total_warn` (fvrf_file.c) into `fv_context`
- [ ] Move `static char **cards`, `ncards`, `tmpkwds`, `ttype`, `tform`, `tunit`, `curhdu`, `curtype` (fvrf_head.c) into `fv_context`
- [ ] Move `static` variables in `iterdata()` (fvrf_data.c) into `fv_context` or `UserIter`
- [ ] Move `static char cont_fmt[80]`, `save_nprompt` (fvrf_misc.c `print_fmt`) into `fv_context`
- [ ] Fix `static char errmes[256]` and `static char comm[...]` in `fverify.h` — these must not be in the header
- [ ] Thread `fv_context*` parameter through all functions (this is the bulk of the mechanical work)
- [ ] Remove `int totalhdu` global — move into context

### 1.4 Replace FILE* Output with Callbacks
- [ ] Define callback signature: `typedef void (*fv_output_fn)(void *userdata, const fv_message *msg)`
- [ ] Define `fv_message` struct: `{ int code; int severity; int hdu_num; const char *text; }`
- [ ] Refactor `wrtout()` to call the output callback
- [ ] Refactor `wrterr()` to create structured error message and call callback
- [ ] Refactor `wrtwrn()` to create structured warning message and call callback
- [ ] Refactor `wrtferr()` / `wrtserr()` similarly
- [ ] Refactor `wrtsep()`, `print_fmt()`, `print_title()`, `print_header()`, `print_summary()`, `hdus_summary()` to use callbacks
- [ ] Implement default FILE*-based callback for CLI backward compatibility
- [ ] Implement result-accumulating callback (stores messages in a list on `fv_context`)

### 1.5 Structured Error Codes
- [ ] Catalog all distinct error conditions across `fvrf_head.c`, `fvrf_data.c`, `fvrf_file.c`, `fvrf_key.c`
  - Assign each a unique enum value
  - Classify severity (severe / regular / warning) per the OPUS categorization as baseline
  - Document each code
- [ ] Assign error codes to every `wrterr()` / `wrtwrn()` call site
- [ ] Add error code to `fv_message` struct

### 1.6 In-Memory Verification
- [ ] Add `fv_verify_memory(ctx, buffer, size)` entry point
- [ ] Use `fits_open_memfile()` instead of `fits_open_diskfile()`
- [ ] Ensure no code path assumes a filename exists on disk after the file is opened
- [ ] Test with FITS data read into memory from various sources

### 1.7 Rebuild CLI
- [ ] Create new `cli/fitsverify.c` as thin wrapper around `libfitsverify`
- [ ] Support all original flags: `-l`, `-H`, `-q`, `-e`, `-h`
- [ ] Add new flags: `-s` (severe only, à la OPUS), `--json` (JSON output)
- [ ] Preserve exit code behavior: `min(errors + warnings, 255)`
- [ ] Strip out HEADAS/PIL stubs, WEBTOOL code — these are no longer needed

### 1.8 Testing
- [ ] Create/collect test FITS files: valid files, files with known errors, edge cases
- [ ] Verify that refactored library produces identical error/warning output to original on a test corpus
- [ ] Test in-memory verification path
- [ ] Test multi-threaded concurrent verification
- [ ] Test `MAXERRORS` abort path (no longer calls `exit()`)

## Phase 2: Python Package

### 2.1 C Binding Layer
- [ ] Choose binding approach: `cffi` (preferred) vs `ctypes` vs C extension module
- [ ] Create `_fitsverify_cffi.py` or equivalent binding definitions
- [ ] Wrap `fv_context_new/free`, `fv_set_option`, `fv_verify_file`, `fv_verify_memory`
- [ ] Implement callback bridge: C callback → Python callable
- [ ] Handle GIL release for verification calls (`Py_BEGIN_ALLOW_THREADS` / `Py_END_ALLOW_THREADS` if using C extension, or `cffi` `nogil` mode)

### 2.2 Pythonic API
- [ ] Design and implement `fitsverify.verify(input)` — accepts filename (str/Path), bytes, or file-like object
- [ ] Design `VerificationResult` class:
  - `.is_valid` → bool
  - `.num_errors` → int
  - `.num_warnings` → int
  - `.errors` → list of `Issue` objects
  - `.warnings` → list of `Issue` objects
  - `.hdus` → per-HDU breakdown
  - `.text_report` → original-style text output
- [ ] Design `Issue` class: `.code`, `.severity`, `.hdu`, `.message`
- [ ] Implement `fitsverify.verify_parallel(files, max_workers=N)` — true C-level parallelism
- [ ] Astropy integration: accept `astropy.io.fits.HDUList` by serializing to memory buffer
- [ ] Consider integration with `astropy.io.fits` verification (complementary, not competing)

### 2.3 Packaging
- [ ] Create `pyproject.toml` with build system (setuptools + cffi, or scikit-build-core + CMake)
- [ ] Bundle C source for building from source
- [ ] CFITSIO dependency handling: require system cfitsio, or bundle, or use astropy's bundled copy
- [ ] Build and test on Linux, macOS, Windows
- [ ] Publish to PyPI (eventually)

## Phase 3: Bug Fixes and Code Quality

### Known Bugs
- [ ] Fix `static char errmes[256]` and `static char comm[FLEN_FILENAME+6]` in `fverify.h` — creates separate copies per translation unit, wastes memory, and can cause subtle bugs if any code expects shared state through these
- [ ] Fix `total_err=1` initialization in `fvrf_file.c:3` — fragile assumption that may misreport errors
- [ ] Audit `print_fmt()` static `cont_fmt` buffer — not properly null-terminated on first use
- [ ] Review `sprintf` → `snprintf` throughout (buffer overflow risk)

### Code Quality Improvements
- [ ] Remove `#include "fitsverify.c"` pattern — use proper separate compilation
- [ ] Remove `#ifdef WEBTOOL` code — dead code path, not relevant to this project
- [ ] Remove `#ifdef ERR2OUT` — the callback system makes this unnecessary
- [ ] Remove HEADAS/PIL stubs — no longer needed with clean library API
- [ ] Audit memory management: ensure no leaks when abort path is taken (previously `exit()` made leaks moot)
- [ ] Consider FITS Standard v4.0 (2016) validation updates (new conventions for unsigned integers, long string keywords)

## Future / Nice-to-Have
- [ ] JSON output mode for CLI
- [ ] Selective HDU verification (verify only specific HDUs by index or EXTNAME)
- [ ] Configurable rule sets (disable specific checks, custom severity mappings)
- [ ] Progress callbacks for large files or batch operations
- [ ] Cancellation token for aborting mid-verification
- [ ] FITS Standard version selection (`fv_set_standard(ctx, FV_FITS_V30)` vs `FV_FITS_V40`)
- [ ] pkg-config `.pc` file for the C library
- [ ] Man page for CLI

---

## Status Log

| Date | Note |
|------|------|
| 2026-02-05 | Initial plan created. Original source analyzed. All items are open. |
