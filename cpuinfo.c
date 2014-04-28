/*
 * cpuinfo.c - CPU information monitor for XFCE genmon plugin.
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
static char *prog = "cpuinfo";
static char *vers = "1.0.2";

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Option parsing */
static int cpuusage = 0;
static int debug = 0;
static char iconfile[256];
static int pango = 0;
static int showfarenheit = 0;
static int showicon = 1;

/* Pango colors */
char *coldefault = "default", *yellow = "yellow", *orange = "orange", *red = "red";

static void
show_version (void)
{
	printf ("%s %s - (C) 2013 Digirium, see <https://github.com/Digirium>\n", prog, vers);
	printf ("Released under the GNU GPL.\n\n");
}

static void
show_help (void)
{
	show_version ();
	
	printf ("-c --cpuusage		Display CPU core usage.\n");
	printf ("-d --debug		Display debugging output.\n");
	printf ("-F --farenheit		Display temperature in farenheit.\n");
	printf ("-h --help		Display this help.\n");
	printf ("-i[FILE] --icon[=FILE]	Set the icon filename, or disable the icon.\n");
	printf ("-p --pango		Generate Pango Markup Language output.\n");
	printf ("-v --version		Display version information.\n");

	printf ("\nLong options may be passed with a single dash.\n\n");
}

static void
get_options (int argc, char *argv[])
{
	char *home = getenv ("HOME");
	assert (home != NULL);

	sprintf (iconfile, "%s/.genmon-icon/%s.png", home, prog);

	if (argc == 1) return;

	static struct option long_opts[] =
	{
		{ "cpuusage",	no_argument,		0, 'c' },
		{ "debug",	no_argument,		0, 'd' },
		{ "farenheit",	no_argument,		0, 'F' },
		{ "help",	no_argument,		0, 'h' },
		{ "icon",	optional_argument,	0, 'i' },
		{ "pango",	no_argument,		0, 'p' },
		{ "version",	no_argument,		0, 'v' },
		{ 0,0,0,0 }
	};

	int opt, opti;

	while ((opt = getopt_long (argc, argv, "cdfhi::pv", long_opts, &opti)))
	{
		if (opt == EOF) break;

		switch (opt)
		{
		case 'c':
			cpuusage = 1;
			break;

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
			/* Enabling Pango Markup Language, or Pango Text Markup Language. Using this option
			 * allows the CPU monitor to exploit the markup to color the text displaying CPU temperature
			 * and core utilitization visually in steps for example: yellow, orange and red as
			 * the CPU gets warmer or as the core is more heavily utilized.
			 */
			pango = 1;
			break;

		case 'v':
			show_version ();
			exit (0);

		default:
			exit (1);
		}
	}
}

static char *
p2s (int percent) /* Percent to string */
{
	char *buffer = (char *)malloc (128);

	if (pango)
	{
		if (percent < 80)
			sprintf (buffer, "%2d%%", percent);
		else
		{
			char *color;
			
			if      (percent < 90)	color = yellow;	/* between 80-90 */
			else if (percent < 100)	color = orange; /* between 90-100 */

			if (percent < 100)
				sprintf (buffer, "<span foreground=\"%s\">%2d%%</span>", color, percent);
			else
				sprintf (buffer, "<span foreground=\"%s\">100</span>", red);
		}
	}
	else
	{
		if (percent < 100)	sprintf (buffer, "%2d%%", percent);
		else			strcpy (buffer, "100");
	}

	return buffer;
}

