/*
 * fv_hints.c — fix hints and explanations for all fv_error_code values
 *
 * Each entry provides:
 *   fix_hint: short, actionable suggestion (1-2 lines)
 *   explain:  beginner-friendly explanation of what the error means
 *
 * The lookup function uses a switch for O(1) dispatch.
 */
#include <stdio.h>
#include <string.h>
#include "fv_hints.h"
#include "fv_context.h"
#include "fitsio.h"

/* ---- File / HDU structure (100-149) ------------------------------------ */

static const fv_hint h_extra_hdus = {
    "Remove extraneous data after the last valid HDU.",
    "The file contains additional HDU-like structures beyond what is expected. "
    "This usually indicates file corruption or an incomplete write."
};

static const fv_hint h_extra_bytes = {
    "Truncate the file at the end of the last HDU's 2880-byte block.",
    "FITS files must end exactly at a 2880-byte block boundary after the last HDU. "
    "Extra bytes beyond this boundary violate the standard and may indicate "
    "file corruption or concatenation errors."
};

static const fv_hint h_bad_hdu = {
    "Check the HDU structure; the header or data section may be malformed.",
    "The HDU could not be parsed correctly. This may indicate a corrupted header, "
    "incorrect NAXIS/NAXISn values, or a data section that does not match the "
    "header description."
};

static const fv_hint h_read_fail = {
    "Check that the file is accessible and not truncated.",
    "An error occurred while reading the file data. The file may be truncated, "
    "the disk may have errors, or the file may not be a valid FITS file."
};

/* ---- Mandatory keyword (150-199) --------------------------------------- */

static const fv_hint h_missing_keyword = {
    "Add the missing mandatory keyword to the header.",
    "Certain keywords are required by the FITS Standard in every HDU. "
    "For the primary HDU: SIMPLE, BITPIX, NAXIS, and NAXISn. "
    "For extensions: XTENSION, BITPIX, NAXIS, NAXISn, PCOUNT, GCOUNT."
};

static const fv_hint h_keyword_order = {
    "Reorder mandatory keywords to follow the FITS Standard sequence.",
    "Mandatory keywords must appear in a specific order at the beginning of the "
    "header. For example, SIMPLE must be first in the primary HDU, followed by "
    "BITPIX, NAXIS, and NAXISn in sequence."
};

static const fv_hint h_keyword_duplicate = {
    "Remove the duplicate mandatory keyword; it must appear exactly once.",
    "Mandatory keywords must appear only once in a header. Having duplicates "
    "creates ambiguity about which value should be used."
};

static const fv_hint h_keyword_value = {
    "Correct the keyword value to a legal value per the FITS Standard.",
    "The mandatory keyword has a value that is not permitted by the standard. "
    "For example, BITPIX must be one of 8, 16, 32, 64, -32, or -64."
};

static const fv_hint h_keyword_type = {
    "Change the keyword value to the required datatype (integer, string, etc.).",
    "FITS requires mandatory keywords to have specific datatypes. For example, "
    "BITPIX and NAXIS must be integers, not floating-point or string values."
};

static const fv_hint h_missing_end = {
    "Add an END keyword and pad the header to a 2880-byte boundary.",
    "Every FITS header must terminate with an END keyword in columns 1-3, "
    "followed by blank-filled records to complete the 2880-byte block."
};

static const fv_hint h_end_not_blank = {
    "Fill columns 9-80 of the END keyword record with blank spaces.",
    "The END keyword card must have blanks (ASCII 32) in columns 9 through 80. "
    "No other characters are permitted after 'END' on this card."
};

static const fv_hint h_not_fixed_format = {
    "Write the mandatory keyword value in fixed format (value in columns 11-30).",
    "Mandatory keywords must use fixed-format notation: the value indicator '= ' "
    "in columns 9-10, and the value right-justified in columns 11-30."
};

/* ---- Keyword format / value (200-249) ---------------------------------- */

static const fv_hint h_nonascii_header = {
    "Replace non-ASCII characters with printable ASCII (codes 32-126).",
    "FITS headers are restricted to the printable ASCII character set "
    "(codes 32 through 126). Characters outside this range — including tabs, "
    "extended Latin, or UTF-8 — are not permitted."
};

static const fv_hint h_illegal_name_char = {
    "Rename the keyword using only uppercase A-Z, digits 0-9, hyphen, and underscore.",
    "FITS keyword names may only contain uppercase Latin letters, digits, "
    "hyphens, and underscores. Lowercase letters and other characters are not allowed. "
    "The name must be left-justified in columns 1-8."
};

static const fv_hint h_name_not_justified = {
    "Left-justify the keyword name in columns 1-8.",
    "Keyword names must start in column 1 with no leading spaces."
};

static const fv_hint h_bad_value_format = {
    "Fix the keyword value to conform to FITS value-field syntax.",
    "The value field (columns 11-80) must follow FITS formatting rules: "
    "strings in single quotes, integers without decimal points, "
    "floating-point with decimal point, logical as T or F in column 30."
};

static const fv_hint h_no_value_separator = {
    "Add a '/' separator between the value and comment fields.",
    "When both a value and comment are present, they must be separated by "
    "a slash character '/'. The slash should follow the value (after any trailing spaces)."
};

static const fv_hint h_bad_string = {
    "Ensure string values contain only printable ASCII characters.",
    "String keyword values (enclosed in single quotes) must contain only "
    "printable ASCII characters (codes 32-126). Control characters and "
    "non-ASCII bytes are not permitted."
};

static const fv_hint h_missing_quote = {
    "Add the missing closing single quote to the string value.",
    "String values must be enclosed in single quotes. A string that starts "
    "with a quote in column 11 must have a matching closing quote within "
    "columns 11-80 (or use the CONTINUE long-string convention)."
};

static const fv_hint h_bad_logical = {
    "Set the logical value to T or F in column 30.",
    "Logical (boolean) keyword values must be the character T (true) or "
    "F (false) in column 30, with spaces in columns 11-29."
};

static const fv_hint h_bad_number = {
    "Fix the numeric value to use valid FITS integer or floating-point format.",
    "Numeric values must follow Fortran-style formatting: integers with optional "
    "sign, floating-point with a decimal point, and optional exponent using 'E' or 'D'."
};

static const fv_hint h_lowercase_exponent = {
    "Change the lowercase exponent letter (d/e) to uppercase (D/E).",
    "The FITS Standard requires that exponent indicators in floating-point "
    "values use uppercase 'E' or 'D', not lowercase."
};

static const fv_hint h_complex_format = {
    "Format the complex value as (real, imaginary) with proper parentheses and comma.",
    "Complex keyword values must be written as two numbers enclosed in parentheses "
    "and separated by a comma, e.g. (1.0, 2.0)."
};

static const fv_hint h_bad_comment = {
    "Remove non-printable characters from the comment field.",
    "Comments (after the '/' separator) may only contain printable ASCII characters."
};

static const fv_hint h_unknown_type = {
    "Check that the keyword value conforms to one of the FITS value types.",
    "The keyword value does not match any recognized FITS type (string, integer, "
    "floating-point, complex, or logical). Verify the formatting."
};

static const fv_hint h_wrong_type = {
    "Change the keyword value to the expected datatype.",
    "This keyword is expected to have a specific datatype (e.g., string, integer) "
    "but the value found is of a different type."
};

static const fv_hint h_null_value = {
    "Provide a value for the keyword, or remove it if not needed.",
    "The keyword has no value (the value field is blank). If the keyword is "
    "intended to carry information, it needs a valid value."
};

static const fv_hint h_card_too_long = {
    "Ensure the header card does not exceed 80 characters.",
    "Each FITS header record is exactly 80 characters. Cards longer than 80 "
    "characters violate the standard."
};

