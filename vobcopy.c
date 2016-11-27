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

/* CONTRIBUTORS (direct or through source "borrowing")
 * rosenauer@users.sf.net - helped me a lot! 
 * Billy Biggs <vektor@dumbterm.net> - took some of his play_title.c code 
 * and implemeted it here 
 * Håkan Hjort <d95hjort@dtek.chalmers.se> and Billy Biggs - libdvdread
 * Stephen Birch <sgbirch@imsmail.org> - debian packaging
 */

/*TODO:
 * mnttab reading is "wrong" on solaris
 * - error handling with the errno and strerror function
 * with -t still numbers get added to the name, -T with no numbers or so.
 */

/*THOUGHT-ABOUT FEATURES
 * - being able to specify
     - which title
     - which chapter
     - which angle 
     - which language(s)
     - which subtitle(s)
     to copy
 * - print the total playing time of that title
 */

/*If you find a bug, contact me at robos@muon.de*/

#include "vobcopy.h"

extern int errno;

char *name;
time_t starttime;
bool force_flag = false;
off_t disk_vob_size = 0;
int verbosity_level = 0;
bool overwrite_flag = false;
bool overwrite_all_flag = false;
int overall_skipped_blocks = 0;

/* --------------------------------------------------------------------------*/
/* MAIN */
/* --------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind, optopt;

	int op, i, num_of_files, partcount;
	int streamout, block_count, blocks, file_block_count;

	unsigned char *bufferin = NULL;
	long long unsigned int end_sector   = 0;
	long long unsigned int start_sector = 0;

	char *pwd                   = NULL;
	char *onefile               = NULL;
	char *dvd_path              = NULL;
	char *vobcopy_call          = NULL;
	char *logfile_path          = NULL;
	char *provided_input_dir    = NULL;
	char *provided_output_dir   = NULL;
	char **alternate_output_dir = NULL;

	char dvd_name[35];
	char provided_dvd_name[35];

	dvd_reader_t *dvd      = NULL;
	dvd_file_t   *dvd_file = NULL;

	int dvd_count           = 0;
	int paths_taken         = 0;
	int fast_factor         = 1; /*What does this magic "1" mean?*/
	int options_char        = 0;
	int watchdog_minutes    = 0;
	int alternate_dir_count = 0;
	int get_dvd_name_return = 0;

	off_t offset                        = 0;
	off_t pwd_free                      = 0;
	off_t vob_size                      = 0;
	off_t free_space                    = 0;
	off_t angle_blocks_skipped          = 0;
	ssize_t file_size_in_blocks         = 0;
	off_t max_filesize_in_blocks_summed = 0;
	off_t max_filesize_in_blocks        = MEGA; /* for 2^31 / DVD_SECTOR_SIZE */

	/*Boolean flags*/
	bool mounted                  = false;
	bool cut_flag                 = false;
	bool info_flag                = false;
	bool quiet_flag               = false;
	bool stdout_flag              = false;
	bool fast_switch              = false;
	bool mirror_flag              = false;
	bool verbose_flag             = false;
	bool onefile_flag             = false;
	bool titleid_flag             = false;
	bool large_file_flag          = false;
	bool longest_title_flag       = false;
	bool provided_dvd_name_flag   = false;
	bool provided_input_dir_flag  = false;
	bool provided_output_dir_flag = false;

	/*########################################################################
	 * The new part taken from play-title.c
	 *########################################################################*/
	int chapid        = 0;
	int titleid       = 2; /*What does this magic "2" mean?*/
	int angle         = 0;
	int sum_angles    = 0;
	int sum_chapters  = 0;
	int most_chapters = 0;
	tt_srpt_t    *tt_srpt  = NULL;
	ifo_handle_t *vmg_file = NULL;

	/*Zero buffers*/
	memset(dvd_name, 0, sizeof(dvd_name));
	memset(provided_dvd_name, 0, sizeof(provided_dvd_name));

	/*Allocating buffers*/
	pwd                 = palloc(sizeof(char), MAX_PATH_LEN);
	name                = palloc(sizeof(char), MAX_PATH_LEN);
	onefile             = palloc(sizeof(char), MAX_PATH_LEN);
	dvd_path            = palloc(sizeof(char), MAX_PATH_LEN);
	vobcopy_call        = palloc(sizeof(char), MAX_PATH_LEN);
	provided_output_dir = palloc(sizeof(char), MAX_PATH_LEN);
	provided_input_dir  = palloc(sizeof(char), MAX_PATH_LEN);
	logfile_path        = palloc(sizeof(char), MAX_PATH_LEN);
	bufferin            = palloc(sizeof(unsigned char), (DVD_VIDEO_LB_LEN * BLOCK_COUNT));

	alternate_output_dir = palloc(sizeof(char*), MAX_PATH_LEN);
	for (i = 0; i < 4; i++)
		alternate_output_dir[i] = palloc(sizeof(char), MAX_PATH_LEN);
	

	/**
	 *getopt-long
	 */
