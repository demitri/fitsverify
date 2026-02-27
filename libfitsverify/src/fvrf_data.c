#include "fv_internal.h"
#include "fv_context.h"
#include "fv_hints.h"
typedef struct {
   int nnum;
   int ncmp;
   int nfloat;
   int *indatatyp;
   double *datamax;
   double *datamin;
   double *tnull;
   unsigned char *mask;      /* for bit X column only */
   int ntxt;
   FitsHdu *hduptr;
   FILE* out;
   fv_context *ctx;
   int *flag_minmax;
   long *repeat;
   int *datatype;
   int find_badbit;
   int find_baddot;
   int find_badspace;
   int find_badchar;
   int find_badlog;
}UserIter;

/*************************************************************
*
*      test_data
*
*   Test the HDU data
*
*  This routine reads every row and column of ASCII tables to
*  verify that the values have the correct format.
*
*  This routine checks the following types of columns in binary tables:
*
*    Logical L - value must be T, F or zero
*    Bit    nX - if n != a multiple of 8, then check that fill bits = 0
*    String  A - must contain ascii text, or zero
*
*   It is impossible to write an invalid value to the other types of
*   columns in binary tables (B, I, J, K, E, D, C and M) so these
*   columns are not read.
*
*  Since it is impossible to write an invalid value in a FITS image,
*  this routine does not read the image pixels.
*
*************************************************************/
void test_data(fv_context *ctx,
              fitsfile *infits, 	/* input fits file   */
	      FILE	*out,		/* output ascii file */
	      FitsHdu    *hduptr	/* fits hdu pointer  */
            )

