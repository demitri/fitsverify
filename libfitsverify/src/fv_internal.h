/*
 * fv_internal.h — internal types and prototypes for libfitsverify
 *
 * Based on the original fverify.h. Key changes:
 *   - Removed static errmes[] and comm[] (moved to fv_context)
 *   - Removed extern declarations of globals (moved to fv_context)
 *   - All function prototypes take fv_context *ctx as first parameter
 *   - Forward declaration of fv_context
 */
#ifndef FV_INTERNAL_H
#define FV_INTERNAL_H

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include "fitsio.h"

#define MAXERRORS  200
#define MAXWRNS   200

/* Forward declaration — full definition in fv_context.h */
typedef struct fv_context fv_context;

/********************************
*                               *
*       Keywords                *
*                               *
********************************/

typedef enum {
    STR_KEY,   /* string key           */
    LOG_KEY,   /* Logical key          */
    INT_KEY,   /* Integer key          */
    FLT_KEY,   /* Float key            */
    CMI_KEY,   /* Complex integer key  */
    CMF_KEY,   /* Complex float key    */
    COM_KEY,   /* history, comment, " ", and end */
    UNKNOWN    /* Unknown types        */
} kwdtyp;

/* error number masks of the keyword test */
#define BAD_STR           0X0001
#define NO_TRAIL_QUOTE    0X0002
#define BAD_NUM           0X0004
#define LOWCASE_EXPO      0X0008
#define NO_TRAIL_PAREN    0X0010
#define NO_COMMA          0X0020
#define TOO_MANY_COMMA    0X0040
#define BAD_REAL          0X0080
#define BAD_IMG           0X0100
#define BAD_LOGICAL       0x0200
#define NO_START_SLASH    0X0400
#define BAD_COMMENT       0x0800
#define UNKNOWN_TYPE      0x1000

/* keyword structure */
typedef struct {
    char   kname[FLEN_KEYWORD]; /* fits keyword name        */
    kwdtyp ktype;               /* fits keyword type        */
    char   kvalue[FLEN_VALUE];  /* fits keyword value       */
    int    kindex;              /* position in the header   */
    int    goodkey;             /* good keyword flag (1=good) */
} FitsKey;

int  fits_parse_card(fv_context *ctx, FILE *out, int pos, char *card,
                     char *kname, kwdtyp *ktype, char *kvalue, char *kcomm);
void get_str(char **p, char *kvalue, unsigned long *stat);
void get_log(char **p, char *kvalue, unsigned long *stat);
void get_num(char **p, char *kvalue, kwdtyp *ktype, unsigned long *stat);
void get_cmp(char **p, char *kvalue, kwdtyp *ktype, unsigned long *stat);
int  check_str(fv_context *ctx, FitsKey *pkey, FILE *out);
int  check_int(fv_context *ctx, FitsKey *pkey, FILE *out);
int  check_flt(fv_context *ctx, FitsKey *pkey, FILE *out);
int  check_cmi(fv_context *ctx, FitsKey *pkey, FILE *out);
int  check_cmf(fv_context *ctx, FitsKey *pkey, FILE *out);
int  check_log(fv_context *ctx, FitsKey *pkey, FILE *out);
int  check_fixed_int(fv_context *ctx, char *card, FILE *out);
int  check_fixed_log(fv_context *ctx, char *card, FILE *out);
int  check_fixed_str(fv_context *ctx, char *card, FILE *out);

void get_unknown(char **p, char *kvalue, kwdtyp *ktype, unsigned long *stat);
void get_comm(char **p, char *kcomm, unsigned long *stat);
void pr_kval_err(fv_context *ctx, FILE *out, int pos, char *keyname,
                 char *keyval, unsigned long stat);

/********************************
*                               *
*       Headers                 *
*                               *
********************************/
typedef struct {
    int      hdutype;
    int      hdunum;
    int      isgroup;
    int      istilecompressed;
    int      gcount;
    LONGLONG pcount;
    int      bitpix;
    int      naxis;
    LONGLONG *naxes;
    int      ncols;
    char     extname[FLEN_VALUE];
    int      extver;
    char   **datamax;
    char   **datamin;
    char   **tnull;
    int      nkeys;
    int      tkeys;
    int      heap;
    FitsKey **kwds;
    int      use_longstr;
} FitsHdu;

typedef struct {
    char *name;
    int   index;
} ColName;

