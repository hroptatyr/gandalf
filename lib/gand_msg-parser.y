%defines
%pure-parser
%lex-param{void *scanner}
%parse-param{void *scanner}
%parse-param{gand_msg_t msg}

%{
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "gandalf.h"
#include "gand_msg-private.h"

/* for the select snarfer */
#define __snarf_select		snarf_select
#include "gand_msg-select.c"

extern int yylex(void*, void *scanner);


#define YYENABLE_NLS		0
#define YYLTYPE_IS_TRIVIAL	1
#define YYSTACK_USE_ALLOCA	1

#if !defined UNUSED
# define UNUSED(_x)	_x __attribute__((unused))
#endif	/* !UNUSED */

static int
yyerror(void *UNUSED(sca), gand_msg_t UNUSED(msg), char *UNUSED(s))
{
	return -1;
}

%}

%union {
	long int ival;
	char *sval;
}

%expect 0
%token
TOK_BOLLOCKS
TOK_DATE
TOK_INUM
TOK_RANGE
TOK_AND
TOK_ALT
TOK_SYM
TOK_KEY
TOK_NOW
TOK_THEN
TOK_FILTER
TOK_SELECT
TOK_IGNCASE

TOK_GET_SER
TOK_GET_DAT
TOK_GET_NFO

%%

cmd:
cmd_get_ser {
	gand_set_msg_type(msg, GAND_MSG_GET_SER);
	YYACCEPT;
} |
cmd_get_dat {
	gand_set_msg_type(msg, GAND_MSG_GET_DAT);
	YYACCEPT;
} |
cmd_get_nfo {
	gand_set_msg_type(msg, GAND_MSG_GET_NFO);
};

cmd_get_ser:
cmd_get_ser_mand opt_lists;

cmd_get_dat:
cmd_get_dat_mand opt_lists;

cmd_get_nfo:
cmd_get_nfo_mand opt_lists;

opt_lists:
|
opt_lists opt_list;

opt_list:
TOK_IGNCASE {
	msg->igncase = 1;
} |
valflav_list |
select_list;

cmd_get_ser_mand:
TOK_GET_SER rolf_obj |
TOK_GET_SER rolf_obj date_range_list;

cmd_get_dat_mand:
TOK_GET_DAT date_range |
TOK_GET_DAT date_range rolf_obj_list;

cmd_get_nfo_mand:
TOK_GET_NFO rolf_obj_list;

rolf_obj_list:
rolf_obj |
rolf_obj_list rolf_obj;

rolf_obj:
TOK_INUM {
	resize_rolf_objs(msg);
	msg->rolf_objs[msg->nrolf_objs++].rolf_id = $<ival>1;
} |
TOK_SYM {
	resize_rolf_objs(msg);
	msg->rolf_objs[msg->nrolf_objs++].rolf_sym = strdup($<sval>1);
};

date_range_list:
date_range |
date_range_list date_range;

date_range:
date {
	size_t idx = msg->ndate_rngs;
	idate_t idt = $<ival>1;

	resize_date_rngs(msg);
	msg->date_rngs[idx].beg = idt;
	msg->date_rngs[idx].end = idt;
	msg->ndate_rngs++;
} |
date TOK_RANGE date {
	size_t idx = msg->ndate_rngs;
	idate_t idt1 = $<ival>1;
	idate_t idt2 = $<ival>3;

	resize_date_rngs(msg);
	if (idt1 <= idt2) {
		msg->date_rngs[idx].beg = idt1;
		msg->date_rngs[idx].end = idt2;
	} else {
		msg->date_rngs[idx].beg = idt2;
		msg->date_rngs[idx].end = idt1;
	}
	msg->ndate_rngs++;
} |
date TOK_RANGE TOK_INUM {
	/* special syntax 2010-02-19 -3, 3 points till 2010-02-19 */
	size_t idx = msg->ndate_rngs;
	idate_t idt1 = $<ival>1;
	idate_t idt2 = $<ival>3;

	resize_date_rngs(msg);
	msg->date_rngs[idx].beg = idt2;
	msg->date_rngs[idx].end = idt1;
	msg->ndate_rngs++;
} |
date TOK_AND TOK_INUM {
	/* special syntax 2010-02-19 +3, 3 points from 2010-02-19 */
	size_t idx = msg->ndate_rngs;
	idate_t idt1 = $<ival>1;
	idate_t idt2 = $<ival>3;

	resize_date_rngs(msg);
	msg->date_rngs[idx].beg = idt1;
	msg->date_rngs[idx].end = idt2;
	msg->ndate_rngs++;
};

date:
TOK_DATE |
TOK_NOW |
TOK_THEN;

valflav_list:
TOK_FILTER valflav |
valflav_list TOK_AND valflav;

valflav:
TOK_SYM {
	resize_valflavs(msg);
	msg->valflavs[msg->nvalflavs++].this = strdup($<sval>1);
} |
valflav TOK_ALT TOK_SYM {
	struct valflav_s *vf = msg->valflavs + msg->nvalflavs - 1;
	resize_alts(vf);
	vf->alts[vf->nalts++] = strdup($<sval>3);
};

select_list:
TOK_SELECT select |
select_list TOK_AND select;

select:
TOK_SYM {
	const struct __select_s *sel;
	const char *v = $<sval>1;

	if ((sel = snarf_select(v, strlen(v))) != NULL) {
		msg->sel |= sel->val;
	}
};

%%

/* gand_msg-parser.y ends here */
