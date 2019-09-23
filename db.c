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

#include <errno.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <search.h>
#include <stdarg.h>
#include <signal.h>
#ifdef USE_SYS_SYSMACROS_H
# include <sys/sysmacros.h>
#endif
#ifdef USE_SYS_MKDEV_H
# include <sys/mkdev.h>
#endif
#include "compat.h"
#include "diff.h"
#include "main.h"
#include "ui.h"
#include "ui2.h"
#include "db.h"
#include "gq.h"
#include "tc.h"
#include "dl.h"
#include "misc.h"

static void db_dl_free(char **);
#ifdef HAVE_LIBAVLBST
static int diff_cmp(union bst_val, union bst_val);
static int ddl_cmp(union bst_val, union bst_val);
static int bdl_cmp(union bst_val, union bst_val);
static void mk_list(struct bst_node *);
static void diff_db_delete(struct bst_node *);
static void del_strs(struct bst_node *);
static void mk_ddl(struct bst_node *);
static void mk_bdl(struct bst_node *);
static void mk_str_list(struct bst_node *);
#else
struct curs_pos {
	char *path;
	unsigned uv[2];
};

static int name_cmp(const void *, const void *);
static int diff_cmp(const void *, const void *);
static int curs_cmp(const void *, const void *);
static int ext_cmp(const void *, const void *);
static int uz_cmp(const void *, const void *);
static int ptr_db_cmp(const void *, const void *);
static int ddl_cmp(const void *, const void *);
static int bdl_cmp(const void *, const void *);
static void mk_list(const void *, const VISIT, const int);
static void mk_ddl(const void *, const VISIT, const int);
static void mk_bdl(const void *, const VISIT, const int);
static void mk_str_list(const void *, const VISIT, const int);
#endif

enum sorting sorting;
unsigned db_num[2];
struct filediff **db_list[2];
static struct filediff **cur_list;
static size_t tusrlen, tgrplen;
size_t usrlen[2], grplen[2];
static off_t maxsiz;
static unsigned long maxmajor, maxminor;
unsigned short bsizlen[2];
unsigned short majorlen[2], minorlen[2];
short noequal, real_diff;
void *scan_db;
void *name_db;
void *skipext_db;
void *uz_path_db;
static void *alias_db;
bool sortic;
bool nohidden; /* in db.c because it must be possible to toggle with ',' */

static void *curs_db[2];
static void *ext_db;
static void *uz_ext_db;
static unsigned db_idx, tot_db_num[2];
static char **str_list;
static struct scan_db *scan_db_list;

#ifdef HAVE_LIBAVLBST
static struct bst diff_db[2] = { { NULL, diff_cmp },
                                 { NULL, diff_cmp } };
static struct bst ddl_db = { NULL, ddl_cmp };
static struct bst bdl_db = { NULL, bdl_cmp };
#else
static void *diff_db[2];
static void *ddl_db;
static void *bdl_db;
#endif

#ifdef HAVE_LIBAVLBST
void
db_init(void)
{
	scan_db    = db_new(name_cmp);
	name_db    = db_new(name_cmp);
	curs_db[0] = db_new(name_cmp);
	curs_db[1] = db_new(name_cmp);
	ext_db     = db_new(name_cmp);
	uz_ext_db  = db_new(name_cmp);
	skipext_db = db_new(name_cmp);
	uz_path_db = db_new(name_cmp);
	alias_db   = db_new(name_cmp);
}

void *
db_new(int (*compare)(union bst_val, union bst_val))
{
	struct bst *bst;

	bst = malloc(sizeof(struct bst));
	bst->root = NULL;
	bst->cmp = compare;
	return (void *)bst;
}
#endif

/**********
 * ptr DB *
 **********/

/* 0: Node found */

int
ptr_db_add(void **db, char *key, void *dat)
{
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
	int br;
    union bst_val k, v;
    k.p = key;
    v.p = dat;
    if (!(br = bst_srch(*db, k, &n)))
		return 0; /* was already in DB */
    avl_add_at(*db, k, v, br, n);
	return 1; /* was not in DB, now added */
#else
	struct ptr_db_ent *pe;
	void *vp;

	pe = malloc(sizeof(struct ptr_db_ent));
	pe->key = key;
	pe->dat = dat;
	vp = tsearch(pe, db, ptr_db_cmp);

	if (*(struct ptr_db_ent **)vp != pe) {
		free(pe);
		return 0; /* free mem */
	} else
		return 1; /* don't free mem */
#endif
}

/* 0: Node found */

int
ptr_db_srch(void **db, char *key, void **dat, void **n)
{
#ifdef HAVE_LIBAVLBST
	int i;
	struct bst_node *n1;

	if (!n)
		n = (void **)&n1;
    union bst_val k;
    k.p = key;
    if (!(i = bst_srch(*db, k, (struct bst_node **)n)) && dat)
		*dat = (*(struct bst_node **)n)->data.p;

	return i;
#else
	struct ptr_db_ent pe;
	void *vp;

	pe.key = key;

	vp = tfind(&pe, db, ptr_db_cmp);

	if (vp) {
		if (dat)
			*dat = (*(struct ptr_db_ent **)vp)->dat;

		if (n)
			*n = *(struct ptr_db_ent **)vp;

		return 0;
	} else
		return 1;
#endif
}

