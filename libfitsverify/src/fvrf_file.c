#include "fv_internal.h"
#include "fv_context.h"

int get_total_warn(fv_context *ctx)
{
    return (ctx->file_total_warn);
}
int get_total_err(fv_context *ctx)
{
    return (ctx->file_total_err);
}

/* Get the total hdu number and allocate the memory for hdu array */
void init_hduname(fv_context *ctx)
{
    int i;
    /* allocate memories for the hdu structure array  */
    ctx->hduname = (HduName **)malloc(ctx->totalhdu*sizeof(HduName *));
    for (i=0; i < ctx->totalhdu; i++) {
	ctx->hduname[i] = (HduName *)calloc(1, sizeof(HduName));
	ctx->hduname[i]->hdutype = -1;
        ctx->hduname[i]->errnum = 0;
        ctx->hduname[i]->wrnno = 0;
        strcpy(ctx->hduname[i]->extname,"");
        ctx->hduname[i]->extver = 0;
    }
    return;
}
/* set the hduname memeber hdutype, extname, extver */
void set_hduname(  fv_context *ctx,
                   int hdunum,		/* hdu number */
		   int hdutype,		/* hdutype */
		   char* extname,	/* extension name */
                   int  extver 		/* extension version */
                )
{
    int i;
    i = hdunum - 1;
    ctx->hduname[i]->hdutype = hdutype;
    if(extname!=NULL)
        strcpy (ctx->hduname[i]->extname,extname);
    else
        strcpy(ctx->hduname[i]->extname,"");
    ctx->hduname[i]->extver = extver;
    return;
}


/* get the total errors and total warnings in this hdu */
void set_hduerr(fv_context *ctx,
                int hdunum	/* hdu number */
                )
{
    int i;
    i = hdunum - 1;
    num_err_wrn(ctx, &(ctx->hduname[i]->errnum), &(ctx->hduname[i]->wrnno));
    reset_err_wrn(ctx);   /* reset the error and warning counter */
    return;
}

/* set the basic information for hduname structure */
void set_hdubasic(fv_context *ctx, int hdunum, int hdutype)
{
   set_hduname(ctx, hdunum, hdutype, NULL, 0);
   set_hduerr(ctx, hdunum);
   return;
}

/* test to see whether the two extension having the same name */
/* return 1: identical 0: different */
int test_hduname (fv_context *ctx,
                  int hdunum1,		/* index of first hdu */
		  int hdunum2		/* index of second hdu */
                  )
{
    HduName *p1;
    HduName *p2;

    p1 = ctx->hduname[hdunum1-1];
    p2 = ctx->hduname[hdunum2-1];
    if(!strlen(p1->extname) || !strlen(p2->extname)) return 0;
    if(!strcmp(p1->extname,p2->extname) && p1->hdutype == p2->hdutype
       && p2->extver == p1->extver && hdunum1 != hdunum2){
	   return 1;
    }
    return 0;
}

/* Added the error numbers */
void total_errors (fv_context *ctx, int *toterr, int * totwrn)
{
   int i = 0;
   int ierr, iwrn;
   *toterr = 0;
   *totwrn = 0;

   if (ctx->totalhdu == 0) { /* this means the file couldn't be opened */
       *toterr = 1;
       return;
   }

   for (i = 0; i < ctx->totalhdu; i++) {
       *toterr += ctx->hduname[i]->errnum;
       *totwrn += ctx->hduname[i]->wrnno;
   }
   /*check the end of file errors */
   num_err_wrn(ctx, &ierr, &iwrn);
   *toterr +=ierr;
   *totwrn +=iwrn;
   return;
}

/* print the extname, exttype, extver, errnum and wrnno in a  table */
void hdus_summary(fv_context *ctx, FILE *out)
{
   HduName *p;
   int i;
   int ierr, iwrn;
   char temp[FLEN_VALUE];
   char temp1[FLEN_VALUE];

   wrtsep(out,'+'," Error Summary  ",60);
   wrtout(out," ");
   sprintf(ctx->comm," HDU#  Name (version)       Type             Warnings  Errors");
   wrtout(out,ctx->comm);

   sprintf(ctx->comm," 1                          Primary Array    %-4d      %-4d  ",
	   ctx->hduname[0]->wrnno,ctx->hduname[0]->errnum);
   wrtout(out,ctx->comm);
   for (i=2; i <= ctx->totalhdu; i++) {
       p = ctx->hduname[i-1];
       strcpy(temp,p->extname);
       if(p->extver && p->extver!= -999) {
           sprintf(temp1," (%-d)",p->extver);
           strcat(temp,temp1);
       }
       switch(ctx->hduname[i-1]->hdutype){
	   case IMAGE_HDU:
               sprintf(ctx->comm," %-5d %-20s Image Array      %-4d      %-4d  ",
	               i,temp, p->wrnno,p->errnum);
               wrtout(out,ctx->comm);
	       break;
	   case ASCII_TBL:
               sprintf(ctx->comm," %-5d %-20s ASCII Table      %-4d      %-4d  ",
	               i,temp, p->wrnno,p->errnum);
               wrtout(out,ctx->comm);
	       break;
	   case BINARY_TBL:
               sprintf(ctx->comm," %-5d %-20s Binary Table     %-4d      %-4d  ",
	               i,temp, p->wrnno,p->errnum);
               wrtout(out,ctx->comm);
	       break;
           default:
               sprintf(ctx->comm," %-5d %-20s Unknown HDU      %-4d      %-4d  ",
	               i,temp, p->wrnno,p->errnum);
               wrtout(out,ctx->comm);
	       break;
      }
   }
   /* check the end of file */
   num_err_wrn(ctx, &ierr, &iwrn);
   if (iwrn || ierr) {
     sprintf(ctx->comm," End-of-file %-30s  %-4d      %-4d  ", "", iwrn,ierr);
     wrtout(out,ctx->comm);
   }
   wrtout(out," ");
   return;
}



