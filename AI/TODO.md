# fitsverify Refactoring — Task Tracker

> **Instructions for agents**: Keep this file current. Mark items `[x]` when done, add new items as discovered, note blockers. Date significant updates.

## Phase 1: C Library (`libfitsverify`)

### 1.1 Foundation: Context Struct and Build System
- [x] Design the `fv_context` struct that will hold all state
  - Configuration flags: `prhead`, `prstat`, `testdata`, `testcsum`, `testfill`, `heasarc_conv`, `testhierarch`, `err_report`
  - Error/warning counters (replacing `static int nerrs/nwrns` in fvrf_misc.c)
  - HDU tracking array (replacing `static HduName **hduname` in fvrf_file.c)
  - Header parsing state (replacing statics in fvrf_head.c: `cards`, `ncards`, `tmpkwds`, `ttype`, `tform`, `tunit`, `curhdu`, `curtype`)
  - Data validation state (replacing statics in fvrf_data.c: `UserIter` fields, `find_bad*` flags)
  - Scratch buffers (replacing `static char errmes[256]`, `static char comm[...]`, `static char temp[...]`)
  - Session-level accumulators (replacing `long totalerr, totalwrn`)
  - Output callback function pointer and userdata
  - Result accumulator (structured errors/warnings list)
- [x] Create CMakeLists.txt for building the library
  - Find CFITSIO 4.x via `find_package` or `pkg-config`
  - Build `libfitsverify` as both static and shared library
  - Build `fitsverify` CLI executable linking against the library
  - Install targets for library, headers, and CLI
- [x] Create the public header `fitsverify.h` (new, distinct from original `fverify.h`)
  - `fv_context` opaque type
  - `fv_context_new()` / `fv_context_free()`
  - `fv_set_option()` / `fv_get_option()`
  - `fv_verify_file()` / `fv_verify_memory()`
  - `fv_result` struct (error/warning counts, per-HDU breakdown)
  - `fv_message` struct (error code, severity, HDU number, text)
  - Callback typedefs
  - Error code enum (`FV_ERR_MISSING_END`, `FV_WARN_DUPLICATE_EXTNAME`, etc.) — deferred to 1.5
- [x] Set up initial directory structure: `libfitsverify/`, `cli/`, copy original source as starting point

### 1.2 Remove `exit()` Calls
- [x] Replace `exit(1)` in `wrterr()` (fvrf_misc.c:86) with error return/longjmp
- [x] Replace `exit(1)` in `wrtferr()` (fvrf_misc.c:129) with error return/longjmp
- [x] Replace `exit(1)` in `wrtserr()` (fvrf_misc.c:182) with error return/longjmp
- [x] Design the abort-propagation mechanism: `setjmp`/`longjmp` chosen (simpler, one-shot)
- [x] Audit all callers of `wrterr`/`wrtferr`/`wrtserr` to handle the "abort" return
- [x] Ensure `close_report()` and memory cleanup still run on abort path

### 1.3 Eliminate Global State
- [x] Move the 8 `extern` config variables into `fv_context`
  - `prhead`, `prstat`, `testdata`, `testcsum`, `testfill`, `heasarc_conv`, `testhierarch`, `err_report`
- [x] Move `long totalerr, totalwrn` into `fv_context`
- [x] Move `static int nerrs, nwrns` (fvrf_misc.c) into `fv_context`
- [x] Move `static HduName **hduname`, `total_err`, `total_warn` (fvrf_file.c) into `fv_context`
- [x] Move `static char **cards`, `ncards`, `tmpkwds`, `ttype`, `tform`, `tunit`, `curhdu`, `curtype` (fvrf_head.c) into `fv_context`
- [x] Move `static` variables in `iterdata()` (fvrf_data.c) into `fv_context` or `UserIter`
- [x] Move `static char cont_fmt[80]`, `save_nprompt` (fvrf_misc.c `print_fmt`) into `fv_context`
- [x] Fix `static char errmes[256]` and `static char comm[...]` in `fverify.h` — moved to `fv_context`
- [x] Thread `fv_context*` parameter through all functions (this is the bulk of the mechanical work)
- [x] Remove `int totalhdu` global — move into context

