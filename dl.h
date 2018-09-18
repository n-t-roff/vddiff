#ifndef DL_H
#define DL_H

#include "compat.h"

extern unsigned bdl_num;
extern unsigned ddl_num;
extern char ***bdl_list;
extern char ***ddl_list;

void dl_add(void);
int dl_list(void);
void dl_info_bdl(FILE *);
void dl_info_ddl(FILE *);

#endif /* DL_H */