void
ptr_db_del(void **db, void *node)
{
#ifdef HAVE_LIBAVLBST
	avl_del_node(*db, node);
#else
	tdelete(node, db, ptr_db_cmp);
	free(node);
#endif
}

void *
ptr_db_get_node(void *db)
{
#ifdef HAVE_LIBAVLBST
	return ((struct bst *)db)->root;
#else
	return db ? *(struct ptr_db_ent **)db : NULL;
#endif
}

#ifndef HAVE_LIBAVLBST
static int
ptr_db_cmp(const void *a, const void *b)
{
	return strcmp(
	    ((const struct ptr_db_ent *)a)->key,
	    ((const struct ptr_db_ent *)b)->key);
}
#endif

/********************
 * simple char * DB *
 ********************/

#ifdef HAVE_LIBAVLBST
int
str_db_add(void **db, char *s, int br, struct bst_node *n)
{
    union bst_val k, v;
    k.p = s;
    v.i = 0;
    return avl_add_at(*db, k, v, br, n);
}
#else
char *
str_db_add(void **db, char *s)
{
    void *const vp = tsearch(s, db, name_cmp);
    if (!vp)
        return NULL;
	return *(char **)vp;
}
#endif

#ifdef HAVE_LIBAVLBST
int
str_db_srch(void **db, const char *const s, struct bst_node **n)
{
    union bst_val k;
    k.cp = s;
    return bst_srch(*db, k, n);
}
#else
int
str_db_srch(void **db, const char *const s, char **n)
{
	void *vp;

	vp = tfind(s, db, name_cmp);

	if (n) {
		*n = vp ? *(char **)vp : NULL;
	}

	return vp ? 0 : 1;
}
#endif

void
str_db_del(void **db, void *node)
{
#ifdef HAVE_LIBAVLBST
	free(((struct bst_node *)node)->key.p);
	avl_del_node(*db, node);
#else
	tdelete(node, db, name_cmp);
	free(node);
#endif
}

void *
str_db_get_node(void *db)
{
#ifdef HAVE_LIBAVLBST
	return ((struct bst *)db)->root;
#else
	return db ? *(char **)db : NULL;
#endif
}

char **
str_db_sort(void *db, unsigned long n)
{
	char **r = NULL;

#if defined(TRACE)
	fprintf(debug, "->str_db_sort(n=%lu)\n", n);
#endif
	if (!n) {
        goto ret;
	}

	str_list = malloc(sizeof(char *) * n);
	db_idx = 0; /* shared with diff_db */
#ifdef HAVE_LIBAVLBST
	mk_str_list(((struct bst *)db)->root);
#else
	twalk(db, mk_str_list);
#endif
	r = str_list;

ret:
#if defined(TRACE)
	fprintf(debug, "<-str_db_sort\n");
#endif
	return r;
}

#ifdef HAVE_LIBAVLBST
static void
mk_str_list(struct bst_node *n)
{
	if (!n) {
		return;
	}

	mk_str_list(n->left);
#if defined(TRACE)
    fprintf(debug, "<>mk_str_list set [%u]=%s\n",
            db_idx, (char *)n->key.p);
#endif
	str_list[db_idx++] = n->key.p;
	mk_str_list(n->right);
}
#else
static void
mk_str_list(const void *n, const VISIT which, const int depth)
{
	(void)depth;

	switch (which) {
	case postorder:
	case leaf:
#if defined(TRACE)
		fprintf(debug, "<>mk_str_list set [%u]\n", db_idx);
#endif
		str_list[db_idx++] = *(char * const *)n;
		break;
	default:
		;
	}
}
#endif

void
free_strs(void **db)
{
#ifdef HAVE_LIBAVLBST
	del_strs(((struct bst *)*db)->root);
	((struct bst *)*db)->root = NULL;
#else
	char *s;

	while (*db) {
		s = *(char **)*db;
		tdelete(s, db, name_cmp);
		free(s);
	}
#endif
}

#ifdef HAVE_LIBAVLBST
static void
del_strs(struct bst_node *n)
{
	if (!n)
		return;

	del_strs(n->left);
	del_strs(n->right);
	free(n->key.p);
	free(n);
}
#endif

#ifdef HAVE_LIBAVLBST
int name_cmp(union bst_val a, union bst_val b)
#else
static int name_cmp(const void *a, const void *b)
#endif
{
#ifdef HAVE_LIBAVLBST
	char *s1 = a.p,
	     *s2 = b.p;
#else
	const char *s1 = a,
	           *s2 = b;
#endif

	return strcmp(s1, s2);
}

/***********
 * scan DB *
 ***********/

