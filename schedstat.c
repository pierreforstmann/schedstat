/*
 *      schedstat.c
 *
 *      schedstat measures the scheduling latency of some processes from
 *	the information provided in /proc/<pid>/schedstat. 
 *
 *      This program is open source, licensed under the GPL.
 *      For license terms, see the LICENSE file.
 *
 *	Original code:
 *	http://eaglet.pdxhosts.com/rick/linux/schedstat/v12/latency.c
 *	copyright Rick Lindsey 2004.
 *
 *	Modified code:
 *	copyright Pierre Forstmann 2022, 2023.
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

typedef struct data {
	int pid;
	int ok;
	unsigned long run_time;
	unsigned long wait_time;
	unsigned long old_run_time;
	unsigned long old_wait_time;

} data;

void usage(char *progname)
{
    fprintf(stderr,"Usage: %s [-a ] [-s sleeptime ] [-v] -p <pid,pid,...>\n", progname);
    fprintf(stderr,"use -a to print only all CPU stats. Do not use -a with -p.\n");
    exit(-1);
}

/*
 * get_pid_stats() -- we presume that we are interested in the first three
 *	fields of the line we are handed, and further, that they contain
 *	only numbers and single spaces.
 */
void get_pid_stats(char *buf, unsigned long *run_time, unsigned long *wait_time)
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
 * get_cpu_stats() -- we presume that we are interested in the 7th and 8th
 *	fields of the line we are handed, and further, that they contain
 *	only a leading string followed by numbers and single spaces.
 */

