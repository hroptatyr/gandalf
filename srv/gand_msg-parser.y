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
} |
cmd_get_dat {
	gand_set_msg_type(msg, GAND_MSG_GET_DATE);
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
TOK_INUM;

date_range_list:
date_range |
date_range_list date_range;

date_range:
date |
date TOK_RANGE date;

date:
TOK_DATE;

%%

/* gand_msg-parser.y ends here */
