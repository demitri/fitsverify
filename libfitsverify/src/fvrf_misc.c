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
#include "fv_internal.h"
#include "fv_context.h"
#include "fv_hints.h"

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

static void dispatch_msg(fv_context *ctx, fv_msg_severity severity,
                         int code, const char *text)
{
    fv_message msg;
    msg.severity = severity;
    msg.code     = (fv_error_code)code;
    msg.hdu_num  = ctx->curhdu;
    msg.text     = text;
    msg.fix_hint = NULL;
    msg.explain  = NULL;

    if ((ctx->fix_hints || ctx->explain) && code != FV_OK) {
        const fv_hint *h = fv_generate_hint(ctx, (fv_error_code)code);
        if (h) {
            if (ctx->fix_hints) msg.fix_hint = h->fix_hint;
            if (ctx->explain)   msg.explain  = h->explain;
        }
    }

    ctx->output_fn(&msg, ctx->output_udata);
    FV_HINT_CLEAR(ctx);
}

/* Print hint/explain text after an error/warning in FILE* mode */
static void print_hints_file(fv_context *ctx, FILE *out, int code)
{
    const fv_hint *h;
    if (!ctx->fix_hints && !ctx->explain) { FV_HINT_CLEAR(ctx); return; }
    if (code == FV_OK) { FV_HINT_CLEAR(ctx); return; }
    h = fv_generate_hint(ctx, (fv_error_code)code);
    if (!h) { FV_HINT_CLEAR(ctx); return; }
    if (ctx->fix_hints && h->fix_hint) {
        if (out && out != stdout && out != stderr)
            fprintf(out, "    Fix: %s\n", h->fix_hint);
        fprintf(stderr, "    Fix: %s\n", h->fix_hint);
    }
    if (ctx->explain && h->explain) {
        if (out && out != stdout && out != stderr)
            fprintf(out, "    Explanation: %s\n", h->explain);
        fprintf(stderr, "    Explanation: %s\n", h->explain);
    }
    FV_HINT_CLEAR(ctx);
}

int wrtout(fv_context *ctx, FILE *out, char *mess)
{
    if(ctx && ctx->output_fn) {
        dispatch_msg(ctx, FV_MSG_INFO, FV_OK, mess);
        return 0;
    }
    if(out != NULL )fprintf(out,"%s\n",mess);
    if(out == stdout) fflush(stdout);
    return 0;
}

int wrtwrn(fv_context *ctx, FILE *out, char *mess, int isheasarc, int code)
{
    if(ctx->maxerrors_reached) { FV_HINT_CLEAR(ctx); return 0; }
    if(ctx->err_report) { FV_HINT_CLEAR(ctx); return 0; }
    if(!ctx->heasarc_conv && isheasarc) { FV_HINT_CLEAR(ctx); return 0; }
    ctx->nwrns++;
    strcpy(ctx->misc_temp,"*** Warning: ");
    strcat(ctx->misc_temp,mess);
    if(isheasarc) strcat(ctx->misc_temp," (HEASARC Convention)");
    if(ctx->output_fn) {
        dispatch_msg(ctx, FV_MSG_WARNING, code, ctx->misc_temp);
        return ctx->nwrns;
    }
    print_fmt(ctx,out,ctx->misc_temp,13);
    print_hints_file(ctx, out, code);
    return ctx->nwrns;
}

int wrterr(fv_context *ctx, FILE *out, char *mess, int severity, int code)
{
    if(ctx->maxerrors_reached) {
        FV_HINT_CLEAR(ctx);
        fits_clear_errmsg();
        return ctx->nerrs;
    }
    if(severity < ctx->err_report) {
        FV_HINT_CLEAR(ctx);
        fits_clear_errmsg();
        return 0;
    }
    ctx->nerrs++;

    strcpy(ctx->misc_temp,"*** Error:   ");
    strcat(ctx->misc_temp,mess);
    if(ctx->output_fn) {
        dispatch_msg(ctx, severity >= 2 ? FV_MSG_SEVERE : FV_MSG_ERROR,
                     code, ctx->misc_temp);
        if(ctx->nerrs > MAXERRORS) {
            dispatch_msg(ctx, FV_MSG_SEVERE, FV_ERR_TOO_MANY,
                         "??? Too many Errors! I give up...");
            ctx->maxerrors_reached = 1;
        }
        fits_clear_errmsg();
        return ctx->nerrs;
    }
    if(out != NULL) {
         if ((out!=stdout) && (out!=stderr)) print_fmt(ctx,out,ctx->misc_temp,13);
         print_fmt(ctx,stderr,ctx->misc_temp,13);
    }
    print_hints_file(ctx, out, code);

    if(ctx->nerrs > MAXERRORS ) {
	 fprintf(stderr,"??? Too many Errors! I give up...\n");
         ctx->maxerrors_reached = 1;
    }
    fits_clear_errmsg();
    return ctx->nerrs;
}

