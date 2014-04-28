/*
 * diskinfo.c - Disk information monitor for XFCE genmon plugin.
 * Copyright (C) 2013 Digirium, see <https://github.com/Digirium/>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
static char *prog = "diskinfo";
static char *vers = "1.0.3";

#include <assert.h>
#include <getopt.h>
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statfs.h>
#include <sys/stat.h>
#include <unistd.h>

/* Option parsing */
static int debug = 0;
static char iconfile[256];
static char *mountpath = NULL;
static char *hddtemppath = NULL;
static int showbar = 0;
static int showfarenheit = 0;
static int showicon = 1;

static void
show_version (void)
{
	printf ("%s %s - (C) 2013 Digirium, see <https://github.com/Digirium>\n", prog, vers);
	printf ("Released under the GNU GPL.\n\n");
}

static void
show_usage (void)
{
	printf ("Usage: %s [options] <mountpath>\n", prog);
}

static void
show_help (void)
{
	show_version ();
	show_usage ();
	
	printf ("\n-d --debug		Display debugging output.\n");
	printf ("-h --help		Display this help.\n");
	printf ("-F --farenheit		Display temperature in farenheit.\n");
	printf ("-i[FILE] --icon[=FILE]	Set the icon filename, or disable the icon.\n");
	printf ("-p --percentbar		Display the percent bar.\n");
	printf ("-tDISK --disktemp=DISK	Set the disk path to read temperature from.\n");
	printf ("-v --version		Display version information.\n");

	printf ("\nLong options may be passed with a single dash.\n\n");
}

static void
get_options (int argc, char *argv[])
{
	char *home = getenv ("HOME");
	assert (home != NULL);

	sprintf (iconfile, "%s/.genmon-icon/%s.png", home, prog);

	static struct option long_opts[] =
	{
		{ "debug",	no_argument,		0, 'd' },
		{ "disktemp",	required_argument,	0, 't' },
		{ "farenheit",	no_argument,		0, 'F' },
		{ "help",	no_argument,		0, 'h' },
		{ "icon",	optional_argument,	0, 'i' },
		{ "percentbar",	no_argument,		0, 'p' },
		{ "version",	no_argument,		0, 'v' },
		{ 0,0,0,0 }
	};

	int opt, opti;

	while ((opt = getopt_long (argc, argv, "dFhi::pt:v", long_opts, &opti)))
	{
		if (opt == EOF) break;

		switch (opt)
		{
		case 'd':
			debug = 1;
			break;

		case 'F':
			showfarenheit = 1;
			break;

		case 'h':
			show_help ();
			exit (0);

		case 'i':
			if (!optarg)
			{
				showicon = 0;
				break;
			}

			if (*optarg == '/')
				strcpy (iconfile, optarg);
			else
				sprintf (iconfile, "%s/.genmon-icon/%s", home, optarg);

			break;

		case 'p':
			showbar = 1;
			break;

		case 't':
			hddtemppath = optarg;
			break;

		case 'v':
			show_version ();
			exit (0);

		default:
			exit (1);
		}
	}

	if (optind >= argc)
	{
		show_usage ();
		exit (1);
	}

	mountpath = argv[optind];
}

static char *
du (float usage)
{
	char *buf = (char *)malloc (32);

	if (usage > 100.0)	sprintf (buf, "%d", (int)usage);
	else			sprintf (buf, "%.1f", usage);
		
	return buf;
}

