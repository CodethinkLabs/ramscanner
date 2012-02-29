#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

#include <linux/limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <glib.h>

#include "ramscanner_literals.h"
/**
 * Bit masks used in pagemap. See documentation/vm/pagemap.txt 
 * for a full list of bits.
 * PAGESHIFT     : The number of bits that need to be shifted down to express the
 *                 pageshift as a number.
 * PAGEPRESENT   : Bit 63 is page present.
 * PAGESWAPPED   : Bit 62 is page swapped.
 * PFNBITS       : Bits 0-54 store the page frame number if not swapped.
 * PAGEBITS      : Number of bits which pageshift is store in.
 * PAGESHIFTBITS : Bits 55-60 page shift (page size = 1 << pageshift).
 */
#define PAGESHIFT     55
#define PAGEPRESENT   (1LL << 63)
#define PAGESWAPPED   (1LL << 62)
#define PFNBITS       ((1LL << 55) - 1) 
#define PAGEBITS      6
#define PAGESHIFTBITS (((1LL << PAGEBITS) - 1) << PAGESHIFT)

/**
 * Bit masks used in kpageflags. See documentation/vm/pagemap.txt for details.
 * PAGEFLAG_LOCKED     : Bit 0 is locked.
 * PAGEFLAG_REFERENCED : Bit 2 is referenced.
 * PAGEFLAG_DIRTY      : Bit 4 is dirty.
 * PAGEFLAG_ANON       : Bit 12 is anonymous.
 * PAGEFLAG_SWAPCACHE  : Bit 13 is swapcache.
 * PAGEFLAG_SWAPBACKED : Bit 14 is swapbacked.
 * PAGEFLAG_KSM        : Bit 21 is KSM - identical memory pages dynamically
 *                                       shared between one or more processes.
 */
#define PAGEFLAG_LOCKED     (1 << 0)
#define PAGEFLAG_REFERENCED (1 << 2)
#define PAGEFLAG_DIRTY 	    (1 << 4)
#define PAGEFLAG_ANON       (1 << 12)
#define PAGEFLAG_SWAPCACHE  (1 << 13)
#define PAGEFLAG_SWAPBACKED (1 << 14)
#define PAGEFLAG_KSM        (1 << 21)



#define KBSIZE 1024 /** 
                     * The number of bytes in a kilobyte. Defined to avoid
                     * magic numbers.
                     */

/**
 * User-made bit masks for setting the flags. These are used if the flag is set
 * in command-line args.
 */
#define FLAG_SUMMARY 1 /* Bit mask for flags bit-field, whether to give summary */
#define FLAG_DETAIL 2  /* as above, for whether to give details per-page */


/**
 * Sizestats holds information about the memory used by the process in
 * different ways.
 */
typedef struct {
	uint32_t vss;    /**< Virtual set size: The sum of memory with  
                          *   addresses in the primary process' memory map.
                          */
	uint32_t rss;    /**< Resident set size: The sum of memory which has 
                          *   pages allocated.
                          */
	uint32_t pss;    /**< Proportional set size: A weighted indicator of 
                          *   how much memory is shared by this process. If a 
                          *   process had 1000 kB only mapped once, and 1000  
                          *   kB mapped twice (itself and another process),  
                          *   then the total would be 1500 kB.
                          */
	uint32_t uss;    /**< Unique set size: The sum of memory which is only
                          *   mapped once by the primary process.
                          */
	uint32_t sss;    /**< Self set size: The sum of memory only mapped 
                          *   within the process, but mapped any number of
                          *   times.
                          */
	uint32_t gss;    /**< Group set size: The sum of memory mapped by the 
                          *   primary process and only shared with the 
                          *   secondary processes.
                          */
	uint32_t refd;   /**< Referenced: The sum of memory in RAM recently
                          *   accessed by any process. This information is  
                          *   used by the LRU.
                          */
	uint32_t swap;   /**< Swapped: The sum of memory that exists in a swap 
                          *   file or partition.
                          */
	uint32_t anon;   /**< Anonymous: The sum of memory that is not 
                          *   associated with a file.
                          */
	uint32_t locked; /**< Locked: The sum of memory that has been locked 
                          *   by a process
                          */
} sizestats;

/**
 * Page data : Information stored about the individual page for summary
 */
