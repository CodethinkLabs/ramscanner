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

#define BUFFER_SIZE 256 /*Size of buffers pre-defined as quite large, for lack of knowing exactly what size they need to be*/

/*Bit masks used in pagemap. See documentation/vm/pagemap.txt for a full list of bits*/
#define PAGEPRESENT   0x8000000000000000 /*Bit 63 is page present*/
#define PAGESWAPPED   0x4000000000000000 /*Bit 62 is page swapped*/
#define PFNBITS       0x007FFFFFFFFFFFFF /*Bits 0-54 store the page frame
					  *number if page present and not
					  *swapped*/
#define PAGESHIFTBITS 0x1F80000000000000 /*Bits 55-60 page shift 
					  *(page size = 1<<page shift)*/
#define PAGESHIFT 55 /*The number of bits that need to be shifted down to
		      *express the pageshift as a number*/
#define KBSIZE 1024 /*The number of bytes in a kilobyte*/
#define DEBUG 1

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


pid_t *PIDs; /*Global array of PIDs used, so that they can be restarted if
	      *the program is told to terminate early*/
uint PIDcount;


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
	/*char pathbuf[PATH_MAX]; Deprecated, not used any more */
	for (i=0; i< PIDcount; i++)
		kill(PIDs[i], SIGCONT);/*Clears the queue of STOP signals.*/
	if (DEBUG)
		printf("Program terminated successfully\n");
	exit(0);
}

void printsummary(struct sizestats *stats)
{
	printf( "Vss: \t\t %8u kB\n"
		"Rss: \t\t %8u kB\n"
		"Pss: \t\t %8u kB\n"
		"Uss: \t\t %8u kB\n"
		"Sss: \t\t %8u kB\n"
		"Gss: \t\t %8u kB\n"
		"Referenced: \t %8u kB\n"
		"Swap: \t\t %8u kB\n"
		"Anonymous: \t %8u kB\n"
		"Locked: \t %8u kB\n",
		stats->vss, stats->rss, stats->pss, stats->uss, stats->sss,
		stats->gss, stats->refd, stats->swap, stats->anon,
		stats->locked);
}

