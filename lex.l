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

#include <sys/stat.h>
#include <string.h>
#include <regex.h>
#include <errno.h>
#include "compat.h"
#include "main.h"
#include "y.tab.h"
#include "lex.h"
#include "pars.h"
#include "misc.h"

#ifdef LEX_HAS_BUFS
struct incl_file {
    const char *name;
	unsigned long line;
	YY_BUFFER_STATE buf;
	struct incl_file *next;
};

static struct incl_file *incstack;
#endif

static void include(const char *);
static void qstr_append(const char *, size_t);

const char *cur_rc_filenam;
dev_t cur_rc_dev;
ino_t cur_rc_ino;
unsigned rc_nline = 1;
unsigned rc_col = 1;
static size_t qstrsiz;
static size_t qstrlen;
static char *qstr;
static short incl;
%}

%x QSTR

S [ \t]
NS [^ \t\"\n]

%%
^#.*		|
{S}#.*		{ ; }
difftool	{ rc_col += yyleng; return DIFFTOOL     ; }
viewtool	{ rc_col += yyleng; return VIEWTOOL     ; }
ext		{ rc_col += yyleng; return EXT          ; }
skipext		{ rc_col += yyleng; return SKIPEXT      ; }
bg		{ rc_col += yyleng; return BG           ; }
wait		{ rc_col += yyleng; return WAIT         ; }
fkey		{ rc_col += yyleng; return FKEY         ; }
fkey_set { rc_col += yyleng; return FKEY_SET; }
filesfirst	{ rc_col += yyleng; return FILES        ; }
mixed		{ rc_col += yyleng; return MIXED        ; }
followlinks	{ rc_col += yyleng; return FOLLOW       ; }
mono		{ rc_col += yyleng; return MONO         ; }
noequal		{ rc_col += yyleng; return NOEQUAL      ; }
real_diff	{ rc_col += yyleng; return REAL_DIFF    ; }
recursive	{ rc_col += yyleng; return RECURSIVE    ; }
left_color	{ rc_col += yyleng; return LEFT_COLOR   ; }
right_color	{ rc_col += yyleng; return RIGHT_COLOR  ; }
diff_color	{ rc_col += yyleng; return DIFF_COLOR   ; }
dir_color	{ rc_col += yyleng; return DIR_COLOR    ; }
unknown_color	{ rc_col += yyleng; return UNKNOWN_COLOR; }
link_color	{ rc_col += yyleng; return LINK_COLOR   ; }
normal_color	{ rc_col += yyleng; return NORMAL_COLOR ; }
cursor_color	{ rc_col += yyleng; return CURSOR_COLOR ; }
error_color	{ rc_col += yyleng; return ERROR_COLOR  ; }
mark_color	{ rc_col += yyleng; return MARK_COLOR   ; }
mmrk_color	{ rc_col += yyleng; return MMRK_COLOR   ; }
bg_color	{ rc_col += yyleng; return BG_COLOR     ; }
histsize	{ rc_col += yyleng; return HISTSIZE     ; }
noic		{ rc_col += yyleng; return NOIC         ; }
magic		{ rc_col += yyleng; return MAGIC        ; }
nows		{ rc_col += yyleng; return NOWS         ; }
scale		{ rc_col += yyleng; return SCALE        ; }
nobold		{ rc_col += yyleng; return NOBOLD       ; }
shell		{ rc_col += yyleng; return SHELL        ; }
sh		{ rc_col += yyleng; return SH           ; }
alias		{ rc_col += yyleng; return ALIAS        ; }
twocolumn	{ rc_col += yyleng; return TWOCOLUMN    ; }
readonly	{ rc_col += yyleng; return READONLY     ; }
disp_all    { rc_col += yyleng; return DISP_ALL     ; }
disp_perms	{ rc_col += yyleng; return DISP_PERM    ; }
disp_owner	{ rc_col += yyleng; return DISP_OWNER   ; }
disp_group	{ rc_col += yyleng; return DISP_GROUP   ; }
disp_hsize	{ rc_col += yyleng; return DISP_HSIZE   ; }
disp_mtime	{ rc_col += yyleng; return DISP_MTIME   ; }
nodisp_perms { rc_col += yyleng; return NO_DISP_PERM ; }
nodisp_owner { rc_col += yyleng; return NO_DISP_OWNER; }
nodisp_group { rc_col += yyleng; return NO_DISP_GROUP; }
nodisp_hsize { rc_col += yyleng; return NO_DISP_HSIZE; }
nodisp_mtime { rc_col += yyleng; return NO_DISP_MTIME; }
locale		{ rc_col += yyleng; return LOCALE       ; }
file_exec	{ rc_col += yyleng; return FILE_EXEC    ; }
uz_add		{ rc_col += yyleng; return UZ_ADD       ; }
uz_del		{ rc_col += yyleng; return UZ_DEL       ; }
dotdot		{ rc_col += yyleng; return DOTDOT       ; }
nodotdot	{ rc_col += yyleng; return NO_DOTDOT    ; }
sortic		{ rc_col += yyleng; return SORTIC       ; }
preserve_all { rc_col += yyleng; return PRESERVE_ALL; }
preserve_mtim { rc_col += yyleng; return PRESERVE_MTIM; }
nopreserve { rc_col += yyleng; return NO_PRESERVE; }
hidden { rc_col += yyleng; return HIDDEN; }
nohidden { rc_col += yyleng; return NO_HIDDEN; }
override { rc_col += yyleng; return OVERRIDE; }
vi_cursor_keys { rc_col += yyleng; return VI_CURSOR_KEYS; }
include		{ rc_col += yyleng; incl = 1            ; }
{S}+		{ rc_col += yyleng; }

[0-9]+ {
		rc_col += yyleng;
		yylval.integer = atoi(yytext);
		return INTEGER;
	}

\"	{
		rc_col += yyleng;
		BEGIN QSTR;
	}