static const fv_hint h_nontext_chars = {
    "Remove non-text characters from the string value.",
    "String values should contain only text characters. Control characters "
    "or other non-printable bytes are not permitted."
};

static const fv_hint h_leading_space = {
    "Remove leading spaces from the keyword value.",
    "Certain keyword values (XTENSION, TFORMn, TDISPn, TDIMn) must not have "
    "leading spaces within the quoted string."
};

static const fv_hint h_reserved_value = {
    "Correct the reserved keyword to its required value.",
    "Reserved keywords (like EXTEND, BLOCKED) have specific allowed values "
    "defined by the FITS Standard."
};

/* ---- HDU-type keyword placement (250-299) ------------------------------ */

static const fv_hint h_xtension_in_primary = {
    "Remove the XTENSION keyword from the primary HDU.",
    "XTENSION is used to identify extension HDUs. It must not appear in the "
    "primary HDU, which uses the SIMPLE keyword instead."
};

static const fv_hint h_image_key_in_table = {
    "Remove image-specific keywords (BSCALE, BZERO, BUNIT, BLANK, DATAMAX, DATAMIN) from the table HDU.",
    "Keywords like BSCALE, BZERO, BUNIT, BLANK, DATAMAX, and DATAMIN are only "
    "valid in image HDUs. In table HDUs, use the column-specific equivalents "
    "(TSCALn, TZEROn, TUNITn, TNULLn)."
};

static const fv_hint h_table_key_in_image = {
    "Remove table-specific keywords (TFIELDS, TTYPEn, TFORMn, etc.) from the image HDU.",
    "Column-related keywords like TFIELDS, TTYPEn, TFORMn, TBCOLn are only "
    "valid in table extensions (ASCII_TBL or BINARY_TBL), not in images."
};

static const fv_hint h_primary_key_in_ext = {
    "Remove SIMPLE, EXTEND, or BLOCKED from this extension HDU.",
    "The keywords SIMPLE, EXTEND, and BLOCKED are only valid in the primary HDU. "
    "They must not appear in any extension."
};

static const fv_hint h_table_wcs_in_image = {
    "Remove table WCS keywords (TCTYPn, TCRPXn, TCRVLn, etc.) from the image HDU.",
    "Table-specific WCS keywords (those with column index 'n') are only valid "
    "in table extensions. Image HDUs use CTYPEn, CRPIXn, CRVALn without the 'T' prefix."
};

static const fv_hint h_keyword_not_allowed = {
    "Remove the keyword that is not permitted in this HDU type.",
    "This keyword is not valid in the current HDU type. Check the FITS Standard "
    "for which keywords are allowed in each HDU type."
};

/* ---- Table structure (300-349) ----------------------------------------- */

static const fv_hint h_bad_tfields = {
    "Set TFIELDS to the correct number of columns in the table.",
    "TFIELDS specifies how many columns the table contains. It must match "
    "the actual number of TFORMn keywords present."
};

static const fv_hint h_naxis1_mismatch = {
    "Adjust NAXIS1 to equal the sum of all column widths.",
    "In a table HDU, NAXIS1 is the number of bytes per row. It must equal "
    "the sum of the widths of all columns as specified by TFORMn (and TBCOLn "
    "for ASCII tables)."
};

static const fv_hint h_bad_tform = {
    "Correct the TFORMn value to a valid FITS column format.",
    "TFORMn specifies the data format for column n. Valid formats include "
    "integer widths for ASCII tables (e.g., I10, F12.5) and type codes for "
    "binary tables (e.g., 1J, 20A, 1E)."
};

static const fv_hint h_bad_tdisp = {
    "Fix TDISPn to be consistent with the column datatype.",
    "TDISPn specifies the display format for column n. It must be compatible "
    "with the column's data format (e.g., an integer column should not have "
    "a floating-point TDISPn)."
};

static const fv_hint h_index_exceeds_tfields = {
    "Ensure column keyword index n does not exceed the TFIELDS value.",
    "A column-indexed keyword (TTYPEn, TFORMn, etc.) has an index greater "
    "than TFIELDS. Either increase TFIELDS or remove the excess keyword."
};

static const fv_hint h_tscal_wrong_type = {
    "Remove TSCALn/TZEROn from ASCII, logical, or bit columns.",
    "TSCALn and TZEROn are scaling keywords valid only for numeric binary table "
    "columns (integer or floating-point). They are not applicable to ASCII, "
    "logical, or bit-type columns."
};

static const fv_hint h_tnull_wrong_type = {
    "Remove TNULLn from this floating-point column; use NaN instead.",
    "TNULLn defines a null value for integer columns only. For floating-point "
    "columns, IEEE NaN is the standard null representation."
};

static const fv_hint h_blank_wrong_type = {
    "Remove BLANK from this floating-point image; use NaN instead.",
    "The BLANK keyword defines null pixels for integer images only. "
    "For floating-point images (BITPIX = -32 or -64), IEEE NaN represents null."
};

static const fv_hint h_theap_no_pcount = {
    "Remove THEAP or set PCOUNT > 0 to allocate a variable-length data heap.",
    "THEAP specifies the heap offset for variable-length arrays. It is meaningless "
    "when PCOUNT = 0 (no heap exists)."
};

static const fv_hint h_tdim_in_ascii = {
    "Remove TDIMn from the ASCII table; it is only valid for binary tables.",
    "TDIMn defines multi-dimensional array structure for binary table columns. "
    "ASCII tables do not support this feature."
};

static const fv_hint h_tbcol_in_binary = {
    "Remove TBCOLn from the binary table; it is only valid for ASCII tables.",
    "TBCOLn specifies the starting column position in ASCII tables. Binary tables "
    "use sequential packing based on TFORMn and do not use TBCOLn."
};

static const fv_hint h_var_format = {
    "Fix the variable-length array format descriptor in TFORMn.",
    "Variable-length array columns use the format 'nPt(max)' or 'nQt(max)' "
    "where t is the data type code. Check that the format string is valid."
};

static const fv_hint h_tbcol_mismatch = {
    "Correct TBCOLn values so columns are properly positioned within the row.",
    "TBCOLn values must correctly specify the starting byte position of each "
    "column, forming a consistent layout that does not exceed NAXIS1."
};

/* ---- Data validation (350-399) ----------------------------------------- */

static const fv_hint h_var_exceeds_maxlen = {
    "Reduce the variable-length array size or increase the maximum in TFORMn.",
    "A variable-length array entry exceeds the maximum length declared in "
    "the TFORMn descriptor (the value in parentheses). Either the data is "
    "corrupt or the declared maximum is too small."
};

static const fv_hint h_var_exceeds_heap = {
    "Fix the variable-length array descriptor; its address extends beyond the heap.",
    "The descriptor for a variable-length array column points to an address "
    "outside the allocated heap area (beyond PCOUNT bytes after the fixed table). "
    "This usually indicates data corruption."
};

static const fv_hint h_bit_not_justified = {
    "Left-justify the bit values and zero-fill unused trailing bits.",
    "Bit columns (TFORMn = 'nX') must be left-justified, with any unused "
    "bits in the last byte set to zero."
};

static const fv_hint h_bad_logical_data = {
    "Set logical column values to 'T' (true), 'F' (false), or 0 (null).",
    "Logical columns in binary tables may only contain the byte values "
    "'T' (0x54), 'F' (0x46), or 0 (null/undefined)."
};

static const fv_hint h_nonascii_data = {
    "Replace non-ASCII characters in the string column with printable ASCII.",
    "String columns in binary tables must contain only printable ASCII characters "
    "or null bytes for padding."
};

static const fv_hint h_no_decimal = {
    "Add a decimal point to the floating-point value in the ASCII table.",
    "Floating-point values in ASCII table columns (TFORMn = En.d, Fn.d, Dn.d) "
    "must contain a decimal point."
};

