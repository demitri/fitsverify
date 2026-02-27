Installation
============

libfitsverify has three components:

1. **Python package** (recommended for most users)
2. **C library** (for embedding in C/C++ programs)
3. **CLI tool** (drop-in replacement for the original ``fitsverify``)

All three require `CFITSIO <https://heasarc.gsfc.nasa.gov/fitsio/>`_ 4.x to be
installed on your system.


Prerequisites: CFITSIO
----------------------

libfitsverify requires CFITSIO 4.x (tested with 4.6.3).  The core API is stable
across 4.x releases.

**macOS (Homebrew)**::

    brew install cfitsio

**Ubuntu / Debian**::

    sudo apt install libcfitsio-dev

**Fedora / RHEL**::

    sudo dnf install cfitsio-devel

**From source** (any platform)::

    curl -OL https://heasarc.gsfc.nasa.gov/FTP/software/fitsio/c/cfitsio-4.6.3.tar.gz
    tar xzf cfitsio-4.6.3.tar.gz
    cd cfitsio-4.6.3
    ./configure --prefix=/usr/local/cfitsio-4.6.3
    make && make install

If you install CFITSIO to a non-standard location, set the ``CFITSIO_DIR``
environment variable::

    export CFITSIO_DIR=/usr/local/cfitsio-4.6.3


Python Package
--------------

**From the source tree**::

    cd python/
    pip install .

**Development install** (editable, for hacking on the code)::

    cd python/
    pip install -e .

The build script (``_build_ffi.py``) compiles all libfitsverify C sources
directly into the Python extension module --- no separate C library installation
is needed.  It will find CFITSIO via:

1. The ``CFITSIO_DIR`` environment variable
2. ``pkg-config``
3. Common system paths (``/usr/local``, ``/opt/homebrew``, etc.)

**Verify the installation**::

    python -c "import fitsverify; print(fitsverify.version())"


C Library and CLI
-----------------

The C library and CLI are built with CMake.

::

    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make

If CFITSIO is in a non-standard location::

    cmake .. -DCFITSIO_INCLUDE_DIRS=/path/to/cfitsio/include \
             -DCFITSIO_LIBRARIES=cfitsio \
             -DCFITSIO_LIBRARY_DIRS=/path/to/cfitsio/lib

This produces:

- ``libfitsverify/libfitsverify.a`` --- the static library
- ``cli/fitsverify`` --- the CLI executable
- ``tests/test_*`` --- test executables

**Run the tests**::

    cd build/tests
    ./gen_test_fits         # generate test FITS files
    ./test_library_api      # 40 tests
    ./test_output_callback  # 33 tests
    ./test_abort            # 3 tests
    ./test_threaded         # 7 tests
    bash test_regression.sh # 3 tests

**Install** (optional)::

    cmake --install build --prefix /usr/local

This installs the library, header, and CLI tool.


Platform Support
----------------

libfitsverify is tested on macOS (ARM and Intel) and Linux.  Windows support is
planned but not yet tested.

The C library requires a C99 compiler.  The Python package requires Python 3.9+
and `cffi <https://cffi.readthedocs.io/>`_ >= 1.16.
