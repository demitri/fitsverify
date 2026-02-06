/******************************************************************************
* Function
*      wrtout: print messages in the streams of stdout and out.
*      wrterr: print error messages in the streams of stderr and out.
*      wrtferr: print cfitsio error messages in the streams of stderr and out.
*      wrtwrn: print warning messages in the streams of stdout and out.
*      wrtsep: print separators.
*      num_err_wrn: Return the number of errors and warnings.
*
*******************************************************************************/
#include <setjmp.h>
#include "fv_internal.h"
#include "fv_context.h"

void num_err_wrn(fv_context *ctx, int *num_err, int *num_wrn)
{
    *num_wrn = ctx->nwrns;
    *num_err = ctx->nerrs;
    return;
}

void reset_err_wrn(fv_context *ctx)
{
    ctx->nwrns = 0;
    ctx->nerrs = 0;
    return;
}

int wrtout(FILE *out, char *mess)
{
    if(out != NULL )fprintf(out,"%s\n",mess);
    if(out == stdout) fflush(stdout);
    return 0;
}

int wrtwrn(fv_context *ctx, FILE *out, char *mess, int isheasarc)
{
    if(ctx->err_report) return 0;
    if(!ctx->heasarc_conv && isheasarc) return 0;
    ctx->nwrns++;
    strcpy(ctx->misc_temp,"*** Warning: ");
    strcat(ctx->misc_temp,mess);
    if(isheasarc) strcat(ctx->misc_temp," (HEASARC Convention)");
    print_fmt(ctx,out,ctx->misc_temp,13);
    return ctx->nwrns;
}

int wrterr(fv_context *ctx, FILE *out, char *mess, int severity )
{
    if(severity < ctx->err_report) {
        fits_clear_errmsg();
        return 0;
    }
    ctx->nerrs++;

    strcpy(ctx->misc_temp,"*** Error:   ");
    strcat(ctx->misc_temp,mess);
    if(out != NULL) {
         if ((out!=stdout) && (out!=stderr)) print_fmt(ctx,out,ctx->misc_temp,13);
#ifdef ERR2OUT
         print_fmt(ctx,stdout,ctx->misc_temp,13);
#else
         print_fmt(ctx,stderr,ctx->misc_temp,13);
#endif
    }

    if(ctx->nerrs > MAXERRORS ) {
#ifdef ERR2OUT
	 fprintf(stdout,"??? Too many Errors! I give up...\n");
#else
	 fprintf(stderr,"??? Too many Errors! I give up...\n");
#endif
         close_report(ctx, out);
         if (ctx->abort_set)
             longjmp(ctx->abort_jmp, 1);
    }
    fits_clear_errmsg();
    return ctx->nerrs;
}

int wrtferr(fv_context *ctx, FILE *out, char* mess, int *status, int severity)
{
    char ttemp[255];

    if(severity < ctx->err_report) {
        fits_clear_errmsg();
        return 0;
    }
    ctx->nerrs++;

    strcpy(ctx->misc_temp,"*** Error:   ");
    strcat(ctx->misc_temp,mess);
    fits_get_errstatus(*status, ttemp);
    strcat(ctx->misc_temp,ttemp);
    if(out != NULL ) {
        if ((out!=stdout) && (out!=stderr)) print_fmt(ctx,out,ctx->misc_temp,13);
#ifdef ERR2OUT
         print_fmt(ctx,stdout,ctx->misc_temp,13);
#else
         print_fmt(ctx,stderr,ctx->misc_temp,13);
#endif
    }

    *status = 0;
    fits_clear_errmsg();
    if(ctx->nerrs > MAXERRORS ) {
#ifdef ERR2OUT
	 fprintf(stdout,"??? Too many Errors! I give up...\n");
#else
	 fprintf(stderr,"??? Too many Errors! I give up...\n");
#endif
         close_report(ctx, out);
         if (ctx->abort_set)
             longjmp(ctx->abort_jmp, 1);
    }
    return ctx->nerrs;
}

int wrtserr(fv_context *ctx, FILE *out, char* mess, int *status, int severity)
{
    char* errfmt = "             %.67s\n";
    int i;
    char tmp[20][80];
    int nstack = 0;

    if(severity < ctx->err_report) {
        fits_clear_errmsg();
        return 0;
    }
    ctx->nerrs++;

    strcpy(ctx->misc_temp,"*** Error:   ");
    strcat(ctx->misc_temp,mess);
    strcat(ctx->misc_temp,"(from CFITSIO error stack:)");
    while(nstack < 20) {
        tmp[nstack][0] = '\0';
        i = fits_read_errmsg(tmp[nstack]);
        if(!i && tmp[nstack][0]=='\0') break;
        nstack++;
    }

    if(out !=NULL) {
        if ((out!=stdout) && (out!=stderr)) {
           print_fmt(ctx,out,ctx->misc_temp,13);
           for(i=0; i<=nstack; i++) fprintf(out,errfmt,tmp[i]);
         }
#ifdef ERR2OUT
           print_fmt(ctx,stdout,ctx->misc_temp,13);
           for(i=0; i<=nstack; i++) fprintf(stdout,errfmt,tmp[i]);
#else
           print_fmt(ctx,stderr,ctx->misc_temp,13);
           for(i=0; i<=nstack; i++) fprintf(stderr,errfmt,tmp[i]);
#endif
    }

    *status = 0;
    fits_clear_errmsg();
    if(ctx->nerrs > MAXERRORS ) {
#ifdef ERR2OUT
	 fprintf(stdout,"??? Too many Errors! I give up...\n");
#else
	 fprintf(stderr,"??? Too many Errors! I give up...\n");
#endif
         close_report(ctx, out);
         if (ctx->abort_set)
             longjmp(ctx->abort_jmp, 1);
    }
    return ctx->nerrs;
}

