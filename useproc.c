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

#define BUFFER_SIZE 256 /*Size of buffers pre-defined as quite large, for lack of knowing exactly what size they need to be*/

/*Bit masks used in pagemap. See documentation/vm/pagemap.txt for a full list of bits*/
#define PAGEPRESENT   0x8000000000000000 /*Bit 63 is page present*/
#define PAGESWAPPED   0x4000000000000000 /*Bit 62 is page swapped*/
#define PFNBITS       0x007FFFFFFFFFFFFF /*Bits 0-54 store the page frame number if page present and not swapped*/
#define PAGESHIFTBITS 0x1F80000000000000 /*Bits 55-60 page shift (page size = 1<<page shift)*/


struct sizestats{
	unsigned int uss; /*Count of pages unique to the process (only mapped once)*/
	unsigned int sss; /*Count of pages only mapped within the process*/
	unsigned int gss; /*Count of pages only mapped by the processes specified as arguments to this program*/
};
struct pagedata {
	unsigned int memmapped; /*Internal store of how many times this page has been mapped by the memory manager*/
	unsigned int procmapped;/*Internal store of how many times this page has been mapped by the PIDs defined*/
};

unsigned int *PIDs; /*Global array of PIDs used, so that they can be restarted if the program is told to terminate early*/
unsigned int PIDcount;

void * newkey(unsigned long long key){ /*Allocates an unsigned long long from argument, to be passed into Hash Table as key*/
	long long *temp = malloc(sizeof(*temp));
	*temp = key;
	return temp;
}
void destroyval(void *val){
	free(val);
}

void cleanup(int signal){
	int loopval = 0;
	char pathbuf[PATH_MAX];
	for (loopval=0; loopval< PIDcount; loopval++){
		sprintf(pathbuf, "kill -CONT %d", PIDs[loopval]);
		system(pathbuf);
	}
	printf("Program terminated successfully\n");
	exit(0);
}

void printsummary(struct sizestats *stats){
	printf("The number of pages used only once in this process (USS): %d\n", stats->uss);
	printf("The number of pages used only by this process (SSS): %d\n", stats->sss);
}

void countgss(void *key, void *value, void *data){
	struct pagedata *page = value;
	struct sizestats *stats = data;
	if (page->memmapped == page->procmapped){
		stats->gss += 1;
	}
}

void countsss(void *key, void *value, void *data){
	struct pagedata *page = value;
	struct sizestats *stats = data;
	if (page->memmapped == page->procmapped){
		stats->sss += 1;
	}
}

int lookup_addresses(const char* buffer, int fd, GHashTable *pages){
	//parse string into addresses and look them up in pagemap (fd)
	//get the PFN (if available) and lookup in hashtable pages.
}

int lookup_other_PIDs(const int *procs, unsigned int proccount, GHashTable *pages){ /*Reminder: Currently not called anywhere*/
	FILE* filemaps;
	int loopcounter;
	for(loopcounter = 1; loopcounter < proccount; loopcounter++){
		char pathbuf[PATH_MAX];
		char buffer[BUFFER_SIZE];
		int PID = procs[loopcounter];
		int fdmaps, fdpagemap;
		sprintf(pathbuf, "/proc/%d/maps", PID);
		fdmaps = open64(pathbuf, O_RDONLY);
		if(errno){
			fprintf(stderr,"Error occurred opening %s\n", pathbuf);
			exit(errno);
		}
		sprintf(pathbuf, "/proc/%d/pagemap", PID);
		fdpagemap = open64(pathbuf, O_RDONLY);
		if(errno){
			fprintf(stderr,"Error occurred opening %s\n", pathbuf);
			exit(errno);
		}
		filemaps = fdopen(fdmaps, "r");
		if(errno){
			fprintf(stderr,"Error occurred making file for maps\n");
		}
		while(fgets(buffer, BUFFER_SIZE,filemaps) != NULL){
			lookup_addresses(buffer, fdpagemap, pages);
		}
		close(fdmaps);
		if(errno){
			fprintf(stderr,"Error occurred closing maps file\n");
			exit(errno);
		}
		close(fdpagemap);
		if(errno){
			fprintf(stderr,"Error occurred closing pagemap file\n");
			exit(errno);
		}
	}

}

int lookup_pageflags(unsigned long long PFN, int fdpageflags, struct pagedata* pData, struct sizestats *stats){
	unsigned long long index;
	unsigned long long offset;
	unsigned long long bitbuffer;
	int retval = 0;
	index = PFN*8;
	offset = lseek64(fdpageflags, index, SEEK_SET);
	if (offset != index){
		fprintf(stderr, "Error seeking to offset %Ld using index %Ld\n", offset, index);
		exit(errno);
	}
	retval = read(fdpageflags, &bitbuffer, sizeof(char)*8);
	if (retval < 0){
		fprintf(stderr, "Error occurred reading from file for pageflags\n");
		exit(errno);
	}
	printf("%016Lx|",bitbuffer);
	return 0;
}

