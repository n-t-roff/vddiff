#include "misc.h"

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
