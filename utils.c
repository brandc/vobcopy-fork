#ifndef __UTILS_H
#define __UTILS_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

void die(int ret, const char *cause_of_death, ...)
{
	va_list args;

	va_start(args, cause_of_death);
	vfprintf(stderr, cause_of_death, args);
	va_end(args);

	exit(ret);
}

/*printf for stderr*/
void printe(char *str, ...)
{
	va_list args;

	va_start(args, str);
	if (vfprintf(stderr, str, args) < 0)
		die(1, "vfprintf failed\n");
	va_end(args);

	return;
}

void strrepl(char *str, char orig, char new) {
	/* B. Watson, aka Urchlay on freenode
	 * wrote this entire function
	 */
	while (*str) {
		if (*str == orig)
			*str = new;
		str++;
	}
}

#endif /*__UTILS_H*/