static const fv_hint h_embedded_space = {
    "Remove embedded spaces from the numeric value in the ASCII table.",
    "Numeric values in ASCII table columns must not contain embedded spaces. "
    "Leading spaces are allowed, but spaces within the number are not."
};

static const fv_hint h_nonascii_table = {
    "Replace non-ASCII characters in the ASCII table with valid ASCII.",
    "ASCII tables must contain only ASCII characters (codes 0-127). "
    "Characters with values above 127 violate the standard."
};

static const fv_hint h_data_fill = {
    "Fix data fill bytes: use blanks (0x20) for ASCII tables, zeros (0x00) for others.",
    "Fill bytes after the last row of data must be ASCII blanks (space, 0x20) "
    "for ASCII tables, or binary zeros (0x00) for all other HDU types, "
    "out to the next 2880-byte boundary."
};

static const fv_hint h_header_fill = {
    "Fill unused header bytes after END with blank spaces (ASCII 32).",
    "All bytes in the header block after the END keyword must be filled with "
    "ASCII blank characters (space, code 32) up to the 2880-byte boundary."
};

static const fv_hint h_ascii_gap = {
    "Replace characters with values > 127 in ASCII table column gaps.",
    "Gaps between defined columns in ASCII tables (bytes not covered by any "
    "TBCOLn/TFORMn range) must contain only ASCII characters (codes 0-127)."
};

/* ---- WCS (400-419) ----------------------------------------------------- */

static const fv_hint h_wcsaxes_order = {
    "Move WCSAXES before all other WCS keywords in the header.",
    "When present, the WCSAXES keyword must appear before any other WCS "
    "keywords (CRPIXn, CRVALn, CTYPEn, CDELTn, etc.) so that the WCS "
    "dimensionality is known before the per-axis keywords are read."
};

static const fv_hint h_wcs_index = {
    "Reduce the WCS keyword index to not exceed the WCSAXES value.",
    "WCS keywords with axis indices (CRPIXn, CRVALn, etc.) must have "
    "index n <= WCSAXES. Indices beyond this range are invalid."
};

/* ---- CFITSIO / I-O (450-479) ------------------------------------------- */

static const fv_hint h_cfitsio = {
    "Check the CFITSIO error message for details on the I/O or parsing failure.",
    "CFITSIO reported an error while processing the file. This may indicate "
    "file corruption, an unsupported feature, or a system I/O problem."
};

static const fv_hint h_cfitsio_stack = {
    "Review the CFITSIO error stack messages for the root cause.",
    "CFITSIO reported one or more errors. The error stack shows the sequence "
    "of CFITSIO function calls that led to the failure."
};

/* ---- Internal / abort (480-499) ---------------------------------------- */

static const fv_hint h_too_many = {
    "Fix the most critical errors first; the file has too many problems to list completely.",
    "Verification was aborted because the error count exceeded the maximum "
    "threshold (200). The file likely has a fundamental structural problem "
    "that causes cascading errors."
};

/* ---- Warnings (500-599) ------------------------------------------------ */

static const fv_hint h_simple_false = {
    "Set SIMPLE = T unless the file intentionally uses non-standard features.",
    "SIMPLE = F indicates the file may not conform to the FITS Standard. "
    "Most FITS readers expect SIMPLE = T. Only use F if the file contains "
    "non-standard data that requires special handling."
};

static const fv_hint h_deprecated = {
    "Replace deprecated keywords: EPOCH -> EQUINOX, BLOCKED -> (remove).",
    "The EPOCH keyword is deprecated in favor of EQUINOX. The BLOCKED keyword "
    "is deprecated and should be removed; it was related to tape blocking "
    "which is no longer relevant."
};

static const fv_hint h_duplicate_extname = {
    "Give each HDU a unique combination of EXTNAME, EXTVER, and EXTLEVEL.",
    "Multiple HDUs share the same EXTNAME, EXTVER, and EXTLEVEL values. "
    "While not strictly forbidden, this makes it impossible to uniquely "
    "identify HDUs by name, which breaks many FITS tools."
};

static const fv_hint h_zero_scale = {
    "Set BSCALE/TSCALn to a non-zero value.",
    "A scale factor of zero would map all raw values to the same physical value "
    "(the offset), which is almost certainly unintended. The standard formula "
    "is: physical = raw * BSCALE + BZERO."
};

static const fv_hint h_tnull_range = {
    "Set BLANK/TNULLn to a value within the valid range for the datatype.",
    "The null value indicator must be representable in the column's or image's "
    "datatype. For example, TNULLn for a 16-bit integer column must be between "
    "-32768 and 32767."
};

static const fv_hint h_raw_not_multiple = {
    "Adjust the TFORMn 'rAw' format so r is a multiple of w.",
    "For character columns in binary tables with format rAw, the repeat count r "
    "should be a multiple of the character width w. Otherwise the last "
    "sub-string is truncated."
};

static const fv_hint h_y2k = {
    "Use the DATE format 'YYYY-MM-DD' instead of 'DD/MM/YY'.",
    "The old DATE format 'DD/MM/YY' is ambiguous for years near 2000. "
    "The FITS Standard requires the ISO 8601 format 'YYYY-MM-DD' "
    "(or 'YYYY-MM-DDThh:mm:ss')."
};

static const fv_hint h_wcs_index_warn = {
    "Add a WCSAXES keyword, or ensure WCS indices do not exceed NAXIS.",
    "A WCS keyword has an axis index exceeding NAXIS. If the WCS has more "
    "axes than the data (e.g., for celestial + spectral), add WCSAXES to "
    "declare the WCS dimensionality."
};

static const fv_hint h_duplicate_keyword = {
    "Remove the duplicate keyword or rename one of the copies.",
    "The same keyword appears more than once in the header. Only COMMENT, "
    "HISTORY, blank, and CONTINUE keywords may be duplicated."
};

static const fv_hint h_bad_column_name = {
    "Rename the column using only letters, digits, and underscores.",
    "Column names (TTYPEn) should contain only letters (A-Z, a-z), "
    "digits (0-9), and underscores. Other characters may cause problems "
    "with FITS processing software."
};

static const fv_hint h_no_column_name = {
    "Add a TTYPEn keyword to give the column a descriptive name.",
    "Every table column should have a TTYPEn keyword with a descriptive name. "
    "While technically optional, unnamed columns are difficult to work with "
    "in most FITS tools."
};

static const fv_hint h_duplicate_column = {
    "Rename one of the duplicate columns to have a unique TTYPEn value.",
    "Multiple columns share the same name. While not forbidden by the standard, "
    "duplicate column names cause ambiguity when accessing columns by name."
};

static const fv_hint h_bad_checksum = {
    "Recompute CHECKSUM and DATASUM using a FITS checksum utility (e.g., fchecksum).",
    "The stored CHECKSUM or DATASUM does not match the computed value, "
    "indicating the file has been modified since the checksums were written. "
    "Recompute them if the current data is correct, or investigate if the "
    "file may be corrupt."
};

static const fv_hint h_missing_longstrn = {
    "Add 'LONGSTRN = OGIP 1.0' to the header when using CONTINUE long strings.",
    "The header uses CONTINUE keywords for long string values but lacks the "
    "LONGSTRN convention keyword that declares this usage."
};

static const fv_hint h_var_exceeds_32bit = {
    "Use 'Q' format (64-bit descriptor) instead of 'P' for large variable-length arrays.",
    "A variable-length array descriptor value exceeds the 32-bit range. "
    "The 'P' format uses 32-bit descriptors (max ~2 GB). For larger data, "
    "use the 'Q' format with 64-bit descriptors."
};