### 1.4 Replace FILE* Output with Callbacks
- [x] Define callback signature: `typedef void (*fv_output_fn)(const fv_message *msg, void *userdata)`
- [x] Define `fv_message` struct: `{ fv_msg_severity severity; int hdu_num; const char *text; }` (error code deferred to 1.5)
- [x] Refactor `wrtout()` to call the output callback (also added `fv_context *ctx` parameter)
- [x] Refactor `wrterr()` to create structured error message and call callback
- [x] Refactor `wrtwrn()` to create structured warning message and call callback
- [x] Refactor `wrtferr()` / `wrtserr()` similarly
- [x] Refactor `wrtsep()`, `print_fmt()` to use callbacks; `print_title()`, `print_header()`, `print_summary()`, `hdus_summary()` route through `wrtout()`/`wrtsep()` which now dispatch to callback
- [x] Default FILE*-based output preserved when no callback registered (backward compatible)
- [ ] Implement result-accumulating callback (stores messages in a list on `fv_context`) — deferred to Phase 2

### 1.5 Structured Error Codes
- [x] Catalog all distinct error conditions across `fvrf_head.c`, `fvrf_data.c`, `fvrf_file.c`, `fvrf_key.c`
  - Assign each a unique enum value
  - Classify severity (severe / regular / warning) per the OPUS categorization as baseline
  - Document each code
- [x] Assign error codes to every `wrterr()` / `wrtwrn()` / `wrtferr()` / `wrtserr()` call site (~212 sites across 5 files)
- [x] Add `fv_error_code code` field to `fv_message` struct
- [x] Add `int code` parameter to `wrterr()`, `wrtwrn()`, `wrtferr()`, `wrtserr()` signatures (compile-time verified)
- [x] Define `fv_error_code` enum with ~70 codes organized by category:
  - File/HDU structure (100–149), Mandatory keyword (150–199), Keyword format (200–249)
  - HDU-type placement (250–299), Table structure (300–349), Data validation (350–399)
  - WCS (400–419), CFITSIO (450–479), Internal (480–499), Warnings (500–599)

### 1.6 In-Memory Verification
- [x] Add `fv_verify_memory(ctx, buffer, size, label, out, result)` entry point
- [x] Use `fits_open_memfile()` instead of `fits_open_diskfile()`
- [x] Ensure no code path assumes a filename exists on disk after the file is opened
- [x] Test with FITS data read into memory (test_library_api tests 9-11)

### 1.7 Rebuild CLI
- [x] Create new `cli/fitsverify.c` as thin wrapper around `libfitsverify`
- [x] Support all original flags: `-l`, `-H`, `-q`, `-e`, `-h`
- [x] Add new flags: `-s` (severe only, à la OPUS), `--json` (JSON output)
- [x] Add `@filelist.txt` support (read filenames from text file, one per line)
- [x] Preserve exit code behavior: `min(errors + warnings, 255)`
- [x] Strip out HEADAS/PIL stubs, WEBTOOL code — these are no longer needed

### 1.8 Testing
- [x] Create/collect test FITS files: valid files, files with known errors, edge cases (`gen_test_fits.c` creates 6 test files)
- [x] Verify that refactored library produces identical error/warning output to original on a test corpus (`test_regression.sh`, 3 tests)
- [x] Test in-memory verification path (`test_library_api` tests 9-11)
- [x] Test multi-threaded concurrent verification (`test_threaded`, 7 tests)
  - Context lifecycle (create/destroy) is safe from multiple threads
  - Verification requires mutex around CFITSIO calls (global error stack is not thread-safe)
  - Documented CFITSIO threading limitation in public header and Publication.md
- [x] Test `MAXERRORS` abort path (no longer calls `exit()`) — `test_abort` + callback test #6