#ifdef HAVE_GETOPT_LONG
	int option_index = 0;
	static struct option long_options[] = {
		{"1st_alt_output_dir", 1, 0, '1'},
		{"2st_alt_output_dir", 1, 0, '2'},
		{"3st_alt_output_dir", 1, 0, '3'},
		{"4st_alt_output_dir", 1, 0, '4'},
		{"angle", 1, 0, 'a'},
		{"begin", 1, 0, 'b'},
		{"chapter", 1, 0, 'c'},
		{"end", 1, 0, 'e'},
		{"force", 0, 0, 'f'},
		{"fast", 1, 0, 'F'},
		{"help", 0, 0, 'h'},
		{"input-dir", 1, 0, 'i'},
		{"info", 0, 0, 'I'},
		{"large-file", 0, 0, 'l'},
		{"longest", 0, 0, 'M'},
		{"mirror", 0, 0, 'm'},
		{"title-number", 1, 0, 'n'},
		{"output-dir", 1, 0, 'o'},
		{"quiet", 0, 0, 'q'},
		{"onefile", 1, 0, 'O'},
		{"name", 1, 0, 't'},
		{"verbose", 0, 0, 'v'},
		{"version", 0, 0, 'V'},
		{"watchdog", 1, 0, 'w'},
		{"overwrite-all", 0, 0, 'x'},
		{0, 0, 0, 0}
	};
#endif

	/*for gettext - i18n */
#if defined( __gettext__ )
	setlocale(LC_ALL, "");
	textdomain("vobcopy");
	bindtextdomain("vobcopy", "/usr/share/locale");