int lookup_pagecount(unsigned long long PFN, int fdpagecount, struct pagedata *pData, struct sizestats *stats){
	unsigned long long index;
	unsigned long long offset;
	unsigned long long bitbuffer;
	int retval = 0;
	index = PFN*8;
	offset = lseek64(fdpagecount, index, SEEK_SET);
	if (offset != index){
		fprintf(stderr, "Error seeking to offset %Ld using index %Ld\n", offset, index);
		printf("errno : %d",errno);
		exit(errno);
	}
	retval = read(fdpagecount, &bitbuffer, sizeof(char)*8);
	if (retval < 0){
		fprintf(stderr, "Error occurred reading from file for pagecount\n");
		exit(errno);
	}
	printf("%d,%Ld|",pData->procmapped, bitbuffer);
	pData->memmapped = bitbuffer;
	if (bitbuffer == 1){
		stats->uss += 1;
	}
	return 0;
}

int lookup_page(unsigned long long index, int fdpagemap, GHashTable *pages, int fdpageflags, int fdpagecount, struct sizestats *stats){
	unsigned long long offset, bitbuffer;
	int retval, pageshift;
	struct pagedata *pData = NULL;
	//printf("index page : %Ld\n", index);
	offset = lseek64(fdpagemap, index, SEEK_SET);
	if (offset != index){
		fprintf(stderr, "Error seeking to offset %Ld using index %Ld\n", offset, index);
		exit(errno); 
	}
	retval = read(fdpagemap, &bitbuffer, sizeof(char)*8);
	if (retval < 0){
		fprintf(stderr, "Error occurred reading from file for pagemap\n");
		exit(errno);
	}
	printf("%016Lx|", bitbuffer);
	//pageshift = (bitbuffer & PAGESHIFTBITS) >> 55;
	//printf("%dbytes|", 1 << pageshift);
	//printf("\nbitbuffer & PAGEPRESENT: %016Lx\n",bitbuffer & PAGEPRESENT);
	if (bitbuffer & PAGEPRESENT){
		printf("Present|");
	}else{
		printf("Absent|\n");
		return 0;
	}
	//printf("\nbitbuffer & PAGESWAPPED: %016Lx&%016Lx=%016Lx\n",bitbuffer, PAGESWAPPED, bitbuffer & PAGESWAPPED);
	if (bitbuffer & PAGESWAPPED){ //This behaviour cannot be checked at this stage.
		printf("Swapped|");
	}else{
		unsigned long long pfnbits;
		printf("Unswapped|");
		pfnbits = bitbuffer & PFNBITS;
		pData = g_hash_table_lookup(pages, &pfnbits);
		if (pData == NULL){
			pData = malloc(sizeof(*pData));
			pData->procmapped = 1;
			g_hash_table_insert(pages, newkey(pfnbits), pData);
		}else{
			pData->procmapped += 1;
		}
	//g_hash_table_insert(hashtable, newkey(13), newval(1));
		printf("%Ld|", pfnbits);
		retval = lookup_pagecount(pfnbits, fdpagecount, pData, stats);
		if (retval){
			fprintf(stderr, "Error code %d\n", errno);
			exit(retval);
		}
		retval = lookup_pageflags(pfnbits, fdpageflags, pData, stats);
		if (retval){
			fprintf(stderr, "Error code %d\n", errno);
			exit(retval);
		}
	}
	printf("\n");
	return 0;
}

//lookup_pagemap looks up addresses from the start of the VMA to the end.
int lookup_pagemap(unsigned long from, unsigned long to, const char* path, GHashTable *pages, int fdpagemap, int fdpageflags, int fdpagecount, struct sizestats *stats){
	unsigned long long index,offset;
	int retval = 0;
	size_t pagesize = getpagesize();
	size_t stepsize = sizeof(char)*8;
	size_t fromsize = from/pagesize * stepsize;
	size_t tosize = to/pagesize*stepsize;

	for(index=fromsize;index<tosize;index+= stepsize){
		retval = lookup_page(index, fdpagemap, pages, fdpageflags, fdpagecount, stats);
		if (retval){
			fprintf(stderr, "Error occurred looking up %Ld in %s \n", index, path);
			exit(retval);
		}
	}

	return 0;
}

