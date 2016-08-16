%{
#include "compat.h"
#include "main.h"
void yyerror(const char *);
int yylex(void);
extern unsigned rc_nline, rc_col;
%}
%union {
	char *str;
}
%token DIFFTOOL FILES DIRS MIXED
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
	  difftool
	| sorting
	;
difftool:
	  DIFFTOOL STRING { difftool = $2; }
	;
sorting:
	  FILES { sorting = FILESFIRST; }
	| DIRS  { sorting = DIRSFIRST;  }
	| MIXED { sorting = SORTMIXED;  }
	;
%%
void
parse_rc(void)
{
}
void
yyerror(const char *s)
{
	printf("Parse error: %s on line %u column %u\n", s, rc_nline,
	    rc_col);
}