#endif

	/*
	 * the getopt part (getting the options from command line)
	 */
	while (1) {
#ifdef HAVE_GETOPT_LONG
		options_char = getopt_long(argc, argv,
					   "1:2:3:4:a:b:c:e:i:n:o:qO:t:vfF:lmMhL:Vw:Ix",
					   long_options, &option_index);
#else
		options_char = getopt(argc, argv,
				      "1:2:3:4:a:b:c:e:i:n:o:qO:t:vfF:lmMhL:Vw:Ix-");
#endif

		if (options_char == -1)
			break;

		switch (options_char) {

		case 'a':	/*angle */
			if (!isdigit((int)*optarg))
				die("[Error] The thing behind -a has to be a number! \n");

			sscanf(optarg, "%i", &angle);
			angle--;	/*in the ifo they start at zero */
			if (angle < 0)
				die("[Hint] Um, you set angle to 0, try 1 instead ;-)\n");
			break;

		case 'b':	/*size to skip from the beginning (beginning-offset) */
			start_sector = opt2llu(optarg, 'b');

			cut_flag = true;
			break;

		case 'c':	/*chapter *//*NOT WORKING!!*/
			if (!isdigit((int)*optarg))
				die("[Error] The thing behind -c has to be a number! \n");

			chapid = atoi(optarg);
			if (chapid < 1)
				die("Not a valid chapter number");

			chapid--;	/*in the ifo they start at zero */
			break;

		case 'e':	/*size to stop from the end (end-offset) */
			end_sector = opt2llu(optarg, 'e');

			cut_flag = true;
			break;

		case 'f':	/*force flag, some options like -o, -1..-4 set this themselves */
			force_flag = true;
			break;

		case 'h':	/* good 'ol help */
			usage(argv[0]);
			goto cleanup;
			break;

		case 'i':	/*input dir, if the automatic needs to be overridden */
			if (isdigit((int)*optarg))
				die("[Error] Erm, the number comes behind -n ... \n");

			printe("[Hint] You use -i. Normally this is not necessary, vobcopy finds the input dir by itself." 
			       "This option is only there if vobcopy makes trouble.\n");
			printe("[Hint] If vobcopy makes trouble, please mail me so that I can fix this (robos@muon.de). Thanks\n");

			safestrncpy(provided_input_dir, optarg, MAX_PATH_LEN);
			if (strstr(provided_input_dir, "/dev")) {
				printe("[Error] Please don't use -i /dev/something in this version, only the next version will support this again.\n");
				printe("[Hint] Please use the mount point instead (/cdrom, /dvd, /mnt/dvd or something)\n");
			}
			provided_input_dir_flag = true;
			break;

#if defined( HAS_LARGEFILE ) || defined( MAC_LARGEFILE )
		case 'l':	/*large file output */
			max_filesize_in_blocks = (16*GIGA) / DVD_SECTOR_SIZE; /*16 GB /DVD_SECTOR_SIZE (block) */
			/* 2^63 / DVD_SECTOR_SIZE (not exactly) */
			large_file_flag = true;
			break;
#endif

		case 'm':	/*mirrors the dvd to harddrive completly */
			mirror_flag = true;
			info_flag = true;
			break;

		case 'M':	/*Main track - i.e. the longest track on the dvd */
			titleid_flag = true;
			longest_title_flag = true;
			break;

		case 'n':	/*title number */
			if (!isdigit((int)*optarg))
				die("[Error] The thing behind -n has to be a number! \n");

			sscanf(optarg, "%i", &titleid);
			titleid_flag = true;
			break;

		case 'o':	/*output destination */
			if (isdigit((int)*optarg))
				printe("[Hint] Erm, the number comes behind -n ... \n");

			safestrncpy(provided_output_dir, optarg, MAX_PATH_LEN);

			if (!strcasecmp(provided_output_dir, "stdout") || !strcasecmp(provided_output_dir, "-")) {
				stdout_flag = true;
				force_flag = true;
			} else {
				provided_output_dir_flag = true;
				alternate_dir_count++;
			}
			/*      force_flag = true; */
			break;

		case 'q':	/*quiet flag* - meaning no progress and such output */
			quiet_flag = true;
			break;

		case '1':	/*alternate output destination */
		case '2':
		case '3':
		case '4':
			if (alternate_dir_count < options_char - 48)
				die("[Error] Please specify output dirs in this order: -o -1 -2 -3 -4 \n");

			if (isdigit((int)*optarg))
				printe("[Hint] Erm, the number comes behind -n ... \n");

			safestrncpy(alternate_output_dir[options_char - 49], optarg, sizeof(alternate_output_dir[options_char - 49]));
			provided_output_dir_flag = true;
			alternate_dir_count++;
			force_flag = true;
			break;

		case 't':	/*provided title instead of the one from dvd,
				   maybe even stdout output */
			if (strlen(optarg) > 33)
				printf
				    ("[Hint] The max title-name length is 33, the remainder got discarded");
			safestrncpy(provided_dvd_name, optarg, MAX_PATH_LEN);
			provided_dvd_name_flag = true;
			if (!strcasecmp(provided_dvd_name, "stdout") || !strcasecmp(provided_dvd_name, "-")) {
				stdout_flag = true;
				force_flag = true;
			}

			strrepl(provided_dvd_name, ' ', '_');
			break;

		case 'v':	/*verbosity level, can be called multiple times */
			verbose_flag = true;
			verbosity_level++;
			break;

		case 'w':	/*sets a watchdog timer to cap the amount of time spent
				   grunging about on a heavily protected disc */
			if (!isdigit((int)*optarg))
				die("[Error] The thing behind -w has to be a number!\n");
			sscanf(optarg, "%i", &watchdog_minutes);
			if (watchdog_minutes < 1) {
				printe("[Hint] Negative minutes aren't allowed - disabling watchdog.\n");
				watchdog_minutes = 0;
			}
			break;

		case 'x':	/*overwrite all existing files without (further) question */
			overwrite_all_flag = true;
			break;

		case 'F':	/*Fast-switch */
			if (!isdigit((int)*optarg))
				die("[Error] The thing behind -F has to be a number! \n");
			sscanf(optarg, "%i", &fast_factor);
			if (fast_factor > BLOCK_COUNT) {	/*atm is BLOCK_COUNT == 64 */
				printe("[Hint] The largest value for -F is %d at the moment - used that one...\n", BLOCK_COUNT);
				fast_factor = BLOCK_COUNT;
			}

			fast_switch = true;
			break;

		case 'I':	/*info, doesn't do anything, but simply prints some infos
				   ( about the dvd ) */
			info_flag = true;
			break;

		case 'L':	/*logfile-path (in case your system crashes every time you
				   call vobcopy (and since the normal logs get written to 
				   /tmp and that gets cleared at every reboot... ) */
			safestrncpy(logfile_path, optarg, sizeof(logfile_path) - 1); /* -s so room for '/' */
			strcat(logfile_path, "/");
			logfile_path[sizeof(logfile_path) - 1] = '\0';

			verbose_flag = true;
			verbosity_level = 2;
			break;

		case 'O':	/*only one file will get copied */
			onefile_flag = true;
			safestrncpy(onefile, optarg, MAX_PATH_LEN);

			for (i = 0; onefile[i]; i++) {
				onefile[i] = toupper(onefile[i]);

				if (onefile[i - 1] == ',')
					onefile[i] = 0;
				else {
					onefile[i] = ',';
					onefile[i + 1] = 0;
				}
			}

			mirror_flag = true;
			break;
		case 'V':	/*version number output */
			fprintf(stderr, "Vobcopy " VERSION " - GPL Copyright (c) 2001 - 2009 robos@muon.de\n");
			goto cleanup;
			break;

		case '?':	/*probably never gets here, the others should catch it */
			printe("[Error] Wrong option.\n");
			usage(argv[0]);
			goto cleanup;
			break;

#ifndef HAVE_GETOPT_LONG
		case '-':	/* no getopt, complain */
			printe("[Error] %s was compiled without support for long options.\n", argv[0]);
			usage(argv[0]);
			goto cleanup;
			break;
#endif

		default: /*probably never gets here, the others should catch it */
			printe("[Error] Wrong option.\n");
			usage(argv[0]);
			goto cleanup;
		}
	} /*End of optopt while loop*/

	printe("Vobcopy " VERSION " - GPL Copyright (c) 2001 - 2009 robos@muon.de\n");
	printe("[Hint] All lines starting with \"libdvdread:\" are not from vobcopy but from the libdvdread-library\n");


	/*get the current working directory */
	if (provided_output_dir_flag)
		strcpy(pwd, provided_output_dir);
	else if (!getcwd(pwd, MAX_PATH_LEN))
		die("\n[Error] Hmm, the path length of your current directory is really large (>255)\n" \
		    "[Hint] Change to a path with shorter path length pleeeease ;-)\n");

	add_end_slash(pwd);

	if (quiet_flag) {
		char tmp_path[MAX_PATH_LEN];

		safestrncpy(tmp_path, pwd, MAX_PATH_LEN - strlen(QUIET_LOG_FILE));
		strcat(tmp_path, QUIET_LOG_FILE);
		printe("[Hint] Quiet mode - All messages will now end up in %s\n", tmp_path);
		redirectlog(tmp_path);
	}

	if (verbosity_level > 1) {	/* this here starts writing the logfile */
		printe("[Info] High level of verbosity\n");

		if (strlen(logfile_path) < 3)
			strcpy(logfile_path, pwd);
		/*This concatenates "vobcopy_", the version, and then ".log"*/
		strcat(logfile_path, "vobcopy_" VERSION ".log");

		redirectlog(logfile_path);

		strcpy(vobcopy_call, argv[0]);
		for (i = 1; i != argc; i++) {
			strcat(vobcopy_call, " ");
			strcat(vobcopy_call, argv[i]);
		}

		printe("--------------------------------------------------------------------------------\n");
		printe("[Info] Called: %s\n", vobcopy_call);
	}

	/*sanity check: -m and -n are mutually exclusive... */
	if (titleid_flag && mirror_flag)
		die("\n[Error] There can be only one: either -m or -n...\n");

	/*
	 * Check if the provided path is too long
	 */
	if (optind < argc) {	/* there is still the path as in vobcopy-0.2.0 */
		provided_input_dir_flag = true;
		if (strlen(argv[optind]) >= MAX_PATH_LEN)
			/*Seriously? "_Bloody_ path"?*/
			die("\n[Error] Bloody path to long '%s'\n", argv[optind]);
		safestrncpy(provided_input_dir, argv[optind], MAX_PATH_LEN);
	}

	if (provided_input_dir_flag) {	/*the path has been given to us */
		int result;
		/* 'mounted' is an enum, it should not get assigned an int -- lb */
		if ((result = get_device(provided_input_dir, dvd_path)) < 0) {
			printe("[Error] Could not get the device which belongs to the given path!\n");
			printe("[Hint] Will try to open it as a directory/image file\n");
			/*exit(1);*/
		}
		if (result == 0)
			mounted = false;

		if (result == 1)
			mounted = true;
	} else {/*need to get the path and device ourselves ( oyo = on your own ) */
		if ((dvd_count = get_device_on_your_own(provided_input_dir, dvd_path)) <= 0) {
			printe("[Warning] Could not get the device and path! Maybe not mounted the dvd?\n");
			printe("[Hint] Will try to open it as a directory/image file\n");
			/* exit( 1 );*/
		}
		if (dvd_count > 0)
			mounted = true;
		else
			mounted = false;
	}

	if (!mounted)
		/*see if the path given is a iso file or a VIDEO_TS dir */
		safestrncpy(dvd_path, provided_input_dir, MAX_PATH_LEN);

	/*
	 * Is the path correct
	 */
	printe("\n[Info] Path to dvd: %s\n", dvd_path);

	if (!(dvd = DVDOpen(dvd_path))) {
		printe("\n[Error] Path thingy didn't work '%s'\n", dvd_path);
		printe("[Error] Try something like -i /cdrom, /dvd  or /mnt/dvd \n");
		if (dvd_count > 1)
			printe("[Hint] By the way, you have %i cdroms|dvds mounted, that probably caused the problem\n", dvd_count);
		DVDClose(dvd);
		exit(1);
	}

	/*
	 * this here gets the dvd name
	 */
	if (provided_dvd_name_flag)
		safestrncpy(dvd_name, provided_dvd_name, MAX_PATH_LEN);
	else
		get_dvd_name_return = get_dvd_name(dvd_path, dvd_name);

	printe("[Info] Name of the dvd: %s\n", dvd_name);

	/*########################################################################
	 * The new part taken from play-title.c
	 *########################################################################*/

	/**
	 * Load the video manager to find out the information about the titles on
	 * this disc.
	 */
	vmg_file = ifoOpen(dvd, 0);
	if (!vmg_file) {
		printe("[Error] Can't open VMG info.\n");
		DVDClose(dvd);
		return -1;
	}
	tt_srpt = vmg_file->tt_srpt;
	ifoClose(vmg_file);

	/**
	 * Get the title with the most chapters since this is probably the main part
	 */
	for (i = 0; i < tt_srpt->nr_of_srpts; i++) {
		sum_chapters += tt_srpt->title[i].nr_of_ptts;

		if (i > 0 && (tt_srpt->title[i].nr_of_ptts > tt_srpt->title[most_chapters].nr_of_ptts))
			most_chapters = i;
	}

	if (longest_title_flag) {	/*no title specified (-n ) */
		titleid = get_longest_title(dvd);
		printe("[Info] longest title %d.\n", titleid);
	}

	if (!titleid_flag) /*no title specified (-n ) */
		titleid = most_chapters + 1;	/*they start counting with 0, I with 1... */

	/**
	 * Make sure our title number is valid.
	 */
	printe("[Info] There are %d titles on this DVD.\n", tt_srpt->nr_of_srpts);
	if (titleid <= 0 || (titleid - 1) >= tt_srpt->nr_of_srpts) {
		printe("[Error] Invalid title %d.\n", titleid);
		DVDClose(dvd);
		return -1;
	}

	/**
	 * Make sure the chapter number is valid for this title.
	 */

	printe("[Info] There are %i chapters on the dvd.\n", sum_chapters);
	printe("[Info] Most chapters has title %i with %d chapters.\n", (most_chapters + 1), tt_srpt->title[most_chapters].nr_of_ptts);

	if (info_flag) {
		printe("[Info] All titles:\n");
		for (i = 0; i < tt_srpt->nr_of_srpts; i++) {
			int chapters = tt_srpt->title[i].nr_of_ptts;
			printe("[Info] Title %i has %d chapter(s).\n", (i + 1), chapters);
		}
	}

	if (chapid < 0 || chapid >= tt_srpt->title[titleid - 1].nr_of_ptts) {
		printe("[Error] Invalid chapter %d\n", chapid + 1);
		DVDClose(dvd);
		return -1;
	}

	/**
	 * Make sure the angle number is valid for this title.
	 */
	for (i = 0; i < tt_srpt->nr_of_srpts; i++)
		sum_angles += tt_srpt->title[i].nr_of_angles;

	printe("\n[Info] There are %d angles on this dvd.\n", sum_angles);
	if (angle < 0 || angle >= tt_srpt->title[titleid - 1].nr_of_angles) {
		printe("[Error] Invalid angle %d\n", angle + 1);
		DVDClose(dvd);
		return -1;
	}

	if (info_flag) {
		printe("[Info] All titles:\n");
		for (i = 0; i < tt_srpt->nr_of_srpts; i++) {
			int angles = tt_srpt->title[i].nr_of_angles;

			printe("[Info] Title %i has %d angle(s).\n", (i + 1), angles);
		}

		disk_vob_size = get_used_space(provided_input_dir);
	}

	/*
	 * get the whole vob size via stat( ) manually
	 */
	if (mirror_flag) {	/* this gets the size of the whole dvd */
		add_end_slash(provided_input_dir);
		disk_vob_size = get_used_space(provided_input_dir);
	} else if (mounted && !mirror_flag) {
		vob_size = (get_vob_size(titleid, provided_input_dir)) - get_sector_offset(start_sector) - get_sector_offset(end_sector);

		if (vob_size == 0 || vob_size > (9*GIGA)) {
			printe("\n[Error] Something went wrong during the size detection of the");
			printe("\n[Error] vobs, size check at the end won't work (probably), but I continue anyway\n\n");
			vob_size = 0;
		}
	}

	/*
	 * check if on the target directory is enough space free
	 * see man statfs for more info
	 */

	pwd_free = get_free_space(pwd);

	if (fast_switch)
		block_count = fast_factor;
	else
		block_count = 1;

	set_signal_handlers();

	if (watchdog_minutes) {
		printe("\n[Info] Setting watchdog timer to %d minutes\n", watchdog_minutes);
		alarm(watchdog_minutes * 60);
	}

	/**
	 * this is the mirror section
	 */

	if (mirror_flag) {
		mirror(dvd_name, provided_dvd_name_flag, provided_dvd_name,
		       pwd, pwd_free, onefile_flag, force_flag,
		       alternate_dir_count, stdout_flag, onefile, provided_input_dir,
		       dvd, block_count);

		goto cleanup;
	}

	/*
	 * Open now up the actual files for reading
	 * they come from libdvdread merged together under the given title number
	 * (thx again for the great library)
	 */
	printe("[Info] Using Title: %i\n", titleid);
	printe("[Info] Title has %d chapters and %d angles\n", tt_srpt->title[titleid - 1].nr_of_ptts, tt_srpt->title[titleid - 1].nr_of_angles);
	printe("[Info] Using Chapter: %i\n", chapid + 1);
	printe("[Info] Using Angle: %i\n", angle + 1);

	if (info_flag && vob_size != 0) {
		printe("\n[Info] DVD-name: %s\n", dvd_name);
		printe("[Info]  Disk free: %f MB\n", (double)pwd_free / (double)MEGA);
		printe("[Info]  Vobs size: %f MB\n", (double)vob_size / (double)MEGA);
		DVDCloseFile(dvd_file);
		DVDClose(dvd);
		/*hope all are closed now... */
		exit(0);
	}

	/**
	 * We've got enough info, time to open the title set data.
	 */
	dvd_file = DVDOpenFile(dvd, tt_srpt->title[titleid - 1].title_set_nr, DVD_READ_TITLE_VOBS);
	if (!dvd_file) {
		printe("[Error] Can't open title VOBS (VTS_%02d_1.VOB).\n", tt_srpt->title[titleid - 1].title_set_nr);
		DVDClose(dvd);
		return -1;
	}

	file_size_in_blocks = DVDFileSize(dvd_file);

	if (vob_size == (-get_sector_offset(start_sector) - get_sector_offset(end_sector))) {
		vob_size =
		           ((off_t) (file_size_in_blocks) * (off_t) DVD_VIDEO_LB_LEN) -
			   get_sector_offset(start_sector) - get_sector_offset(end_sector);
		if (verbosity_level >= 1)
			printe("[Info] Vob_size was 0\n");
	}

	/*debug-output: difference between vobsize read from cd and size returned from libdvdread */
	if (mounted && verbose_flag) {
		printe("\n[Info] Difference between vobsize read from cd and size returned from libdvdread:\n");
		/*printe("vob_size (stat) = %lu\nlibdvdsize      = %lu\ndiff            = %lu\n", TODO:the diff returns only crap...
		   vob_size, 
		   ( off_t ) ( file_size_in_blocks ) * ( off_t ) DVD_VIDEO_LB_LEN, 
		   ( off_t ) vob_size - ( off_t ) ( ( off_t )( file_size_in_blocks ) * ( off_t ) ( DVD_VIDEO_LB_LEN ) ) ); */
		printe("[Info] Vob_size (stat) = %lu\n[Info] libdvdsize      = %lu\n",
			(long unsigned int)vob_size,
			(long unsigned int)((off_t) (file_size_in_blocks) *
					    (off_t) DVD_VIDEO_LB_LEN));
	}

	if (info_flag) {
		printe("\n[Info] DVD-name: %s\n", dvd_name);
		printe("[Info]  Disk free: %.0f MB\n", (float)(pwd_free / MEGA));
		/* Should be the *disk* size here, right? -- lb */
		printe("[Info]  Vobs size: %.0f MB\n", (float)(disk_vob_size / MEGA));

		DVDCloseFile(dvd_file);
		DVDClose(dvd);
		/*hope all are closed now... */
		exit(0);
	}

	/* now the actual check if enough space is free */
	if (pwd_free < vob_size) {
		printe("\n[Info]  Disk free: %.0f MB", (float)pwd_free / (float)MEGA);
		printe("\n[Info]  Vobs size: %.0f MB", (float)vob_size / (float)MEGA);
		if (!force_flag)
			printe("\n[Error] Hmm, better change to a dir with enough space left or call with -f (force) \n");
		if (pwd_free == 0 && !force_flag) {
			printe("[Error] Hmm, statfs (statvfs) seems not to work on that directory. \n");
			printe("[Hint] Nevertheless, do you want vobcopy to continue [y] or do you want to check for \n");
			printe("[Hint] enough space first [q]?\n");

			op = get_option("[Error] Please choose [y] to continue or [n] to quit: ", "ynq");

			if (op == 'y') {
				force_flag = true;
				if (verbosity_level >= 1)
					printe("[Info] y pressed - force write\n");
			} else if (op == 'n' || op == 'q') {
				if (verbosity_level >= 1)
					printe("[Info] n/q pressed\n");
				exit(1);
			}
		}

		if (!force_flag)
			exit(1);
	}

	/*********************
	 * this is the main read and copy loop
	 *********************/
	printe("\n[Info] DVD-name: %s\n", dvd_name);

	/*if the user has given a name for the file */
	if (provided_dvd_name_flag && !stdout_flag) {
		printe("\n[Info] Your name for the dvd: %s\n", provided_dvd_name);
		safestrncpy(dvd_name, provided_dvd_name, MAX_PATH_LEN);
	}

	partcount    = 0;
	num_of_files = 0;
	while (offset < (file_size_in_blocks - start_sector - end_sector)) {
		partcount++;

		/*if the stream doesn't get written to stdout */
		if (!stdout_flag) {
			/*part to distribute the files over different directories */
			if (paths_taken == 0) {
				add_end_slash(pwd);
				free_space = get_free_space(pwd);

				if (verbosity_level > 1)
					printe("[Info] Free space for -o dir: %.0f\n", (float)free_space);
				if (large_file_flag)
					make_output_path(pwd, name, get_dvd_name_return, dvd_name, titleid, -1);
				else
					make_output_path(pwd, name, get_dvd_name_return, dvd_name, titleid, partcount);
			} else {
				for (i = 1; i < alternate_dir_count; i++) {
					if (paths_taken == i) {
						add_end_slash(alternate_output_dir[i - 1]);
						free_space = get_free_space (alternate_output_dir[i - 1]);

						if (verbosity_level > 1)
							printe("[Info] Free space for -%i dir: %.0f\n", i, (float)free_space);
						if (large_file_flag)
							make_output_path(alternate_output_dir[i - 1], name, get_dvd_name_return, dvd_name, titleid, -1);
						else
							make_output_path(alternate_output_dir[i - 1], name, get_dvd_name_return, dvd_name, titleid, partcount);
						/*alternate_dir_count--;*/
					}
				}
			}
			/*here the output size gets adjusted to the given free space */

			if (!large_file_flag && force_flag && free_space < (2*GIGA)) {
				max_filesize_in_blocks = ((free_space - (2*MEGA)) / DVD_SECTOR_SIZE);
				if (verbosity_level > 1)
					printe("[Info] Taken max_filesize_in_blocks(2GB version): %.0f\n",
						(float)max_filesize_in_blocks);
				paths_taken++;
			} else if (large_file_flag && force_flag) {	/*lfs version */
				max_filesize_in_blocks = ((free_space - (2*MEGA)) / DVD_SECTOR_SIZE);
				if (verbosity_level > 1)
					printe("[Info] Taken max_filesize_in_blocks(lfs version): %.0f\n",
						(float)max_filesize_in_blocks);
				paths_taken++;
			} else if (!large_file_flag) {
				max_filesize_in_blocks = (2*MEGA); /*if free_space is more than  2 GB fall back to max_filesize_in_blocks=2GB */
			}

			streamout = open_partial(name);
			if (streamout < 0) {
				printe("[Error] Either don't have access or couldn't open %s\n", name);
				goto cleanup;
			}
		}

		if (stdout_flag)			/*this writes to stdout */
			streamout = STDOUT_FILENO;	/*in other words: 1, see "man stdout" */

		/* this here is the main copy part */

		printe("\n");
		memset(bufferin, 0, (BLOCK_COUNT * DVD_VIDEO_LB_LEN * sizeof(unsigned char)));

		file_block_count = block_count;
		starttime = time(NULL);
		for (;(offset + (off_t) start_sector) < ((off_t) file_size_in_blocks - (off_t) end_sector) &&
		      offset - (off_t) max_filesize_in_blocks_summed - (off_t) angle_blocks_skipped < max_filesize_in_blocks;
		      offset += file_block_count) {
			int tries = 0, skipped_blocks = 0;
			/* Only read and write as many blocks as there are left in the file */
			if ((offset + file_block_count + (off_t) start_sector) > ((off_t) file_size_in_blocks - (off_t) end_sector)) {
				file_block_count = (off_t)file_size_in_blocks - (off_t)end_sector - offset - (off_t)start_sector;
			}

			if (offset + file_block_count -
			    (off_t) max_filesize_in_blocks_summed -
			    (off_t) angle_blocks_skipped >
			    max_filesize_in_blocks) {
				file_block_count = max_filesize_in_blocks - (offset + file_block_count -
						   (off_t)max_filesize_in_blocks_summed - (off_t)angle_blocks_skipped);
			}

			while ((blocks = DVDReadBlocks(dvd_file, (offset + start_sector), file_block_count, bufferin)) <= 0 && tries < 10) {
				if (tries == 9) {
					offset += file_block_count;
					skipped_blocks += 1;
					overall_skipped_blocks += 1;
					tries = 0;
				}
				/*if( verbosity_level >= 1 ) 
				 *	printe("[Warn] Had to skip %d blocks (reading block %d)! \n ",skipped_blocks, i ); */
				tries++;
			}

			if (verbosity_level >= 1 && skipped_blocks > 0)
				printe("[Warn] Had to skip (couldn't read) %d blocks (before block %d)! \n ",
					skipped_blocks,
					offset);

			/*TODO: this skipping here writes too few bytes to the output */

			ssize_t write_ret;
			if ((write_ret = write(streamout, bufferin, DVD_VIDEO_LB_LEN * blocks)) < 0)
				die("\n[Error] Write() error\n"
				       "[Error] It's possible that you try to write files\n"
				       "[Error] greater than 2GB to filesystem which\n"
				       "[Error] doesn't support it? (try without -l)\n"
				       "[Error] Error: %s\n",
				       strerror(errno)
				);

			/*this is for people who report that it takes vobcopy ages to copy something */
			/* TODO */

			progressUpdate(starttime,
				       (int)offset / BLOCK_SIZE,
				       (int)(file_size_in_blocks - start_sector - end_sector) / BLOCK_SIZE,
				       false);
		}

		if (!stdout_flag) {
			if (fdatasync(streamout) < 0)
				die("\n[Error] error writing to %s \n"
				       "[Error] error: %s\n",
				       name,
				       strerror(errno)
				);
			progressUpdate(starttime,
				       (int)offset / BLOCK_SIZE,
				       (int)(file_size_in_blocks - start_sector - end_sector) / BLOCK_SIZE,
				       true);

			close(streamout);

			if (verbosity_level >= 1) {
				printe("[Info] max_filesize_in_blocks %8.0f \n", (float)max_filesize_in_blocks);
				printe("[Info] offset at the end %8.0f \n",      (float)offset);
				printe("[Info] file_size_in_blocks %8.0f \n",    (float)file_size_in_blocks);
			}

			/* now lets see whats the size of this file in bytes */
			struct stat fileinfo;
			stat(name, &fileinfo);
			disk_vob_size += (off_t)fileinfo.st_size;

			if (large_file_flag && !cut_flag) {
				if ((vob_size - disk_vob_size) < MAX_DIFFER)
					rename_partial(name);
				else {
					printe("\n[Error] File size (%.0f) of %s differs largely from that on dvd,"
					       " therefore keeps it's .partial\n",
					       (float)fileinfo.st_size,
					       name);
				}
			} else if (!cut_flag)
				rename_partial(name);
			else if (cut_flag)
				rename_partial(name);

			if (verbosity_level >= 1) {
				printe("[Info] Single file size (of copied file %s ) %.0f\n", name, (float)fileinfo.st_size);
				printe("[Info] Cumulated size %.0f\n", (float)disk_vob_size);
			}
		}
		max_filesize_in_blocks_summed += max_filesize_in_blocks;
		printe("\n[Info] Successfully copied file %s\n", name);

		num_of_files++; /* # of seperate files we have written */
	}
	/*end of main copy loop */

	if (verbosity_level >= 1)
		printe("[Info] # of separate files: %i\n", num_of_files);



	/******************
	 * Print the status
	 */
	printe("\n[Info] Copying finished! Let's see if the sizes match (roughly)\n\n");
	printe("\n[Info] Difference in bytes: %lld\n", (vob_size - disk_vob_size)); /*REMOVE, TESTING ONLY*/
	printe(  "[Info] Combined size of title-vobs: %.0f (%.0f MB)\n", (float)vob_size, (float)vob_size / (float)MEGA);
	printe(  "[Info] Copied size (size on disk):  %.0f (%.0f MB)\n", (float)disk_vob_size, (float)disk_vob_size / (float)MEGA);

	if ((vob_size - disk_vob_size) > MAX_DIFFER) {
		printe("[Error] Hmm, the sizes differ by more than %d\n", MAX_DIFFER);
		printe("[Hint] Take a look with MPlayer if the output is ok\n");
		goto cleanup;
	}

	printe("[Info] Everything seems to be fine, the sizes match pretty good\n");
	printe("[Hint] Have a lot of fun!\n");

	/*********************************
	 * clean up and close everything *
	 *********************************/

	cleanup:
		free(pwd);
		free(name);
		free(onefile);
		free(bufferin);
		free(dvd_path);
		free(vobcopy_call);
		free(logfile_path);
		free(provided_input_dir);
		free(provided_output_dir);

		for (i = 0; i < 4; i++)
			free(alternate_output_dir[i]);
		free(alternate_output_dir);

		if (dvd_file)
			DVDCloseFile(dvd_file);
		if (dvd)
			DVDClose(dvd);

	return 0;
}