int main(int argc, char *argv[]){
	int fdmaps, fdpagemap, fdpageflags, fdpagecount;
	char pathbuf[PATH_MAX];
	FILE* filemaps;
	char buffer[BUFFER_SIZE];
	unsigned long addrstart, addrend, pagecount;
	int retval, loopval;
	size_t pagesize = getpagesize();
	GHashTable *pages = g_hash_table_new_full(g_int64_hash,
		 g_int64_equal, &destroyval, &destroyval);
	struct sizestats stats;
		stats.uss = 0;
		stats.gss = 0;
		stats.sss = 0;

	if (signal(SIGTERM, cleanup) == SIG_ERR){//terminate, sent by kill
		fprintf(stderr, "Error occurred setting signal handler\n");
		exit(-1);
	}
	if (signal(SIGINT, cleanup) == SIG_ERR){//interrupt, sent by Ctrl-C in terminal 
		fprintf(stderr, "Error occurred setting signal handler\n");
		exit(-1);
	}

	//Check for valid input
	if (argc < 2){
		printf("Format must be %s PID ...\n", argv[0]); //currently only supporting one process inspected at once.
		exit(-2);
	}
	PIDcount = argc - 1; //Assuming only arguments are PIDs: start
	PIDs = malloc(sizeof(unsigned int)*PIDcount);
	
	for (loopval=1; loopval< argc; loopval++){
		sprintf(pathbuf, "kill -STOP %s", argv[loopval]);
		PIDs[loopval-1] = strtoul(argv[loopval], NULL, 0);
		retval = system(pathbuf);
		printf("%s returned %d\n", pathbuf, retval);
		if (retval != 0){
			exit(retval);
		}
	}//Assuming only arguments are PIDs: end

	// Open up maps procfile
	sprintf(pathbuf, "/proc/%s/maps", argv[1]);
	fdmaps = open64(pathbuf, O_RDONLY);
	if (fdmaps < 0){
		fprintf(stderr, "Error occurred opening %s\n", pathbuf);
		exit(errno);
	}

	filemaps = fdopen(fdmaps, "r");
	sprintf(pathbuf, "/proc/%s/pagemap", argv[1]);
	fdpagemap = open64(pathbuf, O_RDONLY);
	if (fdpagemap < 0){
		fprintf(stderr, "Error occurred opening %s\n", pathbuf);
		exit(errno);
	}
	sprintf(pathbuf, "/proc/kpageflags");
	fdpageflags = open64(pathbuf, O_RDONLY);
	if (fdpageflags < 0){
		fprintf(stderr, "Error occurred opening %s\n", pathbuf);
		exit(errno);
	}
	sprintf(pathbuf, "/proc/kpagecount");
	fdpagecount = open64(pathbuf, O_RDONLY);
	if (fdpagecount < 0){
		fprintf(stderr, "Error occurred opening %s\n", pathbuf);
		exit(errno);
	}


	sprintf(pathbuf, "/proc/%s/pagemap", argv[1]);//no important but this is the pathbuf

	while (fgets(buffer, BUFFER_SIZE, filemaps) != NULL){
		printf("%s", buffer);
		retval = sscanf(buffer, "%lx-%lx", &addrstart, &addrend);
		if (retval < 2){
			fprintf(stderr, "Error: Could not find both address start and address end\n");
			exit(errno);
		}
		pagecount = (addrend - addrstart)/pagesize;
		printf("VMA is %ld pages long\n", pagecount);
		if (pagecount < 1){
			fprintf(stderr, "Error: VMA contains no pages\n");
			exit(-3);
		}

		retval=lookup_pagemap(addrstart, addrend, pathbuf, pages, fdpagemap, fdpageflags, fdpagecount, &stats);
		if(retval != 0){
			fprintf(stderr, "Error occurred looking up pagemap\n");
			exit(retval);
		}
		//break;
	}
	close(fdmaps);
	if(errno){
		fprintf(stderr, "Error occurred closing maps file\n");
		exit(errno);
	}
	close(fdpagemap);
	if(errno){
		fprintf(stderr, "Error occurred closing pagemap file\n");
		exit(errno);
	}
	close(fdpageflags);
	if(errno){
		fprintf(stderr, "Error occurred closing pageflags file\n");
		exit(errno);
	}
	close(fdpagecount);
	if(errno){
		fprintf(stderr, "Error occurred closing pagecount file\n");
		exit(errno);
	}
	fclose(filemaps);
	if(errno){
		fprintf(stderr, "Error occurred closing maps file pointer\n");
		exit(errno);
	}
	
	g_hash_table_foreach(pages, countsss, &stats);

	printsummary(&stats);
	cleanup(0);
	return 0;
}
