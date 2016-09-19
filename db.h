enum sorting { DIRSFIRST, FILESFIRST, SORTMIXED };

void db_init(void);
void *db_new(int (*)(union bst_val, union bst_val));
void db_destroy(void *);
void str_db_add(void *, char *);
void *db_srch_str(void *, char *);
void db_add(struct filediff *);
void db_sort(void);
void db_restore(struct ui_state *);
void db_store(struct ui_state *);
void db_free(void);
void free_names(void);
int name_cmp(union bst_val, union bst_val);
void db_def_ext(char *, char *, int);
void db_set_curs(char *, unsigned, unsigned);

extern enum sorting sorting;
extern unsigned db_num;
extern struct filediff **db_list;
extern short noequal, real_diff;
extern void *scan_db;
extern void *name_db;
extern void *curs_db;
extern void *ext_db;
