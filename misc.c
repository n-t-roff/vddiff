#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <regex.h>
#include "compat.h"
#include "misc.h"
#include "main.h"
#include "ui.h"

int
getuwidth(unsigned long u)
{
	int w;

	if (u < 10) w = 1;
	else if (u < 100) w = 2;
	else if (u < 1000) w = 3;
	else if (u < 10000) w = 4;
	else if (u < 100000) w = 5;
	else w = 6;

	return w;
}

char *
msgrealpath(const char *p)
{
	char *s;

	if (!(s = realpath(p, NULL))) {
		printerr(strerror(errno), LOCFMT "realpath \"%s\"" LOCVAR, p);
	}

	return s;
}
