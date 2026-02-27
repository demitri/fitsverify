#include "fv_internal.h"
#include "fv_context.h"
#include "fv_hints.h"

int fits_parse_card(fv_context *ctx,
                    FILE *out,		/* output file pointer */
                    int  kpos,          /* keyposition starting from 1 */
		    char *card,  	/* key card */
		    char *kname,	/* key name */
		    kwdtyp *ktype,	/* key type */
		    char *kvalue,	/* key value */
		    char *kcomm		/* comment */
                   )
/* Ref: Defininition of the Flexible Image Transport System(FITS),
	Sec. 5.1 and 5.2.
*/
{
    char vind[3];
    char *p;
    char **pt;
    int i;
    char temp1[FLEN_CARD];
    unsigned long stat = 0;

    *kname = '\0';
    *kvalue = '\0';
    *kcomm = '\0';
    *ktype =  UNKNOWN;

    if(strlen(card) > FLEN_CARD-1 ) {
	strncpy(temp1,card,20);
	temp1[21]='\0';
	snprintf(ctx->errmes, sizeof(ctx->errmes), "card %s is > 80.",card);
	wrterr(ctx,out,ctx->errmes,1,FV_ERR_CARD_TOO_LONG);
	return 1;
    }
    card[FLEN_CARD-1] = '\0';

    /* get the kname */
    strncpy(kname, card, 8);
    kname[8] = '\0';

    /* take out the trailing space */
    i = 7;
    p = &kname[7];
    while(isspace((int)*p) && i >= 0) {*p = '\0'; p--; i--;}

    FV_HINT_SET_KEYWORD(ctx, kname);

    /* Whether the keyword name is left justified */
    i = 0;
    p = &kname[0];
    while(isspace((int)*p) && *p != '\0' ) {  p++; i++;}
    if( i < 8 && i > 0) {
        snprintf(ctx->errmes, sizeof(ctx->errmes), "Keyword #%d: Name %s is not left justified.",
           kpos,kname);
	wrterr(ctx,out,ctx->errmes,1,FV_ERR_NAME_NOT_JUSTIFIED);
    }
    /* Whether the characters in keyword name are valid */
    while(*p != '\0' ){
	if((*p < 'A'  ||  *p > 'Z')&&
	   (*p < '0'  ||  *p > '9')&&
	   (*p != '-' &&  *p  != '_') ) {
	    snprintf(ctx->errmes, sizeof(ctx->errmes),
"Keyword #%d: Name \"%s\" contains char \"%c\" which is not upper case letter, digit, \"-\", or \"_\".",kpos,kname,*p);
	    wrterr(ctx,out,ctx->errmes,1,FV_ERR_ILLEGAL_NAME_CHAR);
	    break;
        }
	p++; i++;
    }

    /* COMMENT, HISTORY, HIERARCH and "" keywords */
    if( !strcmp(kname,"COMMENT") ||
        !strcmp(kname,"HISTORY") ||
        !strcmp(kname,"HIERARCH") ||
        !strcmp(kname,"CONTINUE") ||
        !strcmp(kname,"")   ){

        *ktype =  COM_KEY;

        p = &card[8];
        strcpy(kcomm, p);
        kcomm[FLEN_COMMENT-1] = '\0';
        for( ; *p != '\0'; p++) {
	    if(!isprint((int)*p)) {
		snprintf(ctx->errmes, sizeof(ctx->errmes),
                "Keyword #%d, %s: String contains non-text characters.",
		    kpos,kname);
                wrterr(ctx,out,ctx->errmes,1,FV_ERR_NONTEXT_CHARS);
		return 1;
	    }
        }
	p = kname;
        while(!isspace((int)*p)&& *p != '\0')p++;
	*p = '\0';
        return 0;
    }

    /* End Keyword: 9-80 shall be filled with ASCII blanks \x20 */
    if( !strcmp(kname,"END") ){
        *ktype =  COM_KEY;
        if(card[3] == '\0') return 0;
        for( p = &card[8]; *p != '\0'; p++) {
            if(*p != '\x20' ){
		wrterr(ctx,out,"END keyword contains non-blank characters.",1,FV_ERR_END_NOT_BLANK);
		return 1;
            }
        }
	kname[3] = '\0';
	return 0;
    }


    /* check for value indicator */
    p = &card[8];
    strncpy(vind,p,2);
    vind[2] = '\0';
    if(strcmp(vind,"= ") && strcmp(vind,"=") ){
        /* no value indicator, so this is a commentary keyword */
       *ktype =  COM_KEY;
        strcpy(kcomm, p);
        kcomm[FLEN_COMMENT-1] = '\0';
        for( ; *p != '\0'; p++) {
	    if(!isprint((int)*p)) {
		snprintf(ctx->errmes, sizeof(ctx->errmes),
                "Keyword #%d, %s: String contains non-text characters.",
		    kpos,kname);
                wrterr(ctx,out,ctx->errmes,1,FV_ERR_NONTEXT_CHARS);
		return 1;
	    }
        }
	p = kname;
        while(!isspace((int)*p)&& *p != '\0')p++;
	*p = '\0';
        return 0;
    }

    p = &card[10];
    while (isspace((int)*p) && *p != '\0')  p++;
    pt = &p;
    switch (*p) {
	case '\'': 	/* string */
	    get_str(pt, kvalue,&stat);
	    *ktype = STR_KEY;
            p = *pt;
	    if(*p != '\0') get_comm(pt,kcomm,&stat);
	    break;
	case 'T': case 'F':	 	/*logical */
	    get_log(pt, kvalue, &stat);
	    *ktype = LOG_KEY;
            p = *pt;
	    if(*p != '\0') get_comm(pt,kcomm,&stat);
	    break;
	case '+': case '-': case '.':	/* number */
	case '0': case '1': case '2':
	case '3': case '4': case '5':
	case '6': case '7': case '8':
	case '9':
	    get_num(pt, kvalue, ktype, &stat);
            p = *pt;
	    if(*p != '\0') get_comm(pt,kcomm,&stat);
	    break;
	case '(':			/* complex number */
	    get_cmp(pt, kvalue, ktype, &stat);
            p = *pt;
	    if(*p != '\0') get_comm(pt,kcomm,&stat);
	    break;
	case '/':			/* comment */
	    if(*p != '\0') get_comm(pt,kcomm,&stat);
            *ktype = UNKNOWN;
            break;
        default:
            get_unknown(pt,kvalue,ktype,&stat);
            p = *pt;
	    if(*p != '\0') get_comm(pt,kcomm,&stat);
    }
    /* take out the trailing blanks for non-string keys */
    if(*ktype != STR_KEY) {
        i = strlen(kvalue);
        p = &kvalue[i-1];
        while(isspace((int)*p) && i >0) {
            *p = '\0';
            p--; i--;
        }
        if(i == 0 && isspace((int)*p))*p = '\0';
    }
    pr_kval_err(ctx,out,kpos,kname,kvalue,stat);
    if(stat != 0) return 1;
    return 0;
}

