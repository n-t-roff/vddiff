void fs_mkdir(short tree);
void fs_rename(int);
void fs_chmod(int, long, int);
void fs_chown(int, int, long, int);
int fs_rm(int, char *, long, int, unsigned);
int fs_cp(int, long, int, unsigned);
void rebuild_db(short);
