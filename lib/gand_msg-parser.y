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

extern int yylex(void*, void *scanner);


#define YYENABLE_NLS		0
#define YYLTYPE_IS_TRIVIAL	1
#define YYSTACK_USE_ALLOCA	1

static int
yyerror(void *sca, gand_msg_t msg, char *s)
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
TOK_RANGE
TOK_INUM

TOK_GET_SER
TOK_GET_DAT

%%

cmd:
cmd_get_ser {
	gand_set_msg_type(msg, GAND_MSG_GET_SERIES);
	YYACCEPT;
} |
cmd_get_dat {
	gand_set_msg_type(msg, GAND_MSG_GET_DATE);
	YYACCEPT;
};

cmd_get_ser:
TOK_GET_SER rolf_obj |
TOK_GET_SER rolf_obj date_range_list;

cmd_get_dat:
TOK_GET_DAT date |
TOK_GET_DAT date rolf_obj_list;

rolf_obj_list:
rolf_obj |
rolf_obj_list rolf_obj;

rolf_obj:
TOK_INUM {
	resize_rolf_objs(msg);
	msg->rolf_objs[msg->nrolf_objs++].rolf_id = $<ival>1;
};

date_range_list:
date_range |
date_range_list date_range;

date_range:
date {
	resize_date_rngs(msg);
	msg->date_rngs[msg->ndate_rngs++].beg = __to_idate($<sval>1);
} |
date TOK_RANGE date {
	size_t idx = msg->ndate_rngs;

	resize_date_rngs(msg);
	msg->date_rngs[idx].beg = __to_idate($<sval>1);
	msg->date_rngs[idx].end = __to_idate($<sval>3);
	msg->ndate_rngs++;
};

date:
TOK_DATE;

%%

/* gand_msg-parser.y ends here */
