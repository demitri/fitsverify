"""Tests for the fitsverify Python package."""
import io
import json
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


class TestFileLikeObject:
    def test_binary_file_object(self):
        """verify() accepts an open binary file."""
        import fitsverify
        path = _fits_path("valid_minimal.fits")
        with open(path, 'rb') as f:
            result = fitsverify.verify(f)
        assert result.is_valid

    def test_bytesio(self):
        """verify() accepts a BytesIO object."""
        import fitsverify
        path = _fits_path("valid_minimal.fits")
        with open(path, 'rb') as f:
            data = f.read()
        result = fitsverify.verify(io.BytesIO(data))
        assert result.is_valid

    def test_text_mode_raises(self):
        """verify() rejects text-mode file-like objects."""
        import fitsverify
        with pytest.raises(TypeError, match="binary mode"):
            fitsverify.verify(io.StringIO("not binary"))


class TestToDict:
    def test_issue_to_dict(self):
        import fitsverify
        result = fitsverify.verify(_fits_path("valid_minimal.fits"))
        assert len(result.issues) > 0
        d = result.issues[0].to_dict()
        assert 'severity' in d
        assert 'code' in d
        assert 'hdu' in d
        assert 'message' in d
        assert isinstance(d['severity'], str)

    def test_result_to_dict(self):
        import fitsverify
        result = fitsverify.verify(_fits_path("err_bad_bitpix.fits"))
        d = result.to_dict()
        assert d['is_valid'] is False
        assert d['num_errors'] > 0
        assert isinstance(d['messages'], list)
        assert len(d['messages']) > 0
        assert 'severity' in d['messages'][0]

    def test_result_to_json(self):
        import fitsverify
        result = fitsverify.verify(_fits_path("valid_minimal.fits"))
        j = result.to_json()
        parsed = json.loads(j)
        assert parsed['is_valid'] is True
        assert parsed['num_errors'] == 0

    def test_to_json_indent(self):
        import fitsverify
        result = fitsverify.verify(_fits_path("valid_minimal.fits"))
        j = result.to_json(indent=2)
        assert '\n' in j  # indented output has newlines
        parsed = json.loads(j)
        assert parsed['is_valid'] is True


class TestAstropy:
    def test_hdulist(self):
        """verify() accepts an astropy HDUList."""
        pytest.importorskip("astropy")
        import fitsverify
        from astropy.io import fits
        hdulist = fits.open(_fits_path("valid_minimal.fits"))
        result = fitsverify.verify(hdulist)
        assert result.is_valid
        hdulist.close()

    def test_hdulist_with_errors(self):
        """verify() detects errors in an astropy HDUList."""
        pytest.importorskip("astropy")
        import fitsverify
        from astropy.io import fits
        hdulist = fits.open(_fits_path("err_bad_bitpix.fits"))
        result = fitsverify.verify(hdulist)
        assert result.num_errors > 0
        hdulist.close()