int
main (int argc, char *argv[])
{
	get_options (argc, argv);

	struct statfs fsbuf;
	float disktotal, diskfree, diskused;
	int diskpercent;

	if (statfs (mountpath, &fsbuf) < 0) exit (2);

	float mbratio = ((float)fsbuf.f_bsize) / 1048576.0;

	disktotal	= (fsbuf.f_blocks * mbratio) / 1024.0;
	diskfree	= (fsbuf.f_bfree * mbratio) / 1024.0;
	diskused	= disktotal - diskfree;

	diskpercent = (100 * (fsbuf.f_blocks - fsbuf.f_bfree)) / fsbuf.f_blocks;

	/* For hddtemp, the mount path needs to be looked up in a mount table to find
	 * the corresponding device. To speed things up when the monitor is run again, the
	 * device path is available from the cache. Repeated mount table scans avoided.
	 */
	struct stat mountstat;

	if (stat (mountpath, &mountstat) < 0) exit (3);

	char cachepath[256], cachedisk[256], buffer[256], mntbuffer[256];
	float maxdisktemp = 0.0, disktemp = 0.0;
	int cacheupdate = 0, ret;
	struct mntent mountent;
	char *diskpath = NULL;
	FILE *file;

	sprintf (cachepath, "/dev/shm/diskinfo.%d.%d.%d",
		major (mountstat.st_dev), minor (mountstat.st_dev), getuid ());

	/* If the cache exists, read it instead of scanning the mount table. The device
	 * path is added to the cache the first time the monitor is run. If the cache does
	 * not yet exist, then need to scan a mount table to find the device path.
	 */
	if ((file = fopen (cachepath, "r")))
	{
		char cachemount[256];

		fgets (buffer, 256, file);
		ret = sscanf (buffer, "%s %s %f", cachemount, cachedisk, &maxdisktemp);
		assert (ret == 3);
		fclose (file);

		assert (strcmp (mountpath, cachemount) == 0);
		diskpath = cachedisk;
	}
	else	/* Scan a mount table to get the device path */
	{
		struct stat mntstat;

		file = setmntent ("/proc/mounts", "r");
		assert (file != NULL);

		while (getmntent_r (file, &mountent, mntbuffer, 256))
		{
			if (stat (mountent.mnt_dir, &mntstat)) continue;
			
			/* NB: Will looking for an entry beginning with slash always work? */
			if (mountstat.st_dev == mntstat.st_dev && mountent.mnt_fsname[0] == '/')
			{
				diskpath = mountent.mnt_fsname;
				break;
			}
		}

		endmntent (file);
		cacheupdate = 1;
	}
	assert (diskpath != NULL);

	/* Get the current temperature of the disk. Use hddtemp for this because is has a
	 * reference database for most HDDs.  Might be quicker to consult the daemonized version
	 * of hddtemp, will debate that idea for a future modification to this program. However,
	 * because this program is intended to be run only infrequently say every 30 seconds there
	 * is not all that much overhead in piping input from hddtemp. And there are minimal
	 * processes that need to be left running all the time when using a pipe.
	 */
	char hddtemp[256], *ID = NULL;
	sprintf (hddtemp, "sudo hddtemp %s", (hddtemppath ? hddtemppath : diskpath));
	
	if ((file = popen (hddtemp, "r")))
	{
		char *temp;

		fgets (buffer, 256, file);
		pclose (file);

		temp = strstr((ID = strstr(buffer, ": ") + 2), ": ") + 2;
		*(temp - 2) = temp[strlen (temp) - 1] = '\0';
		disktemp = atof (temp);

		if (disktemp > maxdisktemp)
		{
			maxdisktemp = disktemp;
			cacheupdate = 1;
		}
	}
	assert (ID != NULL);

	/* Create a new cache or update the cache if there is new maximum disk
	 * temperature. Avoid unnecessary cache writes.
	 */
	if (cacheupdate)
		if ((file = fopen (cachepath, "w")))
		{
			fprintf (file, "%s %s %f\n", mountpath, diskpath, maxdisktemp);
			fclose (file);
		}

	/* Recalculate temperatures as farenheit */
	char CF = 'C';
	if (showfarenheit)
	{
		CF = 'F';
		disktemp = (disktemp * 1.8) + 32.0;
		maxdisktemp = (maxdisktemp * 1.8) + 32.0;
	}

	/** XFCE GENMON XML **/

	/* Icon */
	printf ("<img>%s</img>\n", iconfile);

	/* Text */
	printf ("<txt>%d°%c\n%sG</txt>\n", (int)disktemp, CF, du(diskused));

	/* Tool tip */
	printf ("<tool>ID: %s\n", ID);
	printf ("Mount: %s  Device: %s\n", mountpath, diskpath);

	printf ("Total: %.2fG  Available: %.2fG  Used: %.2fG (%d%%)\n",
		disktotal, diskfree, diskused, diskpercent);

	printf ("Maximum temperature observed: %d°%c</tool>\n", (int)maxdisktemp, CF);

	/* Percent bar */
	if (showbar) printf ("<bar>%d</bar>\n", diskpercent);

	return 0;
}
