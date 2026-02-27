# fitsverify Fix Hints Catalog (Starter) — SUPERSEDED

> **This starter catalog has been superseded by the full implementation in `libfitsverify/src/fv_hints.c`.** That file contains context-aware hints and explanations for all ~70 error codes, with runtime generation that names the specific keyword, HDU number, HDU type, and FITS Standard section involved. The static entries below were used as a starting point during development.

## Original Starter Set (Top 15)

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
