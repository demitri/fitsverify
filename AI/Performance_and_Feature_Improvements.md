# Performance and Functionality Improvements

This file captures pragmatic improvements for the C library/CLI and ideas for the Python package wrapper.

## C Library and CLI Improvements (Performance / Functionality)

1. ~~Add a fast header-only mode that skips data validation entirely (`testdata=0`, `testcsum=0`, `testfill=0`).~~ **Done** — set options via `fv_set_option()` or Python kwargs.
2. Cache per-HDU parsed keyword state (`cards`, `tmpkwds`, `ttype`, `tform`, `tunit`) so `print_header` and `test_hdu` don't rescan when both are enabled. — **Evaluated and rejected** (see TODO.md).
3. Implement message buffering in the callback path so bulk output doesn't dominate runtime; use a configurable batch size. — **Evaluated and rejected** (see TODO.md).
4. ~~Add a structured result accumulator (list of messages with codes and severities) so clients can filter without re-parsing text.~~ **Done** — `fv_message` with `code`, `severity`, `hdu_num`, `text`, `fix_hint`, `explain`.
5. Make file-level duplicate EXTNAME/EXTVER checks O(n) instead of O(n^2) using a hash map keyed by EXTNAME+EXTVER+EXTLEVEL. — **Evaluated and rejected** (see TODO.md).
6. ~~Replace `sprintf`/`strcat` with `snprintf` to reduce overflow risk and avoid unnecessary copies.~~ **Done** — 222 call sites replaced (Phase 3).
7. Add a library-level "summary only" option so clients can skip verbose output entirely (beyond CLI `-q`).
8. Implement selective HDU verification (by index or EXTNAME) to avoid scanning the full file when only a subset matters.
9. Implement in-memory HDU parallelism for large buffers with many HDUs. — **Evaluated and rejected** (see TODO.md, CFITSIO threading limitation).
10. ~~Add a JSON output mode to the CLI for automation.~~ **Done** — `--json` flag (Phase 1.7).

## Python Package / Wrapper Features

1. ~~Pythonic API: `fitsverify.verify(input, *, options=None, summary_only=False)` accepting `str/Path`, `bytes`, or file-like objects.~~ **Done** (Phase 2).
2. ~~Structured results: `VerificationResult` with `.errors`, `.warnings`, `.hdus`, `.is_valid`, `.text_report`, and `.to_json()`.~~ **Done** (Phase 2).
3. ~~Issue model: `Issue(code, severity, hdu, message)` with stable enums and string reprs.~~ **Done** (Phase 2).
4. ~~Parallel verification across multiple files: `verify_parallel(files, max_workers=...)` with true C-level parallelism.~~ **Done** — via `multiprocessing.Pool` (Phase 2).
5. ~~Streaming/file-like support: accept file-like objects and read into a memory buffer for `fv_verify_memory`.~~ **Done** (Phase 2).
6. ~~Astropy integration: accept `astropy.io.fits.HDUList` or `HDU` via in-memory serialization.~~ **Done** (Phase 2).
7. ~~Severity filtering: `severities={"warning","error","severe"}` to match OPUS-style workflows.~~ **Done** — via `err_report` option and CLI `-e`/`-s` flags.
8. Callback hooks: allow user-defined Python callbacks for each message; default accumulates results. — **Evaluated and rejected** (see TODO.md).
9. Configuration profiles: pre-defined option sets like `strict`, `header_only`, `checksums_only`.
10. ~~JSON output parity: `VerificationResult.to_json()` matches CLI `--json` schema.~~ **Done** (Phase 2).
11. Memory safety: optional `max_buffer_bytes` guard for file-like inputs.
12. Context manager: `with fitsverify.Context() as ctx:` for repeated checks with shared options. — **Evaluated and rejected** (see TODO.md).

## Fix Hints and Explain Mode — IMPLEMENTED

> **Status: Fully implemented in Phase 4 (including context-aware generation in Phase 4.4).** The implementation went beyond the original proposal: hints are now **context-aware**, using runtime state (keyword name, HDU number, HDU type) to produce specific, actionable suggestions rather than generic advice. See `libfitsverify/src/fv_hints.c` for the implementation.

### Goal

Add two optional modes:
1. Fix-hint mode: short, actionable suggestions.
2. Explain mode: longer, beginner-friendly rationale describing what the error/warning means and why it matters.

### Proposed Output Model

Extend `fv_message` with optional fields:
- `fix_hint` (short, 1–2 lines)
- `explain` (longer, user-friendly)

```c
typedef struct {
    fv_msg_severity severity;
    int             hdu_num;
    const char     *text;
    const char     *fix_hint;
    const char     *explain;
} fv_message;
```

These fields are populated only when the user enables the corresponding mode.

### User-Facing Options