typedef struct {
	uint32_t memmapped;  /**< Internal store of how many times this page has
			      *   been mapped by the memory manager (i.e. all
                              *   processes)
                              */
	uint32_t procmapped; /**< Internal store of how many times this page has
			      *   been mapped by the PIDs defined
                              */
} pagesummarydata;

/**
 * Information stored about the current VMA being worked in.
 */
typedef struct {
	char permissions[5]; /**< A 4-character (plus null-terminator) string
                              *   containing the permissions r, w, x and p/s
                              */
	char path[PATH_MAX]; /**< A string containing the path to the file the
                              *   VMA is associated with, or [heap], [stack],
                              *   etc.
                              */
} vmastats;


/**
 * All the information stored about a contiguous region of identical pages.
 */
typedef struct {
	uint32_t addrstart;    /**< The address into the pagemap for the start
	                        *   of this region.
	                        */
	uint32_t addrend;      /**< The address into the pagemap for the end of
	                        *   this region.
	                        */
	uint16_t vmaindex;     /**< Index into options.vmas to find the 
	                        *   associated vmastats.
	                        */
	uint8_t  present;      /**< If the page is the page marked as present by
	                        *   pagemap.
	                        */
	uint8_t pageshift;     /**< The bits to construct the page size by
	                        *   1 << pageshift, as reported by pagemap.
	                        */
	uint8_t  swap;         /**< If the page is the page marked as swapped by
	                        *   pagemap.
	                        */
	uint64_t pfn;          /**< The Page Frame Number used to look up in
	                        *   kpagemap and kpagecount, if it exists.
	                        */
	uint32_t timesmapped;  /**< The number of times the page has been mapped
	                        *   by processes.
	                        */
	uint8_t  locked;       /**< The page has been locked by a process. */
	uint8_t  referenced;   /**< The page has been recently accessed by a
	                        *   process.
	                        */
	uint8_t  dirty;        /**< The page is dirty - it has been modified by
	                        *   a process.
	                        */
	uint8_t  anonymous;    /**< The page is not associated with a file */
	uint8_t  swapcache;    /**< The page is mapped to swap space - it has an
	                        *   associated swap entry.
	                        */
	uint8_t  swapbacked;   /**< The page is backed by swap/RAM */
	uint8_t  ksm;          /**< The page is part of Kernel SamePage Merging.
	                        */
} pagedetaildata;

/**
 * All the information stored about how to run the program.
 */
typedef struct {
	uint8_t summary;           /**< Program will print a summary. */
	uint8_t detail;            /**< Program will print full details. */
	uint8_t compactdetail;     /**< Program will print compact details. */
	FILE *summaryfile;         /**< The file to print summary to. */
	FILE *detailfile;          /**< The file to print details to. */
	FILE *compactdetailfile;   /**< The file to print compact details to. */
	pid_t *pids;               /**< The list of PIDs to work with, first
	                            *   element is the primary PID, all others
                                    *   are secondary PIDs.
                                    */
	uint16_t pidcount;         /**< The number of PIDs */
	GHashTable *summarypages;  /**< Hash table of all the pages used for
	                            *   summary. Stores a pagesummarydata and is
	                            *   indexed by PFN.
	                            */
	GHashTable *detailpages;   /**< Hash table of all the pages used for
	                            *   detail. Stores a pagedetaildata and is
	                            *   indexed by pagemap address.
	                            */
	sizestats summarystats;   /**< sizestats holding information for the
	                            *   summary.
	                            */
	vmastats *vmas;           /**< Array of VMAs, stored here so they can be
	                           *   cleaned up properly.
	                           */
	int vmacount;             /**< A counter of the number of VMAs stored in
	                           *   vmastats.
	                           */
} options;



pid_t *PIDs = NULL; /* Global array of PIDs used, so that they can be restarted
	             * if the program is told to terminate early */
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

void destroyval(void *val)
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