{
    iteratorCol *iter_col=0;
    int  ncols;

    int nnum = 0;
    int *numlist;	/* the list of the column  whose data
                           type is numerical(scalar and complex) */
    int nfloat = 0;
    int *floatlist;	/* the list of the floating point columns in ASCII table */

    int ncmp = 0;
    int *cmplist;	/* the list of the column  whose data
                           type is numerical(scalar and complex) */
    int ntxt = 0;
    int *txtlist;	/* the list of column  whose data type is
			   string, logical, bit or complex */
    int niter = 0;	/* total columns read into  the literator function */

    int ndesc = 0;
    int *desclist;	/* the list of column which is the descriptor of
			   the variable length array. */
    int *isVarQFormat;  /* Format type for each of the ndesc variable-length
                             columns: 0 = type 'P', 1 = type 'Q' */

    long rows_per_loop = 0, offset;
    UserIter usrdata;

    int datatype;
    long repeat;

    long totalrows;
    LONGLONG length;
    LONGLONG toffset;
    long *maxlen;
    int icol;
    char *cdata;
    double *ndata;
    int *idata;
    int *maxminflag;
    int *dflag;
    char lnull = 2;
    int anynul;

    long rlength;
    long bytelength;
    long maxmax;

    int i = 0;
    int j = 0;
    long jl = 0;
    long k = 0;
    int status = 0;
    char errtmp[80];
    int*  perbyte;

    LONGLONG naxis2;

    int largeVarLengthWarned = 0;
    int largeVarOffsetWarned = 0;

    if(ctx->testcsum)
        test_checksum(ctx,infits,out);

    if(ctx->testfill) {
        test_agap(ctx,infits,out,hduptr);     /* test the bytes between the
                                                   ascii table columns. */
        if(ffcdfl(infits, &status)) {
            wrtferr(ctx,out,"checking data fill: ", &status, 1, FV_ERR_DATA_FILL);
            status = 0;
        }
    }

    if(hduptr->hdutype != ASCII_TBL &&
       hduptr->hdutype != BINARY_TBL ) return;

    ncols = hduptr->ncols;
    if(ncols <= 0) return;

    ffgkyjj(infits, "NAXIS2", &naxis2, NULL, &status);

    if (naxis2 > 2147483647) {
       wrtout(ctx, out, "Cannot test data in tables with more than 2**31 (2147483647) rows.");
       return;
    }

    /* separate the numerical, complex, text and
      the variable length vector columns */
    numlist =(int*)malloc(ncols * sizeof(int));
    floatlist =(int*)malloc(ncols * sizeof(int));
    cmplist =(int*)malloc(ncols * sizeof(int));
    txtlist =(int*)malloc(ncols * sizeof(int));
    desclist =(int*)malloc(ncols * sizeof(int));

    if(hduptr->hdutype == ASCII_TBL) {

        /*read every column of an ASCII table */
	rows_per_loop = 0;
        for (i=0; i< ncols; i++){
            if(fits_get_coltype(infits, i+1, &datatype, NULL, NULL, &status)){
               snprintf(ctx->errmes, sizeof(ctx->errmes), "Column #%d: ",i);
 	       wrtferr(ctx,out,ctx->errmes, &status,2, FV_ERR_CFITSIO);
            }
            if ( datatype != TSTRING ) {
	           numlist[nnum] = i+1;
	           nnum++;

		   if (datatype > TLONG) { /* floating point number column */
		      floatlist[nfloat] = i + 1;
		      nfloat++;
                   }

            } else {
 	       txtlist[ntxt] = i+1;
	       ntxt++;
            }
        }

    } else if (hduptr->hdutype == BINARY_TBL) {

        /* only check Bit, Logical and String columns in Binary tables */
	rows_per_loop = 0;
        for (i=0; i< ncols; i++){
            if(fits_get_coltype(infits, i+1, &datatype, &repeat, NULL,
               &status)){
               snprintf(ctx->errmes, sizeof(ctx->errmes), "Column #%d: ",i);
 	       wrtferr(ctx,out,ctx->errmes, &status,2, FV_ERR_CFITSIO);
            }

	    if(datatype < 0) {    /* variable length column */
	       desclist[ndesc] = i+1;
	       ndesc++;

            } else if(datatype == TBIT && (repeat%8) )
                {  /* bit column that does not have a multiple of 8 bits */
	           numlist[nnum] = i+1;
	           nnum++;

            } else if( (datatype == TLOGICAL) ||
                       (datatype == TSTRING ) )  {
	           txtlist[ntxt] = i+1;
	           ntxt++;
            }
            /* ignore all other types of columns (B I J K E D C and M ) */
        }
    }


    /*  Use Iterator to read the columns that are not variable length arrays */
    /* columns from  1 to nnum are scalar numerical columns.
       columns from  nnum+1 to  nnum+ncmp are complex columns.
       columns from  nnum+ncmp are text columns */
    niter = nnum + ncmp + ntxt + nfloat;

    if(niter)iter_col = (iteratorCol *) malloc (sizeof(iteratorCol)*niter);

    for (i=0; i< nnum; i++){
	fits_iter_set_by_num(&iter_col[i], infits, numlist[i], TDOUBLE,
	   InputCol);
    }
    for (i=0; i< ncmp; i++){
	j = nnum + i;
	fits_iter_set_by_num(&iter_col[j], infits, cmplist[i], TDBLCOMPLEX,
	   InputCol);
    }
    for (i=0; i< ntxt; i++){
	j = nnum + ncmp + i;
	fits_iter_set_by_num(&iter_col[j], infits, txtlist[i], 0,
	   InputCol);
    }
    for (i=0; i< nfloat; i++){
	j = nnum + ncmp + ntxt + i;
	fits_iter_set_by_num(&iter_col[j], infits, floatlist[i], TSTRING,
	   InputCol);
    }


    offset = 0;
    usrdata.nnum = nnum;
    usrdata.ncmp = ncmp;
    if (nnum > 0 || ncmp > 0) {
        usrdata.datamax  = (double *)calloc((nnum+ncmp), sizeof(double));
        usrdata.datamin  = (double *)calloc((nnum+ncmp), sizeof(double));
    }
    usrdata.tnull = (double *)calloc(ncols, sizeof(double));
    usrdata.ntxt = ntxt;
    usrdata.hduptr = hduptr;
    usrdata.out = out;
    usrdata.nfloat = nfloat;
    usrdata.ctx = ctx;

    /* get the mask for the bit X column
        for column other than the X, it always 255
        for Column nX, it will be 000...111, where # of 0 is n%8,
        # of 1 is 8 - n%8.
    */

    if(nnum > 0) usrdata.mask =
            (unsigned char *)calloc(nnum,sizeof( unsigned char));
    if(nnum > 0) usrdata.indatatyp =
            (int *)calloc(nnum,sizeof( int));
    for (i=0; i< nnum; i++){
        j = fits_iter_get_colnum(&(iter_col[i]));
        if(fits_get_coltype(infits, j, &datatype, &repeat, NULL, &status)){
           snprintf(ctx->errmes, sizeof(ctx->errmes), "Column #%d: ",i);
 	   wrtferr(ctx,out,ctx->errmes, &status,2, FV_ERR_CFITSIO);
        }
        usrdata.indatatyp[i] = datatype;
        usrdata.mask[i] = 255;
        if(datatype == TBIT) {
            repeat = repeat%8;
            usrdata.mask[i] = (usrdata.mask[i])>>repeat;
            if(!repeat) usrdata.mask[i] = 0;
        }
    }


    if(niter > 0) {
	if(fits_iterate_data(niter, iter_col, offset,rows_per_loop, iterdata,
            &usrdata,&status)){
            wrtserr(ctx,out,"When Reading data, ",&status,2, FV_ERR_CFITSIO_STACK);
        }
    }

    if(niter>0) free(iter_col);
    free(numlist);
    free(floatlist);
    free(cmplist);
    free(txtlist);
    if(nnum > 0) free(usrdata.mask);
    if(nnum > 0) free(usrdata.indatatyp);
    if(nnum > 0 || ncmp > 0) {
          free(usrdata.datamax);
          free(usrdata.datamin);
    }
    free(usrdata.tnull);
    if(!ndesc ) {
	goto data_end;
    }

    /* ------------read the variable length vectors -------------------*/
    usrdata.datamax  = (double *)calloc(ndesc, sizeof(double));
    usrdata.datamin  = (double *)calloc(ndesc, sizeof(double));
    usrdata.tnull  = (double *)calloc(ndesc, sizeof(double));
    maxminflag     = (int *) calloc(ndesc , sizeof(int));
    maxlen         = (long *) calloc(ndesc , sizeof(long));
    dflag          = (int *) calloc(ndesc , sizeof(int));
    perbyte        = (int *) calloc(ndesc , sizeof(int));
    isVarQFormat   = (int *) calloc(ndesc , sizeof(int));
    fits_get_num_rows(infits,&totalrows,&status);
    status = 0;

  /* this routine now only reads and test BIT, LOGICAL, and STRING columns */
  /* There is no point in reading the other columns because the other datatypes */
  /* have no possible invalid values.  */

    for (i = 0; i < ndesc; i++) {
        icol = desclist[i];
        parse_vtform(ctx,infits,out,hduptr,icol,&datatype,&maxlen[i],&isVarQFormat[i]);
	dflag[i] = 4;
        switch (datatype) {
          case -TBIT:
              dflag[i] = 1;
              perbyte[i] = -8;
              break;
          case -TBYTE:
              perbyte[i] = 1;
              break;
          case -TLOGICAL:
              dflag[i] = 3;
              perbyte[i] = 1;
              break;
          case -TSTRING:
              dflag[i] = 0;
              perbyte[i] = 1;
              break;
          case -TSHORT:
              perbyte[i] = 2;
              break;
          case -TLONG:
              perbyte[i] = 4;
              break;
          case -TFLOAT:
              perbyte[i] = 4;
              break;
          case -TDOUBLE:
              perbyte[i] = 8;
              break;
          case -TCOMPLEX:
              dflag[i] = 2;
              perbyte[i] = 8;
              break;
          case -TDBLCOMPLEX:
              dflag[i] = 2;
              perbyte[i] = 16;
              break;
          default:
              break;
        }
    }

    maxmax = maxlen[0];
    for (i = 1; i < ndesc; i++) {
	if(maxmax < maxlen[i]) maxmax = maxlen[i];
    }
    if(maxmax < 0) maxmax = 100;
    ndata = (double *)malloc(2*maxmax*sizeof(double));
    cdata = (char *)malloc((maxmax+1) *sizeof(char));
    idata = (int *)malloc(maxmax *sizeof(int));


    for (jl = 1; jl <= totalrows; jl++) {
        if (ctx->maxerrors_reached) break;
        for (i = 0; i < ndesc; i++) {
            icol = desclist[i];
            FV_HINT_SET_COLNUM(ctx, icol);

            /* read and check the descriptor length and offset values */
            if(fits_read_descriptll(infits, icol ,jl,&length,
		   &toffset, &status)){

                snprintf(errtmp, sizeof(errtmp), "Row #%ld Col.#%d: ",jl,icol);
	        wrtferr(ctx,out,errtmp,&status,2, FV_ERR_CFITSIO);
            }
            if (!isVarQFormat[i])
            {
               if (!largeVarLengthWarned && length > 2147483647)
               {
                  strcpy(ctx->errmes,"Var row length exceeds maximum 32-bit signed int.  ");
                  snprintf(errtmp, sizeof(errtmp), "First detected for Row #%ld Column #%d",jl,icol);
                  strcat(ctx->errmes,errtmp);
                  wrtwrn(ctx,out,ctx->errmes,0, FV_WARN_VAR_EXCEEDS_32BIT);
                  largeVarLengthWarned = 1;
               }
               if (!largeVarOffsetWarned && toffset > 2147483647)
               {
                  strcpy(ctx->errmes,"Heap offset for var length row exceeds maximum 32-bit signed int.  ");
                  snprintf(errtmp, sizeof(errtmp), "First detected for Row #%ld Column #%d",jl,icol);
                  strcat(ctx->errmes,errtmp);
                  wrtwrn(ctx,out,ctx->errmes,0, FV_WARN_VAR_EXCEEDS_32BIT);
                  largeVarOffsetWarned = 1;
               }
            }

	    if(length > maxlen[i] && maxlen[i] > -1 ) {
	        snprintf(ctx->errmes, sizeof(ctx->errmes), "Descriptor of Column #%d at Row %ld: ",
                     icol, jl);
                snprintf(errtmp, sizeof(errtmp), "nelem(%ld) > maxlen(%ld) given by TFORM%d.",
                    (long) length,maxlen[i],icol);
                strcat(ctx->errmes,errtmp);
                {
                    char colname[FLEN_VALUE] = "";
                    char tformval[FLEN_VALUE] = "";
                    char typechar = '?';
                    const char *p;
                    int tmpstat = 0;
                    snprintf(errtmp, sizeof(errtmp), "TTYPE%d", icol);
                    fits_read_key_str(infits, errtmp, colname, NULL, &tmpstat);
                    tmpstat = 0;
                    snprintf(errtmp, sizeof(errtmp), "TFORM%d", icol);
                    fits_read_key_str(infits, errtmp, tformval, NULL, &tmpstat);
                    /* Extract type char: in "1PE(0)", type is 'E' (after P/Q) */
                    for (p = tformval; *p; p++) {
                        if (*p == 'P' || *p == 'Q') { typechar = *(p+1); break; }
                        if (*p == 'p' || *p == 'q') { typechar = *(p+1); break; }
                    }
                    if (colname[0]) {
                        FV_HINT_SET_FIX(ctx,
                            "Column '%s' (col %d) has TFORM%d = '%s' "
                            "declaring max %ld elements, but row %ld "
                            "contains %ld. Change TFORM%d to '1%c%c(%ld)'.",
                            colname, icol, icol, tformval,
                            maxlen[i], jl, (long)length,
                            icol,
                            isVarQFormat[i] ? 'Q' : 'P', typechar,
                            (long)length);
                    } else {
                        FV_HINT_SET_FIX(ctx,
                            "Column %d has TFORM%d = '%s' declaring "
                            "max %ld elements, but row %ld contains %ld. "
                            "Change TFORM%d to '1%c%c(%ld)'.",
                            icol, icol, tformval, maxlen[i],
                            jl, (long)length,
                            icol,
                            isVarQFormat[i] ? 'Q' : 'P', typechar,
                            (long)length);
                    }
                    FV_HINT_SET_EXPLAIN(ctx,
                        "Variable-length array columns use TFORM = "
                        "'1P<type>(<max>)' where <max> declares the "
                        "maximum array size. The data in row %ld has %ld "
                        "elements which exceeds the declared maximum of "
                        "%ld. Either increase <max> in TFORM%d or the "
                        "data is corrupt. See FITS Standard Section 7.3.5.",
                        jl, (long)length, maxlen[i], icol);
                }
                wrterr(ctx,out,ctx->errmes,1, FV_ERR_VAR_EXCEEDS_MAXLEN);
            }

            if( perbyte[i] < 0)
                 bytelength = length/8;
            else
                 bytelength = length*perbyte[i];

            if(toffset + bytelength > hduptr->pcount ) {
	        snprintf(ctx->errmes, sizeof(ctx->errmes), "Descriptor of Column #%d at Row %ld: ",
                     icol, jl);
	        snprintf(errtmp, sizeof(errtmp),
                    " offset of first element(%ld) + nelem(%ld)",
                     (long) toffset, (long) length);
                strcat(ctx->errmes,errtmp);
                if(perbyte[i] < 0)
	            snprintf(errtmp, sizeof(errtmp), "/8 >  total heap area  = %ld.",
		       (long) hduptr->pcount);
                else
	            snprintf(errtmp, sizeof(errtmp), "*%d >  total heap area  = %ld.",
		       perbyte[i], (long) hduptr->pcount);
                strcat(ctx->errmes,errtmp);
                wrterr(ctx,out,ctx->errmes,2, FV_ERR_VAR_EXCEEDS_HEAP);
            }

            if(!length) continue;  /* skip the 0 length array */

            /* now check the values in BIT, LOGICAL, and String columns */
	    rlength = length;
	    if(length > maxmax) rlength = maxmax;

            if(dflag[i] == 1) { /* read BIT column */
		anynul = 0;

/*  NOT YET IMPLEMENTED:  This code should test that the fill bits that
    pad out the last byte are all zero.  Currently this test is applied
    to fixed length logical arrays, but has not yet been done for
    the variable length logical array case.  It is probably safe to assume
    that not many FITS files will contain variable length Logical columns,
    to adding this test is not a high priority.

	        if(fits_read_col(infits, TDOUBLE, icol , jl, 1,
	            nelem, &nullval, ndata, &anynul, &status)) {
	       	    wrtferr(ctx,out,"",&status,2);
                }
*/
            }

            else if(dflag[i] == 0) { /* read String column */
	        if(fits_read_col(infits, TSTRING, icol, jl, 1,
		    rlength, NULL, &cdata, &anynul, &status)) {
                    snprintf(errtmp, sizeof(errtmp), "Row #%ld Col.#%d: ",jl,icol);
	            wrtferr(ctx,out,errtmp,&status,2, FV_ERR_CFITSIO);
                }
                else {
                  j = 0;
                  while (cdata[j] != 0) {

                    if ((cdata[j] > 126) || (cdata[j] < 32) ) {
                      snprintf(ctx->errmes, sizeof(ctx->errmes),
                      "String in row #%ld, and column #%d contains non-ASCII text.", jl,icol);
                      wrterr(ctx,out,ctx->errmes,1, FV_ERR_NONASCII_DATA);
                        strcpy(ctx->errmes,
            "             (This error is reported only once; other rows may have errors).");
                      print_fmt(ctx,out,ctx->errmes,13);
                      break;
                    }
                    j++;
                  }
                }
            }
            else if(dflag[i] == 3) { /* read Logical column */
	        if(fits_read_col(infits, TLOGICAL, icol, jl, 1,
		    rlength, &lnull, cdata, &anynul, &status)) {
                    snprintf(errtmp, sizeof(errtmp), "Row #%ld Col.#%d: ",jl,icol);
	            wrtferr(ctx,out,errtmp,&status,2, FV_ERR_CFITSIO);
                }
                else {
		  for (k = 0; k < rlength; k++) {
                    if (cdata[k] > 2) {
                      snprintf(ctx->errmes, sizeof(ctx->errmes),
                      "Logical value in row #%ld, column #%d not equal to 'T', 'F', or 0",
                         jl, icol);
                       wrterr(ctx,out,ctx->errmes,1, FV_ERR_BAD_LOGICAL_DATA);
                       strcpy(ctx->errmes,
           "             (This error is reported only once; other rows may have errors).");
                       print_fmt(ctx,out,ctx->errmes,13);
                       break;
                    }
                  }
                }
	    }
        }
    }
    free(ndata);
    free(cdata);
    free(idata);

    free(usrdata.datamax);
    free(usrdata.datamin);
    free(usrdata.tnull);
    free(maxminflag);
    free(maxlen);
    free(dflag);
    free(perbyte);
    free(isVarQFormat);

data_end:
    free(desclist);
    for ( i = 0; i< ncols; i++) {
	(hduptr->datamax[i])[12] = '\0';
	(hduptr->datamin[i])[12] = '\0';
	(hduptr->tnull[i])[11] = '\0';
    }

    return;
}

