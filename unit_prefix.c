#include "unit_prefix.h"

static int unit_prefix(char *const buf, const size_t bufsiz,
                       FILE *file_ptr, const intmax_t value,
                       const unsigned mode);

const struct unit_prefix UnitPrefix = {
    .dont_scale = 1,
    .dont_group = 2,
    .decimal    = 4,
    .space      = 8,
    .unit_prefix = unit_prefix
};

static int unit_prefix(char *const buf, const size_t bufsiz,
                       FILE *file_ptr, const intmax_t value,
                       const unsigned mode)
{
    if (!file_ptr)
        file_ptr = stdout;

    /* Don't group -> implies "don't scale" */
    if ((mode & UnitPrefix.dont_group) ||
            (value > -1024 && value < 1024))
    {
        if (buf)
            return snprintf(buf, bufsiz, "%jd", value);
        else
            return fprintf(file_ptr, "%jd", value);
    }
    if (mode & UnitPrefix.dont_scale) {
        if (buf)
            return snprintf(buf, bufsiz, "%'jd", value);
        else
            return fprintf(file_ptr, "%'jd", value);
    }
    const double pw = (mode & UnitPrefix.decimal) ? 1000 : 1024;
    const double thr = 999.9;
    double f = value / pw;
    const char *unit = "K";

    if (f < -thr || f > thr) {
        f /= pw;
        unit = "M";
    }
    if (f < -thr || f > thr) {
        f /= pw;
        unit = "G";
    }
    if (f < -thr || f > thr) {
        f /= pw;
        unit = "T";
    }
    const char *const space = (mode & UnitPrefix.space) ? " " : "";

    if (f > -10 && f < 10) {
        if (buf)
            return snprintf(buf, bufsiz, "%.1f%s%s", f, space, unit);
        else
            return fprintf(file_ptr, "%.1f%s%s", f, space, unit);
    }
    if (f < 0)
        f -= .5;
    else
        f += .5;
    if (buf)
        return snprintf(buf, bufsiz, "%.0f%s%s", f+.5, space, unit);
    else
        return fprintf(file_ptr, "%.0f%s%s", f+.5, space, unit);
}
