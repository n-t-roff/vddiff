%{
/*
Copyright (c) 2016, Carsten Kunze <carsten.kunze@arcor.de>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
*/
#include "y.tab.h"
unsigned rc_nline = 1;
unsigned rc_col = 1;
%}
S [ \t]
NS [^ \t\n]
%%
#.*			{ ; }
^{S}*difftool		{ rc_col += yyleng; return DIFFTOOL     ; }
^{S}*filesfirst		{ rc_col += yyleng; return FILES        ; }
^{S}*mixed		{ rc_col += yyleng; return MIXED        ; }
^{S}*followlinks	{ rc_col += yyleng; return FOLLOW       ; }
^{S}*mono		{ rc_col += yyleng; return MONO         ; }
^{S}*noequal		{ rc_col += yyleng; return NOEQUAL      ; }
^{S}*real_diff		{ rc_col += yyleng; return REAL_DIFF    ; }
^{S}*recursive		{ rc_col += yyleng; return RECURSIVE    ; }
^{S}*left_color		{ rc_col += yyleng; return LEFT_COLOR   ; }
^{S}*right_color	{ rc_col += yyleng; return RIGHT_COLOR  ; }
^{S}*diff_color		{ rc_col += yyleng; return DIFF_COLOR   ; }
^{S}*dir_color		{ rc_col += yyleng; return DIR_COLOR    ; }
^{S}*unknown_color	{ rc_col += yyleng; return UNKNOWN_COLOR; }
^{S}*link_color		{ rc_col += yyleng; return LINK_COLOR   ; }
[0-9]+			{ yylval.integer = atoi(yytext);
			  return INTEGER; }
\"[^\"]*\"		{ rc_col += yyleng;
			  yytext[yyleng-1] = 0;
			  yylval.str = yytext+1;
			  return STRING; }
{NS}+			{ rc_col += yyleng;
			  yylval.str = yytext;
			  return STRING; }
{S}+			{ rc_col += yyleng; }
\n			{ rc_nline++;
			  rc_col = 1; }
%%
int
yywrap(void)
{
	return 1;
}
