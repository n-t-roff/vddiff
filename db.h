#ifdef HAVE_LIBAVLBST
# include <avlbst.h>
#endif

enum sorting { DIRSFIRST, FILESFIRST, SORTMIXED, SORTMTIME, SORTSIZE };

#ifdef HAVE_LIBAVLBST
void db_init(void);
void str_db_add(void **, char *, int, struct bst_node *);
int str_db_srch(void **, char *, struct bst_node **);
#else
struct ptr_db_ent {
	char *key;
	void *dat;
};

char *str_db_add(void **, char *);
int str_db_srch(void **, char *);
#endif
void diff_db_add(struct filediff *);
void diff_db_sort(void);
void diff_db_restore(struct ui_state *);
void diff_db_store(struct ui_state *);
void diff_db_free(void);
void free_names(void);
void db_def_ext(char *, char *, tool_flags_t);
struct tool *db_srch_ext(char *);
void db_set_curs(char *, unsigned, unsigned);
unsigned *db_get_curs(char *);
char *str_tolower(char *);
void uz_db_add(struct uz_ext *);
enum uz_id uz_db_srch(char *);
int ptr_db_add(void **, char *, void *);
int ptr_db_srch(void **, char *, void **, void **);
void ptr_db_del(void **, void *);
void *ptr_db_get_node(void *);

extern enum sorting sorting;
extern unsigned db_num;
extern struct filediff **db_list;
extern short noequal, real_diff;
extern void *scan_db;
extern void *name_db;
extern void *skipext_db;
extern void *uz_path_db;
extern void *alias_db;
