%{
#include "y.tab.h"
unsigned rc_nline = 1;
unsigned rc_col = 1;
%}
S [ \t]
NS [^ \t\n]
%%
#.*			{ ; }
^{S}*difftool		{ rc_col += yyleng; return DIFFTOOL; }
^{S}*filesfirst		{ rc_col += yyleng; return FILES; }
^{S}*dirsfirst		{ rc_col += yyleng; return DIRS; }
^{S}*mixed		{ rc_col += yyleng; return MIXED; }
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