void parse_smaps_file(FILE *file, struct sizestats *stats)
{
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
		fprintf(stderr, "Error occurred when %s, with error: %s\n",  context, 
strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void lookup_addresses(const char* buffer, int fd, GHashTable *pages)
{
/*parse string into addresses and look them up in pagemap (fd)
 *get the PFN (if available) and lookup in hashtable pages.*/
	uint32_t addrstart;
	uint32_t addrend;
	uint32_t address;
	uint64_t bitbuffer;
	int retval;
	size_t pagesize = getpagesize();
	size_t stepsize = sizeof(uint64_t);

	retval = sscanf(buffer, "%x-%x", &addrstart, &addrend);
	handle_errno("parsing addresses from a line of maps file");
	if (retval < 2){
		fprintf(stderr, "Error, could not find both address "
			"start and address end\n");
		exit(EXIT_FAILURE);
	}
	if (((addrend - addrstart) / pagesize) < 1){
		fprintf(stderr, "Error: VMA contains no pages\n");
		exit(EXIT_FAILURE);
	}

	addrstart /= (pagesize / stepsize);
	addrend /= (pagesize / stepsize);
	for(address = addrstart; address < addrend; address += stepsize){
		uint32_t offset;
		offset = lseek(fd, address, SEEK_SET);

		if (offset != address){
			char buffer[BUFFER_SIZE];
			sprintf(buffer, "seeking to offset %uld using index"
					" %uld", offset, address);
			handle_errno(buffer);
		}
		read(fd, &bitbuffer, sizeof(uint64_t));
		handle_errno("reading from file for pagemap");
		if ((bitbuffer & PAGEPRESENT) && !(bitbuffer & PAGESWAPPED)){
			uint64_t pfn = bitbuffer & PFNBITS;
			struct pagedata *pData = g_hash_table_lookup(pages,
				&pfn);
			if (pData){
				pData->procmapped += 1;
			}
		}
	}

}

void lookup_other_PIDs(const pid_t *procs, uint proccount, GHashTable *pages){ /*Reminder: Currently not called anywhere*/
	FILE* filemaps;
	int i;

	if(DEBUG)
		printf("In function lookup_other_PIDs\n");

	for(i = 1; i < proccount; i++){

		char pathbuf[PATH_MAX];
		char buffer[BUFFER_SIZE];
		pid_t PID = procs[i];
		int fdpagemap;

		sprintf(pathbuf, "/proc/%d/maps", PID);
		filemaps = fopen(pathbuf, "r");
		handle_errno("opening maps file");
		sprintf(pathbuf, "/proc/%d/pagemap", PID);
		fdpagemap = open(pathbuf, O_RDONLY);
		handle_errno("opening pagemap file");


		while(fgets(buffer, BUFFER_SIZE,filemaps) != NULL){
			lookup_addresses(buffer, fdpagemap, pages);
		}
		fclose(filemaps);
		handle_errno("closing maps file");
		close(fdpagemap);
		handle_errno("closing pagemap file");

	}

}

void lookup_pageflags(uint64_t PFN, int fdpageflags, 
struct pagedata* pData, struct sizestats *stats)
{
	uint64_t index;/*Number of bytes to seek to in pageflags*/
	uint64_t offset;/*The position in the file that seek reaches*/
	uint64_t bitbuffer;/*64 bits to be read and interpreted*/

	index = PFN*8;
	offset = lseek(fdpageflags, index, SEEK_SET);

	if (offset != index){
		char buffer[BUFFER_SIZE];
		sprintf(buffer, "seeking to offset %Ld using index %Ld",
			 offset, index);
		handle_errno(buffer);
	}
	read(fdpageflags, &bitbuffer, sizeof(uint64_t));
	handle_errno("reading from file for pageflags");

	printf("%016Lx|",bitbuffer);

}

void lookup_pagecount(uint64_t PFN, int fdpagecount, struct pagedata *pData,
struct sizestats *stats)
{
	uint64_t index;/*Number of bytes to seek to in pageflags*/
	uint64_t offset;/*The position in the file that seek reaches*/
	uint64_t bitbuffer;/*64 bits to be read and interpreted*/
	size_t pagesize = getpagesize();
	index = PFN*8;
	offset = lseek(fdpagecount, index, SEEK_SET);
	if (offset != index){
		char buffer[BUFFER_SIZE];
		sprintf(buffer, "seeking to offset %Ld using index %Ld",
			 offset, index);
		handle_errno(buffer);
	}
	read(fdpagecount, &bitbuffer, sizeof(uint64_t));
	handle_errno("reading from file for pagecount");

	printf("%d,%Ld|", pData->procmapped, bitbuffer);
	pData->memmapped = bitbuffer;
	if (bitbuffer == 1)
		stats->uss += pagesize;
}

void lookup_page(uint64_t index, int fdpagemap, GHashTable *pages,
int fdpageflags, int fdpagecount, struct sizestats *stats)
{
	uint64_t offset;
	uint64_t bitbuffer;
	/*uint8_t pageshift;Bits 55-60 page shift (page size = 1<<page
	 * shift) currently unused*/
	struct pagedata *pData = NULL;

	offset = lseek(fdpagemap, index, SEEK_SET);
	if (offset != index){
		char buffer[BUFFER_SIZE];
		sprintf(buffer, "seeking to offset %Ld using index %Ld",
			 offset, index);
		handle_errno(buffer);
	}
	read(fdpagemap, &bitbuffer, sizeof(uint64_t));
	handle_errno("reading from file for pagemap");

	printf("%016Lx|", bitbuffer);

	/*pageshift = (bitbuffer & PAGESHIFTBITS) >> PAGESHIFT;
	printf("%dbytes|", 1 << pageshift);
	printf("\nbitbuffer & PAGEPRESENT: %016Lx\n",bitbuffer & PAGEPRESENT);
	*/

	if (bitbuffer & PAGEPRESENT){
		printf("Present|");
	}else{
		printf("Absent|\n");
		return;
	}

	if (bitbuffer & PAGESWAPPED){ /*This behaviour cannot be checked at
					this stage.*/
		printf("Swapped|");
	}else{
		uint64_t pfnbits;
		printf("Unswapped|");
		pfnbits = bitbuffer & PFNBITS;
		pData = g_hash_table_lookup(pages, &pfnbits);
		if (pData == NULL){
			pData = malloc(sizeof(*pData));
			handle_errno("allocating memory for new page info");
			pData->procmapped = 1;
			g_hash_table_insert(pages, newkey(pfnbits), pData);
		}else{
			pData->procmapped += 1;
		}

		printf("%Ld|", pfnbits);

		lookup_pagecount(pfnbits, fdpagecount, pData, stats);

		lookup_pageflags(pfnbits, fdpageflags, pData, stats);
	}
	printf("\n");

}

/*lookup_pagemap looks up addresses from the start of the VMA to the end.*/
void lookup_pagemap(uint32_t from, uint32_t to, GHashTable *pages,
int fdpagemap, int fdpageflags, int fdpagecount, struct sizestats *stats)
{
	uint64_t index;
	size_t pagesize = getpagesize();
	size_t stepsize = sizeof(uint64_t);
	size_t fromsize = (from / pagesize) * stepsize;
	size_t tosize = (to / pagesize) * stepsize;

	for(index = fromsize;index < tosize;index += stepsize)
		lookup_page(index, fdpagemap, pages, fdpageflags,
			fdpagecount, stats);

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

int main(int argc, char *argv[])
{
	int fdpagemap;
	int fdpageflags;
	int fdpagecount;
	char pathbuf[PATH_MAX];
	FILE* filemaps;
	char buffer[BUFFER_SIZE];
	uint32_t addrstart;
	uint32_t addrend;
	uint32_t pagecount;
	int loopval;
	size_t pagesize = getpagesize();
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

	if (signal(SIGTERM, cleanup) == SIG_ERR){/*terminate, sent by kill*/
		fprintf(stderr, "Error occurred setting signal handler\n");
		exit(-1);
	}
	if (signal(SIGINT, cleanup) == SIG_ERR){/*interrupt, sent by Ctrl-C
						in terminal */
		fprintf(stderr, "Error occurred setting signal handler\n");
		exit(-1);
	}

	if (argc < 2){
		printf("Format must be %s PID ...\n", argv[0]); 
		exit(-2);
	}
	PIDcount = argc - 1; /*Assuming only arguments are PIDs: start*/
	PIDs = malloc(sizeof(pid_t)*PIDcount);
	handle_errno("allocating memory for list of PIDs");
	for (loopval=1; loopval< argc; loopval++){
		PIDs[loopval-1] = strtoul(argv[loopval], NULL, 0);
		kill(PIDs[loopval-1], SIGSTOP);/*SIGSTOP overrides signal
						*handling, consider SIGSTP*/
		if (DEBUG) 
			printf("Stopping process %s\n", argv[loopval]);
		handle_errno("stopping process");
	}/*Assuming only arguments are PIDs: end*/

	
	sprintf(pathbuf, "/proc/%s/maps", argv[1]);
	filemaps = fopen(pathbuf, "r");
	handle_errno("opening maps file");

	sprintf(pathbuf, "/proc/kpageflags");
	fdpageflags = open(pathbuf, O_RDONLY);
	handle_errno("opening kpageflags file");

	sprintf(pathbuf, "/proc/kpagecount");
	fdpagecount = open(pathbuf, O_RDONLY);
	handle_errno("opening kpagecount file");

	sprintf(pathbuf, "/proc/%s/pagemap", argv[1]);
	fdpagemap = open(pathbuf, O_RDONLY);
	handle_errno("opening pagemap file");

	while (fgets(buffer, BUFFER_SIZE, filemaps) != NULL){
		int retval;
		printf("%s", buffer);
		retval = sscanf(buffer, "%x-%x", &addrstart, &addrend);
		handle_errno("parsing addresses from a line of maps file");
		if (retval < 2){
			fprintf(stderr, "Error: Could not find both"
				" address start and address end\n");
			exit(EXIT_FAILURE);
		}
		pagecount = (addrend - addrstart) / pagesize;
		printf("VMA is %uld pages long\n", pagecount);
		if (pagecount < 1){
			fprintf(stderr, "Error: VMA contains no pages\n");
			exit(EXIT_FAILURE);
		}

		lookup_pagemap(addrstart, addrend, pages, fdpagemap, fdpageflags, fdpagecount, &stats);
	}

	close(fdpagemap);
	handle_errno("closing pagemap file");
	close(fdpageflags);
	handle_errno("closing kpageflags file");
	close(fdpagecount);
	handle_errno("closing kpagecount file");
	fclose(filemaps);
	handle_errno("closing maps file");

	g_hash_table_foreach(pages, countsss, &stats);
	lookup_other_PIDs(PIDs, PIDcount, pages);
	g_hash_table_foreach(pages, countgss, &stats);
	lookup_smaps(PIDs[0], &stats);
	stats.uss /= KBSIZE;
	stats.gss /= KBSIZE;
	stats.sss /= KBSIZE;
	printsummary(&stats);
	cleanup(0);
	exit(0);
}
