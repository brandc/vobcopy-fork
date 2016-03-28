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
*Rename: move file name from bla.partial to bla or mark as dublicate 
*/

void re_name(char *name)
{
	char new_output_file[MAX_PATH_LEN];

	safestrncpy(new_output_file, name, MAX_PATH_LEN);
	new_output_file[strlen(new_output_file) - 8] = 0;

	/*Check errno on error*/
	if (link(name, new_output_file)) {
		if (errno == EEXIST) {
			if (overwrite_flag)
				rename(name, new_output_file);
			else {
				printe("[Error] File %s already exists! Gonna name the new one %s.dupe \n",
					new_output_file,
					new_output_file);
				strcat(new_output_file, ".dupe");
				rename(name, new_output_file);
			}
		} else if (errno == EPERM) { /*EPERM means that the filesystem doesn't allow hardlinks, e.g. smb */
			/*this here is a stdio function which simply overwrites an existing file. Bad but I don't want to include another test... */
			rename(name, new_output_file);
			/*printe("[Info] Removed \".partial\" from %s since it got copied in full \n",output_file ); */
		} else
			printe("[Error] Unknown error, errno %d\n", errno);
	}

	if (unlink(name)) {
		printe("[Error] Could not remove old filename: %s \n", name);
		printe("[Hint] This: %s is a hardlink to %s. Dunno what to do... \n", new_output_file, name);
	}

	if (strstr(name, ".partial"))
		name[strlen(name) - 8] = 0;
}

/*
* Creates a directory with the given name, checking permissions and reacts accordingly (bails out or asks user)
*/

int makedir(char *name)
{
	int op;

	if (!mkdir(name, 0777))
		return 0;

	/*Check errno*/
	if (errno == EEXIST) {
		if (!overwrite_all_flag) {
			printe("[Error] The directory %s already exists!\n", name);
			printe("[Hint] You can either [c]ontinue writing to it, [x]overwrite all or you can [q]uit: ");
		}

		if (overwrite_all_flag == true)
			op = 'c';
		else
			op = get_option("\n[Hint] please choose [c]ontinue, [x]overwrite all or [q]uit\n", "cxq");

		if (op == 'c' || op == 'x') {
			if (op == 'x')
				overwrite_all_flag = true;
			return 0;
		} else if (op == 'q')
			exit(1);

	} else /*most probably the user has doesn't the permissions for creating a dir or there isn't enough space'*/
		die("[Error] Creating of directory %s\n failed! \n"
			"[Error] error: %s\n",
			name,
			strerror(errno));

	return 0;
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
/*

void redirectlog(char *filename)
{
	struct stat finfo;

	memset(&finfo, 0, sizeof(struct stat));

	if (stat(filename, &finfo) < 0) {
		switch (errno) {
		case EACCES:
			die("Failed to redirect log to %s\n", filename);
		case EFAULT:
			break;
		case ELOOP:
			die("Too many links to %s\n", filename);
		case ENAMETOOLONG:
			die("\"%s\" too long\n", filename);
		case ENOENT:
			die("Directory in path \"%s\" not a directory\n", filename);
		case NOMEM:
			die("Not enough memory\n");
		case EOVERFLOW:
			die("Program compiled for 32bit, and not 64bit\n");
		default:
			die("Unknown error");
		}
	}

	if (finfo.st_mode & S_ISREG) {
		
	}
}

*/






