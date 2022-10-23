/*
 *      schedstat.c
 *
 *      schedstat measures the scheduling latency of a particular process from
 *	the information provided in /proc/<pid>/schedstat. 
 *
 *	Initial code  
 *	http://eaglet.pdxhosts.com/rick/linux/schedstat/v12/latency.c
 *	copyright Rick Lindsey 2004.
 *	Modified code
 *	copyright Pierre Forstmann 2022.
 */
#include <time.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define PROCBUF_MAX_LENGTH	512
#define STATNAME_MAX_LENGTH 	64	
#define DATEBUF_MAX_LENGTH	20
#define MAX_PROCS		64

typedef struct piddata {
	int pid;
	int ok;
	unsigned long run_time;
	unsigned long wait_time;
	unsigned long old_run_time;
	unsigned long old_wait_time;

} piddata;

char procbuf[PROCBUF_MAX_LENGTH];
char statname[STATNAME_MAX_LENGTH];
char datebuf[DATEBUF_MAX_LENGTH];
piddata  pidtab[MAX_PROCS];
char *Progname;
FILE *fp;

void usage()
{
    fprintf(stderr,"Usage: %s [-s sleeptime ] [-v] -p <pid,pid,...>\n", Progname);
    exit(-1);
}

/*
 * get_stats() -- we presume that we are interested in the first three
 *	fields of the line we are handed, and further, that they contain
 *	only numbers and single spaces.
 */
void get_stats(char *buf, unsigned long *run_time, unsigned long *wait_time)
{
    char *ptr;

    /* sanity */
    if (!buf || !run_time || !wait_time)
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

}

/*
 * get_datetime -- get current date and time
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
    int c, i, pid_processed_count = 0;
    unsigned int sleeptime = 1, verbose = 0;
    char *pidlist, *ptr;
    int pidcount;

    Progname = argv[0];
    pidlist = NULL;
    while ((c = getopt(argc,argv,"p:s:hv")) != -1) {
	switch (c) {
	    case 's':
		sleeptime = atoi(optarg);
		break;
	    case 'v':
		verbose++;
		break;
	    case 'p':
		pidlist = optarg;
		break;
	    default:
		usage();
	}
    }

    if (optind != argc) {
	    usage();
    }

    if (pidlist == NULL) {
	    usage();
    }
     
    ptr = pidlist; 
    i = 0;
    while ( i < MAX_PROCS && ptr < pidlist + strlen(pidlist)) {
        while (*ptr && *ptr == ',') 
            ptr++;
	if (!isdigit(*ptr)) {
		usage();
	}
	pidtab[i].pid = atoi(ptr);
	pidtab[i].ok = 0;
	pidtab[i].run_time = 0;
	pidtab[i].wait_time = 0;
	pidtab[i].old_run_time = 0;
	pidtab[i].old_wait_time = 0;
	i++;
	if ( i == MAX_PROCS) {
		printf("Too many pid specified (max. is %d) \n", MAX_PROCS);
		exit(-1);
	}
        while (*ptr && isdigit(*ptr))
            ptr++;
    }
    pidcount = i;

    for (i = 0 ; i < pidcount ; i++) {
        sprintf(statname,"/proc/%d/stat", pidtab[i].pid);
        if ((fp = fopen(statname, "r")) != NULL) {
       	    printf("pid %d OK \n", pidtab[i].pid);
	    pidtab[i].ok = 1;
	    fclose(fp);
            }
	else {
       	    printf("pid %d does not exist \n", pidtab[i].pid);
	    exit(-1);
	}
    }

    /*
     * now just spin collecting the stats
     */
    while (1) {
      pid_processed_count = 0; 
      for (i = 0 ; i < pidcount ; i++) {
	if (pidtab[i].ok == 0)
		continue;
        sprintf(statname,"/proc/%d/schedstat", pidtab[i].pid);
        if ((fp = fopen(statname, "r")) != NULL) {
	        if (!fgets(procbuf, sizeof(procbuf), fp)) {
	            pidtab[i].ok = 0;
  	            printf("pid %d has exited \n", pidtab[i].pid);
		    continue;
		}

		pid_processed_count++;
	        get_stats(procbuf, &pidtab[i].run_time, &pidtab[i].wait_time);
	        get_datetime(datebuf);

	        if (verbose)
		     printf("%s %d %ld %ld \n",
		         datebuf,
		         pidtab[i].pid, pidtab[i].run_time, pidtab[i].wait_time);
	        else
		    printf("%s %d run=%ldns wait=%ldns\n",
		        datebuf,
		        pidtab[i].pid,
		       (pidtab[i].run_time - pidtab[i].old_run_time), 
		       (pidtab[i].wait_time - pidtab[i].old_wait_time));
	        fclose(fp);
	        pidtab[i].old_run_time = pidtab[i].run_time;
	        pidtab[i].old_wait_time = pidtab[i].wait_time;
	        sleep(sleeptime);
        }
	else {
	    pidtab[i].ok = 0;
	    printf("pid %d has exited \n", pidtab[i].pid);
      }
    }
    if (pid_processed_count == 0) {
	    printf("all processes have exited.\n") ;
	    exit(0);
    }
  }
  exit(0);
}