void
push_scan_db(bool os)
{
	struct scan_db *p;

#if defined(TRACE)
	fprintf(debug, "<>push_scan_db(%d)\n", os ? 1 : 0);
#endif
	p = malloc(sizeof(struct scan_db));
	p->db = scan_db;
	p->next = scan_db_list;
	scan_db_list = p;
	scan_db = NULL;

	if (os) {
		one_scan = TRUE;
#ifdef HAVE_LIBAVLBST
		scan_db = db_new(name_cmp);
#endif
	}
}

void
pop_scan_db(void)
{
	struct scan_db *p;

#if defined(TRACE)
	fprintf(debug, "<>pop_scan_db()\n");
#endif
	free_scan_db(FALSE);
#ifdef HAVE_LIBAVLBST
	free(scan_db);
#endif

	if (!(p = scan_db_list)) {
		return;
	}

	scan_db = p->db;
	scan_db_list = p->next;
	free(p);
}

void
free_scan_db(bool os)
{
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
#else
	char *n;
#endif

#if defined(TRACE)
	fprintf(debug, "<>free_scan_db(%d)\n", os ? 1 : 0);
#endif

	while ((n = str_db_get_node(scan_db))) {
		str_db_del(&scan_db, n);
	}

	one_scan = os;
}

/****************
 * unzip ext DB *
 ****************/

void
uz_db_add(char *ext, enum uz_id id)
{
	struct uz_ext *p;

#if defined(TRACE)
	fprintf(debug, "<>uz_db_add(%s, %d)\n", ext, id);
#endif
	p = malloc(sizeof(struct uz_ext));
	p->str = ext;
	p->id = id;
#ifdef HAVE_LIBAVLBST
    union bst_val k, v;
    k.cp = p->str;
    v.p = p;
    avl_add(uz_ext_db, k, v);
#else
	tsearch(p, &uz_ext_db, uz_cmp);
#endif
}

enum uz_id
uz_db_srch(char *str)
{
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
#else
	struct uz_ext key;
	void *vp;
#endif

#if defined(TRACE)
	fprintf(debug, "<>uz_db_srch(%s)\n", str);
#endif

#ifdef HAVE_LIBAVLBST
    union bst_val k;
    k.p = str;
    if (!bst_srch(uz_ext_db, k, &n))
		return ((struct uz_ext *)n->data.p)->id;
#else
	key.str = str;

	if ((vp = tfind(&key, &uz_ext_db, uz_cmp)))
		return (*(struct uz_ext **)vp)->id;
#endif
	else
		return UZ_NONE;
}

void
uz_db_del(char *ext)
{
	struct uz_ext *ue;
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
#else
	struct uz_ext key;
	void *vp;
#endif

#if defined(TRACE)
	fprintf(debug, "->uz_db_del(%s)\n", ext);
#endif

#ifdef HAVE_LIBAVLBST
    union bst_val k;
    k.p = ext;
    if (bst_srch(uz_ext_db, k, &n)) {
		goto ret;
	}

	ue = n->data.p;
	avl_del_node(uz_ext_db, n);
#else
	key.str = ext;
#if defined(TRACE)
	fprintf(debug, "  tfind...\n");
#endif

	if (!(vp = tfind(&key, &uz_ext_db, uz_cmp))) {
		goto ret;
	}

#if defined(TRACE)
	fprintf(debug, "  tdelete...\n");
#endif
	ue = *(struct uz_ext **)vp;
	tdelete(ue, &uz_ext_db, uz_cmp);
#endif
    free(const_cast_ptr(ue->str));
	free(ue);

ret:
#if defined(TRACE)
	fprintf(debug, "<-uz_db_del\n");
#endif
    return;
}

#ifndef HAVE_LIBAVLBST
static int
uz_cmp(const void *a, const void *b)
{
	const char *as = ((const struct uz_ext *)a)->str;
	const char *bs = ((const struct uz_ext *)b)->str;

#if defined(TRACE)
	fprintf(debug, "<>uz_cmp(%p(%s),%p(%s))\n",a,as,b,bs);
#endif
	return strcmp(as, bs);
}
#endif

/************
 * Alias DB *
 ************/

