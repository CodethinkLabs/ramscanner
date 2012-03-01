#ifndef _RAMSCANNER_COMMON_H_
#define _RAMSCANNER_COMMON_H_

#include <stdint.h>
#include <stdio.h>

#include <glib.h>
#include "ramscanner_literals.h"

extern pid_t *PIDs;
extern uint16_t PIDcount;
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

void * 
newkey(uint64_t key);

void
destroyval(void *val);

void 
cleanup_and_exit(int signal);

pid_t
try_to_read_PID(const char *arg);

void 
add_pid_to_array(pid_t pid, pid_t **pids, uint16_t *pidcount);

FILE*
open_arg(const char *arg);

void 
handle_args(int argc, char *argv[], options *opt);

void
set_signals();

void
stop_PIDs(const pid_t *pids, uint16_t count);


#endif