<QSTR>\\\" {
		rc_col += yyleng;
		qstr_append(yytext + 1, 1);
	}

<QSTR>[^\"\\\n]+ |
<QSTR>\\. {
		rc_col += yyleng;
		qstr_append(yytext, yyleng);
	}

\n |
<QSTR>\\\n {
		rc_col = 1;
		rc_nline++;
	}

<QSTR>\n {
		yyerror("Unterminated string");
		exit(1);
	}

<QSTR>\" {
		rc_col += yyleng;
		BEGIN 0;

		if (incl) {
			include(qstr);
			free(qstr);
			qstr = NULL;
		} else {
			yylval.str = qstr;
			qstr = NULL;
			return STRING;
		}
	}

{NS}+ {
		rc_col += yyleng;

		if (incl) {
			include(yytext);
		} else {
			yylval.str = strdup(yytext);
			return STRING;
		}
	}
%%

int
yywrap(void)
{
#ifdef LEX_HAS_BUFS
	struct incl_file *inc;

	if (!incstack) {
#endif
		return 1;
#ifdef LEX_HAS_BUFS
	}

	if (fclose(yyin) == EOF) {
		printf("fclose \"%s\": %s\n", cur_rc_filenam, strerror(errno));
	}

    free(const_cast_ptr(cur_rc_filenam));
	cur_rc_filenam = incstack->name;
	rc_nline = incstack->line;
	yy_delete_buffer(YY_CURRENT_BUFFER);
	yy_switch_to_buffer(incstack->buf);
	inc = incstack;
	incstack = incstack->next;
	free(inc);
    cur_rc_dev = 0;
    cur_rc_ino = 0;
	return 0;
#endif
}

static void
include(const char *s)
{
#ifndef LEX_HAS_BUFS
# warning lex(1) on this system does not support buffer switching. \
	The include statement will not work.
	incl = 0;
	printf("lex(1) on this system does not support buffer switching.\n"
	    "The statement \"include %s\" will not work.\n", s);
#else
	char *p;

	incl = 0;

	if (*s == '/') {
		p = strdup(s);
	} else if (!(p = add_home_pth(s))) {
		return;
	}

# if defined(TRACE)
	fprintf(debug, "<>include(%s)\n", p);
# endif

    if (stat(p, &gstat[0]) != -1) {
        if (cur_rc_dev == gstat[0].st_dev &&
            cur_rc_ino == gstat[0].st_ino)
        {
            printf("include \"%s\": infinite loop\n", p);
            exit(1);
        }
        cur_rc_dev = gstat[0].st_dev;
        cur_rc_ino = gstat[0].st_ino;
    } else {
        printf(LOCFMT "include \"%s\": %s\n" LOCVAR, p, strerror(errno));
        exit(1);
    }
    if (!(yyin = fopen(p, "r"))){
        printf(LOCFMT "include \"%s\": %s\n" LOCVAR, p, strerror(errno));
        switch (errno) {
        case EMFILE:
        case ENFILE:
            printf(LOCFMT "Possible infinite loop (circular include file dependency)\n" LOCVAR);
            break;
        }
        exit(1);
	}

    struct incl_file *const inc = malloc(sizeof(struct incl_file));
	inc->name = cur_rc_filenam;
	cur_rc_filenam = p;
	inc->line = rc_nline;
	rc_nline = 1;
	inc->buf = YY_CURRENT_BUFFER;
	inc->next = incstack;
	incstack = inc;
	yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
#endif /* LEX_HAS_BUFS */
}

static void
qstr_append(const char *s, size_t l)
{
	if (!qstr) {
		qstrlen = 0;
		qstrsiz = 1024;
		qstr = malloc(qstrsiz);
	}

	if (qstrsiz <= qstrlen + l) {
		qstrsiz = (qstrlen + l) * 2;
		qstr = realloc(qstr, qstrsiz);
	}

	memcpy(qstr + qstrlen, s, l + 1);
	qstrlen += l;
}