void
add_alias(char *const key, char *value, const tool_flags_t flags)
{
#if defined(TRACE)
    fprintf(debug, "->add_alias(%s->%s)\n", key, value);
#endif
    /* Check if alias exists. */
    struct tool *ot;
    if (!ptr_db_srch(&alias_db, key, (void **)&ot, NULL)) {
#if defined(TRACE)
        fprintf(debug, "  alias \"%s\"->\"%s\" exists with value \"%s\"\n",
                key, value, ot->tool);
#endif
        if (!strcmp(value, ot->tool)) {
#if defined(TRACE)
            fprintf(debug, "  same value -> ignore command\n");
#endif
            free(key);
            free(value);
            return;
        }
        if (!override_prev) {
            printf("Error: Alias \"%s\" -> \"%s\" "
                   "already exists with value \"%s\"\n",
                   key, value, ot->tool);
            exit(EXIT_STATUS_ERROR);
        } else {
#if defined(TRACE)
            fprintf(debug, "  value \"%s\" changed to \"%s\"\n",
                    ot->tool, value);
#endif
            free(ot->tool);
            ot->tool = value;
            free(key);
            return;
        }
    }
    /* Check if alias points to alias.
     * `value` is used as key here. If a value should be set which is the
     * key of another alias, the value of that alias is used instead.
     * Example:
     *   alias video mplayer # alias to tool
     *   alias audio video   # alias to alias */
    if (!ptr_db_srch(&alias_db, value, (void **)&ot, NULL)) {
#if defined(TRACE)
        fprintf(debug, "  alias \"%s\" to alias \"%s\" with value \"%s\"\n",
                key, value, ot->tool);
#endif
        free(value);
		/* `s` is the value of the other alias. Hence the strdup
		 * is necessary, since now a *second* alias will be created
		 * with that value. */
        value = strdup(ot->tool);
	}

    struct tool *const nt = malloc(sizeof(struct tool));
    nt->tool = value;
    nt->flags = flags;
    ptr_db_add(&alias_db, key, nt);
#if defined(TRACE)
    fprintf(debug, "<-add_alias: \"%s\"->\"%s\" set\n", key, nt->tool);
#endif
}

/**********
 * ext DB *
 **********/

