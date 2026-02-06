# fitsverify Refactoring Project — Agent Orientation

**Read this file first.** It provides the context and conventions for all work on this project.

## Project Goal

Transform the NASA/HEASARC `fitsverify` command-line tool (a FITS file standards-compliance validator) into:

1. **`libfitsverify`** — a reentrant, embeddable C library with a clean public API
2. **`fitsverify` CLI** — rebuilt as a thin wrapper around the library
3. **`fitsverify` Python package** — a Pythonic interface embedding the C library

The original code (~3,100 lines of C, dating to 1994/1998) was never designed for library use. It has global state, `exit()` calls that kill the host process, and all output hardwired to `FILE*` streams. This project refactors it for modern embeddability while preserving the battle-tested validation logic.

## Repository Layout

```
demitri-fitsverify/
├── AI/
│   ├── START_HERE.md          ← You are here
│   └── TODO.md                ← Detailed task tracking (keep updated!)
├── source/
│   └── c/                     ← Original fitsverify 4.20 source (unmodified reference)
│       ├── ftverify.c         ← Core verification orchestration
│       ├── fitsverify.c       ← Standalone main() + HEADAS stubs
│       ├── fverify.h          ← Shared header (types, externs, prototypes)
│       ├── fvrf_head.c        ← Header validation + verify_fits() entry point (~3000 lines)
│       ├── fvrf_data.c        ← Data validation (tables, checksums)
│       ├── fvrf_file.c        ← HDU error tracking, report init/close
│       ├── fvrf_key.c         ← Keyword parsing and type checking
│       ├── fvrf_misc.c        ← Output functions, error/warning counters
│       ├── README             ← Original build instructions
│       └── License.txt        ← NASA open source license
├── documentation/
│   └── publication/
│       └── Publication.md     ← Notes toward a publication (keep updated!)
├── fitsverify-archives/       ← Original distribution tarballs
└── README.md
```

As work progresses, new directories will be created:
- `libfitsverify/` — refactored C library source
- `cli/` — rebuilt command-line tool
- `python/` — Python package

## Key Technical Context

### The Original Architecture

```
main() [fitsverify.c]
  → ftverify_work() [ftverify.c]     — opens files, routes output
    → verify_fits() [fvrf_head.c]    — verifies one FITS file
      ├─ init_report()               — sets up HDU tracking
      ├─ for each HDU:
      │   ├─ init_hdu()              — parse keywords
      │   ├─ test_hdu()              — validate header
      │   ├─ test_data()             — validate data
      │   └─ close_hdu()             — free HDU memory
      ├─ test_end()                  — check for extraneous bytes
      └─ close_report()              — summarize, accumulate totals
```

### Critical Problems for Library Use

1. **`exit(1)` in error handlers** — `wrterr()`, `wrtferr()`, `wrtserr()` in `fvrf_misc.c` call `exit(1)` when error count exceeds `MAXERRORS` (200). This kills any host process.

2. **Global mutable state everywhere** — 8 `extern` config variables, plus file-static counters, buffers, and arrays in every `.c` file. Not reentrant, not thread-safe.

3. **`static` variables in the header file** — `fverify.h` lines 14-15 declare `static char errmes[256]` and `static char comm[...]`, creating separate copies in every translation unit. This is a bug.

4. **All output via `FILE*`** — errors go to `stderr`, output to a `FILE*`. No way to capture results programmatically.

5. **`#include "fitsverify.c"`** — `ftverify.c` literally `#include`s another `.c` file for the standalone build.

### CFITSIO Dependency

- **Target: CFITSIO 4.x only** (currently 4.6.3). No need to support 3.x.
- The 3.x → 4.0 transition changed version numbering from a float (`3.47`) to 3-field (`4.0.0`) and made zlib a required external dependency. The core API functions used by fitsverify are unchanged.
- CFITSIO's `fits_open_memfile()` enables in-memory verification (available since 3.x, stable in 4.x).
- CFITSIO is thread-safe when operating on separate `fitsfile*` handles.

### The OPUS Fork (STScI)

[STScI forked fitsverify](https://ssb.stsci.edu/cdbs/temp/gcc_test/opus/hlp/opus_fitsverify.html) as `opus_fitsverify` to add a `-s` (severe errors only) flag for their HST pipeline. They classified errors into "severe" (10 structural issues) vs "regular" (25+ format issues). Their help page stated: *"the as-delivered HEASARC fitsverify tool does not provide the software hooks to enable the leniency options."* This project makes such forks unnecessary by exposing structured, filterable results.

## Design Principles

1. **Preserve the validation logic** — the ~60 error/warning checks are well-tested and standards-correct. Refactor the scaffolding, not the science.

2. **Context struct replaces all global state** — a single `fv_context` carries config, counters, buffers, HDU tracking. Each verification is independent. Multiple threads can verify different files simultaneously.

3. **Callbacks replace FILE* output** — callers register functions to receive messages. Default callbacks reproduce the original CLI output. Library users capture structured results.

4. **Every diagnostic gets a code** — not just text. `FV_ERR_MISSING_END`, `FV_WARN_DUPLICATE_EXTNAME`, etc. This enables the OPUS use case (filter by severity) without forking.

5. **No exit() in the library** — `MAXERRORS` exceeded → return an error code, never `exit()`.

6. **In-memory verification** — accept both filenames and memory buffers via CFITSIO's `fits_open_memfile()`.

7. **Thread-safe by design** — no globals means safe concurrent use.

## Agent Instructions

### Keeping Documents Updated

- **`AI/TODO.md`**: When you complete a task, mark it done. When you discover new work, add it. When priorities change, note it. This is the living task tracker.
- **`documentation/publication/Publication.md`**: When you make a design decision, encounter an interesting problem, or produce a result worth noting, add a brief entry. This collects material for an eventual paper.

### Working Conventions

- Always read the relevant original source before modifying anything.
- The original source in `source/c/` should remain **unmodified** as a reference. All new work goes in new directories.
- Prefer incremental, testable changes. Each phase should produce something that compiles and can be tested.
- When in doubt about a design choice, document the options and tradeoffs rather than guessing.

### Build & Test

- The library should build with CMake.
- Target: CFITSIO 4.x, modern C (C11 or later).
- The Python package should use `pyproject.toml` and build via standard Python packaging tools.
- Test FITS files will be needed — both valid files and files with known errors.

## Quick Reference: Original Source Files

| File | Lines | Role |
|------|-------|------|
| `fvrf_head.c` | ~3000 | Header validation, `verify_fits()` main loop, HDU init |
| `fvrf_data.c` | ~890 | Data validation (tables, checksums, fill bytes) |
| `fvrf_key.c` | ~730 | Keyword parsing and type checking |
| `fvrf_misc.c` | ~340 | Output formatting, error/warning counters, **contains exit() calls** |
| `fvrf_file.c` | ~290 | HDU name tracking, report init/close, end-of-file test |
| `ftverify.c` | ~470 | Orchestration, file list handling, output routing |
| `fitsverify.c` | ~270 | main(), CLI argument parsing, HEADAS stubs |
| `fverify.h` | ~210 | Types, constants, externs, prototypes |