static const fv_hint h_hierarch_duplicate = {
    "Remove or rename the duplicate HIERARCH keyword.",
    "The same HIERARCH keyword appears more than once. Each HIERARCH keyword "
    "should be unique within the header."
};

static const fv_hint h_pcount_no_vla = {
    "Set PCOUNT = 0 or add variable-length array columns.",
    "PCOUNT is non-zero (indicating a variable-length data heap exists) but "
    "no columns use variable-length array format (P or Q descriptors). "
    "The heap space appears unused."
};

static const fv_hint h_continue_char = {
    "Remove the trailing '&' from the column name unless CONTINUE convention is intended.",
    "A column name (TTYPEn) contains an ampersand '&', which is the continuation "
    "character used in the CONTINUE long-string convention. This is unusual for "
    "a column name and may indicate a formatting error."
};

static const fv_hint h_random_groups = {
    "Convert Random Groups data to a binary table extension.",
    "The Random Groups convention has been deprecated since FITS Standard "
    "Version 1. Binary table extensions provide equivalent functionality with "
    "better tool support. See FITS Standard Section 7."
};

static const fv_hint h_legacy_xtension = {
    "Use a standard XTENSION value: IMAGE, TABLE, or BINTABLE.",
    "The FITS Standard defines only three XTENSION values: IMAGE, TABLE, and "
    "BINTABLE. Other values (A3DTABLE, IUEIMAGE, FOREIGN, DUMP) are legacy "
    "or non-standard and may not be supported by FITS readers."
};

static const fv_hint h_timesys_value = {
    "Set TIMESYS to a recognized time scale (e.g., UTC, TAI, TDB, TT).",
    "TIMESYS specifies the time scale for time-related keywords. "
    "Allowed values: UTC, TAI, TDB, TT, ET, UT1, UT, TCG, TCB, TDT, IAT, "
    "GPS, LOCAL. See FITS Standard Section 4.4.2.6."
};

static const fv_hint h_inherit_primary = {
    "Remove INHERIT or ensure the primary HDU has NAXIS = 0.",
    "INHERIT = T allows extensions to inherit primary header keywords, "
    "but is only meaningful when the primary HDU has no data (NAXIS = 0). "
    "See FITS Standard Section 4.4.2.4."
};

/* ---- Lookup function --------------------------------------------------- */

const fv_hint *fv_get_hint(fv_error_code code)
{
    switch (code) {
        /* File / HDU structure */
        case FV_ERR_EXTRA_HDUS:          return &h_extra_hdus;
        case FV_ERR_EXTRA_BYTES:         return &h_extra_bytes;
        case FV_ERR_BAD_HDU:             return &h_bad_hdu;
        case FV_ERR_READ_FAIL:           return &h_read_fail;

        /* Mandatory keyword */
        case FV_ERR_MISSING_KEYWORD:     return &h_missing_keyword;
        case FV_ERR_KEYWORD_ORDER:       return &h_keyword_order;
        case FV_ERR_KEYWORD_DUPLICATE:   return &h_keyword_duplicate;
        case FV_ERR_KEYWORD_VALUE:       return &h_keyword_value;
        case FV_ERR_KEYWORD_TYPE:        return &h_keyword_type;
        case FV_ERR_MISSING_END:         return &h_missing_end;
        case FV_ERR_END_NOT_BLANK:       return &h_end_not_blank;
        case FV_ERR_NOT_FIXED_FORMAT:    return &h_not_fixed_format;

        /* Keyword format / value */
        case FV_ERR_NONASCII_HEADER:     return &h_nonascii_header;
        case FV_ERR_ILLEGAL_NAME_CHAR:   return &h_illegal_name_char;
        case FV_ERR_NAME_NOT_JUSTIFIED:  return &h_name_not_justified;
        case FV_ERR_BAD_VALUE_FORMAT:    return &h_bad_value_format;
        case FV_ERR_NO_VALUE_SEPARATOR:  return &h_no_value_separator;
        case FV_ERR_BAD_STRING:          return &h_bad_string;
        case FV_ERR_MISSING_QUOTE:       return &h_missing_quote;
        case FV_ERR_BAD_LOGICAL:         return &h_bad_logical;
        case FV_ERR_BAD_NUMBER:          return &h_bad_number;
        case FV_ERR_LOWERCASE_EXPONENT:  return &h_lowercase_exponent;
        case FV_ERR_COMPLEX_FORMAT:      return &h_complex_format;
        case FV_ERR_BAD_COMMENT:         return &h_bad_comment;
        case FV_ERR_UNKNOWN_TYPE:        return &h_unknown_type;
        case FV_ERR_WRONG_TYPE:          return &h_wrong_type;
        case FV_ERR_NULL_VALUE:          return &h_null_value;
        case FV_ERR_CARD_TOO_LONG:       return &h_card_too_long;
        case FV_ERR_NONTEXT_CHARS:       return &h_nontext_chars;
        case FV_ERR_LEADING_SPACE:       return &h_leading_space;
        case FV_ERR_RESERVED_VALUE:      return &h_reserved_value;

        /* HDU-type keyword placement */
        case FV_ERR_XTENSION_IN_PRIMARY: return &h_xtension_in_primary;
        case FV_ERR_IMAGE_KEY_IN_TABLE:  return &h_image_key_in_table;
        case FV_ERR_TABLE_KEY_IN_IMAGE:  return &h_table_key_in_image;
        case FV_ERR_PRIMARY_KEY_IN_EXT:  return &h_primary_key_in_ext;
        case FV_ERR_TABLE_WCS_IN_IMAGE:  return &h_table_wcs_in_image;
        case FV_ERR_KEYWORD_NOT_ALLOWED: return &h_keyword_not_allowed;

        /* Table structure */
        case FV_ERR_BAD_TFIELDS:         return &h_bad_tfields;
        case FV_ERR_NAXIS1_MISMATCH:     return &h_naxis1_mismatch;
        case FV_ERR_BAD_TFORM:           return &h_bad_tform;
        case FV_ERR_BAD_TDISP:           return &h_bad_tdisp;
        case FV_ERR_INDEX_EXCEEDS_TFIELDS: return &h_index_exceeds_tfields;
        case FV_ERR_TSCAL_WRONG_TYPE:    return &h_tscal_wrong_type;
        case FV_ERR_TNULL_WRONG_TYPE:    return &h_tnull_wrong_type;
        case FV_ERR_BLANK_WRONG_TYPE:    return &h_blank_wrong_type;
        case FV_ERR_THEAP_NO_PCOUNT:     return &h_theap_no_pcount;
        case FV_ERR_TDIM_IN_ASCII:       return &h_tdim_in_ascii;
        case FV_ERR_TBCOL_IN_BINARY:     return &h_tbcol_in_binary;
        case FV_ERR_VAR_FORMAT:          return &h_var_format;
        case FV_ERR_TBCOL_MISMATCH:      return &h_tbcol_mismatch;

        /* Data validation */
        case FV_ERR_VAR_EXCEEDS_MAXLEN:  return &h_var_exceeds_maxlen;
        case FV_ERR_VAR_EXCEEDS_HEAP:    return &h_var_exceeds_heap;
        case FV_ERR_BIT_NOT_JUSTIFIED:   return &h_bit_not_justified;
        case FV_ERR_BAD_LOGICAL_DATA:    return &h_bad_logical_data;
        case FV_ERR_NONASCII_DATA:       return &h_nonascii_data;
        case FV_ERR_NO_DECIMAL:          return &h_no_decimal;
        case FV_ERR_EMBEDDED_SPACE:      return &h_embedded_space;
        case FV_ERR_NONASCII_TABLE:      return &h_nonascii_table;
        case FV_ERR_DATA_FILL:           return &h_data_fill;
        case FV_ERR_HEADER_FILL:         return &h_header_fill;
        case FV_ERR_ASCII_GAP:           return &h_ascii_gap;

        /* WCS */
        case FV_ERR_WCSAXES_ORDER:       return &h_wcsaxes_order;
        case FV_ERR_WCS_INDEX:           return &h_wcs_index;

        /* CFITSIO */
        case FV_ERR_CFITSIO:             return &h_cfitsio;
        case FV_ERR_CFITSIO_STACK:       return &h_cfitsio_stack;

        /* Internal */
        case FV_ERR_TOO_MANY:            return &h_too_many;

        /* Warnings */
        case FV_WARN_SIMPLE_FALSE:       return &h_simple_false;
        case FV_WARN_DEPRECATED:         return &h_deprecated;
        case FV_WARN_DUPLICATE_EXTNAME:  return &h_duplicate_extname;
        case FV_WARN_ZERO_SCALE:         return &h_zero_scale;
        case FV_WARN_TNULL_RANGE:        return &h_tnull_range;
        case FV_WARN_RAW_NOT_MULTIPLE:   return &h_raw_not_multiple;
        case FV_WARN_Y2K:               return &h_y2k;
        case FV_WARN_WCS_INDEX:          return &h_wcs_index_warn;
        case FV_WARN_DUPLICATE_KEYWORD:  return &h_duplicate_keyword;
        case FV_WARN_BAD_COLUMN_NAME:    return &h_bad_column_name;
        case FV_WARN_NO_COLUMN_NAME:     return &h_no_column_name;
        case FV_WARN_DUPLICATE_COLUMN:   return &h_duplicate_column;
        case FV_WARN_BAD_CHECKSUM:       return &h_bad_checksum;
        case FV_WARN_MISSING_LONGSTRN:   return &h_missing_longstrn;
        case FV_WARN_VAR_EXCEEDS_32BIT:  return &h_var_exceeds_32bit;
        case FV_WARN_HIERARCH_DUPLICATE: return &h_hierarch_duplicate;
        case FV_WARN_PCOUNT_NO_VLA:      return &h_pcount_no_vla;
        case FV_WARN_CONTINUE_CHAR:      return &h_continue_char;
        case FV_WARN_RANDOM_GROUPS:      return &h_random_groups;
        case FV_WARN_LEGACY_XTENSION:    return &h_legacy_xtension;
        case FV_WARN_TIMESYS_VALUE:      return &h_timesys_value;
        case FV_WARN_INHERIT_PRIMARY:    return &h_inherit_primary;

        default:
            return NULL;
    }
}

