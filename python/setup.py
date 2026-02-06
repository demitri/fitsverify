"""setup.py — needed for cffi-modules integration with setuptools."""
from setuptools import setup

setup(
    cffi_modules=["src/fitsverify/_build_ffi.py:ffi"],
)