/* parse And test the string keys */
void get_str(char **pt,     		/* card string from character 11*/
	    char *kvalue,		/* key value string */
	    unsigned long *stat		/* error number */
	   )
{
    char *pi;
    char prev;		/* previous char */
    int nchar = 0;
    char *p;

    p = *pt;
    pi = p;
    p++;
    prev = 'a';
    while(*p != '\0') {
        if( !isprint((int)*p) )*stat |= BAD_STR;
	if(prev == '\'' && *p != '\'') break;
	if(prev == '\'' && *p == '\'') {    /* skip the '' */
	    p++;
	    prev = 'a';
        }
	else {
            prev = *p;
            p++;
        }
    }
    p--;
    if(*p != '\'') *stat |= NO_TRAIL_QUOTE;
    pi++;
    nchar = p - pi ;     /* excluding the ' */
    strncpy(kvalue,pi,nchar);
    *(kvalue+nchar) = '\0';
    pi = kvalue + (nchar -1) ;
    while(isspace((int)*pi)){ *pi = '\0'; pi--;} /* delete the trailing space */
    p++;				  /* skip the  ' */
    while(isspace((int)*p) && *p != '\0')  p++;
    *pt = p;
    return;
}

/* parse and test the logical keys */
void get_log(char **pt,     		/* card string */
	    char *kvalue,		/* key value string */
	    unsigned long *stat		/* error number */
	   )
{
    char *p;

    p = *pt;
    *kvalue = *p;
    kvalue[1] = '\0';
    p++;
    while(isspace((int)*p)) p++;
    if(*p != '/' && *p != '\0') *stat |= BAD_LOGICAL;
    *pt = p;
    return;
}

