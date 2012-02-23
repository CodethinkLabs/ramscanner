#define _LARGEFILE64_SOURCE

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include<errno.h>

#include<linux/limits.h>
#include<stdint.h>
#include<sys/types.h>
#include<unistd.h>
#include<ctype.h>

#include<fcntl.h>
#include<signal.h>

#include<glib.h>

#define BUFFER_SIZE 256 /*Size of buffers pre-defined as quite large, for 
			 *lack of knowing exactly what size they need to be*/

/*Bit masks used in pagemap. See documentation/vm/pagemap.txt 
 *for a full list of bits*/
#define PAGEPRESENT   0x8000000000000000 /*Bit 63 is page present*/
#define PAGESWAPPED   0x4000000000000000 /*Bit 62 is page swapped*/
#define PFNBITS       0x007FFFFFFFFFFFFF /*Bits 0-54 store the page frame
					  *number if page present and not
					  *swapped*/
#define PAGESHIFTBITS 0x1F80000000000000 /*Bits 55-60 page shift 
					  *(page size = 1<<page shift)*/

#define PAGEFLAG_LOCKED 0x1 /* Bit 0 is locked*/
#define PAGEFLAG_REFERENCED 0x4 /* Bit 2 is referenced*/
#define PAGEFLAG_DIRTY 0x10 /* Bit 4 is dirty*/
#define PAGEFLAG_ANON 0x1000 /* Bit 12 is anonymous*/
#define PAGEFLAG_SWAPCACHE 0x2000 /* Bit 13 is swapcache*/
#define PAGEFLAG_SWAPBACKED 0x4000 /* Bit 14 is swapbacked*/
#define PAGEFLAG_KSM 0x200000 /* Bit 21 is KSM: identical memory pages 
			       * dynamically shared between one or more
			       * processes*/

#define PAGESHIFT 55 /*The number of bits that need to be shifted down to
		      *express the pageshift as a number*/
#define KBSIZE 1024 /*The number of bytes in a kilobyte*/

#define PROC_PATH "/proc"
#define KPAGEFLAGS_FILENAME "kpageflags"
#define KPAGECOUNT_FILENAME "kpagecount"
#define MAPS_FILENAME "maps"
#define SMAPS_FILENAME "smaps"
#define PAGEMAP_FILENAME "pagemap"

#define FLAG_SUMMARY 1/*Bit mask for flags bit-field, whether to give summary*/
#define FLAG_DETAIL 2/*as above, for whether to give details per-page*/

#define DEBUG 0

struct sizestats{
	uint vss; /*Amount of memory kB addressed in memory map*/
	uint rss; /*Amount of memory kB memory-mapped in RAM*/
	uint pss; /*Amount of memory kB per page divided by number of 
		   *times mapped*/
	uint uss; /*Amount of memory kB unique to the process 
		   *(only mapped once)*/
	uint sss; /*Amount of memory kB only mapped within the process*/
	uint gss; /*Amount of memory kB only mapped by the processes
	 	   *specified as arguments to this program*/
	uint refd;/*Amount of memory kB in pages referenced*/
	uint swap;/*Amount of memory kB mapped to a swap file/partition*/
	uint anon;/*Amount of memory kB without an associated file*/
	uint locked;/*Amount of memory kB locked*/
};

struct pagedata {
	uint memmapped; /*Internal store of how many times this page has
			 *been mapped by the memory manager*/
	uint procmapped;/*Internal store of how many times this page has
			 *been mapped by the PIDs defined*/
};

struct vmastats {
	char permissions[5];
	char path[BUFFER_SIZE];
};


pid_t *PIDs = NULL;/*Global array of PIDs used, so that they can be restarted if
	      *the program is told to terminate early*/
uint PIDcount = 0;

void * newkey(uint64_t key)/*The key is a 54-bit long PFN*/
{ 
	uint64_t *temp = malloc(sizeof(*temp));
	*temp = key;
	return temp;
}
void destroyval(void *val)
{
	free(val);
}

void cleanup(int signal)
{
	int i = 0;
	for (i=0; i< PIDcount; i++)
		kill(PIDs[i], SIGCONT);/*Clears the queue of STOP signals.*/
	if (DEBUG)
		printf("Program terminated successfully\n");
	exit(0);
}

