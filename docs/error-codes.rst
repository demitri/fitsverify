Error Codes Reference
=====================

Every diagnostic produced by libfitsverify carries a unique error code from the
``fv_error_code`` enum.  These codes enable machine-actionable filtering,
routing, and suppression of specific checks.

Enable **fix hints** and **explanations** to see actionable guidance alongside
each diagnostic.  Hints are **context-aware**: they name the specific keyword,
HDU number, and FITS Standard section involved, rather than giving generic
advice.

.. code-block:: python

   result = fitsverify.verify("myfile.fits", fix_hints=True, explain=True)
   for issue in result.errors:
       print(f"[{issue.code}] {issue.message}")
       if issue.fix_hint:
           print(f"  Fix: {issue.fix_hint}")
       if issue.explain:
           print(f"  Why: {issue.explain}")

In the CLI::

   fitsverify --fix-hints --explain myfile.fits

Example context-aware output::

   *** Error:   Mandatory keyword BITPIX is not present.
       Fix: Add the keyword 'BITPIX' to the header of HDU 2. The mandatory
            keywords for a binary table in order are: XTENSION, BITPIX, NAXIS,
            NAXIS1, NAXIS2, PCOUNT, GCOUNT, TFIELDS, END.
       Explanation: 'BITPIX' specifies the number of bits per data element
            (e.g., 8 for bytes, 16 for short integers, -32 for single-precision
            floats). Without it, FITS readers cannot interpret the binary table.
            See FITS Standard Section 4.4.1.1.

The **Fix** column in the tables below shows the static (fallback) hint text.
When context is available at runtime, hints include the keyword name, HDU
number, HDU type, and FITS Standard section reference.


File / HDU Structure (100--149)
-------------------------------

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - Code
     - Name
     - Description / Fix Hint
   * - 100
     - ``FV_ERR_EXTRA_HDUS``
     - Extraneous HDU(s) beyond last.

       **Fix:** Remove extraneous data after the last valid HDU.
   * - 101
     - ``FV_ERR_EXTRA_BYTES``
     - Extra bytes after last HDU.

       **Fix:** Truncate the file at the end of the last HDU's 2880-byte block.
   * - 102
     - ``FV_ERR_BAD_HDU``
     - Bad HDU structure.

       **Fix:** Check the HDU structure; the header or data section may be malformed.
   * - 103
     - ``FV_ERR_READ_FAIL``
     - Error reading file data.

       **Fix:** Check that the file is accessible and not truncated.


Mandatory Keywords (150--199)
-----------------------------

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - Code
     - Name
     - Description / Fix Hint
   * - 150
     - ``FV_ERR_MISSING_KEYWORD``
     - Mandatory keyword not present.

       **Fix:** Add the missing mandatory keyword to the header.
   * - 151
     - ``FV_ERR_KEYWORD_ORDER``
     - Mandatory keyword out of order.

       **Fix:** Reorder mandatory keywords to follow the FITS Standard sequence.
   * - 152
     - ``FV_ERR_KEYWORD_DUPLICATE``
     - Mandatory keyword duplicated.

       **Fix:** Remove the duplicate; it must appear exactly once.
   * - 153
     - ``FV_ERR_KEYWORD_VALUE``
     - Mandatory keyword has wrong value.

       **Fix:** Correct the keyword value to a legal value per the FITS Standard.
   * - 154
     - ``FV_ERR_KEYWORD_TYPE``
     - Mandatory keyword has wrong datatype.

       **Fix:** Change the keyword value to the required datatype.
   * - 155
     - ``FV_ERR_MISSING_END``
     - END keyword not present.

       **Fix:** Add an END keyword and pad the header to a 2880-byte boundary.
   * - 156
     - ``FV_ERR_END_NOT_BLANK``
     - END not filled with blanks.

       **Fix:** Fill columns 9--80 of the END keyword record with blank spaces.
   * - 157
     - ``FV_ERR_NOT_FIXED_FORMAT``
     - Mandatory keyword not in fixed format.

       **Fix:** Write the value in fixed format (value in columns 11--30).