## Phase 2: Python Package

### 2.1 C Binding Layer
- [x] Choose binding approach: `cffi` API/out-of-line mode
- [x] Create `_build_ffi.py` with cffi definitions matching `fitsverify.h`
- [x] Wrap `fv_context_new/free`, `fv_set_option`, `fv_verify_file`, `fv_verify_memory`
- [x] Implement callback bridge: C `fv_output_fn` → Python `@ffi.def_extern()` via `extern "Python"`
- [x] GIL release: not applicable — cffi callbacks re-enter Python (require GIL), and CFITSIO lock serializes calls anyway. True parallelism via `verify_parallel()` (multiprocessing) instead.

### 2.2 Pythonic API
- [x] Design and implement `fitsverify.verify(input)` — accepts filename (str/Path), bytes, bytearray, or memoryview
- [x] Design `VerificationResult` class:
  - `.is_valid` → bool
  - `.num_errors` → int
  - `.num_warnings` → int
  - `.errors` → list of `Issue` objects (filtered: severity >= ERROR)
  - `.warnings` → list of `Issue` objects (filtered: severity == WARNING)
  - `.issues` → list of all `Issue` objects
  - `.num_hdus` → int
  - `.aborted` → bool
  - `.text_report` → all messages joined with newlines
  - `__bool__` → returns `.is_valid`
- [x] Design `Issue` class: `.code`, `.severity` (Severity enum), `.hdu`, `.message`, `.is_error`, `.is_warning`
- [x] Implement `Severity` IntEnum: INFO=0, WARNING=1, ERROR=2, SEVERE=3
- [x] Implement `fitsverify.verify_all(inputs)` — sequential batch verification
- [x] Implement `fitsverify.version()` — returns libfitsverify version string
- [x] Thread safety: module-level `threading.Lock` around all CFITSIO calls
- [x] Accept file-like objects (`.read()` into bytes, pass to `verify_memory`)
- [x] `VerificationResult.to_dict()` and `.to_json()` (match CLI `--json` schema)
- [x] `Issue.to_dict()` for structured serialization
- [x] Astropy integration: accept `astropy.io.fits.HDUList` by serializing to memory buffer (uses `output_verify='ignore'` to pass through invalid files)
- [x] `fitsverify.verify_parallel(files, max_workers=N)` via `multiprocessing.Pool` (bypasses CFITSIO thread-safety limitation)

### 2.3 Packaging
- [x] Create `pyproject.toml` with setuptools + cffi build system
- [x] Create `setup.py` for cffi-modules integration
- [x] Bundle C source: compiles libfitsverify sources directly into cffi extension module
- [x] CFITSIO dependency: auto-detection via env var (`CFITSIO_DIR`), pkg-config, or common paths
- [x] `pip install -e .` working, 23 pytest tests passing
- [ ] Build and test on Linux, macOS, Windows
- [ ] Publish to PyPI

## Phase 3: Bug Fixes and Code Quality

### Known Bugs
- [x] Fix `static char errmes[256]` and `static char comm[FLEN_FILENAME+6]` in `fverify.h` — moved to `fv_context` (Phase 1.3)
- [x] Fix `total_err=1` initialization in fv_api.c — changed to 0; the init-to-1 sentinel was never read on any code path
- [x] Audit `print_fmt()` `cont_fmt` buffer — added `nprompt > 70` cap to prevent overflow of 80-byte buffer
- [x] Replace `sprintf` → `snprintf` throughout — 222 call sites across all .c files (fvrf_head.c: 161, fvrf_key.c: 33, fvrf_data.c: 27, fvrf_file.c: 13, fv_api.c: 1, cli: 1). All now use `snprintf(buf, sizeof(buf), ...)`