int
main (int argc, char *argv[])
{
	get_options (argc, argv);

	/* Code below was written to support an AMD Phenom(tm) II X4 965 Processor
	 * and was written assuming there are four cores. In the future it would be
	 * desirable to make configuration more flexible. The author hopes this does
	 * not put anyone off adapting the code to the particular CPU that they use.
	 */

	/* Monitor was written for a CPU containing four cores and makes assumptions
	 * about how to get usage statistics, temperature and CPU PWM fan speed. For other
	 * types of CPUs some editing may be required to adapt different configurations.
	 */

	int cpus = sysconf (_SC_NPROCESSORS_CONF);
	assert (cpus == 4 || cpus == 2);

	/* Allocate some storage for the previous total and idle values.
	 */
	int size = sizeof (unsigned long long int) * cpus * 2;
	unsigned long long int *prev = (unsigned long long int *)malloc (size);
	(void)memset (prev, 0, size);

	/* If cache file exists, read the previous total and idle usage statistics
	 * and then read the previous maximum temperature and fan speed.
	 */
	char format[256], buffer[256];
	sprintf (format, "/dev/shm/cpuinfo.%d", getuid ());
	FILE *shm = fopen (format, "r");
	int maxrpm = 0, n, ret;
	float maxtemp = 0.0;

	if (shm)
	{
		for (n = 0; n < cpus; n++)
		{
			fgets (buffer, 256, shm);
			ret = sscanf (buffer, "%llu %llu", prev + 2*n, prev + 2*n + 1);
			assert (ret == 2);
		}

		fgets (buffer, 256, shm);
		ret = sscanf (buffer, "%f %d", &maxtemp, &maxrpm);
		assert (ret == 2);
		fclose (shm);
	}

	/* Prepare to write a new cache. The total and idle statistics are always
	 * increasing so the cache is updated every run.
	 */
	shm = fopen (format, "w");
	assert (shm != NULL);

	enum _PROC { User = 0, Nice, System, Idle, IO, Irq, Soft, Steal, Guest };
	int *percent = (int *)malloc (sizeof (int) * cpus);
	unsigned long long int proc[10];

	/* Prepare to read CPU statistics from the pseudo-filesystem. With the values
	 * saved in the cache, the interval values for total and idle can be calculated
	 * and the CPU usage obtained.
	 */
	FILE *file = fopen ("/proc/stat", "r");
	int i, id = -1;

	for (n = 0; n <= cpus; n++)
	{
		unsigned long long int total = 0;

		fgets (buffer, 256, file);

		if (n == 0) continue;

		ret = sscanf (buffer, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
			&id, proc + 0, proc + 1, proc + 2, proc + 3, proc + 4,
			proc + 5, proc + 6, proc + 7, proc + 8, proc + 9);

		assert (ret == 11);

		/* User + Nice + System + Idle */
		for (i = 0; i < 4; i++) total += proc[i];

		int cpu = n - 1;
		unsigned long long int intervaltotal = total - *(prev + 2*cpu);
		unsigned long long int intervalidle  = proc[Idle] - *(prev + 2*cpu + 1);

		*(percent + cpu) = (int)(100.0 * ((float)(intervaltotal - intervalidle)
			/ (float)intervaltotal));

		fprintf (shm, "%llu %llu\n", total, proc[Idle]);
	}
	fclose (file);

	/* Prepare to get the CPU temperature and PWM fan speed. Going to open
	 * a read pipe to the sensors program to get these values.
	 */
	file = popen ("/usr/bin/sensors", "r");
	assert (file != NULL);

	float temp = 0.0;
	int rpm = 0;

	while (fgets (buffer, 256, file))
	{
		if (cpus == 4)
		{
			switch (buffer[0])
			{
			/* CPU temperature */
			case 't':
				if (strstr (buffer, "temp1:") == buffer)
				{
					ret = sscanf (buffer, "temp1: +%f", &temp);
					assert (ret == 1);
				}

				break;
					
			/* CPU Fan Speed */
			case 'C':
				if (strstr (buffer, "CPU Fan Speed:") == buffer)
				{
					ret = sscanf (buffer, "CPU Fan Speed: %d RPM", &rpm);
					assert (ret == 1);
				}

				break;
			}
		}
		else /* cpus == 2 */
		{
			switch (buffer[0])
			{
			/* CPU temperature */
			case 'C':
				if (strstr (buffer, "Core 0:") == buffer)
				{
					ret = sscanf (buffer, "Core 0: +%f", &temp);
					assert (ret == 1);
				}
			}
		}
	}
	pclose (file);

	/* Write the maximum values seen so far for temperatue and rpm to the
	 * end of the cache.
	 */
	if (temp > maxtemp)	maxtemp = temp;
	if (rpm > maxrpm)	maxrpm = rpm;

	fprintf (shm, "%.1f %d\n", maxtemp, maxrpm);
	fclose (shm);

	/* Recalculate temperatures as farenheit */
	char CF = 'C';
	if (showfarenheit)
	{
		CF = 'F';
		temp = (temp * 1.8) + 32.0;
		maxtemp = (maxtemp * 1.8) + 32.0;
	}


	/** XFCE GENMON XML **/

	/* Icon */
	if (showicon) printf ("<img>%s</img>\n", iconfile);

	/* Text */
	char tempbuf[128], rpmbuf[32];	/* Temperature may include a single pango span */
	char line1[512], line2[512];	/* Each line may include up to 3 pango spans */

	if (pango)
	{
		/* By default, apply no color change and use whatever color is the default
		 * for foreground set by the GTK files for XFCE. This might be white or black,
		 * or some shade of white or black, or whatever the user has setup.
		 */
		char *color = coldefault;

		sprintf (buffer, "%3.1f°%c", temp, CF);

		switch (CF)
		{
		case 'C':
			if (cpus == 4)
			{
				if      (temp < 40)	color = coldefault;
				else if (temp < 45)	color = yellow;
				else if (temp < 50)	color = orange;
				else /* temp >= 50 */	color = red;
			}
			else /* cpus == 2 */
			{
				if      (temp < 60)	color = coldefault;
				else if (temp < 70)	color = yellow;
				else if (temp < 80)	color = orange;
				else /* temp >= 80 */	color = red;
			}
			break;

		case 'F':
			if      (temp < 105)	color = coldefault;
			else if (temp < 113)	color = yellow;
			else if (temp < 122)	color = orange;
			else /* temp >= 122 */	color = red;
			break;
		}

		if (strcmp (color, coldefault))
			sprintf (tempbuf, "<span foreground=\"%s\">%8s</span>", color, buffer);
		else
			sprintf (tempbuf, "%8s", buffer); /* use the default foreground */
	}
	else
	{
		sprintf (buffer, "%3.1f°%c", temp, CF);
		sprintf (tempbuf, "%8s", buffer);
	}

	sprintf (rpmbuf, "%-4drpm", rpm);

	if (cpuusage)
	{
		if (cpus == 4)
		{
			sprintf (line1, "%s %s %s", tempbuf, p2s(*(percent + 0)), p2s(*(percent + 1)));
			sprintf (line2, "%7s %s %s", rpmbuf, p2s(*(percent + 2)), p2s(*(percent + 3)));
		}
		else /* cpus == 2 */
		{
			sprintf (line1, "%s", tempbuf);
			sprintf (line2, "%s %s", p2s(*(percent + 0)), p2s(*(percent + 1)));
		}
	}

	if (cpuusage)	printf ("<txt>%s\n%s</txt>\n", line1, line2);
	else		printf ("<txt>%s\n%7s</txt>\n", tempbuf, rpmbuf);
		
	/* Tool tip */
	if (cpus == 4)
	{
		printf ("<tool>Maximum temperature observed: %.1f°%c\n", maxtemp, CF);
		printf ("Maximum RPM observed: %drpm</tool>", maxrpm);
	}
	else /* cpus == 2 */
	{
		printf ("<tool>Maximum temperature observed: %.1f°%c</tool>", maxtemp, CF);
	}

	return 0;
}
