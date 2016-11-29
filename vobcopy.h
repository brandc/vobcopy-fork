/* vobcopy 1.3.0
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

#define VERSION "1.3.0"

#if defined( __gettext__ )
#	include <locale.h>
#	include <libintl.h>
#	define _(Text) gettext(Text)
#else
#	define _(Text) Text
#endif

/*Allows libdvdcss to make it's presense known via stderr*/
#define DVDCSS_VERBOSE 1
/*Blocks to read at once for some reason*/
#define MAX_STRING  81
#define MAX_DIFFER  2000

/*Standard C lib*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/*POSIX and beyond*/
#include <ctype.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h> /*for readdir*/
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>
#include <termios.h>

#if ( defined( __unix__ ) || defined( unix )) && !defined( USG )
#	include <sys/param.h>
#endif

#if defined( __GNUC__ ) && !( defined( sun ) )
#	include <getopt.h>
#endif


/* I'm trying to have all supported OSes definitions clearly separated here */
/* The appearance could probably be made more readable -- lb                */

/* ////////// Solaris ////////// */
#if defined( __sun )
#	include <sys/mnttab.h>
#	include <sys/statvfs.h>
#else /* Solaris */

/* //////////  *BSD //////////  */
#	if ( defined( BSD ) && ( BSD >= 199306 ) )

#		if !defined( __NetBSD__ ) || ( defined( __NetBSD__) && ( __NetBSD_Version__ < 200040000 ) )
#			include <sys/mount.h>
#			define USE_STATFS 1
#		endif

#		if defined(NetBSD) || defined (OpenBSD)

#			include <sys/param.h>

#			define USE_GETMNTINFO

#			include <sys/statvfs.h>
#			define USE_STATVFS_FOR_DEV
#			define GETMNTINFO_USES_STATVFS
#		endif

#		if defined(__FreeBSD__)
#			define USE_STATFS_FOR_DEV
#			include <sys/statvfs.h>
#		else
#			include <sys/vfs.h>
#		endif

#	else /* *BSD */

		/* ////////// Darwin / OS X ////////// */
#		if defined ( __APPLE__ ) 

		/* ////////// Darwin ////////// */
#			if defined( __GNUC__ )

#				include <sys/param.h> 
#				include <sys/mount.h> 

#				include <sys/statvfs.h>
#				define GETMNTINFO_USES_STATFS 1
#				define USE_GETMNTINFO 1

#			endif

		/* ////////// OS X ////////// */
#		else  /* Darwin / OS X */

		/* ////////// GNU/Linux ////////// */
#			if ( defined( __linux__ ) )

#				include <mntent.h>
#				include <sys/vfs.h>
#				include <sys/statfs.h>

#				define USE_STATFS       1
#				define HAVE_GETOPT_LONG 1

#			elif defined( __GLIBC__ )

#				include <mntent.h>
#				include <sys/vfs.h>
#				include <sys/statvfs.h>

#				define HAVE_GETOPT_LONG 1

#			endif
#		endif
#	endif
#endif


#include <dvdread/dvd_reader.h>

/*for/from play_title.c*/
#include <assert.h>
/* #include "config.h" */
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
/* #include <dvdread/dvd_udf.h> */
#include <dvdread/nav_read.h>
#include <dvdread/nav_print.h>


#include "dvd.h"

#include <errno.h>
#include <limits.h>

/*vobcopy.c*/
char *name;
bool force_flag;
int verbosity_level;
bool overwrite_flag;
bool overwrite_all_flag;

void usage(char *);
int add_end_slash( char * );
int make_output_path( char *, char *, char *, int, int );
int is_nav_pack( unsigned char *buffer );
int makedir( char *name );
void set_signal_handlers();
void watchdog_handler( int signal );
void shutdown_handler( int signal );
char *safestrncpy(char *dest, const char *src, size_t n);
void progressUpdate( int starttime, int cur, int tot, int force );

/*utils.c*/
extern const long long DVD_SECTOR_SIZE;
extern const long long BLOCK_SIZE;
extern const long long KILO;
extern const long long MEGA;
extern const long long GIGA;

const char *QUIET_LOG_FILE;
extern const int O_DETECTED_FLAG;

void printe(char *str, ...);
void redirectlog(char *filename);
off_t get_free_space(char *path);
off_t get_used_space(char *path);
void rename_partial(char *input_file);
void capitalize(char *str, size_t len);
void die(const char *cause_of_death, ...);
void strrepl(char *str, char orig, char new);
void *palloc(size_t element, size_t elements);
long long unsigned int suffix2llu(char input);
char get_option(char *options_str, const char *opts);
off_t get_sector_offset(long long unsigned int sector);
long long unsigned int opt2llu(char *opt, char optchar);
char *safestrncpy(char *dest, const char *src, size_t n);
char *strcasestr(const char *haystack, const char *needle);
int open_partial(char *filename);
char *find_listing(char *path, char *name);
bool have_access(char *pathname, bool prompt);
size_t copy_vob(dvd_file_t *dvd_file, unsigned int start_sector, unsigned int sectors, unsigned int retries, int outfd);

/*mirror.c*/
void mirror(char *dvd_name, char *cwd, off_t pwd_free, bool onefile_flag,
	    bool force_flag, int alternate_dir_count, bool stdout_flag, char *onefile, char *provided_input_dir,
	    dvd_reader_t *dvd);

#if defined(__APPLE__) && defined(__GNUC__)
int fdatasync( int value );
#endif





