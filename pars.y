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
#include "compat.h"
#include "exec.h"
#include "ui.h"
#include "db.h"
#include "main.h"
#include "ed.h"
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
%token DIFF_COLOR DIR_COLOR UNKNOWN_COLOR LINK_COLOR REAL_DIFF RECURSIVE
%token VIEWTOOL EXT BG FKEY BMODE HISTSIZE SKIPEXT
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
	  DIFFTOOL STRING       { set_tool(&difftool, $2, 0)              ; }
	| DIFFTOOL BG STRING    { set_tool(&difftool, $3, 1)              ; }
	| VIEWTOOL STRING       { set_tool(&viewtool, $2, 0)              ; }
	| VIEWTOOL BG STRING    { set_tool(&viewtool, $3, 1)              ; }
	| EXT STRING STRING     { db_def_ext($2, $3, 0)                   ; }
	| EXT STRING BG STRING  { db_def_ext($2, $4, 1)                   ; }
	| SKIPEXT STRING        { str_db_add(&skipext_db, str_tolower($2)); }
	| FKEY INTEGER STRING   { set_fkey($2, $3)                        ; }
	| FILES                 { sorting = FILESFIRST                    ; }
	| MIXED                 { sorting = SORTMIXED                     ; }
	| FOLLOW                { follow(1)                               ; }
	| MONO                  { color = 0                               ; }
	| NOEQUAL               { noequal = 1                             ; }
	| REAL_DIFF             { real_diff = 1                           ; }
	| RECURSIVE             { recursive = 1                           ; }
	| LEFT_COLOR INTEGER    { color_leftonly  = $2                    ; }
	| RIGHT_COLOR INTEGER   { color_rightonly = $2                    ; }
	| DIFF_COLOR INTEGER    { color_diff      = $2                    ; }
	| DIR_COLOR INTEGER     { color_dir       = $2                    ; }
	| UNKNOWN_COLOR INTEGER { color_unknown   = $2                    ; }
	| LINK_COLOR INTEGER    { color_link      = $2                    ; }
	| HISTSIZE INTEGER      { histsize        = $2                    ; }
	| BMODE                 { bmode = 1                               ; }
	;
%%
void
yyerror(const char *s)
{
	printf("Parse error: %s on line %u before column %u\n",
	    s, rc_nline, rc_col);
}