### Code Quality Improvements
- [x] Remove `#include "fitsverify.c"` pattern — already absent from libfitsverify (legacy source/ only)
- [x] Remove `#ifdef WEBTOOL` code — removed 2 `#ifndef WEBTOOL` blocks in fvrf_head.c and fv_api.c
- [x] Remove `#ifdef ERR2OUT` — removed 6 blocks in fvrf_misc.c; errors now go to stderr (callback system handles programmatic use)
- [x] Remove `#ifdef STANDALONE` — removed 1 block in fvrf_head.c; kept `fits_open_diskfile` path. Removed both `-DSTANDALONE` and `-DERR2OUT` from CMakeLists.txt
- [x] Remove HEADAS/PIL stubs — already absent from libfitsverify (legacy source/ only)
- [x] Fix memory leaks on abort path — replaced `setjmp`/`longjmp` with flag-based abort (`ctx->maxerrors_reached`). Error functions now set a flag instead of jumping; the HDU loop checks the flag and breaks cleanly through normal return paths, ensuring `close_hdu()` always runs. Eliminates all memory leaks on the MAXERRORS path and removes undefined behavior from non-volatile locals across longjmp. Abort test now reports actual error count (201) instead of 1.
- [x] FITS Standard v4.0 (2016) validation updates — easy items implemented:
  - [x] Column limit keywords (TLMINn/TLMAXn/TDMINn/TDMAXn) recognized and validated
  - [x] Time WCS keywords (TIMESYS, MJDREF, JDREF, TSTART, TSTOP, etc.) recognized and validated
  - [x] 64-bit unsigned integer (ULONGLONG_IMG) and signed byte (SBYTE_IMG) display
  - [x] Random Groups deprecation warning (FV_WARN_RANDOM_GROUPS = 518)
  - [x] Legacy XTENSION values warning (FV_WARN_LEGACY_XTENSION = 519)
  - [ ] Tile-compressed image validation (ZCMPTYPE, ZBITPIX, ZNAXISn, ZTILEn, etc.) — deferred, HIGH effort
  - [ ] Tile-compressed table validation — deferred, MEDIUM effort

## Phase 4: Fix Hints and Explain Mode

Leverage the structured error codes (Phase 1.5) to provide actionable guidance. See `AI/Performance_and_Feature_Improvements.md` for full design and starter hint set.

### 4.1 Hint Infrastructure
- [x] Create static lookup table keyed by `fv_error_code` → `{ fix_hint, explain }` strings (`fv_hints.c` / `fv_hints.h`)
- [x] Add `FV_OPT_FIX_HINTS` and `FV_OPT_EXPLAIN` options to `fv_option` enum
- [x] Modify message dispatch (`dispatch_msg()` in fvrf_misc.c) to attach hint/explain text to `fv_message` when enabled
- [x] Add `fix_hint` and `explain` fields to `fv_message` (NULL when disabled)
- [x] Add `print_hints_file()` helper for FILE* output mode (prints Fix: / Explanation: lines after errors/warnings)

### 4.2 Hint Content
- [x] Write fix hints and explanations for all ~70 error codes (complete coverage of every `fv_error_code` value)
- [x] Organized as static const hint structs with O(1) switch-based lookup

### 4.3 CLI and Python Integration
- [x] CLI: add `--fix-hints` and `--explain` flags; JSON mode includes hint fields when present
- [x] Python: add `fix_hints=` and `explain=` kwargs to `verify()`; `Issue.fix_hint` and `Issue.explain` attributes; `to_dict()` / `to_json()` include hint fields when populated
- [x] Tests: 8 new C tests (33 total in test_output_callback), 10 new Python tests (44 total). All pass.