void write_summary(const sizestats *stats, FILE *summary)
{
	fprintf(summary,"Type" DELIMITER "         Size(kB)\n");
	fprintf(summary,SUMMARY_VSS    DELIMITER "          %8u\n",stats->vss);
	fprintf(summary,SUMMARY_RSS    DELIMITER "          %8u\n",stats->rss);
	fprintf(summary,SUMMARY_PSS    DELIMITER "          %8u\n",stats->pss);
	fprintf(summary,SUMMARY_USS    DELIMITER "          %8u\n",stats->uss);
	fprintf(summary,SUMMARY_SSS    DELIMITER "          %8u\n",stats->sss);
	fprintf(summary,SUMMARY_GSS    DELIMITER "          %8u\n",stats->gss);
	fprintf(summary,SUMMARY_REFD   DELIMITER "   %8u\n",stats->refd);
	fprintf(summary,SUMMARY_SWAP   DELIMITER "         %8u\n",stats->swap);
	fprintf(summary,SUMMARY_ANON   DELIMITER "    %8u\n",stats->anon);
	fprintf(summary,SUMMARY_LOCKED DELIMITER "       %8u\n",stats->locked);
		
}

int fill_size_for_smaps_field(char *buff, const char *str, uint32_t *val, 
                              FILE *file)
{
/*
 *Tries to find the string str in buff. If it does, it gets the rest of the line
 * and checks to see if it can find a number to add to val. If it does, returns
 * true
 */
	size_t n = 0;
	int temp;
	if (strstr(buff, str)){
		getline(&buff, &n, file);
		if (sscanf (buff, "%d kB\n", &temp) > 0) {
			*val += temp;
			return 1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}

void parse_smaps_file(FILE *file, sizestats *stats)
{
/*
 * Extracts numerical data from the smaps file. Depends on the format staying as
 * Type:   [val] kB
 * Warning! May get confused by paths which contain text of format Type:[val] kB
 */
	char buffer[BUFSIZ];
	char *pt = buffer;
	size_t n = 0;
	while (getdelim(&pt, &n, ':', file) > 0) {
		if(fill_size_for_smaps_field(pt, SMAPS_VSS, 
		   &(stats->vss), file)) {
		} else if(fill_size_for_smaps_field(pt, SMAPS_PSS, 
		          &(stats->pss), file)) {
		} else if(fill_size_for_smaps_field(pt, SMAPS_RSS, 
		          &(stats->rss), file)) {
		} else if(fill_size_for_smaps_field(pt, SMAPS_REFD, 
		          &(stats->refd), file)) {
		} else if(fill_size_for_smaps_field(pt, SMAPS_ANON, 
		          &(stats->anon), file)) {
		} else if(fill_size_for_smaps_field(pt, SMAPS_SWAP, 
		          &(stats->swap), file)) {
		} else if(fill_size_for_smaps_field(pt, SMAPS_LOCKED, 
		          &(stats->locked), file)) {
		}
	}
}

void countgss(void *key, void *value, void *data)
{
	size_t pagesize = getpagesize();
	pagesummarydata *page = value;
	sizestats *stats = data;
	if (page->memmapped == page->procmapped)
		stats->gss += pagesize/KBSIZE;
}

void countsss(void *key, void *value, void *data)
{
	size_t pagesize = getpagesize();
	pagesummarydata *page = value;
	sizestats *stats = data;
	if (page->memmapped == page->procmapped)
		stats->sss += pagesize/KBSIZE;
}

void lookup_smaps(pid_t PID, sizestats *stats)
{
	char buffer[BUFSIZ];
	FILE *file;
	int ret;
	sprintf(buffer, "%s/%d/%s", PROC_PATH, PID, SMAPS_FILENAME);
	errno = 0;
	file = fopen(buffer, "r");
	if (file == NULL) {
		perror("Error opening smaps file");
		cleanup_and_exit(EXIT_FAILURE);
	}
	parse_smaps_file(file, stats);
	errno = 0;
	ret = fclose(file);
	if (ret != 0) {
		perror("Error closing smaps file");
		errno = 0; /* Continue as usual. It's not worthwhile crashing
		            * because a file failed to close properly.
		            */
	}
	
}

/**
 * Tries to interpret a string as a number, and if it succeeds returns the PID
 * Handles the special case of specifying the PID 0, which is forbidden, and 
 * checks if the PID specified corresponds to an existing process 
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

void set_signals()
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
}

void stop_PIDs(const pid_t *pids, uint16_t count)
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

void
store_flags_in_page(uint64_t bitfield, pagedetaildata *currentdpage)
{
	currentdpage->locked     = (bitfield & PAGEFLAG_LOCKED)     ? 1 : 0;
	currentdpage->referenced = (bitfield & PAGEFLAG_REFERENCED) ? 1 : 0;
	currentdpage->dirty      = (bitfield & PAGEFLAG_DIRTY)      ? 1 : 0;
	currentdpage->anonymous  = (bitfield & PAGEFLAG_ANON)       ? 1 : 0;
	currentdpage->swapcache  = (bitfield & PAGEFLAG_SWAPCACHE)  ? 1 : 0;
	currentdpage->swapbacked = (bitfield & PAGEFLAG_SWAPBACKED) ? 1 : 0;
	currentdpage->ksm        = (bitfield & PAGEFLAG_KSM)        ? 1 : 0;

}

void use_pfn(uint64_t pfn, options *opt, int fdpageflags, int fdpagecount, 
             pagedetaildata *currentdpage)
{
/*
 * Looks up the hash table of pages to see if this page has been mapped before.
 * This is done so that it is possible to compare the number of times the page
 * reports it has been mapped to the number of times the primary or secondary
 * processes map it.
 * Looks up the number of times the page reports it has been mapped in 
 * /proc/kpagecount then compares it to the number of times it has been mapped
 * by primary or secondary processes.
 * If the flag for details has been set, it will look up /proc/kpageflags and
 * retrieve a 64-bit bitfield of the flags set on that process, and pass it to
 * the function print_flags
 */
	int ret;
	uint64_t bitfield;

	size_t elementsize = sizeof(uint64_t);
	uint64_t index = pfn * elementsize;

	pagesummarydata *pData = NULL;
	pData = g_hash_table_lookup(opt->summarypages, &pfn);

	if (pData == NULL) {

	/*
	 * flags containing neither is a special case, as it is impossible to 
	 * reach this function if the arguments contain neither detail or 
	 * summary. It means that the process being interrogated is not the 
	 * primary process, so it is used to calculate Gss by checking if the 
	 * secondary process maps a PFN that the  primary process already has.
	 */

		if (opt->summary || opt->detail || opt->compactdetail) {
			errno = 0;
			pData = malloc(sizeof(*pData));
			if (pData == NULL) {
				perror("Error allocating page summary data");
				cleanup_and_exit(EXIT_FAILURE);
			}
			pData->procmapped = 1;
			pData->memmapped = 0; /*Indicates a newly-mapped page*/
			g_hash_table_insert(opt->summarypages, newkey(pfn), pData);
		}
	} else {
		pData->procmapped += 1;
	}

	if (!(opt->summary || opt->detail || opt->compactdetail))
		return;

	errno = 0;
	ret = lseek(fdpagecount, index, SEEK_SET);
	if (ret == -1) {
		perror("Error seeking in kpagecount");
		cleanup_and_exit(EXIT_FAILURE);
	}
	if (ret != index) {
		fprintf(stderr, "Error occurred seeking into kpagecount\n");
		cleanup_and_exit(EXIT_FAILURE);
	}

	errno = 0;
	ret = read(fdpagecount, &bitfield, sizeof(bitfield));
	if (ret == -1) {
		perror("Error reading kpagecount");
		cleanup_and_exit(EXIT_FAILURE);
	}

	if (opt->summary && (pData->memmapped == 0)) {
		/* This block of code is called only on a newly-mapped page. */
		pData->memmapped = bitfield;
		if (bitfield == 1)
			opt->summarystats.uss += getpagesize()/KBSIZE;
	}

	if (!(opt->detail || opt->compactdetail))
		return;

	currentdpage->timesmapped = bitfield;
	errno = 0;
	ret = lseek(fdpageflags, index, SEEK_SET);
	if (ret == -1) {
		perror("Error seeking in kpageflags");
		cleanup_and_exit(EXIT_FAILURE);
	}
	if (ret != index) {
		fprintf(stderr, "Error occurred seeking into kpageflags\n");
		cleanup_and_exit(EXIT_FAILURE);
	}
	errno = 0;
	ret = read(fdpageflags, &bitfield, sizeof(bitfield));
	if (ret == -1) {
		perror("Error reading kpageflags");
		cleanup_and_exit(EXIT_FAILURE);
	}
	store_flags_in_page(bitfield, currentdpage);
}

void parse_bitfield(uint64_t bitfield, options *opt, int fdpageflags, 
                    int fdpagecount, pagedetaildata *currentdpage)
{
/*
 * Prints the start of the detail line for each line, if requested to print 
 * details of the process. This is done because it is the first function that 
 * exists on a per-page context.
 * If the page is present, it continues to find out more about the page.
 * If the page is not swapped, the bitfield contains a PFN which can be used to
 * look up more details in /proc/kpagemaps and /proc/kpagecount, which is
 * performed in the function use_pfn
 */

	uint64_t pfnbits;

	if (opt->detail) {
		uint64_t pageshift;

		if (bitfield & PAGEPRESENT) 
			currentdpage->present = 1;
		else
			currentdpage->present = 0;
		pageshift = bitfield & PAGESHIFTBITS;
		pageshift = pageshift >> PAGESHIFT;
		currentdpage->pageshift = pageshift;
	}
	if (bitfield & PAGESWAPPED) {
		/*Omitting swap type and swap offset information*/
		if (opt->detail)
			currentdpage->swap = 1;
		return;
	}
	if (opt->detail)
		currentdpage->swap = 0;
	pfnbits = bitfield & PFNBITS;

	if (opt->detail)
		currentdpage->pfn = pfnbits;

	use_pfn(pfnbits, opt, fdpageflags, fdpagecount, currentdpage);
}

int
are_pages_identical_and_adjacent(pagedetaildata *prev, pagedetaildata *curr)
{
	/* omit addresses and pfn from identity check. */
	if (prev == NULL)                           return 0;
	if (prev->addrend     != curr->addrstart)   return 0;
	if (prev->vmaindex    != curr->vmaindex)    return 0;
	if (prev->present     != curr->present)     return 0;
	if (prev->pageshift   != curr->pageshift)   return 0;
	if (prev->swap        != curr->swap)        return 0;
	if (prev->timesmapped != curr->timesmapped) return 0;
	if (prev->locked      != curr->locked)      return 0;
	if (prev->referenced  != curr->referenced)  return 0;
	if (prev->dirty       != curr->dirty)       return 0;
	if (prev->anonymous   != curr->anonymous)   return 0;
	if (prev->swapcache   != curr->swapcache)   return 0;
	if (prev->swapbacked  != curr->swapbacked)  return 0;
	if (prev->ksm         != curr->ksm)         return 0;
	return 1;
	
}

void lookup_pagemap_with_addresses(uint32_t addressfrom, uint32_t addressto, 
                                   options *opt,
                                   int fdpageflags, int fdpagecount, 
                                   int fdpagemap, uint16_t vmaindex)
{
/*
 * Looks up every entry over the index range in pagemap and reads the 64-bit
 * bitfield into a uint64_t.
 * Calls parse_bitfield to interpret the meanings of each bit
 */

	size_t entrysize = sizeof(uint64_t);
	size_t pagesize = getpagesize();
	pagedetaildata *previousdpage = NULL;
	pagedetaildata *currentdpage = calloc(1, sizeof(*currentdpage));

	uint32_t entryfrom = addressfrom / pagesize;
	uint32_t entryto = addressto / pagesize;
	/* Multiplying and dividing done in separate steps because multiplying
	 * address by entrysize often causes an overflow. 
	 */
	entryfrom *= entrysize;
	entryto *= entrysize;

	uint32_t i;
	
	for (i = entryfrom; i < entryto; i += entrysize) {
		int inum = i / entrysize;
		int ret;
		uint64_t bitfield;
		uint32_t o;

		errno = 0;
		o = lseek(fdpagemap, i, SEEK_SET);
		if (o == -1) {
			perror("Error seeking in pagemap");
			cleanup_and_exit(EXIT_FAILURE);
		}
		if (o != i) {
			fprintf(stderr, "Error occurred seeking in pagemap");
			cleanup_and_exit(EXIT_FAILURE);
		}
		errno = 0;
		ret = read(fdpagemap, &bitfield, sizeof(bitfield));
		if (ret == -1) {
			perror("Error reading pagemap");
			cleanup_and_exit(EXIT_FAILURE);
		}

		currentdpage->vmaindex = vmaindex;
		currentdpage->addrstart = inum * pagesize;
		currentdpage->addrend = (inum + 1) * pagesize;

		parse_bitfield(bitfield, opt, 
		               fdpageflags, fdpagecount, currentdpage);

		/* Compare currentdpage with previousdpage and decide whether to
                 * add it to the hashtable. */
		if (are_pages_identical_and_adjacent(previousdpage,
		                                     currentdpage)) {
			previousdpage->addrend = currentdpage->addrend;
			memset(currentdpage, 0, sizeof(*currentdpage));
		} else {
			g_hash_table_insert(opt->detailpages, 
			                    newkey(currentdpage->addrstart), 
			                    currentdpage);
			previousdpage = currentdpage;
			currentdpage = calloc(1, sizeof(*currentdpage));
		}
	}
}


vmastats *
make_another_vmst_in_opt(options *opt)
{
	vmastats *temp;
	vmastats *newelement;
	errno = 0;
	temp = realloc(opt->vmas, sizeof(*temp) * (opt->vmacount + 1));
	if (temp == NULL) {
		perror("Error allocating new vmastats");
		cleanup_and_exit(EXIT_FAILURE);
	}
	opt->vmas = temp;
	(opt->vmacount)++;
	newelement = &(temp[opt->vmacount - 1]);
	memset(newelement, 0, sizeof(*newelement));
	return newelement;
}

void lookup_maps_with_PID(pid_t pid, options *opt,int fdpageflags,
                          int fdpagecount)
{
/*
 * Uses the PID to access the appropriate /proc/PID/maps file, which contains
 * Permissions and path information about each Virtual Memory Area (VMA), 
 * and the region that memory addresses exist for. Assumes constant-sized pages
 * to pass the range of indexes to be accessed in /proc/PID/pagemap.
 * Calls the function lookup_pagemap_with_addresses with the range of indexes
 * to interrogate each listing in pagemap
 */

	char pathbuf[PATH_MAX];
	char buffer[BUFSIZ]; /* Buffer for storing lines read from file*/
	FILE *filemaps;
	int fdpagemap;
	int ret;
	char *pos = NULL;

	sprintf(pathbuf, "%s/%u/%s", PROC_PATH, pid, MAPS_FILENAME);
	errno = 0;
	filemaps = fopen(pathbuf, "r");
	if (filemaps == NULL) {
		perror("Error opening maps file");
		cleanup_and_exit(EXIT_FAILURE);
	}

	sprintf(pathbuf, "%s/%u/%s", PROC_PATH, pid, PAGEMAP_FILENAME);
	errno = 0;
	fdpagemap = open(pathbuf, O_RDONLY);
	if (fdpagemap == -1) {
		perror("Error opening pagemap file");
		cleanup_and_exit(EXIT_FAILURE);
	}

	while (fgets(buffer, BUFSIZ, filemaps) != NULL) {
		vmastats *vmst = make_another_vmst_in_opt(opt);
		uint16_t vmaindex = opt->vmacount - 1;
		uint32_t addrstart;
		uint32_t addrend;

		errno = 0;
		ret = sscanf(buffer, "%x-%x %4s", &addrstart, &addrend,
		             vmst->permissions);
		if (ret == EOF && errno != 0) {
			perror("Error parsing addresses from a line of maps"
			       " file");
			cleanup_and_exit(EXIT_FAILURE);
		}

		if (ret < 3){
			fprintf(stderr, "Error: Unexpected format of line in"
 					"maps.\n");
			cleanup_and_exit(EXIT_FAILURE);
		}

		if ((pos = strchr(buffer, '/')) != NULL) {
			char *newlinepos;
			strcpy(vmst->path, pos);
			newlinepos = strrchr(vmst->path, '\n');
			*newlinepos = '\0';
		} else if ((pos = strchr(buffer, '[')) != NULL) {
			char *newlinepos;
			strcpy(vmst->path, pos);
			newlinepos = strrchr(vmst->path, '\n');
			*newlinepos = '\0';
		}

		lookup_pagemap_with_addresses(addrstart, 
		                              addrend, opt,
		                              fdpageflags, fdpagecount,
		                              fdpagemap, vmaindex);
	}

	errno = 0;
	ret = fclose(filemaps);
	if (ret == EOF) {
		perror("Error closing maps file");
		errno = 0;
	}
	errno = 0;
	ret = close(fdpagemap);
	if (ret == EOF) {
		perror("Error closing pagemap file");
		errno = 0;
	}
}

void inspect_processes(options *opt)
{
/* 
 * Function to do most of the hard work. flags tell it what to do, and pids
 * tells it which processes to use. The first PID (primary PID) is inspected
 * in detail, finding out information about each page. The other PIDs (secondary
 * PIDs) are used to calculate Gss
 */

	char pathbuf[PATH_MAX]; /* Buffer for storing path to open files*/

	int fdpageflags;/* file descriptor for /proc/kpageflags */
	int fdpagecount;/* ditto for /proc/kpagecount */
	int i;
	int ret;

	if (opt->detail || opt->compactdetail) {
		sprintf(pathbuf, "%s/%s", PROC_PATH, KPAGEFLAGS_FILENAME);
		errno = 0;
		fdpageflags = open(pathbuf, O_RDONLY);
		if (fdpageflags == -1) {
			perror("Error occurred opening kpageflags");
			cleanup_and_exit(EXIT_FAILURE);
		}
	}

	sprintf(pathbuf, "%s/%s", PROC_PATH, KPAGECOUNT_FILENAME);
	errno = 0;
	fdpagecount = open(pathbuf, O_RDONLY);
	if (fdpagecount == -1) {
		perror("Error occurred opening kpagecount");
		cleanup_and_exit(EXIT_FAILURE);
	}

	lookup_maps_with_PID(opt->pids[0], opt,
	                     fdpageflags, fdpagecount);

	if (opt->summary) {
		options opt2 = *opt;
		g_hash_table_foreach(opt->summarypages, countsss,
		                     &(opt->summarystats));

		opt2.summary = 0;
		opt2.detail = 0;
		opt2.compactdetail = 0;
		for (i = 1; i < opt->pidcount; i++) {
			lookup_maps_with_PID(opt->pids[i], &opt2,
			                     fdpageflags, fdpagecount);		
		}


		g_hash_table_foreach(opt->summarypages, countgss,
		                     &(opt->summarystats));
		lookup_smaps(PIDs[0], &(opt->summarystats));

	}
	if (opt->detail || opt->compactdetail) {
		errno = 0;
		ret = close(fdpageflags);
		if (ret == -1) {
			perror("Error closing kpageflags file");
			errno = 0;
		}
	}
	errno = 0;
	ret = close(fdpagecount);
	if (ret == -1) {
		perror("Error closing kpagecount file");
		errno = 0;
	}
	
}

void
print_detail_format(FILE *file)
{
	fprintf(file,            DETAIL_ADDRSTARTTITLE  DELIMITER
	                         DETAIL_ADDRENDTITLE    DELIMITER
	                         DETAIL_PERMTITLE       DELIMITER 
	                         DETAIL_PATHTITLE       DELIMITER 
	                         DETAIL_PRESENTTITLE    DELIMITER 
	                         DETAIL_SIZETITLE       DELIMITER 
	                         DETAIL_SWAPTITLE       DELIMITER 
	                      /* DETAIL_PFNTITLE        DELIMITER PFN OMITTED */
	                         DETAIL_MAPPEDTITLE     DELIMITER 
	                         DETAIL_LOCKEDTITLE     DELIMITER
	                         DETAIL_REFDTITLE       DELIMITER
	                         DETAIL_DIRTYTITLE      DELIMITER
	                         DETAIL_ANONTITLE       DELIMITER
	                         DETAIL_SWAPCACHETITLE  DELIMITER
	                         DETAIL_SWAPBACKEDTITLE DELIMITER
	                         DETAIL_KSMTITLE        "\n"); 
}

void
print_page_detail_data(pagedetaildata *page, FILE *file, vmastats *vmst){


	fprintf(file, "%5s"DELIMITER,  vmst->permissions);
	fprintf(file, "%s" DELIMITER,  vmst->path);
	fprintf(file, "%s" DELIMITER, (page->present ? 
	                               DETAIL_YESPRESENT : 
	                               DETAIL_NOPRESENT ));
	fprintf(file, "%d" DELIMITER, (1 << page->pageshift));
	fprintf(file, "%s" DELIMITER, (page->swap ? 
	                               DETAIL_YESSWAP : 
	                               DETAIL_NOSWAP ));
	/* PFN OMITTED
	 * fprintf(file, "%llx"DELIMITER, (page->pfn ? page->pfn : ""));
	 */
	fprintf(file, "%d"DELIMITER, page->timesmapped);
	fprintf(file, "%s"DELIMITER, (page->locked ? 
	                              DETAIL_YESLOCKED : 
	                              DETAIL_NOLOCKED ));
	fprintf(file, "%s"DELIMITER, (page->referenced ? 
	                              DETAIL_YESREFD : 
	                              DETAIL_NOREFD ));
	fprintf(file, "%s"DELIMITER, (page->dirty ? 
	                              DETAIL_YESDIRTY : 
	                              DETAIL_NODIRTY ));
	fprintf(file, "%s"DELIMITER, (page->anonymous ? 
	                              DETAIL_YESANON : 
	                              DETAIL_NOANON ));
	fprintf(file, "%s"DELIMITER, (page->swapcache ? 
	                              DETAIL_YESSWAPCACHE : 
	                              DETAIL_NOSWAPCACHE ));
	fprintf(file, "%s"DELIMITER, (page->swapbacked ? 
	                              DETAIL_YESSWAPBACKED : 
	                              DETAIL_NOSWAPBACKED ));
	fprintf(file, "%s"DELIMITER, (page->ksm ? 
	                              DETAIL_YESKSM : 
	                              DETAIL_NOKSM ));

}

void
write_compact_detail_page(void *key, void *val, void *userdata)
{
	options *opt = userdata;
	pagedetaildata *page = val;
	vmastats *currentvmst = &(opt->vmas[page->vmaindex]);
	uint32_t pagecount = (page->addrend - page->addrstart) / getpagesize();
	fprintf(opt->compactdetailfile, "%08x" DELIMITER, page->addrstart);
	fprintf(opt->compactdetailfile, "%08x" DELIMITER, page->addrend);
	print_page_detail_data(page, opt->compactdetailfile, currentvmst);			
	fprintf(opt->compactdetailfile, "%d" DELIMITER" identical pages\n",
	        pagecount);
}

void
write_detail_page(void *key, void *val, void *userdata)
{
	options *opt = userdata;
	pagedetaildata *page = val;
	vmastats *currentvmst = &(opt->vmas[page->vmaindex]);
	uint32_t i;
	size_t size = getpagesize();

	for (i = page->addrstart; i < page->addrend; i += size) {
		fprintf(opt->detailfile, "%x" DELIMITER, i);
		fprintf(opt->detailfile, "%x" DELIMITER, (i + size));
		print_page_detail_data(page, opt->detailfile, currentvmst);
		fprintf(opt->detailfile, "\n");
	}
}


void
write_any_detail(options *opt)
{
	if (opt->compactdetail) {
		print_detail_format(opt->compactdetailfile);
		g_hash_table_foreach(opt->detailpages,
		                     &write_compact_detail_page, opt);
	}
	if (opt->detail) {
		print_detail_format(opt->detailfile);
		g_hash_table_foreach(opt->detailpages, &write_detail_page, opt);
	}
}

/*
 * Sets up the signal handler
 * Calls functions to turn the arguments into PIDs, flags and filenames. 
 * stops the PIDs that are going to be inspected
 * calls the function to inspect the PIDs
 * writes a summary
 */
int 
main(int argc, char *argv[])
{

	options opt = {0};
	set_signals();

	if (argc < 2){
		printf("Format is %s PID [flags + FILENAME] [other PIDs]\n"
			"Accepted flags are:\n \t'-s FILENAME' to get summary\n"
			"\t'-d FILENAME' to get per-page details\n", argv[0]); 
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
	if (opt.detailfile != NULL)
		fclose(opt.detailfile);
	if (opt.summaryfile != NULL)
		fclose(opt.summaryfile);


	free(opt.vmas);
	if (opt.summary)
		g_hash_table_destroy(opt.summarypages);
	if (opt.detail || opt.compactdetail)
		g_hash_table_destroy(opt.detailpages);
	free(PIDs);

	cleanup_and_exit(EXIT_SUCCESS);
	exit(EXIT_SUCCESS);
}
