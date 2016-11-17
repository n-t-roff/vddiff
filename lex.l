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
#include <string.h>
#include "y.tab.h"
#include "lex.h"
unsigned rc_nline = 1;
unsigned rc_col = 1;
%}
S [ \t]
NS [^ \t\n]
%%
#.*			{ ; }
difftool		{ rc_col += yyleng; return DIFFTOOL     ; }
viewtool		{ rc_col += yyleng; return VIEWTOOL     ; }
ext			{ rc_col += yyleng; return EXT          ; }
skipext			{ rc_col += yyleng; return SKIPEXT      ; }
bg			{ rc_col += yyleng; return BG           ; }
fkey			{ rc_col += yyleng; return FKEY         ; }
filesfirst		{ rc_col += yyleng; return FILES        ; }
mixed			{ rc_col += yyleng; return MIXED        ; }
followlinks		{ rc_col += yyleng; return FOLLOW       ; }
mono			{ rc_col += yyleng; return MONO         ; }
noequal			{ rc_col += yyleng; return NOEQUAL      ; }
real_diff		{ rc_col += yyleng; return REAL_DIFF    ; }
recursive		{ rc_col += yyleng; return RECURSIVE    ; }
left_color		{ rc_col += yyleng; return LEFT_COLOR   ; }
right_color		{ rc_col += yyleng; return RIGHT_COLOR  ; }
diff_color		{ rc_col += yyleng; return DIFF_COLOR   ; }
dir_color		{ rc_col += yyleng; return DIR_COLOR    ; }
unknown_color		{ rc_col += yyleng; return UNKNOWN_COLOR; }
link_color		{ rc_col += yyleng; return LINK_COLOR   ; }
normal_color		{ rc_col += yyleng; return NORMAL_COLOR ; }
cursor_color		{ rc_col += yyleng; return CURSOR_COLOR ; }
error_color		{ rc_col += yyleng; return ERROR_COLOR  ; }
mark_color		{ rc_col += yyleng; return MARK_COLOR   ; }
bg_color		{ rc_col += yyleng; return BG_COLOR     ; }
histsize		{ rc_col += yyleng; return HISTSIZE     ; }
noic			{ rc_col += yyleng; return NOIC         ; }
magic			{ rc_col += yyleng; return MAGIC        ; }
nows			{ rc_col += yyleng; return NOWS         ; }
scale                   { rc_col += yyleng; return SCALE        ; }
bmode			{ rc_col += yyleng; return BMODE        ; }
shell			{ rc_col += yyleng; return SHELL        ; }
sh			{ rc_col += yyleng; return SH           ; }
alias			{ rc_col += yyleng; return ALIAS        ; }
twocolumn		{ rc_col += yyleng; return TWOCOLUMN    ; }
[0-9]+			{ rc_col += yyleng;
			  yylval.integer = atoi(yytext);
			  return INTEGER; }
\"[^\"]*\"		{ rc_col += yyleng;
			  yytext[yyleng-1] = 0;
			  yylval.str = strdup(yytext+1);
			  return STRING; }
{NS}+			{ rc_col += yyleng;
			  yylval.str = strdup(yytext);
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