### 4.4 Context-Aware Hint Generation
- [x] Add hint context fields to `fv_context`: `hint_keyword[FLEN_KEYWORD]`, `hint_colnum`, `hint_fix_buf[1024]`, `hint_explain_buf[1024]`
- [x] Add `fv_generate_hint()` function that uses runtime context (keyword name, HDU number, HDU type) to produce specific hints instead of generic ones
- [x] Add context-setting macros: `FV_HINT_SET_KEYWORD(ctx, name)`, `FV_HINT_SET_COLNUM(ctx, col)`, `FV_HINT_CLEAR(ctx)`
- [x] Add helper functions: `fv_hdu_type_name()`, `fv_mandatory_keyword_list()`, `fv_keyword_purpose()`, `fv_keyword_section()` (~30 keywords covered)
- [x] Update `dispatch_msg()` and `print_hints_file()` to call `fv_generate_hint()` with auto-clear after consumption
- [x] Annotate ~107 call sites across `fvrf_head.c`, `fvrf_key.c`, and `fvrf_data.c` with `FV_HINT_SET_KEYWORD` / `FV_HINT_SET_COLNUM`
- [x] Automatic fallback to static hints when no runtime context is available
- [x] Initialize new fields in `fv_context_new()`
- [x] Tests: 5 new C tests (38 total in test_output_callback; 91 C tests total). All pass.
- [x] Documentation updated: all 6 RST pages + README reflect context-aware hints

## Future / Nice-to-Have
- [x] JSON output mode for CLI (done in 1.7)
- [x] Header-only fast mode (already works: set `testdata=False, testcsum=False, testfill=False` in Python, or `-e 2` in CLI)
- [x] Severity filtering (already works: `err_report=` option — 0=all, 1=errors only, 2=severe only; CLI `-s` flag)
- [ ] Selective HDU verification (verify only specific HDUs by index or EXTNAME)
- [ ] Configurable rule sets (disable specific checks, custom severity mappings)
- [ ] Progress callbacks for large files or batch operations
- [ ] Cancellation token for aborting mid-verification
- [ ] FITS Standard version selection (`fv_set_standard(ctx, FV_FITS_V30)` vs `FV_FITS_V40`)
- [ ] pkg-config `.pc` file for the C library
- [ ] Man page for CLI

## Evaluated and Rejected

Items that were proposed, evaluated, and intentionally not pursued. Documented here to avoid re-proposing them.

### In-Memory HDU Parallelism
**Proposal**: Verify individual HDUs within a single FITS file in parallel by opening N `fitsfile*` handles via `fits_open_memfile()` against the same buffer, each with its own `fv_hdu_context`.

**Why not**: CFITSIO's global error message stack is not thread-safe (discovered empirically in Phase 1.8). Every CFITSIO call that touches error reporting — which includes all the functions fitsverify uses to detect standards violations — would corrupt shared state. This is a fundamental blocker that cannot be worked around without requiring users to build CFITSIO with `--enable-reentrant`. Even if CFITSIO were fixed, most FITS files have 1–10 HDUs, so thread creation/synchronization overhead would likely exceed verification time. The refactoring cost is enormous (splitting `fv_context` into two structs, ~20+ function signature changes, ~250+ call sites). File-level parallelism via `multiprocessing` is simpler and more effective.

See `AI/In-Memory HDU Multiprocessing Proposal.md` for the full design that was evaluated.

### Callback Message Buffering
**Proposal**: Buffer callback messages in batches to reduce per-message function call overhead.

**Why not**: Premature optimization. The callback is a function pointer call — negligible overhead compared to the CFITSIO I/O and validation logic that dominates runtime. Adding buffering would complicate the callback contract (callers would need to handle batch delivery, message lifetime becomes ambiguous) for no measurable gain.

### Per-HDU Keyword Cache
**Proposal**: Cache parsed keyword state (`cards`, `tmpkwds`, `ttype`, `tform`, `tunit`) so `print_header` and `test_hdu` don't rescan when both are enabled.

**Why not**: Header parsing is fast (string operations on small arrays, typically < 200 keywords). The two-pass cost is negligible compared to data validation and CFITSIO I/O. Adding a cache layer increases code complexity and creates invalidation bugs for no user-visible improvement.

### O(n) Duplicate EXTNAME Check via Hash Map
**Proposal**: Replace the O(n²) duplicate EXTNAME/EXTVER scan with a hash map.

