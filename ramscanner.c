#define _LARGEFILE64_SOURCE

#include<stdio.h>
#include<stdlib.h>
#include<linux/limits.h>
#include<errno.h>
#include<unistd.h>
#include<fcntl.h>
#include<glib.h>
#include<sys/types.h>
#include<signal.h>
#include<stdint.h>
#include<string.h>
#include<ctype.h>

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
#define MAPS_FILENAME "maps" /*access maps by PROC_PATH/%u/MAPS_FILENAME*/
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
		"Vss, \t\t %8u kB\n"
		"Rss, \t\t %8u kB\n"
		"Pss, \t\t %8u kB\n"
		"Uss, \t\t %8u kB\n"
		"Sss, \t\t %8u kB\n"
		"Gss, \t\t %8u kB\n"
		"Referenced, \t %8u kB\n"
		"Swap, \t\t %8u kB\n"
		"Anonymous, \t %8u kB\n"
		"Locked, \t %8u kB\n",
		stats->vss, stats->rss, stats->pss, stats->uss, stats->sss,
		stats->gss, stats->refd, stats->swap, stats->anon,
		stats->locked);
}

void parse_smaps_file(FILE *file, struct sizestats *stats)
{
/*Extracts numerical data from the smaps file. HIGHLY dependent on the format
 *of smaps output staying the same*/
	int ret = 0;
	int temp=0;
	char buffer[BUFFER_SIZE];
	char* pt = buffer;
	size_t n = 0;
	ret = getline(&pt, &n, file);

	while(ret > 0){
	getdelim(&pt, &n, ':', file);
	fscanf(file,"%d kB\n", &temp);
		stats->vss += temp;
	getdelim(&pt, &n, ':', file);
	fscanf(file,"%d kB\n", &temp);
		stats->rss += temp;
	getdelim(&pt, &n, ':', file);
	fscanf(file,"%d kB\n", &temp);
		stats->pss += temp;
	getdelim(&pt, &n, ':', file);
	getdelim(&pt, &n, ':', file);
	getdelim(&pt, &n, ':', file);
	getdelim(&pt, &n, ':', file);
	getdelim(&pt, &n, ':', file);
	fscanf(file,"%d kB\n", &temp);
		stats->refd += temp;
	getdelim(&pt, &n, ':', file);
	fscanf(file,"%d kB\n", &temp);
		stats->anon += temp;
	getdelim(&pt, &n, ':', file);
	fscanf(file,"%d kB\n", &temp);
		stats->swap += temp;
	getdelim(&pt, &n, ':', file);
	getdelim(&pt, &n, ':', file);
	getdelim(&pt, &n, ':', file);
	fscanf(file,"%d kB\n", &temp);
		stats->locked += temp;
	ret = getline(&pt, &n, file);
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
	char *endpt = NULL;
	pid_t temp = strtoul(arg, &endpt, 0);
	if (endpt == arg) { /* Did not read a number*/
		return 0;
	} else if (temp == 0) {
		fprintf(stderr, "Error: cannot pass PID 0. "
			"Ignoring argument\n");
		return 0;
	} else {
		return temp;
	}
}

void warn_if_looks_like_pid(const char *str)
{
	if (try_to_read_PID(str)) {
		printf("Warning: File argument following a flag"
			" looks like a PID\n");
	}
}

int handle_switch(const char *argv[], int argc, int index, uint8_t *flags, 
FILE **summary, FILE **detail)
{
/*Checks for which switch it received. Returns how many args to skip because 
 * they have been processed in this function*/
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

void print_detail_from_condition(uint32_t condition, const char *yes, 
const char *no, FILE *file)
{
	if(condition)
		fprintf(file, "%s", yes);
	else
		fprintf(file, "%s", no);
} 

void printflags(uint64_t bitfield, FILE *detail)
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
/*Special treatment: If neither summary nor detail, we're using a PID that
 *isn't the first one. All we want it to do is count how many pages they
 *share with the first process*/
	int ret;
	size_t elementsize = sizeof(uint64_t);
	uint64_t index = pfn * elementsize;
	uint64_t bitfield;
	struct pagedata *pData = NULL;
	pData = g_hash_table_lookup(pages, &pfn);
	if (DEBUG)
		printf("in use_pfn\n");

	if ((pData == NULL)) {
		if (flags & (FLAG_DETAIL | FLAG_SUMMARY)) {
			pData = malloc(sizeof(*pData));
			handle_errno("allocating memory for new page info");
			pData->procmapped = 1;
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
	if (flags & FLAG_SUMMARY) {
		pData->memmapped = bitfield;
		if (bitfield == 1)
			stats->uss += getpagesize();
	}
	if (flags & FLAG_DETAIL)
		fprintf(detail, "%Lu,", bitfield);
	else
		return;
	ret = lseek(fdpageflags, index, SEEK_SET);
	handle_errno("seeking into kpageflags");
	if (ret != index) {
		fprintf(stderr, "Error occurred seeking into kpageflags\n");
		exit(EXIT_FAILURE);
	}
	read(fdpageflags, &bitfield, sizeof(bitfield));
	handle_errno("reading kpageflags");
	printflags(bitfield, detail);
}

void parse_bitfield(uint64_t bitfield, uint8_t flags, struct sizestats *stats, 
GHashTable *pages, FILE *detail, int fdpageflags, int fdpagecount,
const struct vmastats *vmst)
{
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
		parse_bitfield(bitfield, flags, stats, pages, detail, 
			fdpageflags, fdpagecount, vmst);
		handle_errno("reading in pagemap");
	}
}

void lookup_maps_with_PID(pid_t pid, uint8_t flags, 
struct sizestats *stats, GHashTable *pages, FILE *detail, 
int fdpageflags, int fdpagecount)
{
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

	if (flags | FLAG_DETAIL)
		fprintf(detail, "permissions,path,present,size,swapped,"
			"times mapped,locked,referenced,dirty,anonymous,"
			"swapcache,swapbacked,KSM\n");

	lookup_maps_with_PID(pids[0], flags, stats, pages, 
		detail, fdpageflags, fdpagecount);

	if (flags | FLAG_SUMMARY) {
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

	inspect_processes(PIDs, PIDcount, flags, &stats,
		pages, summaryfile, detailfile);

	if (DEBUG)
		printf("inspected processes\n");

	if (flags | FLAG_SUMMARY)
		write_summary(&stats, summaryfile);

	fclose(detailfile);
	fclose(summaryfile);
	cleanup(0);
	free(PIDs);

	exit(0);
}