class TestHints:
    def test_no_hints_by_default(self):
        """By default, fix_hint and explain are None."""
        import fitsverify
        result = fitsverify.verify(_fits_path("err_dup_extname.fits"))
        for issue in result.issues:
            assert issue.fix_hint is None
            assert issue.explain is None

    def test_fix_hints_on_warnings(self):
        """fix_hints=True populates fix_hint on warning issues."""
        import fitsverify
        result = fitsverify.verify(
            _fits_path("err_dup_extname.fits"), fix_hints=True)
        warnings = result.warnings
        assert len(warnings) > 0
        for warn in warnings:
            assert warn.fix_hint is not None
            assert isinstance(warn.fix_hint, str)
            assert len(warn.fix_hint) > 0
            # explain should still be None
            assert warn.explain is None

    def test_explain_on_warnings(self):
        """explain=True populates explain on warning issues."""
        import fitsverify
        result = fitsverify.verify(
            _fits_path("err_dup_extname.fits"), explain=True)
        warnings = result.warnings
        assert len(warnings) > 0
        for warn in warnings:
            assert warn.explain is not None
            assert isinstance(warn.explain, str)
            assert len(warn.explain) > 0
            # fix_hint should still be None
            assert warn.fix_hint is None

    def test_both_hints_and_explain(self):
        """Both fix_hints and explain can be enabled simultaneously."""
        import fitsverify
        result = fitsverify.verify(
            _fits_path("err_dup_extname.fits"), fix_hints=True, explain=True)
        warnings = result.warnings
        assert len(warnings) > 0
        for warn in warnings:
            assert warn.fix_hint is not None
            assert warn.explain is not None

    def test_hints_on_errors(self):
        """fix_hints=True populates fix_hint on error issues."""
        import fitsverify
        result = fitsverify.verify(
            _fits_path("err_bad_bitpix.fits"), fix_hints=True)
        errors = result.errors
        assert len(errors) > 0
        # At least one error should have a hint
        hinted = [e for e in errors if e.fix_hint is not None]
        assert len(hinted) > 0

    def test_info_messages_no_hints(self):
        """INFO messages never have hints even when enabled."""
        import fitsverify
        from fitsverify import Severity
        result = fitsverify.verify(
            _fits_path("err_dup_extname.fits"), fix_hints=True, explain=True)
        info_msgs = [i for i in result.issues if i.severity == Severity.INFO]
        assert len(info_msgs) > 0
        for info in info_msgs:
            assert info.fix_hint is None
            assert info.explain is None

    def test_hints_in_to_dict(self):
        """to_dict() includes hint fields when populated."""
        import fitsverify
        result = fitsverify.verify(
            _fits_path("err_dup_extname.fits"), fix_hints=True)
        warnings = result.warnings
        assert len(warnings) > 0
        d = warnings[0].to_dict()
        assert 'fix_hint' in d
        assert isinstance(d['fix_hint'], str)

    def test_no_hints_not_in_dict(self):
        """to_dict() omits hint fields when not populated."""
        import fitsverify
        result = fitsverify.verify(_fits_path("err_dup_extname.fits"))
        warnings = result.warnings
        assert len(warnings) > 0
        d = warnings[0].to_dict()
        assert 'fix_hint' not in d
        assert 'explain' not in d

    def test_hints_in_to_json(self):
        """to_json() includes hint fields when populated."""
        import fitsverify
        result = fitsverify.verify(
            _fits_path("err_dup_extname.fits"),
            fix_hints=True, explain=True)
        j = result.to_json()
        parsed = json.loads(j)
        warning_msgs = [m for m in parsed['messages']
                        if m['severity'] == 'WARNING']
        assert len(warning_msgs) > 0
        assert 'fix_hint' in warning_msgs[0]
        assert 'explain' in warning_msgs[0]

    def test_hints_with_memory_verification(self):
        """Hints work with in-memory verification too."""
        import fitsverify
        path = _fits_path("err_dup_extname.fits")
        with open(path, 'rb') as f:
            data = f.read()
        result = fitsverify.verify(data, fix_hints=True)
        warnings = result.warnings
        assert len(warnings) > 0
        for warn in warnings:
            assert warn.fix_hint is not None


class TestVerifyParallel:
    def test_parallel_basic(self):
        """verify_parallel() returns correct results."""
        import fitsverify
        files = [
            _fits_path("valid_minimal.fits"),
            _fits_path("err_bad_bitpix.fits"),
        ]
        results = fitsverify.verify_parallel(files, max_workers=2)
        assert len(results) == 2
        assert results[0].is_valid
        assert not results[1].is_valid

    def test_parallel_matches_sequential(self):
        """verify_parallel() matches verify_all() results."""
        import fitsverify
        files = [
            _fits_path("valid_minimal.fits"),
            _fits_path("valid_multi_ext.fits"),
            _fits_path("err_bad_bitpix.fits"),
        ]
        seq = fitsverify.verify_all(files)
        par = fitsverify.verify_parallel(files, max_workers=2)
        assert len(seq) == len(par)
        for s, p in zip(seq, par):
            assert s.is_valid == p.is_valid
            assert s.num_errors == p.num_errors
            assert s.num_warnings == p.num_warnings