void write_summary(struct sizestats *stats, FILE* summaryfile)
{
	fprintf( summaryfile,
		"Type,         Size(kB)\n"
		"Vss,          %8u\n"
		"Rss,          %8u\n"
		"Pss,          %8u\n"
		"Uss,          %8u\n"
		"Sss,          %8u\n"
		"Gss,          %8u\n"
		"Referenced,   %8u\n"
		"Swap,         %8u\n"
		"Anonymous,    %8u\n"
		"Locked,       %8u\n",
		stats->vss, stats->rss, stats->pss, stats->uss, stats->sss,
		stats->gss, stats->refd, stats->swap, stats->anon,
		stats->locked);
}

int fill_size_for_smaps_field(char * buff, const char *str, uint *val, 
FILE *file)
{
	size_t n = 0;
	int temp;
	if (strstr(buff, str)){
		getline(&buff, &n, file);
		if (sscanf (buff, "%d", &temp) > 0) {
			*val += temp;
			return 1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}

void parse_smaps_file(FILE *file, struct sizestats *stats)
{
/*Extracts numerical data from the smaps file. Depends on the format staying as
 * Type:   [val] kB*/
	char buffer[BUFFER_SIZE];
	char *pt = buffer;
	size_t n = 0;
	while (getdelim(&pt, &n, ':', file) > 0) {
		if(fill_size_for_smaps_field(pt, "Size", 
			&(stats->vss), file)) {
		} else if(fill_size_for_smaps_field(pt, "Pss", 
			&(stats->pss), file)) {
		} else if(fill_size_for_smaps_field(pt, "Rss", 
			&(stats->rss), file)) {
		} else if(fill_size_for_smaps_field(pt, "Referenced", 
			&(stats->refd), file)) {
		} else if(fill_size_for_smaps_field(pt, "Anonymous", 
			&(stats->anon), file)) {
		} else if(fill_size_for_smaps_field(pt, "Swap", 
			&(stats->swap), file)) {
		} else if(fill_size_for_smaps_field(pt, "Locked", 
			&(stats->locked), file)) {
		}
	}
}

void countgss(void *key, void *value, void *data)
{
	size_t pagesize = getpagesize();
	struct pagedata *page = value;
	struct sizestats *stats = data;
	if (page->memmapped == page->procmapped)
		stats->gss += pagesize;
}

void countsss(void *key, void *value, void *data)
{
	size_t pagesize = getpagesize();
	struct pagedata *page = value;
	struct sizestats *stats = data;
	if (page->memmapped == page->procmapped)
		stats->sss += pagesize;
}

void handle_errno(const char *context)
{
/*Context refers to where the error took place*/
	if(errno){
		fprintf(stderr, "Error occurred when %s, with error: %s\n", 
			context, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void lookup_smaps(pid_t PID, struct sizestats *stats)
{
	char buffer[BUFFER_SIZE];
	FILE *file;
	sprintf(buffer, "/proc/%d/smaps", PID);
	file = fopen(buffer, "r");
	handle_errno("opening smaps file");
	parse_smaps_file(file, stats);
	fclose(file);
	handle_errno("closing smaps file");
}

int is_switch(const char *arg)
{
	if (arg[0] == '-')
		return 1;
	else
		return 0;
}

int try_to_read_PID(const char *arg)
{
/* Tries to interpret a string as a number, and if it succeeds returns the PID
 * Handles the special case of specifying the PID 0, which is forbidden, and 
 * checks if the PID specified corresponds to an existing process */
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

void warn_if_looks_like_pid(const char *str)
{
/* The program requires a file path to be specified after the -s and -d switches
 * and if a PID is stated instead, the output file will use that as the file
 * path. So that the user is aware of this, a warning will be generated */
	if (try_to_read_PID(str)) {
		fprintf(stderr, "Warning: File argument following a flag"
			" looks like a PID\n");
	}
}

int handle_switch(const char *argv[], int argc, int index, uint8_t *flags, 
FILE **summary, FILE **detail)
{
/*Checks for which switch it received. Returns how many args to skip because 
 * they have been processed in this function. It does not crash the program on
 * finding an invalid argument, but does print an error to stderr so the user
 * is aware of it */
	if (tolower(argv[index][1]) == 's') {
		if (argc <= (index + 1)) {
			fprintf(stderr, "Error: -s flag must have the filename"
				" after it. Ignoring flag\n");
			return 0;
		}
		*flags |= FLAG_SUMMARY;
		if (strcmp(argv[index + 1], "stdout") == 0) {
			*summary = stdout;
		} else {
			*summary = fopen(argv[index + 1], "w");
			handle_errno("opening file to write summary to");
			warn_if_looks_like_pid(argv[index + 1]);
		}
		return 1;
	} else if (tolower(argv[index][1]) == 'd') {
		if (argc <= (index + 1)) {
			fprintf(stderr, "Error: -d flag must have the filename"
				" after it. Ignoring flag\n");
			return 0;
		}
		*flags |= FLAG_DETAIL;
		if (strcmp(argv[index + 1], "stdout") == 0) {
			*detail = stdout;
		} else {
			*detail = fopen(argv[index + 1], "w");
			handle_errno("opening file to write details to");
			warn_if_looks_like_pid(argv[index + 1]);
		}
		return 1;
	} else {
		return 0;
	}
}

void add_pid_to_array(pid_t pid, pid_t **pids, uint *pidcount)
{
	*pids = realloc(*pids, sizeof(pid_t) * ((*pidcount) + 1));
	handle_errno("allocating memory for PID array");
	(*pids)[*pidcount] = pid;
	*pidcount += 1;
}

void handle_args(int argc, const char *argv[], pid_t **pids, uint *pidcount, 
uint8_t *flags, FILE **summaryfile, FILE **detailfile)
{
/* Parses the arguments given to the program. It ignores invalid arguments
 * instead of failing. FILE pointers and PID arrays are set in the functions
 * called, so are passed as pointers to pointers */
	int i;
	pid_t pid;
	for (i = 1; i < argc; i++) {
		if (is_switch(argv[i]))
			i += handle_switch(argv, argc, i, flags,
				summaryfile, detailfile);
		else if ((pid = try_to_read_PID(argv[i])))
			add_pid_to_array(pid, pids, pidcount);
	}
}

void set_signals()
{
/* Set handlers for signals sent, to ensure processes get re-started
 * on early terminate */
	struct sigaction sa;
		sa.sa_handler = cleanup;
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

void stop_PIDs(pid_t *pids, uint count)
{
	int i;
	for (i = 0; i < count; i++) {
		kill(pids[i], SIGSTOP);/*SIGSTOP overrides signal
					*handling, consider SIGSTP*/
		if (DEBUG) 
			printf("Stopping process %u\n", pids[i]);

		handle_errno("stopping process");
	}
}

void print_detail_from_condition(uint32_t condition, const char *truestring, 
const char *falsestring, FILE *file)
{
	if(condition)
		fprintf(file, "%s", truestring);
	else
		fprintf(file, "%s", falsestring);
} 

void print_flags(uint64_t bitfield, FILE *detail)
{
	print_detail_from_condition(bitfield & PAGEFLAG_LOCKED,
		 "locked,",",", detail);
	print_detail_from_condition(bitfield & PAGEFLAG_REFERENCED,
		 "referenced,",",", detail);
	print_detail_from_condition(bitfield & PAGEFLAG_DIRTY,
		 "dirty,",",", detail);
	print_detail_from_condition(bitfield & PAGEFLAG_ANON,
		 "anonymous,",",", detail);
	print_detail_from_condition(bitfield & PAGEFLAG_SWAPCACHE,
		 "swapcache,",",", detail);
	print_detail_from_condition(bitfield & PAGEFLAG_SWAPBACKED,
		 "swapbacked,",",", detail);
	print_detail_from_condition(bitfield & PAGEFLAG_KSM,
		 "ksm,",",", detail);
	fprintf(detail, "\n");
}

void use_pfn(uint64_t pfn, uint8_t flags, struct sizestats *stats, 
GHashTable *pages, FILE *detail, int fdpageflags, int fdpagecount)
{
/* Looks up the hash table of pages to see if this page has been mapped before.
 * This is done so that it is possible to compare the number of times the page
 * reports it has been mapped to the number of times the primary or secondary
 * processes map it.
 * Looks up the number of times the page reports it has been mapped in 
 * /proc/kpagecount then compares it to the number of times it has been mapped
 * by primary or secondary processes.
 * If the flag for details has been set, it will look up /proc/kpageflags and
 * retrieve a 64-bit bitfield of the flags set on that process, and pass it to
 * the function print_flags */
	int ret;
	size_t elementsize = sizeof(uint64_t);
	uint64_t index = pfn * elementsize;
	uint64_t bitfield;
	struct pagedata *pData = NULL;
	pData = g_hash_table_lookup(pages, &pfn);
	if (DEBUG)
		printf("in use_pfn\n");

	if ((pData == NULL)) {
/* flags containing neither is a special case, as it is impossible to reach this
 * function if the arguments contain neither detail or summary. It means that
 * the process being interrogated is not the primary process, so it is used to
 * calculate Gss by checking if the secondary process maps a PFN that the 
 * primary process already has.*/
		if (flags & (FLAG_DETAIL | FLAG_SUMMARY)) {
			pData = malloc(sizeof(*pData));
			handle_errno("allocating memory for new page info");
			pData->procmapped = 1;
			pData->memmapped = 0; /*Indicates a newly-mapped page*/
			g_hash_table_insert(pages, newkey(pfn), pData);
		}
	} else {
		pData->procmapped += 1;
	}
	if (!(flags & (FLAG_DETAIL | FLAG_SUMMARY)))
		return;
	ret = lseek(fdpagecount, index, SEEK_SET);
	handle_errno("seeking into kpagecount");
	if (ret != index) {
		fprintf(stderr, "Error occurred seeking into kpagecount\n");
		exit(EXIT_FAILURE);
	}
	read(fdpagecount, &bitfield, sizeof(bitfield));
	handle_errno("reading kpagecount");
	if ((flags & FLAG_SUMMARY) && (pData->memmapped == 0)) {
		/*This block of code is called only on a newly-mapped page*/
		pData->memmapped = bitfield;
		if (bitfield == 1)
			stats->uss += getpagesize();
	}
	if (!(flags & FLAG_DETAIL))
		return;

	fprintf(detail, "%Lu,", bitfield);
	ret = lseek(fdpageflags, index, SEEK_SET);
	handle_errno("seeking into kpageflags");
	if (ret != index) {
		fprintf(stderr, "Error occurred seeking into kpageflags\n");
		exit(EXIT_FAILURE);
	}
	read(fdpageflags, &bitfield, sizeof(bitfield));
	handle_errno("reading kpageflags");
	print_flags(bitfield, detail);
}

void parse_bitfield(uint64_t bitfield, uint8_t flags, struct sizestats *stats, 
GHashTable *pages, FILE *detail, int fdpageflags, int fdpagecount,
const struct vmastats *vmst)
{
/*Prints the start of the detail line for each line, if requested to print 
 * details of the process. This is done because it is the first function that 
 * exists on a per-page context.
 * If the page is present, it continues to find out more about the page.
 * If the page is not swapped, the bitfield contains a PFN which can be used to
 * look up more details in /proc/kpagemaps and /proc/kpagecount, which is
 * performed in the function use_pfn */
	uint64_t pfnbits;
	if (DEBUG)
		printf("in parse_bitfield\n");

	if (flags & FLAG_DETAIL)
		fprintf(detail, "%s,%s,", vmst->permissions, vmst->path);
	if (DEBUG)
		printf("about to check present\n");
	if (!(bitfield & PAGEPRESENT)) {
		if (flags & FLAG_DETAIL)
			fprintf(detail, "absent\n");
		if (DEBUG)
			printf("page not present\n");
		return;
	}
	if (DEBUG)
		printf("page present\n");
	if (flags & FLAG_DETAIL) {
		uint64_t pageshift;
		fprintf(detail, "present,");
		pageshift = bitfield & PAGESHIFTBITS;
		pageshift = pageshift >> PAGESHIFT;
		fprintf(detail, "%u B,", 1 << pageshift); /*page size in bytes*/
	}
	if (bitfield & PAGESWAPPED) {
		if (flags & FLAG_DETAIL)
			fprintf(detail, "swapped\n");
			/*Omitting swap type and swap offset information*/
		if (DEBUG)
			printf("page swapped\n");
		return;
	}
	if (flags & FLAG_DETAIL)
		fprintf(detail, "unswapped,");
	pfnbits = bitfield & PFNBITS;
	if (DEBUG)
		printf("about to use pfn\n");
	use_pfn(pfnbits, flags, stats, pages, detail, fdpageflags, fdpagecount);
}

void lookup_pagemap_with_addresses(uint32_t indexfrom, uint32_t indexto, 
uint8_t flags, struct sizestats *stats, GHashTable *pages, FILE *detail, 
int fdpageflags, int fdpagecount, int fdpagemap, const struct vmastats *vmst)
{
/* Looks up every entry over the index range in pagemap and reads the 64-bit
 * bitfield into a uint64_t.
 * Calls parse_bitfield to interpret the meanings of each bit*/
	size_t entrysize = sizeof(uint64_t);
	uint32_t entryfrom = indexfrom * entrysize;
	uint32_t entryto = indexto * entrysize;
	uint32_t i;
	
	if (DEBUG){
		printf("in lookup_pagemap_with_addresses\n");
		printf("%s,%s\n", vmst->permissions, vmst->path);
	}
	for (i = entryfrom; i < entryto; i += entrysize) {
		uint64_t bitfield;
		uint32_t o = lseek(fdpagemap, i, SEEK_SET);
		handle_errno("seeking in pagemap");
		if (o != i) {
			fprintf(stderr, "Error occurred seeking in pagemap");
			exit(EXIT_FAILURE);
		}
		read(fdpagemap, &bitfield, sizeof(bitfield));
		handle_errno("reading in pagemap");
		parse_bitfield(bitfield, flags, stats, pages, detail, 
			fdpageflags, fdpagecount, vmst);
	}
}

void lookup_maps_with_PID(pid_t pid, uint8_t flags, 
struct sizestats *stats, GHashTable *pages, FILE *detail, 
int fdpageflags, int fdpagecount)
{
/* Uses the PID to access the appropriate /proc/PID/maps file, which contains
 * Permissions and path information about each Virtual Memory Area (VMA), 
 * and the region that memory addresses exist for. Assumes constant-sized pages
 * to pass the range of indexes to be accessed in /proc/PID/pagemap.
 * Calls the function lookup_pagemap_with_addresses with the range of indexes
 * to interrogate each listing in pagemap */
	char pathbuf[PATH_MAX];
	char buffer[BUFFER_SIZE]; /* Buffer for storing lines read from file*/
	char *pos = NULL;
	FILE *filemaps;
	size_t pagesize = getpagesize();
	int fdpagemap;
	if (DEBUG)
		printf("in lookup_maps_with_PID\n");

	sprintf(pathbuf, "%s/%u/%s", PROC_PATH, pid, MAPS_FILENAME);
	filemaps = fopen(pathbuf, "r");
	handle_errno("opening maps file");
	sprintf(pathbuf, "%s/%u/%s", PROC_PATH, pid, PAGEMAP_FILENAME);
	fdpagemap = open(pathbuf, O_RDONLY);
	handle_errno("opening pagemap file");

	while (fgets(buffer, BUFFER_SIZE, filemaps) != NULL) {
		struct vmastats vmst;
		uint32_t addrstart;
		uint32_t addrend;
		int ret;
		
		ret = sscanf(buffer, "%x-%x %4s", &addrstart, &addrend,
			vmst.permissions);
		handle_errno("parsing addresses from a line of maps file");
		if (ret < 3){
			fprintf(stderr, "Error: Unexpected format of line in"
 					"maps.\n");
			exit(EXIT_FAILURE);
		}
		if ((pos = strchr(buffer, '/')) != NULL) {
			strcpy(vmst.path, pos);
			*strrchr(vmst.path, '\n') = '\0';
		} else if ((pos = strchr(buffer, '[')) != NULL) {
			strcpy(vmst.path, pos);
			*strrchr(vmst.path, '\n') = '\0';
		}
		if(DEBUG)
			printf("path=%s, perm=%s\n", vmst.path, 
							vmst.permissions);
		lookup_pagemap_with_addresses(addrstart / pagesize, 
			addrend / pagesize, flags, stats, pages, 
			detail, fdpageflags, fdpagecount, fdpagemap, &vmst);
	}

	fclose(filemaps);
	handle_errno("closing maps file");
	close(fdpagemap);
	handle_errno("closing pagemap file");
}

void inspect_processes(pid_t *pids, uint count, uint8_t flags, 
struct sizestats *stats, GHashTable *pages, FILE *summary, FILE *detail)
{
/* Function to do most of the hard work. flags tell it what to do, and pids
 * tells it which processes to use. The first PID (primary PID) is inspected
 * in detail, finding out information about each page. The other PIDs (secondary
 * PIDs) are used to calculate Gss */
	char pathbuf[PATH_MAX]; /* Buffer for storing path to open files*/

	int fdpageflags;/* file descriptor for /proc/kpageflags */
	int fdpagecount;/* ditto for /proc/kpagecount */
	int i;

	if (DEBUG)
		printf("in inspect_processes\n");

	sprintf(pathbuf, "%s/%s", PROC_PATH, KPAGEFLAGS_FILENAME);
	fdpageflags = open(pathbuf, O_RDONLY);
	handle_errno("opening kpageflags file");

	sprintf(pathbuf, "%s/%s", PROC_PATH, KPAGECOUNT_FILENAME);
	fdpagecount = open(pathbuf, O_RDONLY);
	handle_errno("opening kpagecount file");

	if (flags & FLAG_DETAIL)
		fprintf(detail, "permissions,path,present,size,swapped,"
			"times mapped,locked,referenced,dirty,anonymous,"
			"swapcache,swapbacked,KSM\n");

	lookup_maps_with_PID(pids[0], flags, stats, pages, 
		detail, fdpageflags, fdpagecount);

	if (flags & FLAG_SUMMARY) {
		g_hash_table_foreach(pages, countsss, stats);

		for (i = 1; i < count; i++) {
			lookup_maps_with_PID(pids[i], 0, stats, pages,
				detail, fdpageflags, fdpagecount);		
		}


		g_hash_table_foreach(pages, countgss, stats);
		lookup_smaps(PIDs[0], stats);

		stats->uss /= KBSIZE;
		stats->gss /= KBSIZE;
		stats->sss /= KBSIZE;
	}

	close(fdpageflags);
	handle_errno("closing kpageflags file");
	close(fdpagecount);
	handle_errno("closing kpagecount file");
	
}

int main(int argc, const char *argv[])
{
/*Sets up the signal handler
 * Calls functions to turn the arguments into PIDs, flags and filenames, 
 * stops the PIDs that are going to be inspected
 * calls the function to inspect the PIDs
 * writes a summary*/
	FILE *summaryfile = NULL;
	FILE *detailfile = NULL;

	uint8_t flags = 0; /*Bit field for storing options as flags*/

	/* Hash table used to store information about individual pages */
	GHashTable *pages = g_hash_table_new_full(g_int64_hash,
		 g_int64_equal, &destroyval, &destroyval);

	struct sizestats stats;
		stats.vss = 0;
		stats.rss = 0;
		stats.pss = 0;
		stats.uss = 0;
		stats.gss = 0;
		stats.sss = 0;
		stats.refd = 0;
		stats.swap = 0;
		stats.anon = 0;
		stats.locked = 0;

	set_signals();

	if (argc < 2){
		printf("Format is %s PID [flags + FILENAME] [other PIDs]\n"
			"Accepted flags are:\n \t'-s FILENAME' to get summary\n"
			"\t'-d FILENAME' to get per-page details\n", argv[0]); 
		exit(EXIT_FAILURE);
	}

	handle_args(argc, argv, &PIDs, &PIDcount, 
		&flags, &summaryfile, &detailfile);

	stop_PIDs(PIDs, PIDcount);

	if (DEBUG)
		printf("about to enter inspect_processes\n");

	if (flags & (FLAG_SUMMARY | FLAG_DETAIL))
	inspect_processes(PIDs, PIDcount, flags, &stats,
		pages, summaryfile, detailfile);
	else
		printf("%s must specify at least one of -s and -d\n", argv[0]);

	if (DEBUG)
		printf("inspected processes\n");

	if (flags & FLAG_SUMMARY)
		write_summary(&stats, summaryfile);
	if (detailfile != NULL)
		fclose(detailfile);
	if (summaryfile != NULL)
		fclose(summaryfile);

	cleanup(0);
	g_hash_table_destroy(pages);
	free(PIDs);

	exit(0);
}
