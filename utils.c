/* vobcopy 1.2.0
 *
 * Copyright (c) 2001 - 2009 robos@muon.de
 * Lots of contribution and cleanup from rosenauer@users.sourceforge.net
 * Critical bug-fix from Bas van den Heuvel
 * Takeshi HIYAMA made lots of changes to get it to run on FreeBSD
 * Erik Hovland made changes for solaris
 *  This file is part of vobcopy.
 *
 *  vobcopy is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  vobcopy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with vobcopy; if not, see <http://www.gnu.org/licenses/>.
 */

#include "vobcopy.h"

#if defined(HAS_LARGEFILE) || defined (MAC_LARGEFILE)
const int O_DETECTED_FLAG = O_LARGEFILE;
#else
const int O_DETECTED_FLAG = 0;
#endif

const long long BLOCK_SIZE      = 512LL; /*I believe this has more to do with POSIX*/
const long long DVD_SECTOR_SIZE = 2048LL;

const long long KILO = 1024LL;
const long long MEGA = (1024LL * 1024LL);
const long long GIGA = (1024LL * 1024LL * 1024LL);

const size_t MAX_PATH_LEN = 255;

/*Confident this breaks some preprocessor magic*/
#include <stdarg.h>

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

/*A lot of magic numbers involve sectors*/
off_t get_sector_offset(long long unsigned int sector)
{
	/*Not sure if this is subject to integer overflow*/
	return (off_t)(sector * DVD_SECTOR_SIZE);
}

void *palloc(size_t element, size_t elements)
{
	void *ret;

	ret = calloc(element, elements);
	if (!ret)
		die(1, "[Error] Failed to allocate memory!\n");

	return ret;
}

/*Replace characters in str, that are orig, with new*/
void strrepl(char *str, char orig, char new)
{
	/* B. Watson, aka Urchlay on freenode
	 * wrote this function
	 */
	while (*str) {
		if ((*str) == orig)
			(*str) = new;
		str++;
	}
}

void capitalize(char *str, size_t len)
{
	char c;
	size_t i;

	for (i = 0; (i < len) && (c = str[i]); i++) {
		if ((c <= 'z') && (c >= 'a')) {
			c -= 'a';
			c += 'A';

			str[i] = c;
		}
	}
}

/*This was implemented five different times in a few functions*/
/* options_str: Informs the user of options to take
 * opts:        Valid replies (For function use only)
 */
char get_option(char *options_str, const char *opts)
{
	int c;

	while ((c = fgetc(stdin))) {
		if (c == '\n' || c == EOF)
			continue;
		else if (strchr(opts, c))
			return c;
		else
			printe(options_str);

		/*Sleeps for one second each iteration*/
		sleep(1);
	}

	return c;
}



/*Zero's string, and then copies the source*/
char *safestrncpy(char *dest, const char *src, size_t n)
{
	memset(dest, 0, n);

	if (n <= 1)
		return dest;

	return strncpy(dest, src, n-1);
}

/*Found this copied and pasted twice in the code*/
long long unsigned int suffix2llu(char input)
{
	switch (input) {
	case 'b':
	case 'B':
		return BLOCK_SIZE; /*blocks (normal, not dvd) */
	case 'k':
	case 'K':
		return KILO;
	case 'm':
	case 'M':
		return MEGA;
	case 'g':
	case 'G':
		return GIGA;
	case '?':
	default:
		die(1, "[Error] Wrong suffix behind -b, only b,k,m or g \n");
	}

	die(1, "suffix2llu, Impossible error\n");
	return 0;
}