/***********************************************************************/
/* iterator work function */

    int iterdata(long totaln,
		 long offset,
		 long firstn,
		 long nrows,
		 int narray,
		 iteratorCol *iter_col,
		 void *usrdata
		 )
{
    UserIter *usrpt;
/*
    FitsHdu  *hdupt;
*/
    int nnum;
    int ntxt;
    int ncmp;
    int nfloat;

    double  *data;
    unsigned char *ldata;
    char **cdata;
    unsigned char *ucdata;
    char *floatvalue;

    /* bit column working space */
    unsigned char bdata;

    int i;
    long j,k,l;
    long nelem;

    usrpt = (UserIter *)usrdata;

    if(firstn == 1 ) {  /* first time for this table, so initialize */
/*
	hdupt= usrpt->hduptr;
*/
        nnum = usrpt->nnum;
        ncmp = usrpt->ncmp;
        ntxt = usrpt->ntxt;
        nfloat = usrpt->nfloat;
	usrpt->flag_minmax = (int *)calloc(nnum+ncmp, sizeof(int));
	usrpt->repeat   = (long *)calloc(narray,sizeof(long));
	usrpt->datatype = (int *)calloc(narray,sizeof(int));
        for (i=0; i < narray; i++) {
	    usrpt->repeat[i] = fits_iter_get_repeat(&(iter_col[i]));
	    usrpt->datatype[i] = fits_iter_get_datatype(&(iter_col[i]));
        }
        usrpt->find_badbit = 0;
        usrpt->find_baddot = 0;
        usrpt->find_badspace = 0;
        usrpt->find_badchar = 0;
        usrpt->find_badlog = 0;
    }

    nnum = usrpt->nnum;
    ncmp = usrpt->ncmp;
    ntxt = usrpt->ntxt;
    nfloat = usrpt->nfloat;

    /* columns from  1 to nnum are scalar numerical columns.
       columns from  nnum+1 to  nnum+ncmp are complex columns. (not used any more)
       columns from  nnum+ncmp are text columns */

    /* deal with the numerical column */
    for (i=0; i < nnum+ncmp; i++) {
	data = (double *) fits_iter_get_array(&(iter_col[i]));
	j = 1;
	nelem = nrows * usrpt->repeat[i];
	if(i >= nnum) nelem = 2 * nrows *usrpt->repeat[i];
	if(nelem == 0) continue;
        usrpt->find_badbit = 0;

        /* check for the bit jurisfication  */
        FV_HINT_SET_COLNUM(usrpt->ctx, fits_iter_get_colnum(&(iter_col[i])));
        if(!usrpt->find_badbit && usrpt->indatatyp[i] == TBIT ) {
            for (k = 0; k < nrows; k++) {
               j = (k+1)*usrpt->repeat[i];
               bdata = (unsigned char)data[j];
               if( bdata & usrpt->mask[i] ) {
                  snprintf(usrpt->ctx->errmes, sizeof(usrpt->ctx->errmes),
                    "Row #%ld, and Column #%d: X vector ", firstn+k,
                      fits_iter_get_colnum(&(iter_col[i])));
                  for (l = 1; l<= usrpt->repeat[i]; l++) {
                     snprintf(usrpt->ctx->comm, sizeof(usrpt->ctx->comm), "0x%02x ", (unsigned char) data[k*usrpt->repeat[i]+l]);
                     strcat(usrpt->ctx->errmes,usrpt->ctx->comm);
                  }
                  strcat(usrpt->ctx->errmes,"is not left justified.");
                  wrterr(usrpt->ctx,usrpt->out,usrpt->ctx->errmes,2, FV_ERR_BIT_NOT_JUSTIFIED);
                  strcpy(usrpt->ctx->errmes,
          "             (Other rows may have errors).");
                  print_fmt(usrpt->ctx,usrpt->out,usrpt->ctx->errmes,13);
                  usrpt->find_badbit = 1;
                  break;
               }
            }
        }
    }

    /* deal with character and logical columns */
    for (i = nnum + ncmp; i < nnum + ncmp + ntxt; i++) {
        FV_HINT_SET_COLNUM(usrpt->ctx, fits_iter_get_colnum(&(iter_col[i])));
        if(usrpt->datatype[i] == TSTRING ) {	/* character */
            nelem = nrows;
	    if(nelem == 0) continue;
	    cdata = (char **) fits_iter_get_array(&(iter_col[i]));
            usrpt->find_badchar = 0;

            /* test for illegal ASCII text characters > 126  or < 32 */
            if (!usrpt->find_badchar) {
              for (k = 0; k < nrows; k++) {
                ucdata = (unsigned char *)cdata[k+1];
                j = 0;
                while (ucdata[j] != 0) {

                  if ((ucdata[j] > 126) || (ucdata[j] < 32)) {
                    snprintf(usrpt->ctx->errmes, sizeof(usrpt->ctx->errmes),
                    "String in row #%ld, column #%d contains non-ASCII text.", firstn+k,
                      fits_iter_get_colnum(&(iter_col[i])));
                      wrterr(usrpt->ctx,usrpt->out,usrpt->ctx->errmes,1, FV_ERR_NONASCII_DATA);
                      strcpy(usrpt->ctx->errmes,
          "             (Other rows may have errors).");
                      print_fmt(usrpt->ctx,usrpt->out,usrpt->ctx->errmes,13);
                    usrpt->find_badchar = 1;
                    break;
                  }
                  j++;
                }
              }
            }
        }

	else {  			/* logical value */
            nelem = nrows * usrpt->repeat[i];
	    if(nelem == 0) continue;
	    ldata = (unsigned char *) fits_iter_get_array(&(iter_col[i]));

            /* test for illegal logical column values */
            /* The first element in the array gives the value that is used to represent nulls */
            if (!usrpt->find_badlog) {
                for(j = 1; j <= nrows * usrpt->repeat[i]; j++) {
                  if (ldata[j] > 2) {
                    snprintf(usrpt->ctx->errmes, sizeof(usrpt->ctx->errmes),
                    "Logical value in row #%ld, column #%d not equal to 'T', 'F', or 0",
                       (firstn+j - 2)/usrpt->repeat[i] +1,
                       fits_iter_get_colnum(&(iter_col[i])));
                       wrterr(usrpt->ctx,usrpt->out,usrpt->ctx->errmes,1, FV_ERR_BAD_LOGICAL_DATA);
                       strcpy(usrpt->ctx->errmes,
         "             (Other rows may have similar errors).");
                       print_fmt(usrpt->ctx,usrpt->out,usrpt->ctx->errmes,13);
                       usrpt->find_badlog = 1;
                       break;
                  }
                }
            }
        }
    }

    for (i = nnum + ncmp +ntxt; i < nnum + ncmp + ntxt + nfloat; i++) {
            FV_HINT_SET_COLNUM(usrpt->ctx, fits_iter_get_colnum(&(iter_col[i])));
            nelem = nrows;
	    if(nelem == 0) continue;
	    cdata = (char **) fits_iter_get_array(&(iter_col[i]));
            usrpt->find_baddot = 0;
	    usrpt->find_badspace = 0;

            /* test for missing (implicit) decimal point in floating point numbers */
            if (!usrpt->find_baddot) {
              for (k = 0; k < nrows; k++) {
                floatvalue = (char *)cdata[k+1];
                if (strcmp(cdata[0], floatvalue) && !strchr(floatvalue, '.') ) {

		  while (*floatvalue == ' ')  /* skip leading spaces */
		        floatvalue++;

                  if (strlen(floatvalue)) {  /* ignore completely blank fields */

                    snprintf(usrpt->ctx->errmes, sizeof(usrpt->ctx->errmes),
                     "Number in row #%ld, column #%d has no decimal point:", firstn+k,
                     fits_iter_get_colnum(&(iter_col[i])));
                     wrterr(usrpt->ctx,usrpt->out,usrpt->ctx->errmes,1, FV_ERR_NO_DECIMAL);
                     strcpy(usrpt->ctx->errmes, floatvalue);
                     strcat(usrpt->ctx->errmes,
                  "  (Other rows may have similar errors).");
                     print_fmt(usrpt->ctx,usrpt->out,usrpt->ctx->errmes,13);
                     usrpt->find_baddot = 1;
                     break;
                  }
                }
              }
            }

            if (!usrpt->find_badspace) {
              for (k = 0; k < nrows; k++) {
                floatvalue = (char *)cdata[k+1];

                if (strcmp(cdata[0], floatvalue) ) {  /* not a null value? */
		    while (*floatvalue == ' ')  /* skip leading spaces */
		        floatvalue++;

                    l = strlen(floatvalue) - 1;
		    while (l > 0 &&  floatvalue[l] == ' ') { /* remove trailing spaces */
		        floatvalue[l] = '\0';
			l--;
		    }

                    if (strchr(floatvalue, ' ') ) {
                      snprintf(usrpt->ctx->errmes, sizeof(usrpt->ctx->errmes),
                       "Number in row #%ld, column #%d has embedded space:", firstn+k,
                         fits_iter_get_colnum(&(iter_col[i])));
                         wrterr(usrpt->ctx,usrpt->out,usrpt->ctx->errmes,1, FV_ERR_EMBEDDED_SPACE);
                         strcpy(usrpt->ctx->errmes, floatvalue);
                         strcat(usrpt->ctx->errmes,
                      "  (Other rows may have similar errors).");
                         print_fmt(usrpt->ctx,usrpt->out,usrpt->ctx->errmes,13);
                         usrpt->find_badspace = 1;
                         break;
                    }
		}
            }
          }
    }

    if(firstn + nrows - 1 == totaln) {
	free(usrpt->flag_minmax);
	free(usrpt->datatype);
	free(usrpt->repeat);
    }
    return 0;
}

