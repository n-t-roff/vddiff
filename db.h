void db_add(struct filediff *);
int db_srch(char *);
void db_sort(void);
void db_restore(struct ui_state *);
void db_store(struct ui_state *);
void db_free(void);

extern unsigned db_num;
extern struct filediff **db_list;