Keyword Format / Value (200--249)
---------------------------------

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - Code
     - Name
     - Description / Fix Hint
   * - 200
     - ``FV_ERR_NONASCII_HEADER``
     - Header contains non-ASCII character.

       **Fix:** Replace with printable ASCII (codes 32--126).
   * - 201
     - ``FV_ERR_ILLEGAL_NAME_CHAR``
     - Keyword name contains illegal character.

       **Fix:** Use only uppercase A--Z, digits 0--9, hyphen, and underscore.
   * - 202
     - ``FV_ERR_NAME_NOT_JUSTIFIED``
     - Keyword name not left-justified.

       **Fix:** Left-justify the keyword name in columns 1--8.
   * - 203
     - ``FV_ERR_BAD_VALUE_FORMAT``
     - Keyword value field has illegal format.

       **Fix:** Fix the value to conform to FITS value-field syntax.
   * - 204
     - ``FV_ERR_NO_VALUE_SEPARATOR``
     - Value and comment not separated by ``/``.

       **Fix:** Add a ``/`` separator between the value and comment fields.
   * - 205
     - ``FV_ERR_BAD_STRING``
     - String value contains non-text characters.

       **Fix:** Ensure string values contain only printable ASCII.
   * - 206
     - ``FV_ERR_MISSING_QUOTE``
     - Closing quote missing.

       **Fix:** Add the missing closing single quote.
   * - 207
     - ``FV_ERR_BAD_LOGICAL``
     - Bad logical value.

       **Fix:** Set the logical value to T or F in column 30.
   * - 208
     - ``FV_ERR_BAD_NUMBER``
     - Bad numerical value.

       **Fix:** Use valid FITS integer or floating-point format.
   * - 209
     - ``FV_ERR_LOWERCASE_EXPONENT``
     - Lowercase exponent (d/e) in number.

       **Fix:** Change to uppercase (D/E).
   * - 210
     - ``FV_ERR_COMPLEX_FORMAT``
     - Complex value format error.

       **Fix:** Format as ``(real, imaginary)`` with proper parentheses and comma.
   * - 211
     - ``FV_ERR_BAD_COMMENT``
     - Comment contains non-text characters.

       **Fix:** Remove non-printable characters from the comment field.
   * - 212
     - ``FV_ERR_UNKNOWN_TYPE``
     - Unknown keyword value type.

       **Fix:** Verify the value conforms to a recognized FITS type.
   * - 213
     - ``FV_ERR_WRONG_TYPE``
     - Wrong type (expected str/int/etc.).

       **Fix:** Change the keyword value to the expected datatype.
   * - 214
     - ``FV_ERR_NULL_VALUE``
     - Keyword has null value.

       **Fix:** Provide a value, or remove the keyword if not needed.
   * - 215
     - ``FV_ERR_CARD_TOO_LONG``
     - Card > 80 characters.

       **Fix:** Ensure the header card does not exceed 80 characters.
   * - 216
     - ``FV_ERR_NONTEXT_CHARS``
     - String contains non-text characters.

       **Fix:** Remove non-text characters from the string value.
   * - 217
     - ``FV_ERR_LEADING_SPACE``
     - Value contains leading space(s).

       **Fix:** Remove leading spaces from the keyword value.
   * - 218
     - ``FV_ERR_RESERVED_VALUE``
     - Reserved keyword has wrong value.

       **Fix:** Correct the reserved keyword to its required value.


HDU-Type Keyword Placement (250--299)
--------------------------------------

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - Code
     - Name
     - Description / Fix Hint
   * - 250
     - ``FV_ERR_XTENSION_IN_PRIMARY``
     - XTENSION keyword in primary array.

       **Fix:** Remove XTENSION from the primary HDU.
   * - 251
     - ``FV_ERR_IMAGE_KEY_IN_TABLE``
     - BSCALE/BZERO/etc. in a table.

       **Fix:** Remove image-specific keywords from the table HDU.
   * - 252
     - ``FV_ERR_TABLE_KEY_IN_IMAGE``
     - Column keyword in an image.

       **Fix:** Remove table-specific keywords from the image HDU.
   * - 253
     - ``FV_ERR_PRIMARY_KEY_IN_EXT``
     - SIMPLE/EXTEND/BLOCKED in extension.

       **Fix:** Remove these keywords from extension HDUs.
   * - 254
     - ``FV_ERR_TABLE_WCS_IN_IMAGE``
     - Table WCS keywords in image.

       **Fix:** Remove table WCS keywords (TCTYPn, etc.) from the image HDU.
   * - 255
     - ``FV_ERR_KEYWORD_NOT_ALLOWED``
     - Keyword not allowed in this HDU.

       **Fix:** Remove the keyword that is not permitted in this HDU type.