/*************************************************************
*
*      test_agap
*
*   Test the bytes between the ASCII table column.
*
*
*************************************************************/
void test_agap(fv_context *ctx,
              fitsfile *infits, 	/* input fits file   */
	      FILE	*out,		/* output ascii file */
	      FitsHdu    *hduptr	/* fits hdu pointer  */
            )
{
    int ncols;
    LONGLONG nrows;
    long irows;
    LONGLONG rowlen;
    unsigned char *data;
    int *temp;
    unsigned char *p;
    LONGLONG i, j;
    int k, m, t;
    long firstrow = 1;
    long ntodo;
    long nerr = 0;
    int status = 0;
    char keyname[9];
    char tform[FLEN_VALUE], comment[256];
    int typecode, decimals;
    long width, tbcol;
    nerr = 0;

    if(hduptr->hdutype != ASCII_TBL) return;
    ncols = hduptr->ncols;
    fits_get_num_rowsll(infits,&nrows,&status);
    status = 0;

    fits_get_rowsize(infits, &irows, &status);
    status = 0;
    rowlen = hduptr->naxes[0];
    data = (unsigned char*)malloc(rowlen*sizeof(unsigned char)*irows);

    /* Create a template row with data fields filled with 1s.
       Used below - different ASCII rules apply within data columns
       vs. between data columns. */

    temp = (int*)malloc(rowlen * sizeof(int));
    for (m = 0; m<rowlen; m++ ) temp[m]=0;
    for (k = 1; k<=ncols; k++ ) {
	snprintf(keyname, sizeof(keyname), "TFORM%d",k);
	fits_read_key_str(infits, keyname, tform, comment, &status);
	if (fits_ascii_tform(tform, &typecode, &width, &decimals, &status))
	    wrtferr(ctx,out,"",&status,1, FV_ERR_CFITSIO);
	snprintf(keyname, sizeof(keyname), "TBCOL%d",k);
	fits_read_key_lng(infits, keyname, &tbcol, comment, &status);
	for (t = tbcol; t < tbcol+width; t++) temp[t-1]=1;
    }

    i = nrows;
    while( i > 0) {
	if( i > irows)
	    ntodo = irows;
        else
	    ntodo = i;

        p = data;
        if(fits_read_tblbytes(infits,firstrow,1, rowlen*ntodo,
	    data, &status)){
	    wrtferr(ctx,out,"",&status,1, FV_ERR_CFITSIO);
        }
        for (j = 0; j<rowlen*ntodo; j++ ) {
            if(!isascii(*p))  {
	        if(!nerr) {
#if (USE_LL_SUFFIX == 1)
		     snprintf(ctx->errmes, sizeof(ctx->errmes),
			"row %lld contains non-ASCII characters.", j/rowlen+1);
#else
		     snprintf(ctx->errmes, sizeof(ctx->errmes),
			"row %ld contains non-ASCII characters.", j/rowlen+1);
#endif
                     wrterr(ctx,out,ctx->errmes,1, FV_ERR_NONASCII_TABLE);
                }
                nerr++;
            } else if(isascii(*p) && !isprint(*p))  {
	        if(temp[j%rowlen]) {
	             if(!nerr) {
#if (USE_LL_SUFFIX == 1)
		          snprintf(ctx->errmes, sizeof(ctx->errmes),
			     "row %lld data contains non-ASCII-text characters.", j/rowlen+1);
#else
		          snprintf(ctx->errmes, sizeof(ctx->errmes),
			     "row %ld data contains non-ASCII-text characters.", j/rowlen+1);
#endif
                          wrterr(ctx,out,ctx->errmes,1, FV_ERR_NONASCII_TABLE);
                     }
                     nerr++;
                }
            }
	    p++;
        }
	firstrow += ntodo;
	i -=ntodo;
    }
    if(nerr) {
	snprintf(ctx->errmes, sizeof(ctx->errmes),
	    "This ASCII table contains %ld non-ASCII-text characters",nerr);
        wrterr(ctx,out,ctx->errmes,1, FV_ERR_NONASCII_TABLE);
    }
    free(data);
    free(temp);
    return;
}


/*************************************************************
*
*      test_checksum
*
*   Test the checksum of the hdu
*
*
*************************************************************/
void test_checksum(fv_context *ctx,
              fitsfile *infits, 	/* input fits file   */
	      FILE	*out		/* output ascii file */
            )
{
    int status = 0;
    int dataok, hduok;

    if (fits_verify_chksum(infits, &dataok, &hduok, &status))
    {
        wrtferr(ctx,out,"verifying checksums: ",&status,2, FV_ERR_CFITSIO);
        return;
    }

    if(dataok == -1)
	wrtwrn(ctx,out,
        "Data checksum is not consistent with  the DATASUM keyword",0, FV_WARN_BAD_CHECKSUM);

    if(hduok == -1 )  {
	if(dataok == 1) {
	   wrtwrn(ctx,out,
  "Invalid CHECKSUM means header has been modified. (DATASUM is OK) ",0, FV_WARN_BAD_CHECKSUM);
        }
	else {
	   wrtwrn(ctx,out, "HDU checksum is not in agreement with CHECKSUM.",0, FV_WARN_BAD_CHECKSUM);
        }
    }
    return;
}
