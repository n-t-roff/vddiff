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

#include <sys/types.h>
#include <regex.h>
#include <stdarg.h>
#include <signal.h>
#include "compat.h"
#include "main.h"
#include "exec.h"
#include "ui.h"
#include "ui2.h"
#include "uzp.h"
#include "db.h"
#include "ed.h"
#include "lex.h"
#include "diff.h"
#include "tc.h"

void yyerror(const char *);
int yylex(void);
extern char *yytext;
%}
%union {
	char *str;
	int  integer;
}
%token DIFFTOOL FILES DIRS MIXED FOLLOW MONO NOEQUAL LEFT_COLOR RIGHT_COLOR
%token DIFF_COLOR DIR_COLOR UNKNOWN_COLOR LINK_COLOR REAL_DIFF RECURSIVE
%token VIEWTOOL EXT BG FKEY BMODE HISTSIZE SKIPEXT NOIC MAGIC NOWS SCALE
%token SHELL SH NORMAL_COLOR CURSOR_COLOR ERROR_COLOR MARK_COLOR BG_COLOR
%token ALIAS TWOCOLUMN READONLY DISP_PERM DISP_OWNER DISP_GROUP DISP_HSIZE
%token DISP_MTIME
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
	| DIFFTOOL BG STRING    { set_tool(&difftool, $3, TOOL_BG)        ; }
	| VIEWTOOL STRING       { set_tool(&viewtool, $2, 0)              ; }
	| VIEWTOOL BG STRING    { set_tool(&viewtool, $3, TOOL_BG)        ; }
	| EXT STRING STRING     { db_def_ext($2, $3, 0)                   ; }
	| EXT STRING BG STRING  { db_def_ext($2, $4, TOOL_BG)             ; }
	| SKIPEXT STRING        { str_db_add(&skipext_db, str_tolower($2)
#ifdef HAVE_LIBAVLBST
	                              , 0, NULL
#endif
	                              ); }
	| FKEY INTEGER STRING          { nofkeys = FALSE; set_fkey($2, $3); }
	| FILES                        { sorting = FILESFIRST             ; }
	| MIXED                        { sorting = SORTMIXED              ; }
	| FOLLOW                       { followlinks = 1;                 ; }
	| MONO                         { color = 0                        ; }
	| NOEQUAL                      { noequal = 1                      ; }
	| REAL_DIFF                    { real_diff = 1                    ; }
	| RECURSIVE                    { recursive = 1                    ; }
	| LEFT_COLOR INTEGER           { color_leftonly  = $2             ; }
	| RIGHT_COLOR INTEGER          { color_rightonly = $2             ; }
	| DIFF_COLOR INTEGER           { color_diff      = $2             ; }
	| DIR_COLOR INTEGER            { color_dir       = $2             ; }
	| UNKNOWN_COLOR INTEGER        { color_unknown   = $2             ; }
	| LINK_COLOR INTEGER           { color_link      = $2             ; }
	| NORMAL_COLOR INTEGER         { color_normal    = $2             ; }
	| BG_COLOR INTEGER             { color_bg        = $2             ; }
	| CURSOR_COLOR INTEGER INTEGER { color_cursor_fg = $2             ;
	                                 color_cursor_bg = $3             ; }
	| ERROR_COLOR INTEGER INTEGER  { color_error_fg  = $2             ;
	                                 color_error_bg  = $3             ; }
	| MARK_COLOR INTEGER INTEGER   { color_mark_fg   = $2             ;
	                                 color_mark_bg   = $3             ; }
	| HISTSIZE INTEGER             { histsize        = $2             ; }
	| NOIC                         { noic  = 1                        ; }
	| MAGIC                        { magic = 1                        ; }
	| NOWS                         { nows  = 1                        ; }
	| SCALE                        { scale = 1                        ; }
	| SHELL STRING                 { ishell = $2                      ; }
	| SH STRING                    { nishell = $2                     ; }
	| ALIAS STRING STRING          { add_alias($2, $3)                ; }
	| TWOCOLUMN                    { twocols = TRUE                   ; }
	| READONLY                     { readonly = TRUE; nofkeys = TRUE  ; }
	| DISP_PERM                    { add_mode = TRUE;                 ; }
	| DISP_OWNER                   { add_owner = TRUE;                ; }
	| DISP_GROUP                   { add_group = TRUE;                ; }
	| DISP_HSIZE                   { add_hsize = TRUE;                ; }
	| DISP_MTIME                   { add_mtime = TRUE;                ; }
	;
%%
void
yyerror(const char *s)
{
	printf("Parse error: %s on line %u before column %u\n",
	    s, rc_nline, rc_col);
}
