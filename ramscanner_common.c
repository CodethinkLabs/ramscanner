#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ramscanner_common.h"


pid_t *PIDs = NULL;
uint16_t PIDcount = 0;

/**
 * Allocates, sets with value "key" and returns a void pointer for the hash 
 * table. This function is used with a 54-bit integer, the PFN
 */
void * 
newkey(uint64_t key)
{
	uint64_t *temp = malloc(sizeof(*temp));
	*temp = key;
	return temp;
}

void
destroyval(void *val)
{
	free(val);
}


/**
 * Signal handler for abnormal termination. Tries its best to restart its 
 * stopped processes, but cannot guarantee total signal safety.
 */
void 
cleanup_and_exit(int signal)
{
	int i = 0;
	for (i=0; i< PIDcount; i++)
		kill(PIDs[i], SIGCONT); /* Clears the queue of STOP signals */
	if (signal != EXIT_SUCCESS && signal != EXIT_FAILURE)
		_exit(EXIT_FAILURE);
	else
		_exit(signal);
}

/**
 * Tries to interpret a string as a number, and if it succeeds returns the PID
 * Handles the special case of specifying the PID 0 or PID 1, which are  
 * forbidden, and checks if the PID specified corresponds to an existing 
 * process. 
 */
pid_t
try_to_read_PID(const char *arg)
{

	char *endpt = NULL;
	pid_t temp = strtoul(arg, &endpt, 0);
	if (endpt == arg) { /* Did not read a number*/
		return 0;
	} else if (temp == 0) {
		fprintf(stderr, "Error: cannot pass PID 0. "
			"Ignoring argument\n");
		return 0;
	} else {
		kill(temp, 0);
		if (errno){
			errno = 0;
			return 0;
		} else {
			return temp;
		}
	}
}

void 
add_pid_to_array(pid_t pid, pid_t **pids, uint16_t *pidcount)
{
	pid_t *temp;
	errno = 0;
	temp = realloc(*pids, sizeof(*temp) * ((*pidcount) + 1));
	if (temp == NULL) {
		perror("Error allocating memory for PID array");
		exit(EXIT_FAILURE);
	}
	*pids = temp;
	(*pids)[*pidcount] = pid;
	*pidcount += 1;
}

/**
 * Open file with the name passed in the argument, and return the File* 
 * associated.
 */
FILE*
open_arg(const char *arg)
{
	if (strcmp(arg, "-") == 0) {
		return stdout;
	} else {
		FILE *ret = NULL;
		errno = 0;
		ret = fopen(arg, "w");
		if (ret == NULL) {
			perror("Error opening file");
			exit(EXIT_FAILURE);
		}
		return ret;
	}
}

/**
 * Parses the arguments given to the program. It ignores invalid arguments
 * instead of failing. FILE pointers and PID arrays are set in the functions
 * called, so are passed as pointers to pointers
 */
void 
handle_args(int argc, char *argv[], options *opt)
{
	char *optstr = "-s::d::D::";
	int o;
	pid_t pid;
	while ((o = getopt(argc, argv, optstr)) != -1) {
		switch(o) {
		case 1:
			pid = try_to_read_PID(argv[optind - 1]);
			if (pid != 0)
				add_pid_to_array(pid, &(opt->pids),
				                 &(opt->pidcount));
			else
				fprintf(stderr, "Warning: Received unexpected "
				        "argument: %s. Ignoring.\n",
				        argv[optind - 1]);
			break;
		case 's':
			opt->summary = 1;
			if(optarg)
				opt->summaryfile = open_arg(optarg);
			else
				opt->summaryfile = stdout;
			break;
		case 'd':
			opt->compactdetail = 1;
			if(optarg)
				opt->compactdetailfile = open_arg(optarg);
			else
				opt->compactdetailfile = stdout;
			break;
		case 'D':
			opt->detail = 1;
			if(optarg)
				opt->detailfile = open_arg(optarg);
			else
				opt->detailfile = stdout;
			break;
		default:
			printf("Received an unexpected option\n");
		}
	}
}

void
set_signals()
{
/*
 * Set handlers for signals sent, to ensure processes get re-started
 * on early terminate
 */
	struct sigaction sa;
		sa.sa_handler = cleanup_and_exit;
	if (sigaction(SIGTERM, &sa, NULL) == -1){
		fprintf(stderr, "Error: Failed to set handler"
			" to SIGTERM.");
		exit(EXIT_FAILURE);
	}
	if (sigaction(SIGINT, &sa, NULL) == -1){
		fprintf(stderr, "Error: Failed to set handler"
			" to SIGINT.");
		exit(EXIT_FAILURE);
	}
	if (sigaction(SIGSEGV, &sa, NULL) == -1){
		fprintf(stderr, "Error: Failed to set handler"
			" to SIGSEGV.");
		exit(EXIT_FAILURE);
	}

}

void
stop_PIDs(const pid_t *pids, uint16_t count)
{
	int i;
	for (i = 0; i < count; i++) {
		int ret;
		errno = 0;
		ret = kill(pids[i], SIGSTOP);/* SIGSTOP overrides signal
		                              * handling, consider SIGSTP
		                              */
		if (ret == -1) {
			perror("Error stopping PID");
			cleanup_and_exit(EXIT_FAILURE);
		}
	}
}

