%{
/*
Copyright (c) 2016-2018, Carsten Kunze <carsten.kunze@arcor.de>

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

#include <locale.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>
#include <stdarg.h>
#include <signal.h>
#include <stdlib.h>
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
#include "pars.h"
#include "fs.h"
#include "misc.h"

int yylex(void);
extern char *yytext;
%}
%union {
	char *str;
	int  integer;
}
%token DIFFTOOL FILES DIRS MIXED FOLLOW MONO NOEQUAL LEFT_COLOR RIGHT_COLOR
%token DIFF_COLOR DIR_COLOR UNKNOWN_COLOR LINK_COLOR REAL_DIFF RECURSIVE
%token VIEWTOOL EXT BG FKEY HISTSIZE SKIPEXT NOIC MAGIC NOWS SCALE
%token SHELL SH NORMAL_COLOR CURSOR_COLOR ERROR_COLOR MARK_COLOR BG_COLOR
%token ALIAS TWOCOLUMN READONLY DISP_PERM DISP_OWNER DISP_GROUP DISP_HSIZE
%token DISP_MTIME MMRK_COLOR LOCALE FILE_EXEC UZ_ADD UZ_DEL WAIT NOBOLD DOTDOT
%token SORTIC PRESERVE_ALL PRESERVE_MTIM DISP_ALL NO_DOTDOT HIDDEN NO_HIDDEN
%token NO_DISP_PERM NO_DISP_OWNER NO_DISP_GROUP NO_DISP_HSIZE NO_DISP_MTIME
%token NO_PRESERVE FKEY_SET OVERRIDE
%token VI_CURSOR_KEYS
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
	  DIFFTOOL STRING        { set_tool(&difftool, $2, 0)             ; }
	| DIFFTOOL BG STRING     { set_tool(&difftool, $3, TOOL_BG)       ; }
	| VIEWTOOL STRING        { set_tool(&viewtool, $2, 0)             ; }
	| VIEWTOOL BG STRING     { set_tool(&viewtool, $3, TOOL_BG)       ; }
	| EXT STRING STRING      { db_def_ext($2, $3, 0)                  ; }
	| EXT STRING BG STRING   { db_def_ext($2, $4, TOOL_BG)            ; }
	| EXT STRING WAIT STRING { db_def_ext($2, $4, TOOL_BG|TOOL_WAIT)  ; }
	| ALIAS STRING STRING          { add_alias($2, $3, 0)             ; }
	| ALIAS STRING BG STRING       { add_alias($2, $4, TOOL_BG)       ; }
	| ALIAS STRING WAIT STRING     { add_alias($2, $4, TOOL_BG|TOOL_WAIT); }
    | SKIPEXT STRING         { add_skip_ext($2); }
	| FKEY INTEGER STRING          { nofkeys = FALSE; set_fkey($2, $3, NULL); }
	| FKEY INTEGER STRING STRING   { nofkeys = FALSE; set_fkey($2, $3, $4); }
    | FKEY_SET INTEGER { set_fkey_set($2); }
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
	| MMRK_COLOR INTEGER INTEGER   { color_mmrk_fg   = $2             ;
	                                 color_mmrk_bg   = $3             ; }
	| HISTSIZE INTEGER             { histsize        = $2             ; }
	| NOIC                         { noic  = 1                        ; }
	| MAGIC                        { magic = 1                        ; }
	| NOWS                         { nows  = 1                        ; }
	| SCALE                        { scale = 1                        ; }
	| NOBOLD                       { nobold = 1                       ; }
    | SHELL STRING { if (ishell)
                         free(ishell);
                     ishell = $2; }
    | SH STRING    { if (nishell)
                         free(nishell);
                     nishell = $2; }
	| UZ_ADD STRING STRING         { uz_add($2, $3)                   ; }
    | UZ_DEL STRING                { uz_db_del($2);
                                     free($2); }
	| TWOCOLUMN                    { twocols = TRUE                   ; }
	| READONLY                     { readonly = TRUE; nofkeys = TRUE  ; }
    | DISP_ALL                     { add_mode  = TRUE;
                                     add_owner = TRUE;
                                     add_group = TRUE;
                                     add_hsize = TRUE;
                                     add_mtime = TRUE; }
    | DISP_PERM                    { add_mode  = TRUE                 ; }
	| DISP_OWNER                   { add_owner = TRUE                 ; }
	| DISP_GROUP                   { add_group = TRUE                 ; }
	| DISP_HSIZE                   { add_hsize = TRUE                 ; }
	| DISP_MTIME                   { add_mtime = TRUE                 ; }
    | NO_DISP_PERM                 { add_mode  = FALSE                ; }
    | NO_DISP_OWNER                { add_owner = FALSE                ; }
    | NO_DISP_GROUP                { add_group = FALSE                ; }
    | NO_DISP_HSIZE                { add_hsize = FALSE                ; }
    | NO_DISP_MTIME                { add_mtime = FALSE                ; }
    | FILE_EXEC                    { file_exec = TRUE                 ; }
	| DOTDOT                       { dotdot = TRUE                    ; }
    | NO_DOTDOT                    { dotdot = FALSE                   ; }
    | SORTIC                       { sortic = TRUE                    ; }
    | PRESERVE_ALL                 { preserve_all = TRUE; }
    | PRESERVE_MTIM                { preserve_mtim = TRUE; }
    | NO_PRESERVE { preserve_all = FALSE;
                    preserve_mtim = FALSE; }
    | HIDDEN { nohidden = FALSE; }
    | NO_HIDDEN { nohidden = TRUE; }
    | OVERRIDE { override_prev = TRUE; }
    | VI_CURSOR_KEYS { vi_cursor_keys = TRUE; }
    | LOCALE STRING {
			if (!setlocale(LC_ALL, $2)) {
				printf("locale LC_ALL=%s cannot be set\n",
				    $2);
			}

			free($2);
		}
	;
%%
void
yyerror(const char *s)
{
    printf("Parse error: %s in %s line %u before column %u\n",
        s, cur_rc_filenam, rc_nline, rc_col);
}