/* parse and test the numerical keys */
void get_num(char **pt,     		/* card string */
	    char *kvalue,		/* comment string */
	    kwdtyp *ktype,
	    unsigned long *stat		/* error number */
	   )
{
    char *pi;
    int set_deci = 0;
    int set_expo = 0;
    int nchar;
    char *p;

    p = *pt;
    pi = p;
    *ktype = INT_KEY;

    if( *p != '+' && *p != '-' && !isdigit((int)*p) &&*p != '.') {
        *stat |= BAD_NUM;
	return;
    }
    if(*p == '.') {
	*ktype = FLT_KEY;
	set_deci = 1;
    }

    p++;
    while(!isspace((int)*p) && *p != '\0' && *p != '/') {
        if( *p == '.' && !set_deci ){
	    set_deci = 1;
	    *ktype = FLT_KEY;
	    p++;
	    continue;
        }
        if( (*p == 'd'|| *p == 'e') && !set_expo) {
            set_expo = 1;
	    *ktype = FLT_KEY;
	    p++;
	    if(*p == '+' || *p == '-') p++;
            *stat |= LOWCASE_EXPO;
	    continue;
        }
        if( (*p == 'D'|| *p == 'E') && !set_expo) {
            set_expo = 1;
	    *ktype = FLT_KEY;
	    p++;
	    if(*p == '+' || *p == '-') p++;
	    continue;
        }
	if(!isdigit((int)*p)) *stat |= BAD_NUM;
	p++;
    }
    nchar = p - pi;
    strncpy(kvalue,pi,nchar);
    *(kvalue+nchar) = '\0';
    while(isspace((int)*p) && *p != '\0')  p++;
    *pt = p;
    return;
}

/* parse and test the complex keys */
void get_cmp(char **pt,     		/* card string */
	    char *kvalue,		/* comment string */
	    kwdtyp *ktype,
	    unsigned long *stat		/* error number */
	   )
{
    char *p;
    char **pp;
    char *pr_beg;			/* end of real part */
    char *pr_end=0;			/* end of real part */
    char *pi_beg;			/* beginning of the imaginay part */
    char *pi_end=0;			/* end of real part */
    int  nchar;
    int set_comm = 0;
    int set_paren = 0;

    unsigned long tr  = 0;
    unsigned long ti = 0;
    kwdtyp rtype, itype;
    char temp[FLEN_CARD];
    char card[FLEN_CARD];


    strcpy(card,*pt);			/* save the original */
    card[FLEN_CARD-1] = '\0';

    *ktype = CMI_KEY;			/* default: integer complex */
    p = card + 1;
    pr_beg = p;

    temp[0] = '\0';
    while(*p != '\0' && *p != '/') {
	if(*p == ')') {
	    set_paren = 1;
	    pi_end = p;
	    p++;
	    break;
        }
	if(!set_comm && *p == ',') {
	    set_comm = 1;
	    pr_end = p;
	    pi_beg = p+1;
        }
        else if(*p == ',') {
	    *stat |= TOO_MANY_COMMA;
        }
	p++;
    }
    if(!set_comm) *stat |= NO_COMMA;
    if(!set_paren) {
	*stat |= NO_TRAIL_PAREN;
	pi_end = p;
	pi_end--;
	while(isspace((int)*pi_end))pi_end--;
	pi_end++;
    }

    nchar = pi_end - card ;
    strncpy(kvalue,card,nchar);
    *(kvalue+nchar) = '\0';
    while(isspace((int)*p)&& *p != '\0')  p++;
    *pt = *pt + (p - card);

    /* analyse the real and imagine part */
    *pr_end = '\0';
    *pi_end = '\0';
    while(isspace((int)*pr_beg) && *pr_beg != '\0')  pr_beg++;
    while(isspace((int)*pi_beg) && *pi_beg != '\0')  pi_beg++;
    temp[0] = '\0';
    pp = &pr_beg;
    get_num(pp, temp, &rtype, &tr);
    if(tr)*stat |= BAD_REAL;
    temp[0] = '\0';
    pp = &pi_beg;
    get_num(pp, temp, &itype, &ti);
    if(ti)*stat |= BAD_IMG;
    if(rtype == FLT_KEY || itype == FLT_KEY) *ktype = CMF_KEY;
    return;
}

