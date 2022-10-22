/*
 *      schedstat.c
 *
 *      schedstat measures the scheduling latency of a particular process from
 *	the extra information provided in /proc/<pid>stat. 
 *
 *	PLEASE NOTE: This program does NOT check to make sure that extra 
 *	information is there; it assumes the last three fields in that line 
 *	are the ones it's interested in.  
 *
 *	This currently monitors only one pid at a time but could easily
 *	be modified to do more.
 *
 *	Initial code  
 *	http://eaglet.pdxhosts.com/rick/linux/schedstat/v12/latency.c
 *	copyright Rick Lindsey 2004.
 */
#include <time.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

char procbuf[512];
char statname[64];
char datebuf[20];
char *Progname;
FILE *fp;

void usage()
{
    fprintf(stderr,"Usage: %s [-s sleeptime ] [-v] <pid>\n", Progname);
    exit(-1);
}

/*
 * get_stats() -- we presume that we are interested in the first three
 *	fields of the line we are handed, and further, that they contain
 *	only numbers and single spaces.
 */
void get_stats(char *buf, unsigned long *run_time, unsigned long *wait_time,
    unsigned long *nran)
{
    char *ptr;

    /* sanity */
    if (!buf || !run_time || !wait_time || !nran)
	return;

    /* leading blanks */
    ptr = buf;
    while (*ptr && isblank(*ptr))
	ptr++;

    /* first number -- run_time */
    *run_time = atol(ptr);
    while (*ptr && isdigit(*ptr))
	ptr++;
    while (*ptr && isblank(*ptr))
	ptr++;

    /* second number -- wait_time */
    *wait_time = atol(ptr);
    while (*ptr && isdigit(*ptr))
	ptr++;
    while (*ptr && isblank(*ptr))
	ptr++;

    /* last number -- nran */
    *nran = atol(ptr);
}

/*
 * get_id() -- extract the id field from that /proc/<pid>/stat file
 */
void get_id(char *buf, char *id)

{
    char *ptr;

    /* sanity */
    if (!buf || !id)
	return;

    ptr = index(buf, ')') + 1;
    *ptr = 0;
    strcpy(id, buf);
    *ptr = ' ';

    return;
}

/*
 * get_datetime
 */

void get_datetime(char *buf) 
{
	time_t ltime;
	struct tm *ldt;

	ltime = time(NULL);
	ldt = localtime(&ltime);
	sprintf(buf, "%02d:%02d:%02d", ldt->tm_hour, ldt->tm_min, ldt->tm_sec);
}

int main(int argc, char *argv[])
{
    int c;
    unsigned int sleeptime = 5, pid = 0, verbose = 0;
    char id[32];
    unsigned long run_time, wait_time, nran;
    unsigned long orun_time=0, owait_time=0, oran=0;

    Progname = argv[0];
    id[0] = 0;
    while ((c = getopt(argc,argv,"s:hv")) != -1) {
	switch (c) {
	    case 's':
		sleeptime = atoi(optarg);
		break;
	    case 'v':
		verbose++;
		break;
	    default:
		usage();
	}
    }

    if (optind < argc) {
	pid = atoi(argv[optind]);
    }

    if (!pid)
	usage();

    sprintf(statname,"/proc/%d/stat", pid);
    if ((fp = fopen(statname, "r")) != NULL) {
	if (fgets(procbuf, sizeof(procbuf), fp))
	    get_id(procbuf,id);
	fclose(fp);
    }

    /*
     * now just spin collecting the stats
     */
    sprintf(statname,"/proc/%d/schedstat", pid);
    while ((fp = fopen(statname, "r")) != NULL) {
	    if (!fgets(procbuf, sizeof(procbuf), fp))
		    break;

	    get_stats(procbuf, &run_time, &wait_time, &nran);
	    get_datetime(datebuf);

	    if (verbose)
		printf("%s %s %ld(%ld) %ld(%ld) %ld(%ld) %ld %ld\n",
		    datebuf,
		    id, run_time, run_time - orun_time,
		    wait_time, wait_time - owait_time,
		    nran, nran - oran,
			(run_time-orun_time) ,
			(wait_time-owait_time));
	    else
		printf("%s %s run=%ldns wait=%ldns\n",
		    datebuf,
		    id, 
		    (run_time-orun_time), 
		    (wait_time-owait_time));
	    fclose(fp);
	    oran = nran;
	    orun_time = run_time;
	    owait_time = wait_time;
	    sleep(sleeptime);
	    fp = fopen(statname,"r");
	    if (!fp)
		    break;
    }
    if (id[0])
	printf("Process %s has exited.\n", id);
    else 
	printf("Process %d does not exist.\n", pid);
    exit(0);
}