Table Structure (300--349)
--------------------------

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - Code
     - Name
     - Description / Fix Hint
   * - 300
     - ``FV_ERR_BAD_TFIELDS``
     - Bad TFIELDS value.

       **Fix:** Set TFIELDS to the correct number of columns.
   * - 301
     - ``FV_ERR_NAXIS1_MISMATCH``
     - Column widths inconsistent with NAXIS1.

       **Fix:** Adjust NAXIS1 to equal the sum of all column widths.
   * - 302
     - ``FV_ERR_BAD_TFORM``
     - Bad TFORMn value.

       **Fix:** Correct TFORMn to a valid FITS column format.
   * - 303
     - ``FV_ERR_BAD_TDISP``
     - TDISPn inconsistent with datatype.

       **Fix:** Fix TDISPn to be consistent with the column datatype.
   * - 304
     - ``FV_ERR_INDEX_EXCEEDS_TFIELDS``
     - Column keyword index > TFIELDS.

       **Fix:** Ensure column keyword index does not exceed TFIELDS.
   * - 305
     - ``FV_ERR_TSCAL_WRONG_TYPE``
     - TSCALn/TZEROn on wrong column type.

       **Fix:** Remove TSCALn/TZEROn from ASCII, logical, or bit columns.
   * - 306
     - ``FV_ERR_TNULL_WRONG_TYPE``
     - TNULLn on floating-point column.

       **Fix:** Remove TNULLn; use NaN for floating-point null values.
   * - 307
     - ``FV_ERR_BLANK_WRONG_TYPE``
     - BLANK with floating-point image.

       **Fix:** Remove BLANK; use NaN for floating-point null pixels.
   * - 308
     - ``FV_ERR_THEAP_NO_PCOUNT``
     - THEAP with PCOUNT = 0.

       **Fix:** Remove THEAP or set PCOUNT > 0.
   * - 309
     - ``FV_ERR_TDIM_IN_ASCII``
     - TDIMn in ASCII table.

       **Fix:** Remove TDIMn; it is only valid for binary tables.
   * - 310
     - ``FV_ERR_TBCOL_IN_BINARY``
     - TBCOLn in binary table.

       **Fix:** Remove TBCOLn; it is only valid for ASCII tables.
   * - 311
     - ``FV_ERR_VAR_FORMAT``
     - Variable-length format error.

       **Fix:** Fix the variable-length array format descriptor in TFORMn.
   * - 312
     - ``FV_ERR_TBCOL_MISMATCH``
     - TBCOLn value mismatch.

       **Fix:** Correct TBCOLn values so columns are properly positioned.


Data Validation (350--399)
--------------------------

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - Code
     - Name
     - Description / Fix Hint
   * - 350
     - ``FV_ERR_VAR_EXCEEDS_MAXLEN``
     - Variable-length array > max from TFORMn.

       **Fix:** Reduce the array size or increase the maximum in TFORMn.
   * - 351
     - ``FV_ERR_VAR_EXCEEDS_HEAP``
     - Variable-length array address outside heap.

       **Fix:** Fix the descriptor; it points beyond the allocated heap.
   * - 352
     - ``FV_ERR_BIT_NOT_JUSTIFIED``
     - Bit column not left-justified.

       **Fix:** Left-justify the bit values and zero-fill unused trailing bits.
   * - 353
     - ``FV_ERR_BAD_LOGICAL_DATA``
     - Logical value not T, F, or 0.

       **Fix:** Set logical values to 'T', 'F', or 0 (null).
   * - 354
     - ``FV_ERR_NONASCII_DATA``
     - String column has non-ASCII.

       **Fix:** Replace non-ASCII characters with printable ASCII.
   * - 355
     - ``FV_ERR_NO_DECIMAL``
     - ASCII float missing decimal point.

       **Fix:** Add a decimal point to the floating-point value.
   * - 356
     - ``FV_ERR_EMBEDDED_SPACE``
     - ASCII numeric has embedded space.

       **Fix:** Remove embedded spaces from the numeric value.
   * - 357
     - ``FV_ERR_NONASCII_TABLE``
     - ASCII table has non-ASCII chars.

       **Fix:** Replace non-ASCII characters with valid ASCII.
   * - 358
     - ``FV_ERR_DATA_FILL``
     - Data fill bytes incorrect.

       **Fix:** Use blanks (0x20) for ASCII tables, zeros (0x00) for others.
   * - 359
     - ``FV_ERR_HEADER_FILL``
     - Header fill bytes not blank.

       **Fix:** Fill unused header bytes after END with blank spaces.
   * - 360
     - ``FV_ERR_ASCII_GAP``
     - ASCII table gap has chars > 127.

       **Fix:** Replace characters > 127 in column gaps with ASCII.


WCS (400--419)
--------------

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - Code
     - Name
     - Description / Fix Hint
   * - 400
     - ``FV_ERR_WCSAXES_ORDER``
     - WCSAXES after other WCS keywords.

       **Fix:** Move WCSAXES before all other WCS keywords.
   * - 401
     - ``FV_ERR_WCS_INDEX``
     - WCS keyword index > WCSAXES.

       **Fix:** Reduce the WCS keyword index to not exceed WCSAXES.


CFITSIO / I-O (450--479)
-------------------------

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - Code
     - Name
     - Description / Fix Hint
   * - 450
     - ``FV_ERR_CFITSIO``
     - CFITSIO library error.

       **Fix:** Check the CFITSIO error message for details.
   * - 451
     - ``FV_ERR_CFITSIO_STACK``
     - CFITSIO error stack dump.

       **Fix:** Review the CFITSIO error stack messages for the root cause.