/* ---- Helper functions for context-aware hints -------------------------- */

static const char *fv_hdu_type_name(int curtype)
{
    switch (curtype) {
        case IMAGE_HDU:  return "an image extension";
        case ASCII_TBL:  return "an ASCII table";
        case BINARY_TBL: return "a binary table";
        default:         return "an HDU";
    }
}

static const char *fv_hdu_type_name_with_primary(int curtype, int curhdu)
{
    if (curhdu == 1) return "a primary array";
    return fv_hdu_type_name(curtype);
}

static const char *fv_mandatory_keyword_list(int curtype, int curhdu)
{
    if (curhdu == 1)
        return "SIMPLE, BITPIX, NAXIS, NAXISn, END";
    switch (curtype) {
        case IMAGE_HDU:
            return "XTENSION, BITPIX, NAXIS, NAXISn, PCOUNT, GCOUNT, END";
        case ASCII_TBL:
            return "XTENSION, BITPIX, NAXIS, NAXIS1, NAXIS2, PCOUNT, GCOUNT, TFIELDS, TBCOLn, TFORMn, END";
        case BINARY_TBL:
            return "XTENSION, BITPIX, NAXIS, NAXIS1, NAXIS2, PCOUNT, GCOUNT, TFIELDS, TFORMn, END";
        default:
            return "XTENSION, BITPIX, NAXIS, NAXISn, PCOUNT, GCOUNT, END";
    }
}

static const char *fv_keyword_purpose(const char *kw)
{
    if (!kw || !kw[0]) return NULL;
    if (!strcmp(kw, "SIMPLE"))   return "'SIMPLE' indicates whether the file conforms to the FITS Standard (T = conforming).";
    if (!strcmp(kw, "BITPIX"))   return "'BITPIX' specifies the number of bits per data element (e.g., 8 for bytes, 16 for short integers, -32 for single-precision floats).";
    if (!strcmp(kw, "NAXIS"))    return "'NAXIS' specifies the number of axes (dimensions) in the data array.";
    if (!strncmp(kw, "NAXIS", 5)) return "NAXISn specifies the size of axis n in the data array.";
    if (!strcmp(kw, "XTENSION")) return "'XTENSION' identifies the type of extension (e.g., 'IMAGE', 'TABLE', 'BINTABLE').";
    if (!strcmp(kw, "PCOUNT"))   return "'PCOUNT' is the number of bytes of supplemental data following the main data table (the heap for variable-length arrays).";
    if (!strcmp(kw, "GCOUNT"))   return "'GCOUNT' is the number of groups (always 1 for standard extensions).";
    if (!strcmp(kw, "TFIELDS"))  return "'TFIELDS' specifies the number of columns in a table.";
    if (!strcmp(kw, "EXTEND"))   return "'EXTEND' indicates whether the file may contain extensions after the primary HDU.";
    if (!strcmp(kw, "END"))      return "'END' marks the end of the header; all remaining bytes to the 2880-byte boundary must be blank (ASCII 32).";
    if (!strncmp(kw, "TFORM", 5)) return "TFORMn specifies the data format for column n (e.g., '1J' for 32-bit integer, '20A' for 20-character string).";
    if (!strncmp(kw, "TTYPE", 5)) return "TTYPEn gives column n a descriptive name for identification.";
    if (!strncmp(kw, "TUNIT", 5)) return "TUNITn specifies the physical units of the data in column n.";
    if (!strncmp(kw, "TBCOL", 5)) return "TBCOLn specifies the starting byte position of column n within each row of an ASCII table.";
    if (!strncmp(kw, "TSCAL", 5)) return "TSCALn is the linear scaling factor for column n: physical = raw * TSCALn + TZEROn.";
    if (!strncmp(kw, "TZERO", 5)) return "TZEROn is the offset applied after scaling for column n: physical = raw * TSCALn + TZEROn.";
    if (!strncmp(kw, "TNULL", 5)) return "TNULLn defines the value used to represent undefined (null) entries in integer column n.";
    if (!strncmp(kw, "TDISP", 5)) return "TDISPn specifies the display format for column n (e.g., 'I10', 'F12.5').";
    if (!strncmp(kw, "TDIM",  4)) return "TDIMn describes the multi-dimensional shape of column n's array data (e.g., '(100,200)').";
    if (!strcmp(kw, "BSCALE"))   return "'BSCALE' is the linear scaling factor for image pixels: physical = raw * BSCALE + BZERO.";
    if (!strcmp(kw, "BZERO"))    return "'BZERO' is the offset applied after scaling for image pixels.";
    if (!strcmp(kw, "BUNIT"))    return "'BUNIT' specifies the physical units of the image pixel values.";
    if (!strcmp(kw, "BLANK"))    return "'BLANK' defines the integer value used to represent undefined pixels in integer images.";
    if (!strcmp(kw, "DATAMAX"))  return "'DATAMAX' records the maximum data value in the image.";
    if (!strcmp(kw, "DATAMIN"))  return "'DATAMIN' records the minimum data value in the image.";
    if (!strcmp(kw, "BLOCKED"))  return "'BLOCKED' is a deprecated keyword formerly used for tape blocking.";
    if (!strcmp(kw, "EPOCH"))    return "'EPOCH' is deprecated; use 'EQUINOX' instead to specify the equinox of celestial coordinates.";
    if (!strcmp(kw, "THEAP"))    return "'THEAP' specifies the byte offset of the heap area in a binary table with variable-length arrays.";
    if (!strcmp(kw, "WCSAXES")) return "'WCSAXES' declares the number of WCS axes, which may differ from NAXIS.";
    if (!strcmp(kw, "TIMESYS")) return "'TIMESYS' specifies the time scale used for time-related keywords (e.g., UTC, TAI, TDB).";
    if (!strcmp(kw, "MJDREF"))  return "'MJDREF' specifies the reference Modified Julian Date for time coordinates.";
    if (!strcmp(kw, "DATEREF")) return "'DATEREF' specifies the reference date/time for time coordinates in ISO 8601 format.";
    if (!strcmp(kw, "TIMEUNIT")) return "'TIMEUNIT' specifies the units of time-related keywords (e.g., 's' for seconds, 'd' for days).";
    return NULL;
}