void get_cpu_stats(unsigned long *total_run_time, unsigned long *total_wait_time)
{
    char procbuf[PROCBUF_MAX_LENGTH];
    FILE *fp;
    char *ptr;
    int i;
    int done = 0;

    unsigned long run_time;
    unsigned long wait_time;

    *total_run_time = 0;
    *total_wait_time = 0;

    /* sanity */
    if (!total_run_time || !total_wait_time)
        return;

    if ((fp = fopen("/proc/schedstat", "r")) == NULL) {
	printf("Cannot open /proc/schedstat");
	exit(1);
    }
 
    while (done == 0)
    {
        if (fgets(procbuf, sizeof(procbuf), fp) == NULL) {
	    done = 1;
	    break;
	}

	if (strstr(procbuf, "version") != NULL)
            continue;

	if (strstr(procbuf, "timestamp") != NULL)
            continue;

	if (strstr(procbuf, "domain") != NULL)
            continue;

	if (strstr(procbuf, "cpu") != NULL) {

          ptr = procbuf;
          while (*ptr && (isdigit(*ptr) || isalpha(*ptr)))
            ptr++;
          for (i = 1; i <= 6 ; i++)
          {
            while (*ptr && isblank(*ptr))
              ptr++;
            while (*ptr && isdigit(*ptr))
              ptr++;
          }

          /* 7th number -- run_time */
          while (*ptr && isblank(*ptr))
            ptr++;
          run_time = atol(ptr);
          while (*ptr && isdigit(*ptr))
            ptr++;

          /* 8th number -- wait_time */
          while (*ptr && isblank(*ptr))
            ptr++;
          wait_time = atol(ptr);
          while (*ptr && isdigit(*ptr))
             ptr++;

	  *total_run_time += run_time;
	  *total_wait_time += wait_time;
	  
	}
    }

    fclose(fp);

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

/*
 * open_statname -- returns FILE pointer on /proc/<pid>/schedstat
 */

FILE *open_statname(data *pidtab, int pid_index)
{
    char statname[STATNAME_MAX_LENGTH];

    sprintf(statname,"/proc/%d/schedstat", pidtab[pid_index].pid);
    return fopen(statname, "r");

        
}

/*
 * init_pidtab -- initialize list of process identifiers
 */

data *init_pidtab(char *progname, char *pidlist, int *pidcount)
{
    char *ptr;
    int i;
    FILE *fp;
    data *pidtab;


    pidtab = (data *)malloc(sizeof(data) * MAX_PROCS);
    if (pidtab == NULL) {
	printf("cannot allocated %ld bytes for pidtab \n", 
		sizeof(data) *MAX_PROCS);
	exit(-1);
    }
    ptr = pidlist; 
    i = 0;
    while ( i < MAX_PROCS && ptr < pidlist + strlen(pidlist)) {
        while (*ptr && *ptr == ',') 
            ptr++;
	if (!isdigit(*ptr)) {
		usage(progname);
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
    *pidcount = i;

    for (i = 0 ; i < *pidcount ; i++) {
        if ((fp = open_statname(pidtab, i)) != NULL) {
       	    printf("pid %d OK \n", pidtab[i].pid);
	    pidtab[i].ok = 1;
	    fclose(fp);
            }
	else {
       	    printf("pid %d does not exist \n", pidtab[i].pid);
	    pidtab[i].ok = 0;
	}
    }

    return pidtab;
}

/*
 * check_args
 */

void check_args(int argc, char *argv[], int *sleeptime, int *verbose, char **pidlist, int *option_a_is_used)
{
    int c;
    char *progname;
    int option_p_is_used = 0; 

    progname = argv[0];
    *pidlist = NULL;
    while ((c = getopt(argc,argv,"p:s:hva")) != -1) {
	switch (c) {
	    case 's':
		*sleeptime = atoi(optarg);
		break;
	    case 'v':
		(*verbose)++;
		break;
	    case 'p':
		option_p_is_used = 1;
		*pidlist = optarg;
		break;
	    case 'a':
		*option_a_is_used = 1;
		break;
	    default:
		usage(progname);
	}
    }

    if (optind != argc) {
	    usage(progname);
    }

    if (*pidlist == NULL && *option_a_is_used == 0) {
	    usage(progname);
    }

    if (*option_a_is_used == 1 &&  option_p_is_used == 1) {
	    usage(progname);
    }

}

/*
 * print_verbose
 */
void print_verbose(data *tab, int index)
{
    char datebuf[DATEBUF_MAX_LENGTH];

    get_datetime(datebuf);
    printf("%s %d run=%ldns wait=%ldns \n",
	     datebuf,
	     tab[index].pid, 
	     tab[index].run_time, 
	     tab[index].wait_time);

}

/*
 * print_delta
 */
void print_delta(data *tab, int index)
{
     char datebuf[DATEBUF_MAX_LENGTH];

     get_datetime(datebuf);
     printf("%s pid=%d run=%ldns wait=%ldns\n",
	     datebuf,
	     tab[index].pid,
	     (tab[index].run_time - tab[index].old_run_time),
	     (tab[index].wait_time - tab[index].old_wait_time));

}

/*
 * print_all_cpu_delta
 */
void print_all_cpu_delta(data *tab)
{
     char datebuf[DATEBUF_MAX_LENGTH];

     get_datetime(datebuf);
     get_cpu_stats(&tab[0].run_time, &tab[0].wait_time);
     printf("%s all cpus run=%ldns wait=%ldns\n",
             datebuf,
            (tab[0].run_time - tab[0].old_run_time),
            (tab[0].wait_time - tab[0].old_wait_time));
     tab[0].old_run_time = tab[0].run_time;
     tab[0].old_wait_time = tab[0].wait_time;
}

int main(int argc, char *argv[])
{
    int i, pid_processed_count = 0;
    int sleeptime = 1;
    int verbose = 0;
    char *pidlist;
    int pidcount = 0;
    FILE *fp;
    char procbuf[PROCBUF_MAX_LENGTH];
    data *pidtab;
    data cpustats;
    int option_a_is_used = 0;

    check_args(argc, argv, &sleeptime, &verbose, &pidlist, &option_a_is_used); 

    if (option_a_is_used == 0) {
    	pidtab = init_pidtab(argv[0], pidlist, &pidcount);
    }
    else {
	pidtab = NULL;
    }
    cpustats.pid = 0;
    cpustats.run_time = 0;
    cpustats.wait_time = 0;
    cpustats.old_run_time = 0;
    cpustats.old_wait_time = 0;

    /*
     * now just spin collecting the stats
     */
    while (1) {
      pid_processed_count = 0; 
      for (i = 0 ; i < pidcount ; i++) {
	if (pidtab[i].ok == 0)
		continue;
        if ((fp = open_statname(pidtab, i)) != NULL) {
	        if (!fgets(procbuf, sizeof(procbuf), fp)) {
	            pidtab[i].ok = 0;
  	            printf("pid %d has exited \n", pidtab[i].pid);
		    continue;
		}

		pid_processed_count++;
	        get_pid_stats(procbuf, &pidtab[i].run_time, &pidtab[i].wait_time);

	        if (verbose)
			print_verbose(pidtab, i);
	        else
			print_delta(pidtab, i);

	        fclose(fp);
	        pidtab[i].old_run_time = pidtab[i].run_time;
	        pidtab[i].old_wait_time = pidtab[i].wait_time;

	        print_all_cpu_delta(&cpustats);
        }
	else {
	    pidtab[i].ok = 0;
	    printf("pid %d has exited \n", pidtab[i].pid);
      }
    }

    sleep(sleeptime);

    if (option_a_is_used == 0 && pid_processed_count == 0) {
         printf("all processes have exited.\n") ;
	 exit(0);
    } else if (option_a_is_used == 1) {
	print_all_cpu_delta(&cpustats);
    }

  }
  exit(0);
}