C library options:
- `FV_OPT_FIX_HINTS` (0/1)
- `FV_OPT_EXPLAIN` (0/1)

CLI flags:
- `--fix-hints`
- `--explain`

Python API:
```python
fitsverify.verify(path, fix_hints=True, explain=True)
```

### Where the Text Comes From

Use a central lookup table keyed by error code (Phase 1.5). Example:

```c
typedef struct {
    fv_err_code code;
    const char *fix_hint;
    const char *explain;
} fv_hint_entry;

static const fv_hint_entry k_hints[] = {
  { FV_ERR_MISSING_END,
    "Add an END card to the header and pad to 2880-byte boundary.",
    "Every FITS header must end with an END card. FITS blocks are 2880 bytes long, so the header must be padded to that boundary."
  },
  ...
};
```

At emit time, if `FV_OPT_FIX_HINTS` or `FV_OPT_EXPLAIN` is enabled, attach the strings to the message. The core validator remains non-mutating.

### Example Output

Fix-hint only:
```
*** Error: END keyword is not present
Fix: Add an END card to the header and pad to 2880-byte boundary.
```

Explain mode:
```
*** Error: END keyword is not present
Explanation: Every FITS header must end with an END card. FITS blocks are 2880 bytes long, so the header must be padded to that boundary.
```

### Starter Hint Set (Top 15)

1. Missing END
   - Fix: Add END card and pad to 2880 bytes.
   - Explain: FITS headers terminate with END; padding is required to block size.
2. Mandatory keyword missing/out of order
   - Fix: Add missing keyword or reorder to follow FITS Standard sequence.
   - Explain: Required keywords (e.g., SIMPLE, BITPIX, NAXIS) must appear first in a defined order.
3. Mandatory keyword wrong datatype
   - Fix: Change keyword value to the required datatype.
   - Explain: FITS requires specific types (e.g., BITPIX integer, NAXIS integer).
4. XTENSION in primary header
   - Fix: Remove XTENSION from primary HDU; use in extensions only.
   - Explain: Primary HDU is not an extension.
5. SIMPLE/FIRST keyword issues
   - Fix: Ensure SIMPLE = T in primary header.
   - Explain: SIMPLE must indicate FITS compliance for primary HDU.
6. Extraneous bytes after last HDU
   - Fix: Truncate file after the last HDU end.
   - Explain: FITS files must end at the final 2880-byte block boundary.
7. Illegal characters in header
   - Fix: Replace with ASCII 32–126.
   - Explain: FITS headers are restricted to printable ASCII.
8. Keyword name illegal
   - Fix: Rename keyword to valid FITS name (8 chars, uppercase, A–Z, 0–9, hyphen).
   - Explain: FITS keyword names have strict format rules.
9. Header value/comment format errors
   - Fix: Ensure value field and comment are separated by `/` with proper spacing.
   - Explain: FITS standard requires fixed layout for header cards.
10. Duplicate mandatory keyword
   - Fix: Remove or rename duplicate entry.
   - Explain: Mandatory keywords must appear only once.
11. TFIELDS/TTYPEn mismatch
   - Fix: Ensure TFIELDS equals the highest column index defined.
   - Explain: Table column descriptors must match TFIELDS.
12. TBCOLn in BINTABLE
   - Fix: Remove TBCOLn in binary tables.
   - Explain: TBCOLn is only valid for ASCII tables.
13. TDIMn in ASCII table
   - Fix: Remove TDIMn from ASCII tables.
   - Explain: TDIMn is only valid for binary tables.
14. Checksum mismatch
   - Fix: Recompute DATASUM and CHECKSUM.
   - Explain: Checksums ensure data integrity and must match the actual data.
15. END not blank-filled
   - Fix: Fill columns 9–80 with spaces.
   - Explain: END card must be padded with blanks.

### What Was Actually Implemented

The implementation matches this proposal and extends it significantly:

- **Context-aware hints**: `fv_generate_hint()` uses runtime context (`hint_keyword`, `hint_colnum`, `curhdu`, `curtype`) to produce specific hints. E.g., "Add the keyword 'BITPIX' to the header of HDU 2. The mandatory keywords for a binary table in order are: XTENSION, BITPIX, NAXIS, NAXIS1, NAXIS2, PCOUNT, GCOUNT, TFIELDS, END."
- **~107 annotated call sites** across `fvrf_head.c`, `fvrf_key.c`, and `fvrf_data.c` using `FV_HINT_SET_KEYWORD` / `FV_HINT_SET_COLNUM` macros
- **Helper functions** for ~30 keywords: purpose descriptions and FITS Standard section references
- **Automatic fallback** to static hints when no runtime context is available
- 91 C tests + 44 Python tests, all passing

### Related File

The starter hint catalog in `AI/hints.md` has been superseded by the complete implementation in `libfitsverify/src/fv_hints.c`.
