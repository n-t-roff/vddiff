enum sorting { FILESFIRST, DIRSFIRST, SORTMIXED };

void db_add(struct filediff *);
void add_name(char *);
int srch_name(char *);
void db_sort(void);
void db_restore(struct ui_state *);
void db_store(struct ui_state *);
void db_free(void);
void free_names(void);

extern enum sorting sorting;
extern unsigned db_num;
extern struct filediff **db_list;