void db_def_ext(char *const ext, char *_tool, tool_flags_t flags)
{
	str_tolower(ext);
#ifdef HAVE_LIBAVLBST
    struct bst_node *n;
    union bst_val k, v;
    k.p = ext;
    if (!bst_srch(ext_db, k, &n)) {
        struct tool *const ot = n->data.p;
#else
    struct tool key;
    key.ext = ext;
    void *vp;

    if ((vp = tfind(&key, &ext_db, ext_cmp))) {
        struct tool *const ot = *(struct tool **)vp;
#endif
        if (!override_prev) {
            printf("Error: Tool for extension \"%s\" set twice\n", ext);
            exit(EXIT_STATUS_ERROR);
        } else {
            free(ext);
            struct tool *const t = set_ext_tool(_tool, flags);
            if (!t)
                return;
#ifdef HAVE_LIBAVLBST
            n->data.p = (void *)t;
#else
            t->ext = ot->ext;
            *(struct tool **)vp = t;
#endif
            struct strlst *args = ot->args;
            while (args) {
                struct strlst *const p = args;
                args = args->next;
                free(p);
            }
            free(ot->tool);
            free(ot);
        }
    } else {
        struct tool *const t = set_ext_tool(_tool, flags);
        if (!t)
            return;
#ifdef HAVE_LIBAVLBST
        k.p = ext;
        v.p = t;
        avl_add(ext_db, k, v);
#else
		t->ext = ext;
		tsearch(t, &ext_db, ext_cmp);
#endif
	}
}

struct tool *set_ext_tool(char *_tool, tool_flags_t flags)
{
    struct tool *const t = malloc(sizeof(*t));
    if (!t)
        goto ret;
    t->tool = NULL; /* set_tool makes a free() */
    t->args = NULL;

    struct tool *at; /* "alias tool" */
    if (!ptr_db_srch(&alias_db, _tool, (void **)&at, NULL)) {
        free(_tool);
        _tool = strdup(at->tool);
        if (!_tool) {
            free(t);
            return NULL;
        }
        flags |= at->flags;
    }

    set_tool(t, _tool, flags);
ret:
    return t;
}

struct tool *
db_srch_ext(char *ext)
{
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
    union bst_val k;
    k.p = ext;
    if (!bst_srch(ext_db, k, &n))
		return n->data.p;
#else
	struct tool key;
	void *vp;

	key.ext = ext;

	if ((vp = tfind(&key, &ext_db, ext_cmp)))
		return *(struct tool **)vp;
#endif
	else
		return NULL;
}

#ifndef HAVE_LIBAVLBST
static int
ext_cmp(const void *a, const void *b)
{
	return strcmp(
	    ((const struct tool *)a)->ext,
	    ((const struct tool *)b)->ext);
}
#endif

/***********
 * curs DB *
 ***********/

void
db_set_curs(int col, char *path, unsigned _top_idx, unsigned _curs)
{
	unsigned *uv;

#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
	int br;
    union bst_val k, v;
    k.p = path;
    if (!(br = bst_srch(curs_db[col], k, &n))) {
		uv = n->data.p;
	} else {
		uv = malloc(2 * sizeof(unsigned));
        k.p = strdup(path);
        v.p = uv;
        avl_add_at(curs_db[col], k, v, br, n);
	}
#else
	struct curs_pos *cp, *cp2;
	void *vp;

	cp = malloc(sizeof(struct curs_pos));
	cp->path = strdup(path);
	vp = tsearch(cp, &curs_db[col], curs_cmp);
	cp2 = *(struct curs_pos **)vp;

	if (cp2 != cp) {
		free(cp->path);
		free(cp);
	}

	if (!cp2)
		return;

	uv = (unsigned *)&cp2->uv;
#endif

	*uv++ = _top_idx;
	*uv   = _curs;
}

unsigned *
db_get_curs(int col, char *path)
{
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
    union bst_val k;
    k.p = path;
    if (!bst_srch(curs_db[col], k, &n))
		return n->data.p;
#else
	struct curs_pos *cp, key;
	void *vp;

	key.path = path;

	if ((vp = tfind(&key, &curs_db[col], curs_cmp))) {
		cp = *(struct curs_pos **)vp;
		return (unsigned *)&cp->uv;
	}
#endif
	else
		return NULL;
}

#ifndef HAVE_LIBAVLBST
static int
curs_cmp(const void *a, const void *b)
{
	return strcmp(
	    ((const struct curs_pos *)a)->path,
	    ((const struct curs_pos *)b)->path);
}
#endif

/***********
 * diff DB *
 ***********/

void
diff_db_store(struct ui_state *st)
{
#ifdef HAVE_LIBAVLBST
	st->bst = diff_db->root;
	diff_db->root = NULL;
#else
	st->bst = *diff_db;
	*diff_db = NULL;
#endif
	st->num = *db_num;
	*db_num = 0;
	st->list = *db_list;
	*db_list = NULL;
	st->mmrkd = *mmrkd;
	*mmrkd = 0;
}

void
diff_db_restore(struct ui_state *st)
{
	diff_db_free(0);
#ifdef HAVE_LIBAVLBST
	diff_db->root = st->bst;
#else
	*diff_db = st->bst;
#endif
	*db_num = st->num;
	*db_list = st->list;
	*mmrkd = st->mmrkd;
}

void
diff_db_sort(int i)
{
    db_idx = 0; /* shared with str_db */
	maxsiz = 0;
	maxmajor = 0;
	maxminor = 0;
	tusrlen = 0;
	tgrplen = 0;

	if (!tot_db_num[i]) {
		goto exit;
	}

	if (!db_list[i]) {
		db_list[i] = malloc(tot_db_num[i] * sizeof(struct filediff *));
	}

	cur_list = db_list[i];
#ifdef HAVE_LIBAVLBST
	mk_list(diff_db[i].root);
#else
	twalk(diff_db[i], mk_list);
#endif
exit:
	db_num[i] = db_idx;
	usrlen[i] = tusrlen + 1; /* column separator */
	grplen[i] = tgrplen + 1;

	if (add_bsize) {
		/* 5 instead of 4 for the column separator */
		for (bsizlen[i] = 5; maxsiz >= 10000;
		    bsizlen[i]++, maxsiz /= 10);
#if defined (TRACE)
        fprintf(debug, "  maxmajor=%lu maxminor=%lu\n", maxmajor, maxminor);
#endif
		if (maxmajor || maxminor) {
			for (majorlen[i] = 4; maxmajor >= 1000;
                 majorlen[i]++, maxmajor /= 10)
            {}
			for (minorlen[i] = 4; maxminor >= 1000;
                 minorlen[i]++, maxminor /= 10)
            {}
            const unsigned short j = majorlen[i] + minorlen[i] + 1; /* + ',' */

			if (bsizlen[i] < j) {
				bsizlen[i] = j;
			}
		}
	}
}

#define PROC_DIFF_NODE() \
	do { \
    if ((!file_pattern || \
	     ((S_ISDIR(f->type[0]) || S_ISDIR(f->type[1])) && \
          ((find_dir_name && !regexec(&find_dir_name_regex, f->name, 0, NULL, 0)) || \
           (!find_dir_name && !recursive) || is_diff_dir(f) \
          ) \
         ) || \
         (!S_ISDIR(f->type[0]) && !S_ISDIR(f->type[1]) && \
          (!find_name || !regexec(&fn_re, f->name, 0, NULL, 0)) && \
          (!gq_pattern || !gq_proc(f)))) \
        && \
        (!nohidden || f->name[0] != '.' || \
         (dotdot && f->name[1] == '.' && !f->name[2])) \
        && \
	    (bmode || fmode || \
	     ((!noequal || \
	       f->diff == '!' || \
	       (S_ISDIR(f->type[0]) && (!recursive || is_diff_dir(f))) || \
	       (f->type[0] & S_IFMT) != (f->type[1] & S_IFMT)) \
	      && \
	      (!real_diff || \
	       f->diff == '!' || (S_ISDIR(f->type[0]) && S_ISDIR(f->type[1]) \
	       && (!recursive || is_diff_dir(f)))) \
	      && \
	      (!nosingle || \
	        /* no right without left */ \
	       ((!(nosingle & 2) || f->type[0]) && \
	        /* no left without right */ \
	        (!(nosingle & 1) || f->type[1]))) \
	      && \
	      (!excl_or || \
	       (!f->type[0] &&  f->type[1]) || \
	       ( f->type[0] && !f->type[1]))))) \
	{ \
		cur_list[db_idx++] = f; \
		\
		if (add_bsize) { \
			if (f->type[0]) { \
				if (S_ISCHR(f->type[0]) || \
				    S_ISBLK(f->type[0])) { \
					if ((long)major(f->rdev[0]) > \
					    (long)maxmajor) { \
						maxmajor = major(f->rdev[0]); \
					} \
					\
					if ((long)minor(f->rdev[0]) > \
					    (long)maxminor) { \
						maxminor = minor(f->rdev[0]); \
					} \
				} else if (f->siz[0] > maxsiz) { \
					maxsiz = f->siz[0]; \
				} \
			} \
			\
			if (f->type[1]) { \
				if (S_ISCHR(f->type[1]) || \
				    S_ISBLK(f->type[1])) { \
					if ((long)major(f->rdev[1]) \
					    > (long)maxmajor) { \
						maxmajor = major(f->rdev[1]); \
					} \
					\
					if ((long)minor(f->rdev[1]) \
					    > (long)maxminor) { \
						maxminor = minor(f->rdev[1]); \
					} \
				} else if (f->siz[1] > maxsiz) { \
					maxsiz = f->siz[1]; \
				} \
			} \
		} \
		\
		if (add_owner) { \
			if (f->type[0]) { \
				if (!(pw = getpwuid(f->uid[0]))) { \
					l = 5; \
				} else { \
					l = strlen(pw->pw_name); \
				} \
				\
				if (l > tusrlen) { \
					tusrlen = l; \
				} \
			} \
			\
			if (f->type[1]) { \
				if (!(pw = getpwuid(f->uid[1]))) { \
					l = 5; \
				} else { \
					l = strlen(pw->pw_name); \
				} \
				\
				if (l > tusrlen) { \
					tusrlen = l; \
				} \
			} \
		} \
		\
		if (add_group) { \
			if (f->type[0]) { \
				if (!(gr = getgrgid(f->gid[0]))) { \
					l = 5; \
				} else { \
					l = strlen(gr->gr_name); \
				} \
				\
				if (l > tgrplen) { \
					tgrplen = l; \
				} \
			} \
			\
			if (f->type[1]) { \
				if (!(gr = getgrgid(f->gid[1]))) { \
					l = 5; \
				} else { \
					l = strlen(gr->gr_name); \
				} \
				\
				if (l > tgrplen) { \
					tgrplen = l; \
				} \
			} \
		} \
	} \
	} while (0)

#ifdef HAVE_LIBAVLBST
static void
mk_list(struct bst_node *n)
{
	struct filediff *f;
	struct passwd *pw;
	struct group *gr;
	size_t l;

	if (!n) {
		return;
	}

	mk_list(n->left);
	f = n->key.p;
	PROC_DIFF_NODE();
	mk_list(n->right);
}
#else
static void
mk_list(const void *n, const VISIT which, const int depth)
{
	struct filediff *f;
	struct passwd *pw;
	struct group *gr;
	size_t l;

	(void)depth;

	switch (which) {
	case postorder:
	case leaf:
		f = *(struct filediff * const *)n;
		PROC_DIFF_NODE();
		break;
	default:
		;
	}
}
#endif

#define IS_F_DIR(n) \
    /* both are dirs */ \
    (S_ISDIR(f##n->type[0]) && S_ISDIR(f##n->type[1])) || \
    /* only left dir present */ \
    (S_ISDIR(f##n->type[0]) && !f##n->type[1]) || \
    /* only right dir present */ \
    (S_ISDIR(f##n->type[1]) && !f##n->type[0])

static int
diff_cmp(
#ifdef HAVE_LIBAVLBST
    union bst_val a, union bst_val b
#else
    const void *a, const void *b
#endif
    )
{
	const struct filediff
#ifdef HAVE_LIBAVLBST
	    *f1 = a.p,
	    *f2 = b.p;
#else
	    *f1 = a,
	    *f2 = b;
#endif
	const char *name1 = f1->name;
	const char *name2 = f2->name;
	const bool dotdot2 = str_eq_dotdot(name2);

	if (str_eq_dotdot(name1)) {
		if (dotdot2) {
			return 0;
		} else {
			return -1;
		}
	} else if (dotdot2) {
		return 1;
	} else if (sorting == SORTMTIME) {
        struct timespec t1, t2;

		t1 = f1->type[0] ? f1->mtim[0] : f1->mtim[1];
		t2 = f2->type[0] ? f2->mtim[0] : f2->mtim[1];

        const int i = cmp_timespec(t1, t2);
        if (i)
            return i;
	} else if (sorting == SORTSIZE) {
		off_t t1, t2;
		short f1_dir = IS_F_DIR(1),
		      f2_dir = IS_F_DIR(2);
		short dirsort = f1_dir && !f2_dir ? -1 :
		                f2_dir && !f1_dir ?  1 : 0;

		if (dirsort)
			return dirsort;

		t1 = f1->type[0] ? f1->siz[0] : f1->siz[1];
		t2 = f2->type[0] ? f2->siz[0] : f2->siz[1];

		if      (t1 < t2) return -1;
		else if (t1 > t2) return  1;

    } else if (sorting == SORT_OWNER) {
        uid_t uid1 = f1->type[0] ? f1->uid[0] : f1->uid[1];
        uid_t uid2 = f2->type[0] ? f2->uid[0] : f2->uid[1];
        get_uid_name(uid1, lbuf, sizeof lbuf);
        get_uid_name(uid2, rbuf, sizeof rbuf);
        const int i = strcmp(lbuf, rbuf);
        if (i)
            return i;
    } else if (sorting == SORT_GROUP) {
        gid_t gid1 = f1->type[0] ? f1->gid[0] : f1->gid[1];
        gid_t gid2 = f2->type[0] ? f2->gid[0] : f2->gid[1];
        get_gid_name(gid1, lbuf, sizeof lbuf);
        get_gid_name(gid2, rbuf, sizeof rbuf);
        const int i = strcmp(lbuf, rbuf);
        if (i)
            return i;
    } else if (sorting == SORT_SYMLINK) {
        const char *const lnk1 = f1->type[0] ? f1->link[0] : f1->link[1];
        const char *const lnk2 = f2->type[0] ? f2->link[0] : f2->link[1];
        if (lnk1 && lnk2) {
            const int i = strcmp(lnk1, lnk2);
            if (i)
                return i;
        } else if (lnk1)
            return -1;
        else if (lnk2)
            return 1;
    } else if (sorting != SORTMIXED) {
		short f1_dir = IS_F_DIR(1),
		      f2_dir = IS_F_DIR(2);
		short dirsort = f1_dir && !f2_dir ? -1 :
		                f2_dir && !f1_dir ?  1 : 0;

		if (dirsort) {
			if (sorting == DIRSFIRST)
				return  dirsort;
			else
				return -dirsort;
		}
	}

    int i;
    if (sortic && (i = strcasecmp(name1, name2))) {
        return i;
	} else {
		return strcmp(name1, name2);
	}
}

void
diff_db_add(struct filediff *diff, int i)
{
#if defined(TRACE)
	fprintf(debug, "<>diff_db_add name(%s) ltyp 0%o rtyp 0%o\n",
	    diff->name, diff->type[0], diff->type[1]);
#endif
#ifdef HAVE_LIBAVLBST
    union bst_val k, v;
    k.p = diff;
    v.i = 0;
    avl_add(&diff_db[i], k, v);
#else
	tsearch(diff, &diff_db[i], diff_cmp);
#endif
	tot_db_num[i]++;
}

void
diff_db_free(int i)
{
#if defined(TRACE)
	fprintf(debug, "->diff_db_free(%d)\n", i);
#endif
#ifdef HAVE_LIBAVLBST
	diff_db_delete(diff_db[i].root);
	diff_db[i].root = NULL;
#else
	struct filediff *f;

	while (diff_db[i] != NULL) {
		f = *(struct filediff **)diff_db[i];
		tdelete(f, &diff_db[i], diff_cmp);
		free_diff(f);
	}
#endif
	free(db_list[i]);
	db_list[i] = NULL;
	mmrkd[i] = 0;
	db_num[i] = 0;
#if defined(TRACE)
	fprintf(debug, "<-diff_db_free\n");
#endif
}

/* In the libavlbst case the nodes are not really deleted, just the memory
 * is freed after both subtrees had been visited.  This is much faster than
 * rebalancing the tree for each delete.  It is not dangerous since the tree
 * structure does not change due to the node memory free. */

#ifdef HAVE_LIBAVLBST
static void
diff_db_delete(struct bst_node *n)
{
	struct filediff *f;

	if (!n)
		return;

	diff_db_delete(n->left);
	diff_db_delete(n->right);
	f = n->key.p;
	free_diff(f);
	free(n);
}
#endif

/*******************************
 * Directory list DB *
 *******************************/

#define STRSL(s) \
	l = strlen(s); \
	\
	/* allow "/" (root dir) */ \
	if (l && --l && s[l] == '/') { \
		s[l] = 0; \
	}

/* 0: Was in DB, 1: Now added to DB */

int
db_dl_add(char *d1, char *d2, char *desc)
{
	char **da;
	int rv = 0;
	size_t l;
#ifdef HAVE_LIBAVLBST
	struct bst *db;
	struct bst_node *n;
	int br;
#else
	void *db;
	void *vp;
#endif

#if defined(TRACE)
	fprintf(debug, "->db_dl_add(%s,%s,%s)\n", d1, d2, desc);
#endif
	STRSL(d1);

	if (d2) {
		STRSL(d2);
		db = &ddl_db;
	} else {
		db = &bdl_db;
	}

	/* da[0] = dir or left dir
	   da[1] = desc
	   da[2] = NULL or right dir */
	da = calloc(3, sizeof(char *));
	da[1] = desc;

	if (!(*da = msgrealpath(d1))) {
		goto ret;
	}

	if (d2 && !(da[2] = msgrealpath(d2))) {
		goto ret;
	}

#ifdef HAVE_LIBAVLBST
    union bst_val k, v;
    k.p = da;
    v.i = 0;
    if (!(br = bst_srch(db, k, &n))) {
		goto ret;
	}

    avl_add_at(db, k, v, br, n);
	rv = 1;
#else
	vp = tsearch(da, db, d2 ? ddl_cmp : bdl_cmp);

	if (*(char ***)vp == da) {
		rv = 1;
	}
#endif

ret:
	if (!rv) {
		db_dl_free(da);
	}

#if defined(TRACE)
	fprintf(debug, "<-db_dl_add:%s added\n", rv ? "" : " not");
#endif
	return rv;
}

static void
db_dl_free(char **k)
{
	free(*k);
	free(k[1]);
	free(k[2]);
	free(k);
}

void
ddl_del(char **k)
{
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
    union bst_val key;
    key.p = k;
    bst_srch(&ddl_db, key, &n);
	avl_del_node(&ddl_db, n);
#else
	tdelete(k, &ddl_db, ddl_cmp);
#endif
	db_dl_free(k);
}

void
bdl_del(char **k)
{
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
    union bst_val key;
    key.p = k;
    bst_srch(&bdl_db, key, &n);
	avl_del_node(&bdl_db, n);
#else
	tdelete(k, &bdl_db, bdl_cmp);
#endif
	db_dl_free(k);
}

void
ddl_sort(void)
{
	if (!ddl_num) {
		return;
	}

	ddl_list = malloc(sizeof(char **) * ddl_num);
	db_idx = 0; /* shared with diff_db */
#ifdef HAVE_LIBAVLBST
	mk_ddl(ddl_db.root);
#else
	twalk(ddl_db, mk_ddl);
#endif
}

void
bdl_sort(void)
{
	if (!bdl_num) {
		return;
	}

#if defined(TRACE)
	fprintf(debug, "<>bdl_sort() alloc %u elements\n", bdl_num);
#endif
	bdl_list = malloc(sizeof(char **) * bdl_num);
	db_idx = 0; /* shared with diff_db */
#ifdef HAVE_LIBAVLBST
	mk_bdl(bdl_db.root);
#else
	twalk(bdl_db, mk_bdl);
#endif
}

#ifdef HAVE_LIBAVLBST
static void
mk_ddl(struct bst_node *n)
{
	if (!n) {
		return;
	}

	mk_ddl(n->left);
	ddl_list[db_idx++] = n->key.p;
	mk_ddl(n->right);
}
#else
static void
mk_ddl(const void *n, const VISIT which, const int depth)
{
	(void)depth;

	switch (which) {
	case postorder:
	case leaf:
		ddl_list[db_idx++] = *(char ** const *)n;
		break;
	default:
		;
	}
}
#endif

#ifdef HAVE_LIBAVLBST
static void
mk_bdl(struct bst_node *n)
{
	if (!n) {
		return;
	}

	mk_bdl(n->left);
#if defined(TRACE)
	fprintf(debug, "<>mk_bdl() set element %u\n", db_idx);
#endif
	bdl_list[db_idx++] = n->key.p;
	mk_bdl(n->right);
}
#else
static void
mk_bdl(const void *n, const VISIT which, const int depth)
{
	(void)depth;

	switch (which) {
	case postorder:
	case leaf:
		bdl_list[db_idx++] = *(char ** const *)n;
		break;
	default:
		;
	}
}
#endif

static int
ddl_cmp(
#ifdef HAVE_LIBAVLBST
    union bst_val a, union bst_val b
#else
    const void *a, const void *b
#endif
    )
{
	int i;
#ifdef HAVE_LIBAVLBST
	char **a1 = a.p,
	     **a2 = b.p;
#else
	char *const *a1 = a,
	     *const *a2 = b;
#endif

	i = strcmp(*a1, *a2);
#if defined(TRACE) && 0
	fprintf(debug, "  ddl_cmp(0:%s,%s): %d\n", *a1, *a2, i);
#endif

	if (!i) {
		i = strcmp(a1[2], a2[2]);
#if defined(TRACE) && 0
		fprintf(debug, "  ddl_cmp(1:%s,%s): %d\n", a1[2], a2[2], i);
#endif
	}

	return i;
}

static int
bdl_cmp(
#ifdef HAVE_LIBAVLBST
    union bst_val a, union bst_val b
#else
    const void *a, const void *b
#endif
    )
{
	int i;
#ifdef HAVE_LIBAVLBST
	char **a1 = a.p,
	     **a2 = b.p;
#else
	char *const *a1 = a,
	     *const *a2 = b;
#endif

	i = strcmp(*a1, *a2);
#if defined(TRACE) && 0
	fprintf(debug, "  bdl_cmp(0:%s,%s): %d\n", *a1, *a2, i);
#endif

	return i;
}

/********
 * misc *
 ********/

char *
str_tolower(char *in)
{
	int c;
	char *s = in;

	while ((c = *s))
        *s++ = (char)tolower(c);

	return in;
}