/* parse and test the comment keys */
void get_comm(char **pt,     		/* card string */
	    char *kcomm,		/* comment string */
	    unsigned long *stat		/* error number */
	   )
{
    char *pi;
    int nchar = 0;
    char *p;

    p = *pt;
    pi = p;
    if(*p != '/')  {
      *stat |= NO_START_SLASH;
    }
    p++;
    while(*p != '\0') {
        if(!isprint((int)*p) ) *stat |=  BAD_COMMENT;
        p++;
    }
    nchar = p - pi;
    strncpy(kcomm,pi,nchar);
    *(kcomm+nchar) = '\0';
    return;
}

/* parsing the unknown keyword */
void get_unknown(char **pt,     		/* card string */
	    char *kvalue,		/* comment string */
	    kwdtyp *ktype,
	    unsigned long *stat		/* error number */
	   )
{
     char *p;
     char *p1;
     char temp[FLEN_CARD];

     p = *pt;
     strcpy(temp,*pt);
     p1 = temp;
     while(*p != '\0' && *p != '/') { p++; p1++;}
     *p1 = '\0';
     p1 = temp;
     *pt = p;

     strcpy(kvalue, p1);
     *ktype = UNKNOWN;
     *stat |= UNKNOWN_TYPE;
     return ;
}
/* routine to print out the error of keyword value/comment */
void pr_kval_err(fv_context *ctx,
                 FILE *out,		/* output  FILE */
                 int  kpos,          /* keyposition starting from 1 */
                char *kname,		/* keyword name */
                char *kval,		/* keyword value */
		unsigned long errnum	/* error number */
               )
{
    if(errnum == 0) return;
    FV_HINT_SET_KEYWORD(ctx, kname);
    if(errnum & BAD_STR) {
	snprintf(ctx->errmes, sizeof(ctx->errmes),
        "Keyword #%d, %s: String \"%s\"  contains non-text characters.",
         kpos,kname,kval);
	wrterr(ctx,out,ctx->errmes,1,FV_ERR_BAD_STRING);
    }
    if(errnum & NO_TRAIL_QUOTE) {
	snprintf(ctx->errmes, sizeof(ctx->errmes),
  "Keyword #%d, %s: The closing \"\'\" is missing in the string." ,
         kpos,kname);
	wrterr(ctx,out,ctx->errmes,1,FV_ERR_MISSING_QUOTE);
    }
    if(errnum & BAD_LOGICAL) {
	snprintf(ctx->errmes, sizeof(ctx->errmes), "Keyword #%d, %s: Bad logical value \"%s\".",
         kpos,kname,kval);
	 wrterr(ctx,out,ctx->errmes,1,FV_ERR_BAD_LOGICAL);
    }
    if(errnum & BAD_NUM) {
	snprintf(ctx->errmes, sizeof(ctx->errmes), "Keyword #%d, %s: Bad numerical value \"%s\".",
         kpos,kname,kval);
	wrterr(ctx,out,ctx->errmes,1,FV_ERR_BAD_NUMBER);
    }
    if(errnum & LOWCASE_EXPO) {
	snprintf(ctx->errmes, sizeof(ctx->errmes),
"Keyword #%d, %s: lower-case exponent d or e is illegal in value %s.",
         kpos,kname,kval);
	wrterr(ctx,out,ctx->errmes,1,FV_ERR_LOWERCASE_EXPONENT);
    }
    if(errnum & NO_TRAIL_PAREN) {
	snprintf(ctx->errmes, sizeof(ctx->errmes),
   "Keyword #%d, %s: Complex value \"%s\" misses closing \")\".",
         kpos,kname, kval);
	wrterr(ctx,out,ctx->errmes,1,FV_ERR_COMPLEX_FORMAT);
    }
    if(errnum & NO_COMMA) {
	snprintf(ctx->errmes, sizeof(ctx->errmes),
           "keyword #%d, %s : Complex value \"%s\" misses \",\".",
            kpos,kname,kval);
	wrterr(ctx,out,ctx->errmes,1,FV_ERR_COMPLEX_FORMAT);
    }
    if(errnum & TOO_MANY_COMMA) {
	snprintf(ctx->errmes, sizeof(ctx->errmes),
       "Keyword #%d, %s: Too many \",\" are in the complex value \"%s\".",
         kpos,kname,kval);
	wrterr(ctx,out,ctx->errmes,1,FV_ERR_COMPLEX_FORMAT);
    }
    if(errnum & BAD_REAL) {
	snprintf(ctx->errmes, sizeof(ctx->errmes),
        "Keyword #%d, %s: Real part of complex value \"%s\" is  bad.",
         kpos,kname,kval);
	wrterr(ctx,out,ctx->errmes,1,FV_ERR_COMPLEX_FORMAT);
    }
    if(errnum & BAD_IMG) {
	snprintf(ctx->errmes, sizeof(ctx->errmes),
        "Keyword #%d, %s: Imagine part of complex value \"%s\" is bad.",
        kpos,kname,kval);
	wrterr(ctx,out,ctx->errmes,1,FV_ERR_COMPLEX_FORMAT);
    }
    if(errnum & NO_START_SLASH) {
	snprintf(ctx->errmes, sizeof(ctx->errmes),
    "Keyword #%d, %s: Value and Comment not separated by a \"/\".",
         kpos,kname);
	wrterr(ctx,out,ctx->errmes,1,FV_ERR_NO_VALUE_SEPARATOR);
    }
    if(errnum & BAD_COMMENT) {
	snprintf(ctx->errmes, sizeof(ctx->errmes),
   "Keyword #%d, %s: Comment contains non-text characters.",
         kpos,kname);
	wrterr(ctx,out,ctx->errmes,1,FV_ERR_BAD_COMMENT);
    }

    if(errnum & UNKNOWN_TYPE) {
      if (*kval != 0) {  /* don't report null keywords as an error */
	snprintf(ctx->errmes, sizeof(ctx->errmes),
   "Keyword #%d, %s: Type of value \"%s\" is unknown.",
         kpos,kname,kval);
	wrterr(ctx,out,ctx->errmes,1,FV_ERR_UNKNOWN_TYPE);
      }
    }
    return ;
}

    int check_str(fv_context *ctx, FitsKey* pkey, FILE *out)
{
    FV_HINT_SET_KEYWORD(ctx, pkey->kname);
    if(pkey->ktype == UNKNOWN && *(pkey->kvalue) == 0) {
        snprintf(ctx->errmes, sizeof(ctx->errmes), "Keyword #%d, %s has a null value; expected a string.",
        pkey->kindex,pkey->kname);
        wrterr(ctx,out,ctx->errmes,1,FV_ERR_NULL_VALUE);
        return 0;
    } else if(pkey->ktype != STR_KEY) {
        snprintf(ctx->errmes, sizeof(ctx->errmes), "Keyword #%d, %s: \"%s\" is not a string.",
        pkey->kindex,pkey->kname, pkey->kvalue);
        if (pkey->ktype == INT_KEY || pkey->ktype == FLT_KEY) {
            FV_HINT_SET_FIX(ctx,
                "Add quotes around the value of '%s' in HDU %d. "
                "The current value %s should be a quoted string.",
                pkey->kname, ctx->curhdu, pkey->kvalue);
        } else if (*(pkey->kvalue) == 0) {
            FV_HINT_SET_FIX(ctx,
                "'%s' in HDU %d has no value. Set it to a quoted "
                "string (e.g., %s = 'value').",
                pkey->kname, ctx->curhdu, pkey->kname);
        } else {
            FV_HINT_SET_FIX(ctx,
                "Set '%s' in HDU %d to a properly quoted string "
                "value. The current value '%s' is not recognized "
                "as a string.",
                pkey->kname, ctx->curhdu, pkey->kvalue);
        }
        FV_HINT_SET_EXPLAIN(ctx,
            "'%s' is expected to be a string keyword in the FITS "
            "Standard. String values must be enclosed in single "
            "quotes in columns 11-80 of the header card.",
            pkey->kname);
        wrterr(ctx,out,ctx->errmes,1,FV_ERR_WRONG_TYPE);
        return 0;
    }
    return 1;
}

    int check_int(fv_context *ctx, FitsKey* pkey, FILE *out)
{
    FV_HINT_SET_KEYWORD(ctx, pkey->kname);
    if(pkey->ktype == UNKNOWN && *(pkey->kvalue) == 0) {
        snprintf(ctx->errmes, sizeof(ctx->errmes), "Keyword #%d, %s has a null value; expected an integer.",
        pkey->kindex,pkey->kname);
        wrterr(ctx,out,ctx->errmes,1,FV_ERR_NULL_VALUE);
        return 0;
    } else if(pkey->ktype != INT_KEY) {
        snprintf(ctx->errmes, sizeof(ctx->errmes), "Keyword #%d, %s: value = %s is not an integer.",
        pkey->kindex,pkey->kname, pkey->kvalue);
	if(pkey->ktype == STR_KEY) {
	   strcat(ctx->errmes," The value is entered as a string. ");
           FV_HINT_SET_FIX(ctx,
               "Remove the quotes from '%s' in HDU %d. "
               "The value must be an integer, not a string.",
               pkey->kname, ctx->curhdu);
           FV_HINT_SET_EXPLAIN(ctx,
               "'%s' currently has the quoted string '%s'. "
               "Remove the quotes so it is parsed as an integer.",
               pkey->kname, pkey->kvalue);
        }
        wrterr(ctx,out,ctx->errmes,1,FV_ERR_WRONG_TYPE);
        return 0;
    }
    return 1;
}
    int check_flt(fv_context *ctx, FitsKey* pkey, FILE *out)
{
    FV_HINT_SET_KEYWORD(ctx, pkey->kname);
    if(pkey->ktype == UNKNOWN && *(pkey->kvalue) == 0) {
        snprintf(ctx->errmes, sizeof(ctx->errmes), "Keyword #%d, %s has a null value; expected a float.",
        pkey->kindex,pkey->kname);
        wrterr(ctx,out,ctx->errmes,1,FV_ERR_NULL_VALUE);
        return 0;
    } else if(pkey->ktype != INT_KEY && pkey->ktype != FLT_KEY) {
        snprintf(ctx->errmes, sizeof(ctx->errmes),
        "Keyword #%d, %s: value = %s is not a floating point number.",
        pkey->kindex,pkey->kname, pkey->kvalue);
	if(pkey->ktype == STR_KEY) {
	   strcat(ctx->errmes," The value is entered as a string. ");
           FV_HINT_SET_FIX(ctx,
               "Remove the quotes from '%s' in HDU %d. "
               "The value must be a number, not a string.",
               pkey->kname, ctx->curhdu);
           FV_HINT_SET_EXPLAIN(ctx,
               "'%s' currently has the quoted string '%s'. "
               "This keyword requires a numeric value. Remove the "
               "quotes and provide the actual number.",
               pkey->kname, pkey->kvalue);
        }
        wrterr(ctx,out,ctx->errmes,1,FV_ERR_WRONG_TYPE);
        return 0;
    }
    return 1;
}

    int check_cmi(fv_context *ctx, FitsKey* pkey, FILE *out)
{
    FV_HINT_SET_KEYWORD(ctx, pkey->kname);
    if(pkey->ktype != CMI_KEY ) {
        snprintf(ctx->errmes, sizeof(ctx->errmes),
          "Keyword #%d, %s: value = %s is not a integer complex number.",
        pkey->kindex,pkey->kname, pkey->kvalue);
	if(pkey->ktype == STR_KEY) {
	   strcat(ctx->errmes," The value is entered as a string. ");
           FV_HINT_SET_FIX(ctx,
               "Remove the quotes from '%s' in HDU %d. "
               "The value must be an integer complex number, not a string.",
               pkey->kname, ctx->curhdu);
           FV_HINT_SET_EXPLAIN(ctx,
               "'%s' currently has the quoted string '%s'. "
               "Complex integer values are written as two integers "
               "in parentheses without quotes: (real, imag).",
               pkey->kname, pkey->kvalue);
        }
        wrterr(ctx,out,ctx->errmes,1,FV_ERR_WRONG_TYPE);
        return 0;
    }
    return 1;
}

    int check_cmf(fv_context *ctx, FitsKey* pkey, FILE *out)
{
    FV_HINT_SET_KEYWORD(ctx, pkey->kname);
    if(pkey->ktype != CMI_KEY && pkey->ktype != CMF_KEY) {
        snprintf(ctx->errmes, sizeof(ctx->errmes),
           "Keyword #%d, %s: value = %s is not a floating point complex number.",
        pkey->kindex,pkey->kname, pkey->kvalue);
	if(pkey->ktype == STR_KEY) {
	   strcat(ctx->errmes," The value is entered as a string. ");
           FV_HINT_SET_FIX(ctx,
               "Remove the quotes from '%s' in HDU %d. "
               "The value must be a complex number, not a string.",
               pkey->kname, ctx->curhdu);
           FV_HINT_SET_EXPLAIN(ctx,
               "'%s' currently has the quoted string '%s'. "
               "Complex floating-point values are written as two numbers "
               "in parentheses without quotes: (real, imag).",
               pkey->kname, pkey->kvalue);
        }
        wrterr(ctx,out,ctx->errmes,1,FV_ERR_WRONG_TYPE);
        return 0;
    }
    return 1;
}
    int check_log(fv_context *ctx, FitsKey* pkey, FILE *out)
{
    FV_HINT_SET_KEYWORD(ctx, pkey->kname);
    if(pkey->ktype != LOG_KEY ) {
        snprintf(ctx->errmes, sizeof(ctx->errmes),
            "Keyword #%d, %s: value = %s is not a logical constant.",
        pkey->kindex,pkey->kname, pkey->kvalue);
	if(pkey->ktype == STR_KEY) {
	   strcat(ctx->errmes," The value is entered as a string. ");
           FV_HINT_SET_FIX(ctx,
               "Remove the quotes from '%s' in HDU %d. "
               "The value must be a logical (T or F), not a string.",
               pkey->kname, ctx->curhdu);
           FV_HINT_SET_EXPLAIN(ctx,
               "'%s' currently has the quoted string '%s'. "
               "Logical keywords must have T or F (without quotes) "
               "in column 30 of the header card.",
               pkey->kname, pkey->kvalue);
        }
        wrterr(ctx,out,ctx->errmes,1,FV_ERR_WRONG_TYPE);
        return 0;
    }
    return 1;
}
    int check_fixed_int(fv_context *ctx, char* card, FILE *out)
{
    char *cptr;
    char kw_buf[9];

    /* Set hint keyword from card columns 1-8 */
    strncpy(kw_buf, card, 8);
    kw_buf[8] = '\0';
    { char *ep = kw_buf + 7; while (ep >= kw_buf && *ep == ' ') *ep-- = '\0'; }
    FV_HINT_SET_KEYWORD(ctx, kw_buf);

    /* fixed format integer must be right justified in columns 11-30 */

    cptr = &card[10];

    while (*cptr == ' ')cptr++;  /* skip leading spaces */

    if (*cptr == '-')
        cptr++;  /* skip leading minus sign */
    else if (*cptr == '+')
        cptr++;  /* skip leading plus sign */

    while (isdigit((int) *cptr))cptr++;  /* skip digits */

    /* should be pointing to column 31 of the card */

    if ((cptr - card)  != 30) {
        snprintf(ctx->errmes, sizeof(ctx->errmes),
            "%.8s mandatory keyword is not in integer fixed format:",
        card);
        wrterr(ctx,out,ctx->errmes,1,FV_ERR_NOT_FIXED_FORMAT);
        print_fmt(ctx,out,card,13);
        print_fmt(ctx,out,"          -------------------^",13);

        return 0;
    }
    return 1;
}
    int check_fixed_log(fv_context *ctx, char* card, FILE *out)
{
    char *cptr;
    char kw_buf[9];

    strncpy(kw_buf, card, 8);
    kw_buf[8] = '\0';
    { char *ep = kw_buf + 7; while (ep >= kw_buf && *ep == ' ') *ep-- = '\0'; }
    FV_HINT_SET_KEYWORD(ctx, kw_buf);

    /* fixed format logical must have T or F in column 30 */

    cptr = &card[10];

    while (*cptr == ' ')cptr++;  /* skip leading spaces */

    if (*cptr != 'T' && *cptr != 'F') {
        snprintf(ctx->errmes, sizeof(ctx->errmes),
            "%.8s mandatory keyword does not have T or F logical value.",
        card);
        wrterr(ctx,out,ctx->errmes,1,FV_ERR_BAD_LOGICAL);
        return 0;
    }

    /* should be pointing to column 31 of the card */

    if ((cptr - card)  != 29) {
        snprintf(ctx->errmes, sizeof(ctx->errmes),
            "%.8s mandatory keyword is not in logical fixed format:",
        card);
        wrterr(ctx,out,ctx->errmes,1,FV_ERR_NOT_FIXED_FORMAT);
        print_fmt(ctx,out,card,13);
        print_fmt(ctx,out,"          -------------------^",13);

        return 0;
    }
    return 1;
}
    int check_fixed_str(fv_context *ctx, char* card, FILE *out)
{
    char *cptr;
    char kw_buf[9];

    strncpy(kw_buf, card, 8);
    kw_buf[8] = '\0';
    { char *ep = kw_buf + 7; while (ep >= kw_buf && *ep == ' ') *ep-- = '\0'; }
    FV_HINT_SET_KEYWORD(ctx, kw_buf);

    /* fixed format string must have quotes in columns 11 and >= 20 */
    /* This only applys to the XTENSION and TFORMn keywords. */

    cptr = &card[10];

    if (*cptr != '\'' ) {
        snprintf(ctx->errmes, sizeof(ctx->errmes),
            "%.8s mandatory string keyword does not start in col 11.",
        card);
        wrterr(ctx,out,ctx->errmes,1,FV_ERR_NOT_FIXED_FORMAT);
        print_fmt(ctx,out,card,13);
          print_fmt(ctx,out,"          ^--------^",13);
        return 0;
    }

    cptr++;

    while (*cptr != '\'') {

        if (*cptr == '\0') {
          snprintf(ctx->errmes, sizeof(ctx->errmes),
            "%.8s mandatory string keyword missing closing quote character:",
          card);
          wrterr(ctx,out,ctx->errmes,1,FV_ERR_NOT_FIXED_FORMAT);
          print_fmt(ctx,out,card,13);
          return 0;
        }
        cptr++;
    }

    if ((cptr - card)  < 19) {
        snprintf(ctx->errmes, sizeof(ctx->errmes),
            "%.8s mandatory string keyword ends before column 20.",
        card);
        wrterr(ctx,out,ctx->errmes,1,FV_ERR_NOT_FIXED_FORMAT);
        print_fmt(ctx,out,card,13);
        print_fmt(ctx,out,"          ^--------^",13);

        return 0;
    }

    return 1;
}
