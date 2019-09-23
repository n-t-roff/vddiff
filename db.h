#ifndef DB_H
#define DB_H

#ifdef HAVE_LIBAVLBST
# include <avlbst.h>
#endif
#include "exec.h"
#include "uzp.h"

enum sorting { DIRSFIRST, FILESFIRST, SORTMIXED, SORTMTIME, SORTSIZE,
               SORT_OWNER, SORT_GROUP, SORT_SYMLINK };

struct scan_db {
	void *db;
	struct scan_db *next;
};

#ifdef HAVE_LIBAVLBST
void db_init(void);
void *db_new(int (*)(union bst_val, union bst_val));
int name_cmp(union bst_val, union bst_val);
#else
struct ptr_db_ent {
    char *key;
    void *dat;
};
#endif

/****************
 *  `char *` DB *
 ****************/

#ifdef HAVE_LIBAVLBST
int str_db_add(void **, char *, int, struct bst_node *);
#else
char *str_db_add(void **, char *);
#endif

/* Input:
 *   node: May be NULL
 * Return value: 0: found, !0: not found */
int str_db_srch(void **db, const char *const str,
#ifdef HAVE_LIBAVLBST
                struct bst_node **node);
#else
                char **node);
#endif

void str_db_del(void **, void *);
void *str_db_get_node(void *);
char **str_db_sort(void *, unsigned long);
void free_strs(void **);

void diff_db_add(struct filediff *, int);
void diff_db_sort(int);
void diff_db_restore(struct ui_state *);
void diff_db_store(struct ui_state *);
void diff_db_free(int);
void add_alias(char *const, char *, const tool_flags_t);
void db_def_ext(char *const, char *, tool_flags_t);
struct tool *db_srch_ext(char *);
void db_set_curs(int, char *, unsigned, unsigned);
unsigned *db_get_curs(int, char *);
char *str_tolower(char *);
void uz_db_add(char *, enum uz_id);
enum uz_id uz_db_srch(char *);
void uz_db_del(char *);

int ptr_db_add(void **, char *, void *);
int ptr_db_srch(void **, char *, void **, void **);
void ptr_db_del(void **, void *);
void *ptr_db_get_node(void *);

void push_scan_db(bool);
void pop_scan_db(void);
void free_scan_db(bool);
int db_dl_add(char *, char *, char *);
void ddl_del(char **);
void bdl_del(char **);
void ddl_sort(void);
void bdl_sort(void);

extern enum sorting sorting;
extern unsigned db_num[2];
extern struct filediff **db_list[2];
extern unsigned short bsizlen[2];
extern unsigned short majorlen[2], minorlen[2];
extern size_t usrlen[2], grplen[2];
extern short noequal, real_diff;
extern void *scan_db;
/* Use: UI dir diff: Every file found on left side is put into name_db
 * to check if it is found on right side too or if there are supernumerary
 * files in right side. */
extern void *name_db;
extern void *skipext_db;
extern void *uz_path_db;
extern bool sortic;
extern bool nohidden;

/* private declarations */

struct tool *set_ext_tool(char *_tool, tool_flags_t flags);

#endif /* DB_H */