**Why not**: n = number of HDUs in the file, typically 1–20, rarely > 100. O(n²) with small n is a few microseconds. A hash map adds a dependency (or a hand-rolled implementation), increases code complexity, and solves a problem that doesn't exist in practice.

### Python Context Manager (`with fitsverify.Context() as ctx:`)
**Proposal**: Expose a persistent context object for repeated verifications with shared options.

**Why not**: `verify()` already manages context lifecycle internally (create, configure, verify, free). A persistent context can't reuse CFITSIO state across files anyway — each verification opens and closes its own `fitsfile*`. The only thing shared would be option settings, which are just keyword arguments. The context manager adds API surface area and lifetime management concerns (what happens if the user forgets `with`?) for saving a few keyword arguments.

### Python User Callback Hooks
**Proposal**: Allow user-defined Python callbacks for each message during verification.

**Why not**: The `Issue` list on `VerificationResult` is the Pythonic equivalent — all messages are collected and available after verification. Exposing raw C-level callbacks to Python adds complexity (GIL management, callback lifetime, error handling across the C/Python boundary) without benefit over inspecting the result after the fact. Users who need streaming access to messages during verification have an unusual enough use case that they can use the C library directly.

---

## Status Log

| Date | Note |
|------|------|
| 2026-02-05 | Initial plan created. Original source analyzed. All items are open. |
| 2026-02-06 | Phase 1.1–1.4, 1.6, 1.8 (partial) complete. Library builds clean (0 warnings, C99). Context struct, exit removal, global elimination, output callbacks, in-memory verification all implemented and tested. 71 tests pass across 4 test suites (test_library_api: 40, test_output_callback: 25, test_abort: 3, test_regression: 3). Next: 1.5 (structured error codes). |
| 2026-02-06 | Phase 1.5 complete: structured error codes. Defined `fv_error_code` enum with ~70 codes covering all diagnostic conditions. Added `code` field to `fv_message` and `int code` parameter to all 4 error/warning output functions. All ~212 call sites updated across 5 source files. Codes organized by category with gaps for future additions. Also fixed: `close_hdu()` `&&`→`||` bug (ttype/tform/tunit never freed), `abort_set` not reset after normal runs, `fv_context_free()` memory leaks. 71 tests still pass, 0 build warnings. Next: 1.7 (rebuild CLI). |
| 2026-02-06 | Phase 1.7 complete: CLI rebuilt with new features. Added `@filelist.txt` support, `-s` (severe only) flag, and `--json` (JSON output) flag. JSON mode uses the output callback system to capture all messages and emit structured JSON with per-file results (messages array, error/warning counts, HDU count, abort status) and accumulated totals. All 71 existing tests still pass. Phase 1 essentially complete; remaining: 1.8 multi-threaded tests. |
| 2026-02-06 | Phase 1.8 complete: Multi-threaded tests added (`test_threaded`, 7 tests). Discovered CFITSIO threading limitation: the global error message stack (`fits_clear_errmsg`/`fits_read_errmsg`) is NOT thread-safe, causing segfaults and corrupt results under concurrent verification. Workaround: mutex-protected verification demonstrated in tests. Documented in public header and Publication.md. Total test count: 78 across 5 suites. **Phase 1 complete.** |
| 2026-02-06 | **Phase 2 largely complete.** Python package created using cffi API/out-of-line mode. `_build_ffi.py` declares all public types/functions from `fitsverify.h` and compiles all 6 C sources directly into the extension module. Callback bridge uses cffi `extern "Python"` mechanism. Pythonic API: `verify()` accepts str/Path/bytes/bytearray/memoryview, returns `VerificationResult` with `.is_valid`, `.errors`, `.warnings`, `.issues`, `.text_report`. `Issue` class with `Severity` IntEnum. Thread safety via module-level `threading.Lock`. 23 pytest tests passing. `pip install -e .` working. Remaining: GIL release, Astropy integration, multi-platform testing, PyPI. |
| 2026-02-06 | **Phase 2 complete.** Added: file-like object support (open files, BytesIO), `to_dict()`/`to_json()` on both `Issue` and `VerificationResult`, Astropy `HDUList` integration (serializes to memory with `output_verify='ignore'`), `verify_parallel()` via `multiprocessing.Pool`. GIL release not applicable (cffi callbacks require GIL; CFITSIO lock serializes anyway; `verify_parallel` provides true parallelism). 34 pytest tests passing. Remaining: multi-platform testing, PyPI publishing. |
| 2026-02-06 | **Phase 4 complete: Fix Hints and Explain Mode.** Created `fv_hints.c`/`fv_hints.h` with fix hints and explanations for all ~70 `fv_error_code` values. Added `fix_hint` and `explain` fields to `fv_message` (NULL when disabled). Added `FV_OPT_FIX_HINTS` and `FV_OPT_EXPLAIN` options. `dispatch_msg()` performs O(1) switch-based lookup when enabled. FILE* mode prints "Fix:" and "Explanation:" lines after errors/warnings. CLI: `--fix-hints` and `--explain` flags; JSON mode includes hint fields. Python: `fix_hints=` and `explain=` kwargs on `verify()`; `Issue.fix_hint`/`.explain` attributes; `to_dict()`/`to_json()` include hint fields when populated. Also fixed: stale `STANDALONE`/`ERR2OUT` defines in `_build_ffi.py`. Test count: 86 C tests (40+3+33+7+3), 44 Python tests. All pass, zero build warnings. |
| 2026-02-06 | **Documentation complete.** Sphinx documentation site with ReadTheDocs theme. 8 pages: index (landing page with code examples), installation (CFITSIO prereqs, Python/C/CLI install), quickstart (5-minute tutorials), Python API reference (autodoc-based), C API reference (hand-written), CLI reference (all flags, examples, JSON output), error codes (all ~70 codes with fix hints), changelog (version 1.0.0 by phase). ReadTheDocs configuration (`.readthedocs.yaml`) ready for deployment. Comprehensive README.md rewritten. Sphinx build: zero warnings (`-W` flag). |
| 2026-02-06 | **Phase 4.4: Context-aware hint generation.** Replaced static-only hints with `fv_generate_hint()` that uses runtime context (keyword name, HDU number, HDU type) to produce specific, actionable hints. Added hint context fields to `fv_context` (`hint_keyword`, `hint_colnum`, `hint_fix_buf`, `hint_explain_buf`). Added `FV_HINT_SET_KEYWORD`/`FV_HINT_SET_COLNUM`/`FV_HINT_CLEAR` macros. Annotated ~107 call sites across `fvrf_head.c`, `fvrf_key.c`, and `fvrf_data.c`. Helper functions cover ~30 keywords with purpose descriptions and FITS Standard section references. Fallback to static hints when no context is available. Example output: "Add the keyword 'BITPIX' to the header of HDU 2. The mandatory keywords for a binary table in order are: XTENSION, BITPIX, NAXIS, ...". 5 new C tests (91 total), all pass. Documentation updated across all 6 RST pages, README, and changelog. |
| 2026-02-06 | **FITS Standard 4.0 easy updates.** Implemented 5 low-risk items: (1) Column limit keywords TLMINn/TLMAXn/TDMINn/TDMAXn recognized in image exclusion list and validated as float in tables; (2) Time WCS keywords from WCS Paper IV — TIMESYS with allowed-value validation, 8 float keywords (MJDREF, JDREF, TSTART, TSTOP, etc.), 2 string keywords (DATEREF, TIMEUNIT); (3) SBYTE_IMG and ULONGLONG_IMG pixel types in print_summary; (4) Random Groups deprecation warning (FV_WARN_RANDOM_GROUPS = 518); (5) Legacy XTENSION values warning (FV_WARN_LEGACY_XTENSION = 519). Deferred: tile-compressed image/table validation (high effort). All 135 tests pass (91 C, 44 Python), zero build warnings. |