static const char *fv_keyword_section(const char *kw)
{
    if (!kw || !kw[0]) return NULL;
    if (!strcmp(kw, "SIMPLE"))   return "Section 4.4.1.1";
    if (!strcmp(kw, "BITPIX"))   return "Section 4.4.1.1";
    if (!strcmp(kw, "NAXIS"))    return "Section 4.4.1.1";
    if (!strncmp(kw, "NAXIS", 5)) return "Section 4.4.1.1";
    if (!strcmp(kw, "XTENSION")) return "Section 7.1";
    if (!strcmp(kw, "PCOUNT"))   return "Section 7.1";
    if (!strcmp(kw, "GCOUNT"))   return "Section 7.1";
    if (!strcmp(kw, "TFIELDS"))  return "Section 7.2.1";
    if (!strcmp(kw, "EXTEND"))   return "Section 4.4.2.1";
    if (!strcmp(kw, "END"))      return "Section 4.3.1";
    if (!strncmp(kw, "TFORM", 5)) return "Section 7.2.1 (ASCII), Section 7.3.1 (binary)";
    if (!strncmp(kw, "TTYPE", 5)) return "Section 7.2.1";
    if (!strncmp(kw, "TBCOL", 5)) return "Section 7.2.1";
    if (!strncmp(kw, "TSCAL", 5)) return "Section 7.3.2";
    if (!strncmp(kw, "TZERO", 5)) return "Section 7.3.2";
    if (!strncmp(kw, "TNULL", 5)) return "Section 7.3.2";
    if (!strncmp(kw, "TDISP", 5)) return "Section 7.3.3";
    if (!strncmp(kw, "TDIM",  4)) return "Section 7.3.2";
    if (!strcmp(kw, "BSCALE"))   return "Section 4.4.2.1";
    if (!strcmp(kw, "BZERO"))    return "Section 4.4.2.1";
    if (!strcmp(kw, "BUNIT"))    return "Section 4.4.2.1";
    if (!strcmp(kw, "BLANK"))    return "Section 4.4.2.1";
    if (!strcmp(kw, "THEAP"))    return "Section 7.3.1";
    if (!strcmp(kw, "WCSAXES")) return "Section 8.2";
    if (!strcmp(kw, "TIMESYS")) return "Section 8.4 (WCS Paper IV)";
    if (!strcmp(kw, "MJDREF"))  return "Section 8.4 (WCS Paper IV)";
    if (!strcmp(kw, "DATEREF")) return "Section 8.4 (WCS Paper IV)";
    if (!strcmp(kw, "TIMEUNIT")) return "Section 8.4 (WCS Paper IV)";
    return NULL;
}

/* ---- Context-aware hint generation ------------------------------------- */