int  verify_fits(fv_context *ctx, char *infile, FILE *out);
void leave_early(fv_context *ctx, FILE *out);
void close_err(fv_context *ctx, FILE *out);
void init_hdu(fv_context *ctx, fitsfile *infits, FILE *out,
              int hdunum, int hdutype, FitsHdu *hduptr);
void test_hdu(fv_context *ctx, fitsfile *infits, FILE *out, FitsHdu *hduptr);
void test_ext(fv_context *ctx, fitsfile *infits, FILE *out, FitsHdu *hduptr);
void test_tbl(fv_context *ctx, fitsfile *infits, FILE *out, FitsHdu *hduptr);
void test_array(fv_context *ctx, fitsfile *infits, FILE *out, FitsHdu *hduptr);
void test_prm(fv_context *ctx, fitsfile *infits, FILE *out, FitsHdu *hduptr);
void test_img_ext(fv_context *ctx, fitsfile *infits, FILE *out, FitsHdu *hduptr);
void test_asc_ext(fv_context *ctx, fitsfile *infits, FILE *out, FitsHdu *hduptr);
void test_bin_ext(fv_context *ctx, fitsfile *infits, FILE *out, FitsHdu *hduptr);
void test_header(fv_context *ctx, fitsfile *infits, FILE *out, FitsHdu *hduptr);
void key_match(char **strs, int nstr, char **pattern, int exact,
               int *ikey, int *mkey);
void test_colnam(fv_context *ctx, FILE *out, FitsHdu *hduptr);
void parse_vtform(fv_context *ctx, fitsfile *infits, FILE *out, FitsHdu *hduptr,
                  int colnum, int *datacode, long *maxlen, int *isQFormat);
void print_title(fv_context *ctx, FILE *out, int hdunum, int hdutype);
void print_header(fv_context *ctx, FILE *out);
void print_summary(fv_context *ctx, fitsfile *infits, FILE *out, FitsHdu *hduptr);
void close_hdu(fv_context *ctx, FitsHdu *hduptr);

/********************************
*                               *
*       Data                    *
*                               *
********************************/

void test_data(fv_context *ctx, fitsfile *infits, FILE *out, FitsHdu *hduptr);
void test_agap(fv_context *ctx, fitsfile *infits, FILE *out, FitsHdu *hduptr);
void test_checksum(fv_context *ctx, fitsfile *infits, FILE *out);
int  iterdata(long totaln, long offset, long firstn, long nrows,
              int narrays, iteratorCol *iter_col, void *usrdata);

/********************************
*                               *
*       Files                   *
*                               *
********************************/
typedef struct {
    int  hdutype;
    int  hdunum;
    char extname[FLEN_VALUE];
    int  extver;
    int  errnum;
    int  wrnno;
} HduName;

int  get_total_warn(fv_context *ctx);
int  get_total_err(fv_context *ctx);
void init_hduname(fv_context *ctx);
void set_hduname(fv_context *ctx, int hdunum, int hdutype,
                 char *extname, int extver);
void set_hduerr(fv_context *ctx, int hdunum);
void set_hdubasic(fv_context *ctx, int hdunum, int hdutype);
int  test_hduname(fv_context *ctx, int hdunum1, int hdunum2);
void total_errors(fv_context *ctx, int *totalerr, int *totalwrn);
void hdus_summary(fv_context *ctx, FILE *out);
void destroy_hduname(fv_context *ctx);
void test_end(fv_context *ctx, fitsfile *infits, FILE *out);
void init_report(fv_context *ctx, FILE *out, char *rootnam);
void close_report(fv_context *ctx, FILE *out);
void update_parfile(fv_context *ctx, int numerr, int numwrn);

/********************************
*                               *
*       Miscellaneous           *
*                               *
********************************/
void print_fmt(fv_context *ctx, FILE *out, char *temp, int nprompt);
int  wrtout(FILE *out, char *comm);
int  wrterr(fv_context *ctx, FILE *out, char *comm, int severity);
int  wrtwrn(fv_context *ctx, FILE *out, char *comm, int heasarc);
int  wrtferr(fv_context *ctx, FILE *out, char *mess, int *status, int severity);
int  wrtserr(fv_context *ctx, FILE *out, char *mess, int *status, int severity);
void wrtsep(FILE *out, char fill, char *title, int nchar);
void num_err_wrn(fv_context *ctx, int *num_err, int *num_wrn);
void reset_err_wrn(fv_context *ctx);
int  compkey(const void *key1, const void *key2);
int  compcol(const void *col1, const void *col2);
int  compstrp(const void *str1, const void *str2);
int  compstre(const void *str1, const void *str2);

#endif /* FV_INTERNAL_H */
