#ifndef UNIT_PREFIX_H
#define UNIT_PREFIX_H

#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>

struct unit_prefix {
    const unsigned dont_scale; /* Don't change number, maybe add grouping */
    const unsigned dont_group; /* Don't group number */
    const unsigned decimal; /* Use powers of 1000 instead of 1024 */
    const unsigned space; /* Add space between number and unit prefix */
    int (*const unit_prefix)(char *const buf, /* If NULL printf() is used */
                             const size_t bufsiz,
                             FILE *file_ptr, /* If NULL `stdout` is used */
                             const intmax_t value, const unsigned mode);
};

extern const struct unit_prefix UnitPrefix;

#endif /* UNIT_PREFIX_H */
