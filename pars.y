%{
#include "compat.h"
#include "exec.h"
#include "ui.h"
#include "db.h"
void yyerror(const char *);
int yylex(void);
void follow(int);
extern unsigned rc_nline, rc_col;
extern char *yytext;
%}
%union {
	char *str;
	int  integer;
}
%token DIFFTOOL FILES DIRS MIXED FOLLOW MONO NOEQUAL LEFT_COLOR RIGHT_COLOR
%token DIFF_COLOR DIR_COLOR UNKNOWN_COLOR LINK_COLOR
%token <str>     STRING
%token <integer> INTEGER
%%
config:
	  option_list
	;
option_list:
	| option_list option
	;
option:
	  DIFFTOOL STRING       { set_difftool($2)    ; }
	| FILES                 { sorting = FILESFIRST; }
	| MIXED                 { sorting = SORTMIXED ; }
	| FOLLOW                { follow(1)           ; }
	| MONO                  { color = 0           ; }
	| NOEQUAL               { noequal = 1         ; }
	| LEFT_COLOR INTEGER    { color_leftonly  = $2; }
	| RIGHT_COLOR INTEGER   { color_rightonly = $2; }
	| DIFF_COLOR INTEGER    { color_diff      = $2; }
	| DIR_COLOR INTEGER     { color_dir       = $2; }
	| UNKNOWN_COLOR INTEGER { color_unknown   = $2; }
	| LINK_COLOR INTEGER    { color_link      = $2; }
	;
%%
void
yyerror(const char *s)
{
	printf("Parse error: %s on line %u column %u\n",
	    s, rc_nline, rc_col);
}