/*
 * if you symlinked a dir to some other place the path name might not get
 * ended by a slash after the first tab press, therefore here is a / added
 * if necessary
 */

int add_end_slash(char *path)
{				/* add a trailing '/' to path */
	char *pointer;

	if (path[strlen(path) - 1] != '/') {
		 pointer = path + strlen(path);
		*pointer = '/';
		 pointer++;
		*pointer = '\0';
	}

	return 0;
}

/*
 * this function concatenates the given information into a path name
 */

int make_output_path(char *pwd, char *name, int get_dvd_name_return,
		     char *dvd_name, int titleid, int partcount)
{
	char temp[5];
	safestrncpy(name, pwd, MAX_PATH_LEN);
	strncat(name, dvd_name, MAX_PATH_LEN);

	sprintf(temp, "%d", titleid);
	strcat(name, temp);
	if (partcount >= 0) {
		strcat(name, "-");
		sprintf(temp, "%d", partcount);
		strcat(name, temp);
	}
	strcat(name, ".vob");

	printe("\n[Info] Outputting to %s\n", name);
	return 0;
}

/*
 *The usage function
 */

void usage(char *program_name)
{
	printe( "Vobcopy " VERSION " - GPL Copyright (c) 2001 - 2009 robos@muon.de\n"
		"\nUsage: %s \n"
		"if you want the main feature (title with most chapters) you don't need _any_ options!\n"
		"Options:\n"
#if defined( HAS_LARGEFILE ) || defined ( MAC_LARGEFILE )
		"[-l (large-file support for files > 2GB)] \n"
#endif
		"[-m (mirror the whole dvd)] \n"
		"[-M (Main title - i.e. the longest (playing time) title on the dvd)] \n"
		"[-i /path/to/the/mounted/dvd/]\n"
		"[-n title-number] \n"
		"[-t <your name for the dvd>] \n"
		"[-o /path/to/output-dir/ (can be \"stdout\" or \"-\")] \n"
		"[-f (force output)]\n"
		"[-V (version)]\n"
		"[-v (verbose)]\n"
		"[-v -v (create log-file)]\n"
		"[-h (this here ;-)] \n"
		"[-I (infos about title, chapters and angles on the dvd)]\n"
		"[-1 /path/to/second/output/dir/] [-2 /.../third/..] [-3 /../] [-4 /../]\n"
		"[-b <skip-size-at-beginning[bkmg]>] \n"
		"[-e <skip-size-at-end[bkmg]>]\n"
		"[-O <single_file_name1,single_file_name2, ...>] \n"
		"[-q (quiet)]\n"
		"[-w <watchdog-minutes>]\n"
		"[-x (overwrite all)]\n"
		"[-F <fast-factor:1..64>]\n",
		program_name
	);
}