const fv_hint *fv_generate_hint(struct fv_context *ctx, fv_error_code code)
{
    static fv_hint ctx_hint;
    const fv_hint *fallback;
    int has_kw, has_col;
    const char *kw;
    int hdu, type;
    const char *hdu_name;
    const char *purpose;
    const char *section;
    const char *mand_list;

    if (!ctx)
        return fv_get_hint(code);

    has_kw  = (ctx->hint_keyword[0] != '\0');
    has_col = (ctx->hint_colnum > 0);

    /* If no context is set, return the static hint */
    if (!has_kw && !has_col)
        return fv_get_hint(code);

    kw       = ctx->hint_keyword;
    hdu      = ctx->curhdu;
    type     = ctx->curtype;
    hdu_name = fv_hdu_type_name_with_primary(type, hdu);
    purpose  = has_kw ? fv_keyword_purpose(kw) : NULL;
    section  = has_kw ? fv_keyword_section(kw) : NULL;
    mand_list = fv_mandatory_keyword_list(type, hdu);

    /* Get the static fallback for cases we don't enhance */
    fallback = fv_get_hint(code);

    /* Default: copy static hints into buffers.
       hint_callsite is a bitmask: bit 0 = fix_buf set by call site,
       bit 1 = explain_buf set by call site. Skip overwriting those. */
    if (!(ctx->hint_callsite & 1)) {
        if (fallback && fallback->fix_hint) {
            strncpy(ctx->hint_fix_buf, fallback->fix_hint,
                    sizeof(ctx->hint_fix_buf) - 1);
            ctx->hint_fix_buf[sizeof(ctx->hint_fix_buf) - 1] = '\0';
        } else {
            ctx->hint_fix_buf[0] = '\0';
        }
    }
    if (!(ctx->hint_callsite & 2)) {
        if (fallback && fallback->explain) {
            strncpy(ctx->hint_explain_buf, fallback->explain,
                    sizeof(ctx->hint_explain_buf) - 1);
            ctx->hint_explain_buf[sizeof(ctx->hint_explain_buf) - 1] = '\0';
        } else {
            ctx->hint_explain_buf[0] = '\0';
        }
    }

    /* Now override with context-aware text where possible */
    switch (code) {

    case FV_ERR_MISSING_KEYWORD:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Add the keyword '%s' to the header of HDU %d. "
                "The mandatory keywords for %s in order are: %s.",
                kw, hdu, hdu_name, mand_list);
            if (purpose) {
                snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                    "%s Without it, FITS readers cannot interpret the %s. "
                    "See FITS Standard %s.",
                    purpose, hdu_name,
                    section ? section : "(see relevant section)");
            }
        }
        break;

    case FV_ERR_KEYWORD_ORDER:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Move keyword '%s' to its required position in HDU %d. "
                "The mandatory order for %s is: %s.",
                kw, hdu, hdu_name, mand_list);
            snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                "FITS requires mandatory keywords in a fixed order at the "
                "start of each header. '%s' must appear in its designated "
                "position. See FITS Standard Section 4.4.1.",
                kw);
        }
        break;

    case FV_ERR_KEYWORD_DUPLICATE:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Remove the duplicate '%s' keyword in HDU %d; "
                "it must appear exactly once.",
                kw, hdu);
            snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                "Mandatory keywords must appear only once. Having two '%s' "
                "keywords creates ambiguity about which value should be used. "
                "See FITS Standard Section 4.4.1.",
                kw);
        }
        break;

    case FV_ERR_KEYWORD_VALUE:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Correct the value of '%s' in HDU %d to a legal value "
                "per the FITS Standard.",
                kw, hdu);
            if (purpose) {
                snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                    "%s The current value is not permitted. "
                    "See FITS Standard %s.",
                    purpose, section ? section : "(see relevant section)");
            }
        }
        break;

    case FV_ERR_KEYWORD_TYPE:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Change the value of '%s' in HDU %d to the required datatype.",
                kw, hdu);
            if (purpose) {
                snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                    "%s The value must use the correct datatype "
                    "(e.g., BITPIX must be an integer). "
                    "See FITS Standard %s.",
                    purpose, section ? section : "(see relevant section)");
            }
        }
        break;

    case FV_ERR_NOT_FIXED_FORMAT:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Write '%s' in HDU %d using fixed format "
                "(value indicator '= ' in columns 9-10, "
                "value right-justified in columns 11-30).",
                kw, hdu);
            snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                "Mandatory keywords must use fixed-format notation so that any "
                "reader can parse them without interpreting free-format values. "
                "'%s' must have its value in columns 11-30. "
                "See FITS Standard Section 4.2.1.",
                kw);
        }
        break;

    case FV_ERR_ILLEGAL_NAME_CHAR:
    case FV_ERR_NAME_NOT_JUSTIFIED:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Fix keyword '%s' in HDU %d: names must use only "
                "uppercase A-Z, digits 0-9, hyphen, and underscore, "
                "left-justified in columns 1-8.",
                kw, hdu);
        }
        break;

    case FV_ERR_BAD_STRING:
    case FV_ERR_MISSING_QUOTE:
    case FV_ERR_BAD_LOGICAL:
    case FV_ERR_BAD_NUMBER:
    case FV_ERR_LOWERCASE_EXPONENT:
    case FV_ERR_COMPLEX_FORMAT:
    case FV_ERR_BAD_COMMENT:
    case FV_ERR_NO_VALUE_SEPARATOR:
    case FV_ERR_UNKNOWN_TYPE:
    case FV_ERR_NONTEXT_CHARS:
        if (has_kw) {
            /* Prepend keyword context to the static fix_hint */
            if (fallback && fallback->fix_hint) {
                snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                    "Keyword '%s' in HDU %d: %s",
                    kw, hdu, fallback->fix_hint);
            }
            if (fallback && fallback->explain) {
                snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                    "Keyword '%s': %s See FITS Standard Section 4.2.",
                    kw, fallback->explain);
            }
        }
        break;

    case FV_ERR_WRONG_TYPE:
        if (ctx->hint_callsite) {
            /* Call site provided specific hints.
               Fill in explain if the call site didn't set it. */
            if (!(ctx->hint_callsite & 2) && purpose) {
                snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                    "%s The value must match the expected type. "
                    "See FITS Standard %s.",
                    purpose, section ? section : "(see relevant section)");
            }
            ctx->hint_callsite = 0;
        } else if (has_kw) {
            /* Determine the expected type from the keyword name for a
               more specific hint.  Most wrong-type errors are float
               keywords entered as strings. */
            const char *expected = NULL;
            if (!strncmp(kw, "CRPIX", 5) || !strncmp(kw, "CRVAL", 5) ||
                !strncmp(kw, "CDELT", 5) || !strncmp(kw, "CROTA", 5) ||
                !strncmp(kw, "CRDER", 5) || !strncmp(kw, "CSYER", 5) ||
                !strncmp(kw, "CD",    2) || !strncmp(kw, "PC",    2) ||
                !strncmp(kw, "PV",    2) ||
                !strcmp(kw, "EQUINOX")   || !strcmp(kw, "MJD-OBS")  ||
                !strcmp(kw, "MJD-AVG")   || !strcmp(kw, "LONPOLE")  ||
                !strcmp(kw, "LATPOLE")   || !strcmp(kw, "RESTFRQ")  ||
                !strcmp(kw, "RESTWAV")   || !strcmp(kw, "MJDREF")   ||
                !strcmp(kw, "JDREF")     || !strcmp(kw, "TSTART")   ||
                !strcmp(kw, "TSTOP")     ||
                !strncmp(kw, "TCRVL", 5) || !strncmp(kw, "TCDLT", 5) ||
                !strncmp(kw, "TCRPX", 5) || !strncmp(kw, "TCROT", 5) ||
                !strncmp(kw, "TLMIN", 5) || !strncmp(kw, "TLMAX", 5) ||
                !strncmp(kw, "TDMIN", 5) || !strncmp(kw, "TDMAX", 5) ||
                !strcmp(kw, "BSCALE")    || !strcmp(kw, "BZERO")    ||
                !strcmp(kw, "DATAMAX")   || !strcmp(kw, "DATAMIN")  ||
                !strcmp(kw, "EPOCH"))
                expected = "floating-point number (without quotes)";
            else if (!strncmp(kw, "TSCAL", 5) || !strncmp(kw, "TZERO", 5))
                expected = "floating-point number (without quotes)";
            else if (!strcmp(kw, "BITPIX")  || !strcmp(kw, "NAXIS")   ||
                     !strncmp(kw, "NAXIS", 5) || !strcmp(kw, "PCOUNT") ||
                     !strcmp(kw, "GCOUNT") || !strcmp(kw, "TFIELDS")  ||
                     !strcmp(kw, "EXTVER") || !strcmp(kw, "EXTLEVEL") ||
                     !strncmp(kw, "TNULL", 5) || !strcmp(kw, "BLANK") ||
                     !strncmp(kw, "TBCOL", 5) || !strcmp(kw, "WCSAXES"))
                expected = "integer (without quotes)";
            else if (!strcmp(kw, "SIMPLE") || !strcmp(kw, "EXTEND") ||
                     !strcmp(kw, "GROUPS") || !strcmp(kw, "INHERIT"))
                expected = "logical value (T or F, without quotes)";

            if (expected) {
                snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                    "Change '%s' in HDU %d to a %s. "
                    "If the value is currently a quoted string, remove the quotes.",
                    kw, hdu, expected);
            } else {
                snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                    "Change the value of '%s' in HDU %d to the expected datatype.",
                    kw, hdu);
            }
            if (purpose) {
                snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                    "%s The value must match the expected type. "
                    "See FITS Standard %s.",
                    purpose, section ? section : "(see relevant section)");
            } else {
                snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                    "Keyword '%s' has a value of the wrong datatype. "
                    "Check the FITS Standard for the required type.",
                    kw);
            }
        }
        break;

    case FV_ERR_NULL_VALUE:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Provide a value for '%s' in HDU %d, or remove it if not needed.",
                kw, hdu);
            if (purpose) {
                snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                    "%s The keyword currently has no value (blank value field).",
                    purpose);
            }
        }
        break;

    case FV_ERR_LEADING_SPACE:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Remove leading spaces from the value of '%s' in HDU %d.",
                kw, hdu);
            snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                "Keyword '%s': certain keyword values (XTENSION, TFORMn, "
                "TDISPn, TDIMn) must not have leading spaces within the "
                "quoted string. See FITS Standard Section 4.2.1.",
                kw);
        }
        break;

    case FV_ERR_RESERVED_VALUE:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Correct the value of reserved keyword '%s' in HDU %d.",
                kw, hdu);
            if (purpose) {
                snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                    "%s The current value violates the FITS Standard. "
                    "See FITS Standard %s.",
                    purpose, section ? section : "(see relevant section)");
            }
        }
        break;

    case FV_ERR_KEYWORD_NOT_ALLOWED:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Remove keyword '%s' from HDU %d; it is not permitted "
                "in %s.",
                kw, hdu, hdu_name);
            snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                "Keyword '%s' is not valid in %s. "
                "Check the FITS Standard for which keywords are "
                "allowed in each HDU type.",
                kw, hdu_name);
        }
        break;

    case FV_ERR_PRIMARY_KEY_IN_EXT:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Remove '%s' from HDU %d; it is only valid in the primary HDU.",
                kw, hdu);
            snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                "The keyword '%s' is only valid in the primary HDU (HDU 1). "
                "It must not appear in any extension. "
                "See FITS Standard Section 4.4.2.",
                kw);
        }
        break;

    case FV_ERR_IMAGE_KEY_IN_TABLE:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Remove '%s' from HDU %d (%s); it is only valid in "
                "image HDUs.",
                kw, hdu, hdu_name);
            snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                "Keywords like BSCALE, BZERO, BUNIT, BLANK, DATAMAX, and "
                "DATAMIN are only valid in image HDUs. In tables, use "
                "the column-specific equivalents (TSCALn, TZEROn, TUNITn, TNULLn). "
                "'%s' was found in %s. See FITS Standard Section 7.",
                kw, hdu_name);
        }
        break;

    case FV_ERR_TABLE_KEY_IN_IMAGE:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Remove table keyword '%s' from HDU %d (%s).",
                kw, hdu, hdu_name);
            snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                "Column-related keywords like TFIELDS, TTYPEn, TFORMn are "
                "only valid in table extensions. '%s' was found in %s. "
                "See FITS Standard Section 7.",
                kw, hdu_name);
        }
        break;

    case FV_ERR_INDEX_EXCEEDS_TFIELDS:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Keyword '%s' in HDU %d has a column index exceeding TFIELDS. "
                "Either increase TFIELDS or remove the excess keyword.",
                kw, hdu);
            snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                "Column-indexed keywords (TTYPEn, TFORMn, etc.) must have "
                "index n <= TFIELDS. '%s' exceeds this limit. "
                "See FITS Standard Section 7.2.1.",
                kw);
        }
        break;

    case FV_ERR_BAD_TFORM:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Correct '%s' in HDU %d to a valid FITS column format.",
                kw, hdu);
            snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                "'%s' specifies the data format for a column. Valid formats "
                "include integer widths for ASCII tables (e.g., I10, F12.5) "
                "and type codes for binary tables (e.g., 1J, 20A, 1E). "
                "See FITS Standard %s.",
                kw, section ? section : "Section 7.2.1/7.3.1");
        }
        break;

    case FV_ERR_BAD_TDISP:
        if (ctx->hint_callsite) {
            /* Call site provided specific hints (fix and/or explain).
               Fill in any that weren't set by the call site. */
            if (!(ctx->hint_callsite & 2)) {
                snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                    "TDISPn controls the display format for column n. "
                    "The display format must be compatible with the column's "
                    "TFORMn data type. See FITS Standard Section 7.3.3.");
            }
            ctx->hint_callsite = 0;
        } else if (has_kw) {
            /* Generic fallback (e.g. invalid format string, not a mismatch) */
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Correct the display format in '%s' in HDU %d. "
                "Valid formats: Aw (character), Lw (logical), "
                "Iw/Bw/Ow/Zw (integer), Fw.d/Ew.d/Dw.d/Gw.d (numeric).",
                kw, hdu);
            snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                "TDISPn controls the display format for column n. "
                "The format must be a valid Fortran-style format code "
                "with correct width and precision. "
                "See FITS Standard Section 7.3.3.");
        }
        break;

    case FV_ERR_BLANK_WRONG_TYPE:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Remove '%s' from HDU %d; it must not be used with "
                "floating-point data. Use NaN instead.",
                kw, hdu);
        }
        break;

    case FV_ERR_TSCAL_WRONG_TYPE:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Remove '%s' from HDU %d; scaling keywords are only "
                "valid for numeric (integer/float) binary table columns.",
                kw, hdu);
        }
        break;

    case FV_ERR_TNULL_WRONG_TYPE:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Remove '%s' from this floating-point column in HDU %d; "
                "use IEEE NaN for null values instead.",
                kw, hdu);
        }
        break;

    case FV_WARN_DEPRECATED:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Remove or replace deprecated keyword '%s' in HDU %d.",
                kw, hdu);
            if (!strcmp(kw, "EPOCH")) {
                snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                    "'EPOCH' is deprecated in favor of 'EQUINOX'. "
                    "See FITS Standard Section 8.3.");
            } else if (!strcmp(kw, "BLOCKED")) {
                snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                    "'BLOCKED' is deprecated and should be removed; "
                    "it was related to tape blocking which is no longer relevant.");
            }
        }
        break;

    case FV_WARN_ZERO_SCALE:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Set '%s' in HDU %d to a non-zero value.",
                kw, hdu);
            snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                "A scale factor of zero for '%s' would map all raw values "
                "to the same physical value (the offset). The formula is: "
                "physical = raw * %s + offset. See FITS Standard %s.",
                kw, kw, section ? section : "Section 4.4.2.1");
        }
        break;

    case FV_WARN_DUPLICATE_KEYWORD:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Remove the duplicate '%s' keyword in HDU %d, "
                "or rename one of the copies.",
                kw, hdu);
            snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                "'%s' appears more than once in the header of HDU %d. "
                "Only COMMENT, HISTORY, blank, and CONTINUE keywords "
                "may be duplicated. See FITS Standard Section 4.4.1.",
                kw, hdu);
        }
        break;

    case FV_ERR_NONASCII_DATA:
    case FV_ERR_BAD_LOGICAL_DATA:
    case FV_ERR_BIT_NOT_JUSTIFIED:
    case FV_ERR_NO_DECIMAL:
    case FV_ERR_EMBEDDED_SPACE:
        if (has_col) {
            if (fallback && fallback->fix_hint) {
                snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                    "Column %d in HDU %d: %s",
                    ctx->hint_colnum, hdu, fallback->fix_hint);
            }
            if (fallback && fallback->explain) {
                snprintf(ctx->hint_explain_buf, sizeof(ctx->hint_explain_buf),
                    "Column %d: %s",
                    ctx->hint_colnum, fallback->explain);
            }
        }
        break;

    case FV_ERR_VAR_EXCEEDS_MAXLEN:
    case FV_ERR_VAR_EXCEEDS_HEAP:
        if (ctx->hint_callsite) {
            ctx->hint_callsite = 0;
        } else if (has_col) {
            if (fallback && fallback->fix_hint) {
                snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                    "Column %d in HDU %d: %s",
                    ctx->hint_colnum, hdu, fallback->fix_hint);
            }
        }
        break;

    case FV_WARN_VAR_EXCEEDS_32BIT:
        if (has_col) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Column %d in HDU %d: use 'Q' format (64-bit descriptor) "
                "instead of 'P' for large variable-length arrays.",
                ctx->hint_colnum, hdu);
        }
        break;

    case FV_ERR_WCSAXES_ORDER:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Move WCSAXES before keyword '%s' in HDU %d.",
                kw, hdu);
        }
        break;

    case FV_ERR_WCS_INDEX:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Keyword '%s' in HDU %d: reduce the axis index to "
                "not exceed the WCSAXES value.",
                kw, hdu);
        }
        break;

    case FV_WARN_WCS_INDEX:
        if (has_kw) {
            snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                "Keyword '%s' in HDU %d: add a WCSAXES keyword, "
                "or ensure WCS indices do not exceed NAXIS.",
                kw, hdu);
        }
        break;

    default:
        if (ctx->hint_callsite) {
            /* Call site provided specific hints; keep them. */
            ctx->hint_callsite = 0;
        } else {
            /* For any other code with keyword context, prepend it */
            if (has_kw && fallback && fallback->fix_hint) {
                snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                    "Keyword '%s' in HDU %d: %s",
                    kw, hdu, fallback->fix_hint);
            }
            if (has_col && !has_kw && fallback && fallback->fix_hint) {
                snprintf(ctx->hint_fix_buf, sizeof(ctx->hint_fix_buf),
                    "Column %d in HDU %d: %s",
                    ctx->hint_colnum, hdu, fallback->fix_hint);
            }
        }
        break;
    }

    ctx_hint.fix_hint = ctx->hint_fix_buf[0] ? ctx->hint_fix_buf : NULL;
    ctx_hint.explain  = ctx->hint_explain_buf[0] ? ctx->hint_explain_buf : NULL;
    return &ctx_hint;
}
