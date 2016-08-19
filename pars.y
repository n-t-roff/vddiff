%{
#include "compat.h"
#include "main.h"
#include "ui.h"
void yyerror(const char *);
int yylex(void);
void follow(int);
extern unsigned rc_nline, rc_col;
%}
%union {
	char *str;
}
%token DIFFTOOL FILES DIRS MIXED FOLLOW MONO
%token <str> STRING
%%
config:
	  option_list
	;
option_list:
	  option
	| option_list option
	;
option:
	  DIFFTOOL STRING { difftool = $2       ; }
	| FILES           { sorting = FILESFIRST; }
	| DIRS            { sorting = DIRSFIRST ; }
	| MIXED           { sorting = SORTMIXED ; }
	| FOLLOW          { follow(1)           ; }
	| MONO            { color = 0           ; }
	;
%%
void
yyerror(const char *s)
{
	printf("Parse error: %s on line %u column %u\n", s, rc_nline,
	    rc_col);
}