void print_fmt(fv_context *ctx, FILE *out, char *temp, int nprompt)
{
    char *p;
    int i,j;
    int clen;
    char tmp[81];

    if (out == NULL) return;

    if(nprompt != ctx->save_nprompt) {
        for (i = 0; i < nprompt; i++) ctx->cont_fmt[i] = ' ';
        ctx->cont_fmt[nprompt] = '\0';
        strcat(ctx->cont_fmt,"%.67s\n");
        ctx->save_nprompt = nprompt;
    }

    i = strlen(temp) - 80;
    if(i <= 0) {
        fprintf(out,"%.80s\n",temp);
    }
    else{
        p = temp;
        clen = 80 -nprompt;
        strncpy(tmp,p,80);
        tmp[80] = '\0';
        if(isprint((int)*(p+79)) && isprint((int)*(p+80)) && *(p+80) != '\0') {
           j = 79;
           while(*(p+j) != ' ' && j > 0) j--;
           p += j;
           while( *p == ' ')p++;
           tmp[j] = '\0';
        }
        else if( *(p+80) == ' ') {
             j = 80;
             while( *(p+j) == ' ') j++;
             p +=  j;
        }
        else {
             p += 80;
        }
        fprintf(out,"%.80s\n",tmp);
        while(*p != '\0' && i > 0) {
            strncpy(tmp,p,clen);
            tmp[clen] = '\0';
            i = strlen(p)- clen;
            if(i > 0 && isprint((int)*(p+clen-1))
                     && isprint((int)*(p+clen))
                     && *(p+clen) != '\0') {
                j = clen;
                while(*(p+j)!= ' ' && j > 0) j--;
                p += j;
                while( *p == ' ')p++;
                tmp[j] = '\0';
            }
            else if(i> 0 &&  *(p+clen) == ' ') {
                 j = clen;
                 while( *(p+j) == ' ') j++;
                 p += j;
            }
            else if(i> 0)  {
                 p+= clen;
            }
            fprintf(out,ctx->cont_fmt,tmp);
        }
    }
    if(out==stdout) fflush(stdout);
    return;
}

void wrtsep(FILE *out,char fill, char *title, int nchar)
{
    int ntitle;
    char *line;
    char *p;
    int first_end;
    int i = 0;

    ntitle = strlen(title);
    if(ntitle > nchar) nchar = ntitle;
    if(nchar <= 0) return;
    line = (char *)malloc((nchar+1)*sizeof(char));
    p = line;
    if(ntitle < 1) {
        for (i=0; i < nchar; i++) {*p = fill; p++;}
	*p = '\0';
    }
    else {
	first_end = ( nchar - ntitle)/2;
	for (i = 0; i < first_end; i++) { *p = fill; p++;}
	*p = '\0';
	strcat(line, title);
	p += ntitle;
        for( i = first_end + ntitle; i < nchar; i++) {*p = fill; p++;}
	*p = '\0';
    }
    if(out != NULL )fprintf(out,"%s\n",line);
    if(out == stdout )fflush(out);
    free (line);
    return ;
}


/* comparison function for the FitsKey structure array */
   int compkey (const void *key1, const void *key2)
{
       char *name1;
       char *name2;
       name1 = (*(FitsKey **)key1)->kname;
       name2 = (*(FitsKey **)key2)->kname;
       return strncmp(name1,name2,FLEN_KEYWORD);
}

/* comparison function for the colname structure array */
   int compcol (const void *col1, const void *col2)
{
       char *name1;
       char *name2;
       name1 = (*(ColName **)col1)->name;
       name2 = (*(ColName **)col2)->name;
       return strcmp(name1,name2);
}

/* comparison function for the string pattern matching */
   int compstrp (const void *str1, const void *str2)
{
   char *p;
   char *q;
   p = (char *)(*(char**) str1);
   q = (char *)(*(char**) str2);
   while( *q == *p && *q != '\0') {
       p++;
       q++;
       if(*p == '\0') return 0;
   }
   return (*p - *q);
}

/* comparison function for the string exact matching */
   int compstre (const void *str1, const void *str2)
{
   char *p;
   char *q;
   p = (char *)(*(char**) str1);
   q = (char *)(*(char**) str2);
   return strcmp( p, q);
}
