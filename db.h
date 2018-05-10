#ifdef HAVE_LIBAVLBST
# include <avlbst.h>
#endif

enum sorting { DIRSFIRST, FILESFIRST, SORTMIXED, SORTMTIME, SORTSIZE };

struct scan_db {
	void *db;
	struct scan_db *next;
};

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
int str_db_srch(void **, char *, char **);
#endif
char **str_db_sort(void *, unsigned long);
void diff_db_add(struct filediff *, int);
void diff_db_sort(int);
void diff_db_restore(struct ui_state *);
void diff_db_store(struct ui_state *);
void diff_db_free(int);
void free_strs(void **);
void add_alias(char *, char *, tool_flags_t);
void db_def_ext(char *, char *, tool_flags_t);
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
void str_db_del(void **, void *);
void *str_db_get_node(void *);
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
extern void *name_db;
extern void *skipext_db;
extern void *uz_path_db;
extern bool sortic;