/* from play_title */
void play_title(void)
{

}
/**
 * Returns true if the pack is a NAV pack.  This check is clearly insufficient,
 * and sometimes we incorrectly think that valid other packs are NAV packs.  I
 * need to make this stronger.
 */
int is_nav_pack(unsigned char *buffer)
{
	return (buffer[41] == 0xbf && buffer[1027] == 0xbf);
}

/*
* Check the time determine whether a new progress line should be output (once per second)
*/

int progressUpdate(int starttime, int cur, int tot, int force)
{
	static int progress_time = 0;

	if (progress_time == 0 || progress_time != time(NULL) || force) {
		int barLen, barChar, numChars, timeSoFar, minsLeft, secsLeft,
		    ctr, cols;
		float percentComplete, percentLeft, timePerPercent;
		int curtime, timeLeft;
		struct winsize ws;

		ioctl(0, TIOCGWINSZ, &ws);
		cols = ws.ws_col - 1;

		progress_time = time(NULL);
		curtime = time(NULL);

		printf("\r");
		/*calc it this way so it's easy to change later */
		/*2 for brackets, 1 for space, 5 for percent complete, 1 for space, 6 for time left, 1 for space, 1 for 100.0%*/
		barLen = cols - 2 - 1 - 5 - 1 - 6 - 1 - 1;
		barChar = tot / barLen;
		percentComplete = (float)((float)cur / (float)tot * 100.0);
		percentLeft = 100 - percentComplete;
		numChars = cur / barChar;

		/*guess remaining time */
		timeSoFar = curtime - starttime;
		if (percentComplete == 0)
			timePerPercent = 0;
		else
			timePerPercent = (float)(timeSoFar / percentComplete);

		timeLeft = timePerPercent * percentLeft;
		minsLeft = (int)(timeLeft / 60);
		secsLeft = (int)(timeLeft % 60);

		printf("[");
		for (ctr = 0; ctr < numChars - 1; ctr++) {
			printf("=");
		}
		printf("|");
		for (ctr = numChars; ctr < barLen; ctr++) {
			printf(" ");
		}
		printf("] ");
		printf("%6.1f%% %02d:%02d ", percentComplete, minsLeft, secsLeft);
		fflush(stdout);
	}

	return (0);
}

void set_signal_handlers(void)
{
	struct sigaction action;

	/*SIGALRM or watchdog handler*/
	action.sa_flags = 0;
	action.sa_handler = watchdog_handler;
	sigemptyset(&action.sa_mask);
	sigaction(SIGALRM, &action, NULL);

	/*SIGTERM or ctrl+c handler*/
	action.sa_flags = 0;
	action.sa_handler = shutdown_handler;
	sigemptyset(&action.sa_mask);
	sigaction(SIGTERM, &action, NULL);
}

void watchdog_handler(int signal)
{
	printe("\n[Info] Timer expired - terminating program.\n");
	kill(getpid(), SIGTERM);
}

void shutdown_handler(int signal)
{
	printe("\n[Info] Terminate signal received, exiting.\n");
	_exit(2);
}

#if defined(__APPLE__) && defined(__GNUC__) || defined(OpenBSD)
int fdatasync(int value)
{
	return 0;
}
#endif

/*
* Check the time determine whether a new progress line should be output (once per second)
*/

int check_progress(void)
{
	static int progress_time = 0;

	if (!progress_time || progress_time != time(NULL)) {
		progress_time = time(NULL);
		return 1;
	}

	return 0;
}
