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

const char *QUIET_LOG_FILE = "vobcopy.bla";
const size_t MAX_PATH_LEN = 255;

/*Confident this breaks some preprocessor magic*/
#include <stdarg.h>

void die(const char *cause_of_death, ...)
{
	va_list args;

	va_start(args, cause_of_death);
	vfprintf(stderr, cause_of_death, args);
	va_end(args);

	exit(1);
}

/*printf for stderr*/
void printe(char *str, ...)
{
	va_list args;

	va_start(args, str);
	if (vfprintf(stderr, str, args) < 0)
		die("vfprintf failed\n");
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
		die("[Error] Failed to allocate memory!\n");

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

	printe(options_str);
	while ((c = fgetc(stdin))) {
		if (strchr(opts, c))
			return c;
		else
			printe(options_str);

		/*Sleeps for a 10th of a second each iteration*/
		usleep(100000);
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
		die("[Error] Wrong suffix behind -b, only b,k,m or g \n");
	}

	die("suffix2llu, Impossible error\n");
	return 0;
}

long long unsigned int opt2llu(char *opt, char optchar)
{
	char* suffix;
	long long int intarg;
	long long unsigned int ret;

	if (!isdigit(opt[0]))
		die("[Error] The thing behind -%c has to be a number! \n", optchar);

	intarg = atoll(optarg);
	suffix = strpbrk(optarg, "bkmgBKMG");
	if (!(*suffix))
		die("[Error] Wrong suffix behind -b, only b,k,m or g \n");

	intarg *= suffix2llu(*suffix);

	ret = intarg / DVD_SECTOR_SIZE;
	if (ret < 0)
		ret *= -1;

	return ret;
}

/*
 * get available space on target filesystem
 */

off_t get_free_space(char *path)
{

#ifdef USE_STATFS
	struct statfs fsinfo;
#else
	struct statvfs fsinfo;
#endif
	/*   ssize_t temp1, temp2; */
	long free_blocks, fsblock_size;
	off_t free_bytes;
#ifdef USE_STATFS
	statfs(path, &fsinfo);
	if (verbosity_level >= 1)
		printe("[Info] Used the linux statfs\n");
#else
	statvfs(path, &fsinfo);
	if (verbosity_level >= 1)
		printe("[Info] Used statvfs\n");
#endif
	free_blocks = fsinfo.f_bavail;
	/* On Solaris at least, f_bsize is not the actual block size -- lb */
	/* There wasn't this ifdef below, strange! How could the linux statfs
	 * handle this since there seems to be no frsize??! */
#ifdef USE_STATFS
	fsblock_size = fsinfo.f_bsize;
#else
	fsblock_size = fsinfo.f_frsize;
#endif
	free_bytes = ((off_t)free_blocks * (off_t)fsblock_size);
	if (verbosity_level >= 1) {
		printe("[Info] In freespace_getter:for %s : %.0f free\n", path,
			(float)free_bytes);
		printe("[Info] In freespace_getter:bavail %ld * bsize %ld = above\n",
			free_blocks, fsblock_size);
	}

	/*return ( fsinfo.f_bavail * fsinfo.f_bsize ); */
	return free_bytes;
}

/*
 * get used space on dvd (for mirroring)
 */

off_t get_used_space(char *path)
{

#ifdef USE_STATFS
	struct statfs fsinfo;
#else
	struct statvfs fsinfo;
#endif
	/*   ssize_t temp1, temp2; */
	long used_blocks, fsblock_size;
	off_t used_bytes;
#ifdef USE_STATFS
	statfs(path, &fsinfo);
	if (verbosity_level >= 1)
		printe("[Info] Used the linux statfs\n");
#else
	statvfs(path, &fsinfo);
	if (verbosity_level >= 1)
		printe("[Info] Used statvfs\n");
#endif
	used_blocks = fsinfo.f_blocks;
	/* On Solaris at least, f_bsize is not the actual block size -- lb */
	/* There wasn't this ifdef below, strange! How could the linux statfs
	 * handle this since there seems to be no frsize??! */
#ifdef USE_STATFS
	fsblock_size = fsinfo.f_bsize;
#else
	fsblock_size = fsinfo.f_frsize;
#endif
	used_bytes = ((off_t)used_blocks * (off_t)fsblock_size);
	if (verbosity_level >= 1) {
		printe("[Info] In usedspace_getter:for %s : %.0f used\n", path,
			(float)used_bytes);
		printe("[Info] In usedspace_getter:part1 %ld, part2 %ld\n",
			used_blocks, fsblock_size);
	}
	/*   return ( buf1.f_blocks * buf1.f_bsize ); */
	return used_blocks;
}

bool have_access(char *pathname, bool prompt)
{
	int op;

	if (access(pathname, R_OK | W_OK) < 0) {
		switch (errno) {
		case EACCES:
			printe("[Error] No access to %s\n", pathname);
			return false;
		case ENOENT:
			break;
		default:
			die("[Error] REPORT have_access\n");
		}
	} else if (!overwrite_all_flag && prompt) {
		printe("[Error] %s exists....\n", pathname);
		op = get_option("[Hint] Please choose [o]verwrite, "
				"[x]overwrite all, or [q]uit: ", "oxq");
		switch (op) {
		case 'o':
		case 'x':
			break;
		case 'q':
		case 's':
			return false;
		default:
			die("[Error] REPORT have_access\n");
		}
	}

	return true;
}

/*
*Rename: move file name from bla.partial to bla or mark as dublicate 
*/

void rename_partial(char *name)
{
	char output[MAX_PATH_LEN];

	safestrncpy(output, name, MAX_PATH_LEN);
	strncat(output, ".partial", MAX_PATH_LEN);

	if (!have_access(name, true) || !have_access(output, false))
		die("[Error] Failed to move \"%s\" to \"%s\"\n", output, name);

	if (rename(output, name) < 0) {
		die("[Error] REPORT: errno %s\n", strerror(errno));
	}
}

/*
* Creates a directory with the given name, checking permissions and reacts accordingly (bails out or asks user)
*/

int makedir(char *name)
{
	if (!have_access(name, false))
		die("[Error] Don't have access to create %s\n", name);

	if (mkdir(name, 0777)) {
		if (errno == EEXIST)
			return 0;
		die("[Error] Failed to create directory: %s\n", name);
	}

	return 0;
}

void redirectlog(char *filename)
{
	if (!have_access(filename, true) && !force_flag)
		die("[Error] Can't overwrite \"%s\" without -f\n");

	if (!freopen(filename, "w+", stderr))
		die("[Error] freopen\n");
}

int open_partial(char *filename)
{
	int fd;
	char output[MAX_PATH_LEN];

	memset(output, 0, MAX_PATH_LEN);

	if (strstr(filename, ";?")) {
		printe("\n[Hint] File on dvd ends in \";?\" (%s)\n", filename);
		assert(strlen(filename) > 2);
		safestrncpy(output, filename, strlen(filename)-2);
	} else
		safestrncpy(output, filename, MAX_PATH_LEN);

	printe("[Info] Writing to %s \n", output);

	if (!have_access(filename, false))
		return -1;

	/*append .partial*/
	strncat(output, ".partial", MAX_PATH_LEN);

	if (!have_access(output, true))
		return -1;

	fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		die("\n[Error] Error opening file %s\n" 
		    "[Error] Error: %s\n",
		    filename,
		    strerror(errno));

	return fd;
}


