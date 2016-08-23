%{
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
^{S}*noqueal		{ rc_col += yyleng; return NOEQUAL      ; }
^{S}*left_color		{ rc_col += yyleng; return LEFT_COLOR   ; }
^{S}*right_color	{ rc_col += yyleng; return RIGHT_COLOR  ; }
^{S}*diff_color		{ rc_col += yyleng; return DIFF_COLOR   ; }
^{S}*dir_color		{ rc_col += yyleng; return DIR_COLOR    ; }
^{S}*unknown_color	{ rc_col += yyleng; return UNKNOWN_COLOR; }
^{S}*link_color		{ rc_col += yyleng; return LINK_COLOR   ; }
\"[^\"]*\"		{ rc_col += yyleng;
			  yytext[yyleng-1] = 0;
			  yylval.str = yytext+1;
			  return STRING; }
{NS}+			{ rc_col += yyleng;
			  yylval.str = yytext;
			  return STRING; }
[0-9]+			{ yylval.integer = atoi(yytext);
			  return INTEGER; }
{S}+			{ rc_col += yyleng; }
\n			{ rc_nline++;
			  rc_col = 1; }
%%
int
yywrap(void)
{
	return 1;
}
