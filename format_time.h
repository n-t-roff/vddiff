#ifndef FORMAT_TIME_H
#define FORMAT_TIME_H

#include <stdio.h>
#include <time.h>

struct format_time {
    int (*const time_t_to_hour_min_sec)(char *const buf,
                                        const size_t bufsiz,
                                        FILE *file_ptr,
                                        time_t tot_sec);
};

extern const struct format_time FormatTime;

#endif /* FORMAT_TIME_H */
