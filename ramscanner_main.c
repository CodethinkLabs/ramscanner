#include <stdlib.h>
#include <string.h>

#include "ramscanner_common.h"
#include "ramscanner_collect.h"
#include "ramscanner_display.h"

/**
 * Sets up the signal handler.
 * Calls functions to turn the arguments into PIDs, flags and filenames. 
 * Stops the PIDs that are going to be inspected.
 * Calls the function to inspect the PIDs.
 * Writes in stdout or in the files specified by the arguments. 
 */
int 
main(int argc, char *argv[])
{

	options opt = {0};
	set_signals();

	if (argc < 2){
		printf("Usage: %s [Primary PID] [-s<path>] [-d<path>] "
		       "[-D<path>] [Secondary PIDs]\n"
		       "\t'-s<path>' to write a summary to <path>\n"
		       "\t'-D<path>' to get per-page details\n"
		       "\t'-d<path>' to get compact details\n", argv[0]); 
		exit(EXIT_FAILURE);
	}

	handle_args(argc, argv, &opt);
	PIDs = opt.pids;
	PIDcount = opt.pidcount;
	stop_PIDs(PIDs, PIDcount);

	if (opt.summary) {
		memset(&(opt.summarystats), 0, sizeof(opt.summarystats));
		opt.summarypages = g_hash_table_new_full(g_int64_hash,
		                                         g_int64_equal,
		                                         &destroyval,
		                                         &destroyval);
	}
	if (opt.detail || opt.compactdetail) {
		opt.detailpages = g_hash_table_new_full(g_int64_hash,
		                                         g_int64_equal,
		                                         &destroyval,
		                                         &destroyval);
	}

	if (opt.summary || opt.detail || opt.compactdetail)
		inspect_processes(&opt);
	else
		printf("%s must specify at least one of -s and -d\n", argv[0]);



	if (opt.summary)
		write_summary(&(opt.summarystats), opt.summaryfile);
	if (opt.detail || opt.compactdetail)
		write_any_detail(&opt);
	if (opt.compactdetailfile != NULL)
		fclose(opt.compactdetailfile);
	if (opt.detailfile != NULL)
		fclose(opt.detailfile);
	if (opt.summaryfile != NULL)
		fclose(opt.summaryfile);


	free(opt.vmas);
	if (opt.summary)
		g_hash_table_destroy(opt.summarypages);
	if (opt.detail || opt.compactdetail)
		g_hash_table_destroy(opt.detailpages);

	cleanup_and_exit(EXIT_SUCCESS);
	/* free(PIDs); This function cannot be called because cleanup_and_exit() 
	 *  must happen before that, and it exits. It cannot be called within
	 *  cleanup_and_exit because free() isn't async-signal safe.
	 */
	exit(EXIT_SUCCESS); /* This should never happen, cleanup_and_exit exits.
	                     */
}
