#include "format_time.h"

static int time_t_to_hour_min_sec(char *const buf,
                                  const size_t bufsiz,
                                  FILE *file_ptr,
                                  time_t tot_sec);

const struct format_time FormatTime = {
    .time_t_to_hour_min_sec = time_t_to_hour_min_sec
};

static int time_t_to_hour_min_sec(char *const buf,
                                  const size_t bufsiz,
                                  FILE *file_ptr,
                                  time_t tot_sec)
{
    if (!file_ptr)
        file_ptr = stdout;
    size_t size = 0;
    int result = 0;
    time_t t = tot_sec / (60 * 60); /* hours */
    if (t) {
        if (buf)
            result = snprintf(buf+size, bufsiz-size, "%ld:", t);
        else
            result = fprintf(file_ptr, "%ld:", t);
        if (result < 0)
            return result;
        size += (size_t)result;

        tot_sec %= 60 * 60;
    }
    t = tot_sec / 60; /* minutes */

    if (buf)
        result = snprintf(buf+size, bufsiz-size, "%02ld:", t);
    else
        result = fprintf(file_ptr, "%02ld:", t);
    if (result < 0)
        return result;
    size += (size_t)result;

    if (t)
        tot_sec %= 60;

    if (buf)
        result = snprintf(buf+size, bufsiz-size, "%02ld", tot_sec);
    else
        result = fprintf(file_ptr, "%02ld", tot_sec);
    if (result < 0)
        return result;
    size += (size_t)result;

    return (int)size;
}
