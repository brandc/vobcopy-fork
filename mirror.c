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

/*B. Watson*/
#include <stdbool.h>
#include <sys/types.h>
#include "vobcopy.h"

extern char *name;
extern time_t starttime;
extern int verbosity_level;
extern off_t disk_vob_size;
extern bool overwrite_flag;
extern bool overwrite_all_flag;
extern int overall_skipped_blocks;

/*=========================================================================*/
/*----------Why exactly does mirroring the disk take so much code?---------*/
/*=========================================================================*/
void mirror(char *dvd_name, bool provided_dvd_name_flag, char *provided_dvd_name, char *pwd, off_t pwd_free, bool onefile_flag,
	    bool force_flag, int alternate_dir_count, bool stdout_flag, char *onefile, char *provided_input_dir,
	    dvd_reader_t *dvd, int block_count)
{
	DIR *dir;
	struct dirent *directory;
	struct stat fileinfo;

	int blocks;
	int streamout;

	dvd_file_t *dvd_file = NULL;
	unsigned char bufferin[DVD_VIDEO_LB_LEN * BLOCK_COUNT];

	printe("\n[Info] DVD-name: %s\n", dvd_name);
	if (provided_dvd_name_flag) {
		printe("\n[Info] Your name for the dvd: %s\n", provided_dvd_name);
		safestrncpy(dvd_name, provided_dvd_name, MAX_PATH_LEN);
	}

	printe("[Info]  Disk free: %0.0f MB\n", (float)((float)pwd_free      / (float)(KILO * KILO)));
	printe("[Info]  Vobs size: %0.0f MB\n", (float)((float)disk_vob_size / (float)(KILO * KILO)));

	if (!((force_flag || pwd_free > disk_vob_size) && alternate_dir_count < 2))
		die("[Error] Not enough free space on the destination dir. Please choose another one or -f\n"
		       "[Error] or dirs behind -1, -2 ... are NOT allowed with -m!\n");

	/* no dirs behind -1, -2 ... since its all in one dir */
	char video_ts_dir[MAX_PATH_LEN];
	char number[8];
	char input_file[MAX_PATH_LEN];
	char output_file[MAX_PATH_LEN];
	int i, start, title_nr = 0;
	off_t file_size;
	double tmp_i = 0, tmp_file_size = 0;
	char d_name[MAX_PATH_LEN];

	safestrncpy(name, pwd, MAX_PATH_LEN - 34);
	strncat(name, dvd_name, 33);

	if (!stdout_flag) {
		makedir(name);

		strcat(name, "/VIDEO_TS/");

		makedir(name);

		printe("[Info] Writing files to this dir: %s\n", name);
	}

	/*TODO: substitute with open_dir function */
	safestrncpy(video_ts_dir, provided_input_dir, MAX_PATH_LEN);
	strcat(video_ts_dir, "video_ts");	/*it's either video_ts */
	dir = opendir(video_ts_dir);	/*or VIDEO_TS */
	if (dir == NULL) {
		strncpy(video_ts_dir, provided_input_dir, MAX_PATH_LEN);
		strcat(video_ts_dir, "VIDEO_TS");
		dir = opendir(video_ts_dir);
		if (dir == NULL)
			die("[Error] Hmm, weird, the dir video_ts|VIDEO_TS on the dvd couldn't be opened\n"
			       "[Error] The dir to be opened was: %s\n"
			       "[Hint] Please mail me what your vobcopy call plus -v -v spits out\n",
			       video_ts_dir
			);
	}

	directory = readdir(dir);	/* thats the . entry */
	directory = readdir(dir);	/* thats the .. entry */
	/* according to the file type (vob, ifo, bup) the file gets copied */
	while ((directory = readdir(dir)) != NULL) {	/*main mirror loop */
		safestrncpy(output_file, name, MAX_PATH_LEN);

		/*in dvd specs it says it must be uppercase VIDEO_TS/VTS...
		   but iso9660 mounted dvd's sometimes have it lowercase */
		safestrncpy(d_name, directory->d_name, MAX_PATH_LEN);
		capitalize(d_name, MAX_PATH_LEN);

		/*in order to copy only _one_ file */
		/*                if( onefile_flag )
		   {
		   if( !strstr( onefile, d_name ) ) maybe other way around */
		/*                    continue;
		   } */
		/*in order to copy only _one_ file and do "globbing" a la: -O 02 will copy vts_02_1, vts_02_2 ... all that have 02 in it */
		if (onefile_flag) {
			char *tokenpos, *tokenpos1;
			char tmp[12];
			tokenpos = onefile;
			if (strstr(tokenpos, ",")) {
				/*tokens separated by , *//*this parses every token without modifying the onefile var */
				while ((tokenpos1 = strstr(tokenpos, ","))) {
					int len_begin;
					len_begin = tokenpos1 - tokenpos;
					safestrncpy(tmp, tokenpos, len_begin);
					tokenpos = tokenpos1 + 1;
					tmp[len_begin] = '\0';

					if (strstr(d_name, tmp))
						goto next;	/*if the token is found in the d_name copy */
					else
						continue;	/* next token is tested */
				}

				continue;	/*no token matched, next d_name is tested */
			} else {
				if (strstr(d_name, onefile))	/*if the token is found in the d_name copy */
					goto next;
				else				/*maybe other way around */
					continue;
			}
		}

		/*LABEL!!!!!!!!!!!!!!!!!!!!!!!!!*/
		next:/*for the goto - ugly, I know... */

		if (stdout_flag) /*this writes to stdout */
			streamout = STDOUT_FILENO;	/*in other words: 1, see "man stdout" */
		else {
			streamout = open_partial(output_file);
			if (streamout < 0)
				die("[Error] Either didn't have access or couldn't open %s\n", output_file);
		}

		/* get the size of that file */
		safestrncpy(input_file, video_ts_dir, MAX_PATH_LEN);
		strcat(input_file, "/");
		strcat(input_file, directory->d_name);
		stat(input_file, &fileinfo);
		file_size = fileinfo.st_size;
		tmp_file_size = file_size;

		memset(bufferin, 0, DVD_VIDEO_LB_LEN * sizeof(unsigned char));

		/*this here gets the title number */
		for (i = 1; i <= 99; i++) {	/*there are 100 titles, but 0 is
						   named video_ts, the others are 
						   vts_number_0.bup */
			sprintf(number, "_%.2i", i);

			if (strstr(directory->d_name, number)) {
				title_nr = i;

				break;	/*number found, is in i now */
			}
			/*no number -> video_ts is the name -> title_nr = 0 */
		}

		/*which file type is it */

		if (strstr(directory->d_name, ".bup") || strstr(directory->d_name, ".BUP")) {
			dvd_file = DVDOpenFile(dvd, title_nr, DVD_READ_INFO_BACKUP_FILE);

			/*this copies the data to the new file */
			for (i = 0; i * DVD_VIDEO_LB_LEN < file_size; i++) {
				DVDReadBytes(dvd_file, bufferin, DVD_VIDEO_LB_LEN);
				if (write(streamout, bufferin, DVD_VIDEO_LB_LEN) < 0)
					die("\n[Error] Error writing to %s \n"
					"[Error] Error: %s\n",
					output_file,
					strerror(errno));

				/* progress indicator */
				tmp_i = i;
				printe("%4.0fkB of %4.0fkB written\r", (tmp_i + 1) * (DVD_VIDEO_LB_LEN / KILO), tmp_file_size / KILO);
			}
			fputc('\n', stderr);
			if (!stdout_flag) {
				if (fdatasync(streamout) < 0)
					die("\n[Error] error writing to %s \n"
					       "[Error] error: %s\n",
						output_file,
						strerror(errno));

				close(streamout);
				rename_partial(output_file);
			}
		}

		if (strstr(directory->d_name, ".ifo") || strstr(directory->d_name, ".IFO")) {
			dvd_file = DVDOpenFile(dvd, title_nr, DVD_READ_INFO_FILE);

			/*this copies the data to the new file */
			for (i = 0; i * DVD_VIDEO_LB_LEN < file_size; i++) {
				DVDReadBytes(dvd_file, bufferin, DVD_VIDEO_LB_LEN);
				if (write(streamout, bufferin, DVD_VIDEO_LB_LEN) < 0)
					die("\n[Error] Error writing to %s \n"
					       "[Error] Error: %s\n",
					       output_file,
					       strerror(errno)
					);
				/* progress indicator */
				tmp_i = i;
				printe("%4.0fkB of %4.0fkB written\r", (tmp_i + 1) * (DVD_VIDEO_LB_LEN / KILO), tmp_file_size / KILO);
			}
			printe("\n");
			if (!stdout_flag) {
				if (fdatasync(streamout) < 0)
					die("\n[Error] error writing to %s \n"
					       "[Error] error: %s\n",
					       output_file,
					       strerror(errno)
					);

				close(streamout);
				rename_partial(output_file);
			}
		}

		if (strstr(directory->d_name, ".vob") || strstr(directory->d_name, ".VOB")) {
			if (directory->d_name[7] == '0' || title_nr == 0) {
				/*this is vts_xx_0.vob or video_ts.vob, a menu vob */
				dvd_file = DVDOpenFile(dvd, title_nr, DVD_READ_MENU_VOBS);
				start = 0;
			} else
				dvd_file = DVDOpenFile(dvd, title_nr, DVD_READ_TITLE_VOBS);

			if (directory->d_name[7] == '1' || directory->d_name[7] == '0')
				/* reset start when at beginning of Title */
				start = 0;
			else if (directory->d_name[7] > '1' && directory->d_name[7] <= '9') {
				off_t culm_single_vob_size = 0;
				int a, subvob;

				subvob = (directory->d_name[7] - '0');

				for (a = 1; a < subvob; a++) {
					if (strstr(input_file, ";?"))
						input_file[strlen(input_file) - 7] = (a + '0');
					else
						input_file[strlen(input_file) - 5] = (a + '0');

					/* input_file[ strlen( input_file ) - 5 ] = ( a + 48 ); */
					if (stat(input_file, &fileinfo) < 0)
						die("[Info] Can't stat() %s.\n", input_file);

					culm_single_vob_size += fileinfo.st_size;
					if (verbosity_level > 1)
						printe("[Info] Vob %d %d (%s) has a size of %lli\n", title_nr, subvob, input_file, fileinfo.st_size);
				}

				start = (culm_single_vob_size / DVD_VIDEO_LB_LEN);
				/*start = ( ( ( directory->d_name[7] - 49 ) * 512 * KILO ) - ( directory->d_name[7] - 49 ) );  */
				/* this here seeks d_name[7] 
				 *  (which is the 3 in vts_01_3.vob) Gigabyte (which is equivalent to 512 * KILO blocks 
				 *   (a block is 2kb) in the dvd stream in order to reach the 3 in the above example.
				 * NOT! the sizes of the "1GB" files aren't 1GB... 
				 */
			}

			/*this copies the data to the new file */
			if (verbosity_level > 1)
				printe("[Info] Start of %s at %d blocks \n", output_file, start);

			starttime = time(NULL);
			for (i = start; (i - start) * DVD_VIDEO_LB_LEN < file_size; i += block_count) {
				int tries = 0, skipped_blocks = 0;
				/* Only read and write as many blocks as there are left in the file */
				if ((i - start + block_count) * DVD_VIDEO_LB_LEN > file_size)
					block_count = (file_size / DVD_VIDEO_LB_LEN) - (i - start);

				while ((blocks = DVDReadBlocks(dvd_file, i, block_count, bufferin)) <= 0 && tries < 10) {
					if (tries == 9) {
						i                      += block_count;
						skipped_blocks         += 1;
						overall_skipped_blocks += 1;
						tries                   = 0;
					}
					tries++;
				}

				if (verbosity_level >= 1 && skipped_blocks > 0)
					printe("[Warn] Had to skip (couldn't read) %d blocks (before block %d)! \n ", skipped_blocks, i);

				/*TODO: this skipping here writes too few bytes to the output */

				if (write(streamout, bufferin, DVD_VIDEO_LB_LEN * blocks) < 0)
					die("\n[Error] Error writing to %s \n"
					       "[Error] Error: %s, errno: %d \n",
					       output_file,
					       strerror(errno),
					       errno
					);

				/*progression bar */
				/*this here doesn't work with -F 10 */
				/*                      if( !( ( ( ( i-start )+1 )*DVD_VIDEO_LB_LEN )%MEGA ) ) */
				progressUpdate(starttime, (int)(((i - start + 1) * DVD_VIDEO_LB_LEN)), (int)(tmp_file_size + DVD_SECTOR_SIZE), false);
				/*
				 *if( check_progress() )
				 *{
				 *	tmp_i = ( i-start );
				 *
				 *	percent = ( ( ( ( tmp_i+1 )*DVD_VIDEO_LB_LEN )*100 )/tmp_file_size );
				 *	printe("\r%4.0fMB of %4.0fMB written "),
				 *	( ( tmp_i+1 )*DVD_VIDEO_LB_LEN )/MEGA,
				 *	( tmp_file_size+DVD_SECTOR_SIZE )/MEGA );
				 *	printe("( %3.1f %% ) ",percent );
				 * }
				 */
			}
			/*this is just so that at the end it actually says 100.0% all the time... */
			/*TODO: if it is correct to always assume it's 100% is a good question.... */
			/*		printe("\r%4.0fMB of %4.0fMB written "),
						( ( tmp_i+1 )*DVD_VIDEO_LB_LEN )/MEGA,
						( tmp_file_size+DVD_SECTOR_SIZE )/MEGA );
					printe("( 100.0%% ) ") );
			*/
			progressUpdate(starttime, (int)(((i - start + 1) * DVD_VIDEO_LB_LEN)), (int)(tmp_file_size + DVD_SECTOR_SIZE), true);
			start = i;
			printe("\n");
			if (!stdout_flag) {
				if (fdatasync(streamout) < 0)
					die("\n[Error] error writing to %s \n"
					       "[Error] error: %s\n",
					       output_file,
					       strerror(errno)
					);

				close(streamout);
				rename_partial(output_file);
			}
		}
	}

	DVDCloseFile(dvd_file);
	DVDClose(dvd);
	if (overall_skipped_blocks > 0)
		printe("[Info] %d blocks had to be skipped, be warned.\n", overall_skipped_blocks);
	return;
	/*end of mirror block */
}