Internal / Abort (480--499)
---------------------------

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - Code
     - Name
     - Description / Fix Hint
   * - 480
     - ``FV_ERR_TOO_MANY``
     - MAXERRORS exceeded, aborted.

       **Fix:** Fix the most critical errors first; the file has too many problems.


Warnings (500--599)
-------------------

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - Code
     - Name
     - Description / Fix Hint
   * - 500
     - ``FV_WARN_SIMPLE_FALSE``
     - SIMPLE = F.

       **Fix:** Set SIMPLE = T unless the file intentionally uses non-standard features.
   * - 501
     - ``FV_WARN_DEPRECATED``
     - Deprecated keyword (EPOCH, BLOCKED).

       **Fix:** Replace EPOCH with EQUINOX; remove BLOCKED.
   * - 502
     - ``FV_WARN_DUPLICATE_EXTNAME``
     - HDUs with same EXTNAME/EXTVER.

       **Fix:** Give each HDU a unique EXTNAME/EXTVER/EXTLEVEL combination.
   * - 503
     - ``FV_WARN_ZERO_SCALE``
     - BSCALE or TSCALn = 0.

       **Fix:** Set BSCALE/TSCALn to a non-zero value.
   * - 504
     - ``FV_WARN_TNULL_RANGE``
     - BLANK/TNULLn out of range.

       **Fix:** Set to a value within the valid range for the datatype.
   * - 505
     - ``FV_WARN_RAW_NOT_MULTIPLE``
     - rAw format: r not multiple of w.

       **Fix:** Adjust TFORMn so r is a multiple of w.
   * - 506
     - ``FV_WARN_Y2K``
     - DATE yy < 10 (Y2K?).

       **Fix:** Use the DATE format ``YYYY-MM-DD`` instead of ``DD/MM/YY``.
   * - 507
     - ``FV_WARN_WCS_INDEX``
     - WCS index > NAXIS (no WCSAXES).

       **Fix:** Add a WCSAXES keyword, or ensure WCS indices do not exceed NAXIS.
   * - 508
     - ``FV_WARN_DUPLICATE_KEYWORD``
     - Duplicated keyword.

       **Fix:** Remove the duplicate or rename one of the copies.
   * - 509
     - ``FV_WARN_BAD_COLUMN_NAME``
     - Column name has bad characters.

       **Fix:** Use only letters, digits, and underscores in column names.
   * - 510
     - ``FV_WARN_NO_COLUMN_NAME``
     - Column has no name (no TTYPEn).

       **Fix:** Add a TTYPEn keyword to give the column a descriptive name.
   * - 511
     - ``FV_WARN_DUPLICATE_COLUMN``
     - Duplicate column names.

       **Fix:** Rename one of the duplicate columns to be unique.
   * - 512
     - ``FV_WARN_BAD_CHECKSUM``
     - Checksum mismatch.

       **Fix:** Recompute CHECKSUM and DATASUM (e.g. with ``fchecksum``).
   * - 513
     - ``FV_WARN_MISSING_LONGSTRN``
     - LONGSTRN keyword missing.

       **Fix:** Add ``LONGSTRN = 'OGIP 1.0'`` when using CONTINUE long strings.
   * - 514
     - ``FV_WARN_VAR_EXCEEDS_32BIT``
     - Variable-length > 32-bit range.

       **Fix:** Use 'Q' format (64-bit descriptor) instead of 'P'.
   * - 515
     - ``FV_WARN_HIERARCH_DUPLICATE``
     - Duplicate HIERARCH keyword.

       **Fix:** Remove or rename the duplicate HIERARCH keyword.
   * - 516
     - ``FV_WARN_PCOUNT_NO_VLA``
     - PCOUNT != 0, no VLA columns.

       **Fix:** Set PCOUNT = 0 or add variable-length array columns.
   * - 517
     - ``FV_WARN_CONTINUE_CHAR``
     - Column name uses '&' (CONTINUE).

       **Fix:** Remove the trailing '&' unless CONTINUE convention is intended.
   * - 518
     - ``FV_WARN_RANDOM_GROUPS``
     - Random Groups convention (deprecated since FITS Standard Version 1).

       **Fix:** Convert Random Groups data to a binary table extension. See
       FITS Standard Section 7.
   * - 519
     - ``FV_WARN_LEGACY_XTENSION``
     - Non-standard XTENSION value (e.g. A3DTABLE, IUEIMAGE, FOREIGN, DUMP).

       **Fix:** Use a standard XTENSION value: IMAGE, TABLE, or BINTABLE.