void destroy_hduname(fv_context *ctx)
{
   int i;
   for (i=0; i < ctx->totalhdu; i++) free(ctx->hduname[i]);
   free(ctx->hduname);
   return;
}

/* Routine to test the extra bytes at the end of file */
   void  test_end(fv_context *ctx,
                  fitsfile *infits,
		  FILE *out)

{
   int status = 0;
   LONGLONG headstart, datastart, dataend;
   int hdutype;

   /* check whether there are any HDU left */
   fits_movrel_hdu(infits,1, &hdutype, &status);
   if (!status) {
       wrtout(out,"< End-of-File >");
       sprintf(ctx->errmes,
    "There are extraneous HDU(s) beyond the end of last HDU.");
       wrterr(ctx, out,ctx->errmes,2);
       wrtout(out," ");
       return;
   }

   if (status != END_OF_FILE) {
      wrtserr(ctx, out,"Bad HDU? ",&status,2);
      return;
   }

   status = 0;
   fits_clear_errmsg();
   if(ffghadll(infits, &headstart, &datastart, &dataend, &status))
       wrtferr(ctx, out, "",&status,1);

   /* try to move to the last byte of this extension.  */
   if (ffmbyt(infits, dataend - 1,0,&status))
   {
       sprintf(ctx->errmes,
   "Error trying to read last byte of the file at byte %ld.", (long) dataend);
       wrterr(ctx, out,ctx->errmes,2);
       wrtout(out,"< End-of-File >");
       wrtout(out," ");
       return;
   }

   /* try to move to what would be the first byte of the next extension.
     If successfull, we have a problem... */

   ffmbyt(infits, dataend,0,&status);
   if(status == 0) {
       wrtout(out,"< End-of-File >");
       sprintf(ctx->errmes,
     "File has extra byte(s) after last HDU at byte %ld.", (long) dataend);
       wrterr(ctx, out,ctx->errmes,2);
       wrtout(out," ");
   }

   return;
}



/******************************************************************************
* Function
*      init_report
*
*
* DESCRIPTION:
*      Initialize the fverify report
*
*******************************************************************************/
void init_report(fv_context *ctx,
                 FILE *out,              /* output file */
                 char *rootnam          /* input file name */
                 )
{
    sprintf(ctx->comm,"\n%d Header-Data Units in this file.",ctx->totalhdu);
    wrtout(out,ctx->comm);
    wrtout(out," ");

    reset_err_wrn(ctx);
    init_hduname(ctx);
}

/******************************************************************************
* Function
*      close_report
*
*
* DESCRIPTION:
*      Close the fverify report
*
*******************************************************************************/
void close_report(fv_context *ctx,
                  FILE *out              /* output file */ )
{
    int numerrs = 0;                    /* number of the errors         */
    int numwrns = 0;                    /* number of the warnings       */

    /* print out a summary of all the hdus */
    if(ctx->prstat)hdus_summary(ctx, out);
    total_errors (ctx, &numerrs, &numwrns);

    ctx->file_total_warn = numwrns;
    ctx->file_total_err  = numerrs;

    /* get the total number of errors and warnnings */
    sprintf(ctx->comm,"**** Verification found %d warning(s) and %d error(s). ****",
              numwrns, numerrs);
    wrtout(out,ctx->comm);

    update_parfile(ctx, numerrs,numwrns);
    /* destroy the hdu name */
    destroy_hduname(ctx);
    return ;
}

/******************************************************************************
* Function
*      update_parfile
*
*
* DESCRIPTION:
*      Accumulate error and warning counts into the context totals.
*      (PIL operations removed for library use.)
*
*******************************************************************************/
void update_parfile(fv_context *ctx, int nerr, int nwrn)
{
    ctx->totalerr += (long)nerr;
    ctx->totalwrn += (long)nwrn;
}