int wrtferr(fv_context *ctx, FILE *out, char* mess, int *status, int severity, int code)
{
    char ttemp[255];

    if(ctx->maxerrors_reached) {
        FV_HINT_CLEAR(ctx);
        *status = 0;
        fits_clear_errmsg();
        return ctx->nerrs;
    }
    if(severity < ctx->err_report) {
        FV_HINT_CLEAR(ctx);
        fits_clear_errmsg();
        return 0;
    }
    ctx->nerrs++;

    strcpy(ctx->misc_temp,"*** Error:   ");
    strcat(ctx->misc_temp,mess);
    fits_get_errstatus(*status, ttemp);
    strcat(ctx->misc_temp,ttemp);
    if(ctx->output_fn) {
        dispatch_msg(ctx, severity >= 2 ? FV_MSG_SEVERE : FV_MSG_ERROR,
                     code, ctx->misc_temp);
        *status = 0;
        fits_clear_errmsg();
        if(ctx->nerrs > MAXERRORS) {
            dispatch_msg(ctx, FV_MSG_SEVERE, FV_ERR_TOO_MANY,
                         "??? Too many Errors! I give up...");
            ctx->maxerrors_reached = 1;
        }
        return ctx->nerrs;
    }
    if(out != NULL ) {
        if ((out!=stdout) && (out!=stderr)) print_fmt(ctx,out,ctx->misc_temp,13);
         print_fmt(ctx,stderr,ctx->misc_temp,13);
    }
    print_hints_file(ctx, out, code);

    *status = 0;
    fits_clear_errmsg();
    if(ctx->nerrs > MAXERRORS ) {
	 fprintf(stderr,"??? Too many Errors! I give up...\n");
         ctx->maxerrors_reached = 1;
    }
    return ctx->nerrs;
}

int wrtserr(fv_context *ctx, FILE *out, char* mess, int *status, int severity, int code)
{
    char* errfmt = "             %.67s\n";
    int i;
    char tmp[20][80];
    int nstack = 0;

    if(ctx->maxerrors_reached) {
        FV_HINT_CLEAR(ctx);
        *status = 0;
        fits_clear_errmsg();
        return ctx->nerrs;
    }
    if(severity < ctx->err_report) {
        FV_HINT_CLEAR(ctx);
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

    if(ctx->output_fn) {
        dispatch_msg(ctx, severity >= 2 ? FV_MSG_SEVERE : FV_MSG_ERROR,
                     code, ctx->misc_temp);
        for(i=0; i<nstack; i++)
            dispatch_msg(ctx, FV_MSG_INFO, FV_OK, tmp[i]);
        *status = 0;
        fits_clear_errmsg();
        if(ctx->nerrs > MAXERRORS) {
            dispatch_msg(ctx, FV_MSG_SEVERE, FV_ERR_TOO_MANY,
                         "??? Too many Errors! I give up...");
            ctx->maxerrors_reached = 1;
        }
        return ctx->nerrs;
    }

    if(out !=NULL) {
        if ((out!=stdout) && (out!=stderr)) {
           print_fmt(ctx,out,ctx->misc_temp,13);
           for(i=0; i<=nstack; i++) fprintf(out,errfmt,tmp[i]);
         }
           print_fmt(ctx,stderr,ctx->misc_temp,13);
           for(i=0; i<=nstack; i++) fprintf(stderr,errfmt,tmp[i]);
    }
    print_hints_file(ctx, out, code);

    *status = 0;
    fits_clear_errmsg();
    if(ctx->nerrs > MAXERRORS ) {
	 fprintf(stderr,"??? Too many Errors! I give up...\n");
         ctx->maxerrors_reached = 1;
    }
    return ctx->nerrs;
}

void print_fmt(fv_context *ctx, FILE *out, char *temp, int nprompt)
{
    char *p;
    int i,j;
    int clen;
    char tmp[81];

    if(ctx && ctx->output_fn) {
        dispatch_msg(ctx, FV_MSG_INFO, FV_OK, temp);
        return;
    }

    if (out == NULL) return;

    if(nprompt != ctx->save_nprompt) {
        if (nprompt > 70) nprompt = 70;  /* cap to prevent overflow */
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

void wrtsep(fv_context *ctx, FILE *out, char fill, char *title, int nchar)
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
    if(ctx && ctx->output_fn) {
        dispatch_msg(ctx, FV_MSG_INFO, FV_OK, line);
        free(line);
        return;
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
