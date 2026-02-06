"""Tests for the fitsverify Python package."""
import os
import sys
import pytest

# Test FITS files are in the build/tests directory
_build_tests = os.path.join(
    os.path.dirname(__file__), '..', '..', 'build', 'tests')


def _fits_path(name):
    """Return absolute path to a test FITS file."""
    path = os.path.join(_build_tests, name)
    if not os.path.isfile(path):
        pytest.skip(f"Test file not found: {path} (run gen_test_fits first)")
    return path


@pytest.fixture(autouse=True)
def _check_fits_files():
    """Skip all tests if test FITS files don't exist."""
    if not os.path.isdir(_build_tests):
        pytest.skip(f"Build test directory not found: {_build_tests}")


class TestVersion:
    def test_version_returns_string(self):
        import fitsverify
        v = fitsverify.version()
        assert isinstance(v, str)
        assert len(v) > 0

    def test_version_format(self):
        import fitsverify
        v = fitsverify.version()
        parts = v.split('.')
        assert len(parts) >= 2  # at least major.minor


class TestVerifyFile:
    def test_valid_file(self):
        import fitsverify
        result = fitsverify.verify(_fits_path("valid_minimal.fits"))
        assert result.is_valid
        assert result.num_errors == 0
        assert result.num_warnings == 0
        assert result.num_hdus >= 1
        assert not result.aborted
        assert bool(result) is True

    def test_valid_multi_ext(self):
        import fitsverify
        result = fitsverify.verify(_fits_path("valid_multi_ext.fits"))
        assert result.is_valid
        assert result.num_hdus >= 3

    def test_bad_bitpix(self):
        import fitsverify
        result = fitsverify.verify(_fits_path("err_bad_bitpix.fits"))
        assert not result.is_valid
        assert result.num_errors > 0
        assert bool(result) is False

    def test_dup_extname_warnings(self):
        import fitsverify
        result = fitsverify.verify(_fits_path("err_dup_extname.fits"))
        assert result.num_warnings > 0
        assert len(result.warnings) > 0

    def test_path_object(self):
        """verify() accepts pathlib.Path."""
        import fitsverify
        from pathlib import Path
        result = fitsverify.verify(Path(_fits_path("valid_minimal.fits")))
        assert result.is_valid

    def test_nonexistent_file(self):
        import fitsverify
        result = fitsverify.verify("/nonexistent/path/foo.fits")
        assert result.num_errors > 0 or result.aborted


class TestVerifyMemory:
    def test_valid_from_bytes(self):
        import fitsverify
        path = _fits_path("valid_minimal.fits")
        with open(path, 'rb') as f:
            data = f.read()
        result = fitsverify.verify(data)
        assert result.is_valid
        assert result.num_errors == 0

    def test_bad_bitpix_from_bytes(self):
        import fitsverify
        path = _fits_path("err_bad_bitpix.fits")
        with open(path, 'rb') as f:
            data = f.read()
        result = fitsverify.verify(data)
        assert result.num_errors > 0

    def test_bytearray(self):
        import fitsverify
        path = _fits_path("valid_minimal.fits")
        with open(path, 'rb') as f:
            data = bytearray(f.read())
        result = fitsverify.verify(data)
        assert result.is_valid

    def test_invalid_type(self):
        import fitsverify
        with pytest.raises(TypeError):
            fitsverify.verify(12345)


class TestIssues:
    def test_issues_list(self):
        import fitsverify
        result = fitsverify.verify(_fits_path("valid_minimal.fits"))
        assert isinstance(result.issues, list)
        assert len(result.issues) > 0  # at least info messages

    def test_issue_attributes(self):
        import fitsverify
        result = fitsverify.verify(_fits_path("valid_minimal.fits"))
        issue = result.issues[0]
        assert hasattr(issue, 'severity')
        assert hasattr(issue, 'code')
        assert hasattr(issue, 'hdu')
        assert hasattr(issue, 'message')
        assert isinstance(issue.message, str)

    def test_error_issues_have_codes(self):
        import fitsverify
        result = fitsverify.verify(_fits_path("err_bad_bitpix.fits"))
        errors = result.errors
        assert len(errors) > 0
        for err in errors:
            assert err.is_error
            assert err.code > 0  # should have a meaningful error code

    def test_warning_issues(self):
        import fitsverify
        result = fitsverify.verify(_fits_path("err_dup_extname.fits"))
        warnings = result.warnings
        assert len(warnings) > 0
        for warn in warnings:
            assert warn.is_warning
            assert warn.code > 0

    def test_severity_enum(self):
        from fitsverify import Severity
        assert Severity.INFO == 0
        assert Severity.WARNING == 1
        assert Severity.ERROR == 2
        assert Severity.SEVERE == 3

    def test_text_report(self):
        import fitsverify
        result = fitsverify.verify(_fits_path("valid_minimal.fits"))
        report = result.text_report
        assert isinstance(report, str)
        assert len(report) > 0


class TestOptions:
    def test_err_report_severe(self):
        """err_report=2 should suppress non-severe errors."""
        import fitsverify
        result = fitsverify.verify(
            _fits_path("err_dup_extname.fits"), err_report=2)
        # With severe-only, the dup extname warning should be suppressed
        assert result.num_warnings == 0

    def test_testdata_off(self):
        """testdata=False should skip data validation."""
        import fitsverify
        result = fitsverify.verify(
            _fits_path("valid_minimal.fits"), testdata=False)
        assert result.is_valid


class TestVerifyAll:
    def test_multiple_files(self):
        import fitsverify
        files = [
            _fits_path("valid_minimal.fits"),
            _fits_path("valid_multi_ext.fits"),
            _fits_path("err_bad_bitpix.fits"),
        ]
        results = fitsverify.verify_all(files)
        assert len(results) == 3
        assert results[0].is_valid
        assert results[1].is_valid
        assert not results[2].is_valid


class TestRepr:
    def test_result_repr(self):
        import fitsverify
        result = fitsverify.verify(_fits_path("valid_minimal.fits"))
        r = repr(result)
        assert "VALID" in r
        assert "errors=" in r

    def test_issue_repr(self):
        import fitsverify
        result = fitsverify.verify(_fits_path("valid_minimal.fits"))
        if result.issues:
            r = repr(result.issues[0])
            assert "Issue(" in r
            assert "severity=" in r
