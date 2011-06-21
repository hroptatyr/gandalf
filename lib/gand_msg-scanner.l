/*** gand_msg-scanner.l -- gand semantics  -*- C -*-
 *
 * Copyright (C) 2011  Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@unserding.org>
 *
 * This file is part of gandalf.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/

%{
#define YY_EXTRA_TYPE	void*
%}

%option reentrant
%option noyywrap
%option warn
%option noinput
%option bison-bridge
%option pointer
%option case-insensitive

%{

#include <stdlib.h>
#include <string.h>
#include "gandalf.h"
#include "nifty.h"
#include "gand_msg-parser.h"

#define GCSZ_DEBUG(args...)

#define RETURN_TOKEN(_x)			\
	do {					\
		GAND_DEBUG(#_x "\n");		\
		return _x;			\
	} while (0)

/* turn off tedious warnings */
#if defined __INTEL_COMPILER
# pragma warning (disable:177)
# pragma warning (disable:593)
# pragma warning (disable:869)
# pragma warning (disable:1419)
# pragma warning (disable:2259)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch-default"
# pragma GCC diagnostic ignored "-Wsign-compare"
# pragma GCC diagnostic ignored "-Wunused-parameter"
#endif	/* __GNUC__ || __INTEL_COMPILER */

/* declarations */
extern int yyparse(void *scanner, gand_msg_t msg);
%}

ws		[ \t\f\n\r]+
date		(19|20)[0-9]{2,2}-[0-9]{2,2}-[0-9]{2,2}
range		-
and		\+
alt		"|"|"/"
num		[1-9][0-9]*
sym		[^ \t\f\n\r]+@[[:alnum:]]*
key		[a-z_]+
now		now
then		then
filter		--filt(er)?|--flav(our)?

get_ser		get_ser(ies)?
get_dat		get_date?
get_nfo		get_i?nfo

%x QUO

%%

\" {
	BEGIN(QUO);
}

{range} {
	RETURN_TOKEN(TOK_RANGE);
}

{date} {
	yylval->ival = __to_idate(yytext);
	RETURN_TOKEN(TOK_DATE);
}

{num} {
	yylval->ival = strtol(yytext, NULL, 10);
	RETURN_TOKEN(TOK_INUM);
}

{get_ser} {
	RETURN_TOKEN(TOK_GET_SER);
}

{get_dat} {
	RETURN_TOKEN(TOK_GET_DAT);
}

{get_nfo} {
	RETURN_TOKEN(TOK_GET_NFO);
}

{now} {
	yylval->ival = 99991232;
	RETURN_TOKEN(TOK_NOW);
}

{then} {
	yylval->ival = 0;
	RETURN_TOKEN(TOK_THEN);
}

{and} {
	RETURN_TOKEN(TOK_AND);
}

{alt} {
	RETURN_TOKEN(TOK_ALT);
}

{sym}|{key} {
	yylval->sval = yytext;
	RETURN_TOKEN(TOK_SYM);
}

<QUO>[^\"]*\" {
	yylval->sval = yytext;
	yylval->sval[yyleng] = '\0';
	BEGIN(INITIAL);
	RETURN_TOKEN(TOK_SYM);
}

 /* more ignored shit, whitespace et al */
<*>{ws} {
	/* ignore */
}

<*>. {
	/* ignore? */
	fprintf(stderr, "%02x ", *yytext);
}

%%

int
__parse(gand_msg_t msg, const char *s, size_t l)
{
	yyscan_t scanner;
	YY_BUFFER_STATE buf;
	int res;

	yylex_init(&scanner);
	buf = yy_scan_string(s, scanner);
	/* parse him */
	res = yyparse(scanner, msg);
	yy_delete_buffer(buf, scanner);
        yylex_destroy(scanner);
	return (res == 0) - 1;
}

/* gand_msg-scanner.l ends here */
