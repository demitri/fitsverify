"""
cffi build script for _fitsverify_cffi extension module.

This compiles the libfitsverify C sources directly into a Python
extension module. CFITSIO must be installed on the system.

Usage:
    python _build_ffi.py           # builds _fitsverify_cffi.*.so
    pip install .                   # uses pyproject.toml to invoke this
"""
import os
import subprocess
import cffi

ffi = cffi.FFI()

# ---- C declarations visible to Python ------------------------------------
# This must match the public API in fitsverify.h exactly (types and
# function signatures), but uses cffi's simplified C syntax.

ffi.cdef("""
    /* opaque context */
    typedef struct fv_context fv_context;

    /* error codes (enum values) */
    typedef int fv_error_code;

    /* severity levels */
    typedef enum {
        FV_MSG_INFO    = 0,
        FV_MSG_WARNING = 1,
        FV_MSG_ERROR   = 2,
        FV_MSG_SEVERE  = 3
    } fv_msg_severity;

    /* message struct delivered to callbacks */
    typedef struct {
        fv_msg_severity severity;
        fv_error_code   code;
        int             hdu_num;
        const char     *text;
        const char     *fix_hint;
        const char     *explain;
    } fv_message;

    /* callback type */
    typedef void (*fv_output_fn)(const fv_message *msg, void *userdata);

    /* options */
    typedef enum {
        FV_OPT_PRHEAD       = 0,
        FV_OPT_PRSTAT       = 1,
        FV_OPT_TESTDATA     = 2,
        FV_OPT_TESTCSUM     = 3,
        FV_OPT_TESTFILL     = 4,
        FV_OPT_HEASARC_CONV = 5,
        FV_OPT_TESTHIERARCH = 6,
        FV_OPT_ERR_REPORT   = 7,
        FV_OPT_FIX_HINTS    = 8,
        FV_OPT_EXPLAIN       = 9
    } fv_option;

    /* per-file result */
    typedef struct {
        int  num_errors;
        int  num_warnings;
        int  num_hdus;
        int  aborted;
    } fv_result;

    /* lifecycle */
    fv_context *fv_context_new(void);
    void        fv_context_free(fv_context *ctx);

    /* configuration */
    int  fv_set_option(fv_context *ctx, fv_option opt, int value);
    int  fv_get_option(fv_context *ctx, fv_option opt);

    /* output callback */
    void fv_set_output(fv_context *ctx, fv_output_fn fn, void *userdata);

    /* verification */
    int fv_verify_file(fv_context *ctx, const char *infile,
                       FILE *out, fv_result *result);
    int fv_verify_memory(fv_context *ctx, const void *buffer, size_t size,
                         const char *label, FILE *out, fv_result *result);

    /* accumulated totals */
    void fv_get_totals(const fv_context *ctx,
                       long *total_errors, long *total_warnings);

    /* version */
    const char *fv_version(void);

    /* callback helper defined in our glue code */
    extern "Python" void _python_output_callback(const fv_message *msg,
                                                  void *userdata);
""")

# ---- Locate source files and CFITSIO -------------------------------------

_this_dir = os.path.dirname(os.path.abspath(__file__))
_project_root = os.path.abspath(os.path.join(_this_dir, '..', '..', '..'))
_lib_src = os.path.join(_project_root, 'libfitsverify', 'src')
_lib_inc = os.path.join(_project_root, 'libfitsverify', 'include')

# C source files to compile into the extension.
# setuptools requires relative paths (relative to setup.py directory),
# so we compute them relative to the python/ package root.
_pkg_root = os.path.abspath(os.path.join(_this_dir, '..', '..'))
_rel_src = os.path.relpath(_lib_src, _pkg_root)

_c_sources = [
    os.path.join(_rel_src, 'fv_api.c'),
    os.path.join(_rel_src, 'fv_hints.c'),
    os.path.join(_rel_src, 'fvrf_misc.c'),
    os.path.join(_rel_src, 'fvrf_key.c'),
    os.path.join(_rel_src, 'fvrf_file.c'),
    os.path.join(_rel_src, 'fvrf_data.c'),
    os.path.join(_rel_src, 'fvrf_head.c'),
]

# Try to find CFITSIO
def _find_cfitsio():
    """Find CFITSIO include and library paths."""
    include_dirs = []
    library_dirs = []
    libraries = ['cfitsio']

    # Check CFITSIO_DIR environment variable
    cfitsio_dir = os.environ.get('CFITSIO_DIR', '')
    if cfitsio_dir:
        inc = os.path.join(cfitsio_dir, 'include')
        lib = os.path.join(cfitsio_dir, 'lib')
        if os.path.isdir(inc):
            include_dirs.append(inc)
        if os.path.isdir(lib):
            library_dirs.append(lib)
        return include_dirs, library_dirs, libraries

    # Try pkg-config
    try:
        cflags = subprocess.check_output(
            ['pkg-config', '--cflags', 'cfitsio'],
            stderr=subprocess.DEVNULL
        ).decode().strip().split()
        libs = subprocess.check_output(
            ['pkg-config', '--libs', 'cfitsio'],
            stderr=subprocess.DEVNULL
        ).decode().strip().split()

        for f in cflags:
            if f.startswith('-I'):
                include_dirs.append(f[2:])
        for f in libs:
            if f.startswith('-L'):
                library_dirs.append(f[2:])
            elif f.startswith('-l'):
                if f[2:] not in libraries:
                    libraries.append(f[2:])
        return include_dirs, library_dirs, libraries
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    # Common installation paths
    common_paths = [
        '/usr/local/cfitsio-4.6.3',
        '/usr/local',
        '/usr',
        '/opt/homebrew',
    ]
    for p in common_paths:
        inc = os.path.join(p, 'include')
        lib = os.path.join(p, 'lib')
        if os.path.isfile(os.path.join(inc, 'fitsio.h')):
            include_dirs.append(inc)
            if os.path.isdir(lib):
                library_dirs.append(lib)
            return include_dirs, library_dirs, libraries

    return include_dirs, library_dirs, libraries


cfitsio_inc, cfitsio_lib, cfitsio_libs = _find_cfitsio()

ffi.set_source(
    "fitsverify._fitsverify_cffi",  # module name within the package
    """
    #include "fitsverify.h"
    """,
    sources=_c_sources,
    include_dirs=[_lib_inc, _lib_src] + cfitsio_inc,
    library_dirs=cfitsio_lib,
    libraries=cfitsio_libs + ['m'],
    define_macros=[],
)

if __name__ == '__main__':
    ffi.compile(verbose=True)
