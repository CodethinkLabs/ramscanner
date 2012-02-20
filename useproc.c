//Makes use of proc files to retrieve page information about a defined process
#include<stdio.h>
#include<stdlib.h>
#include<linux/limits.h>
#include<errno.h>
#include<unistd.h>
#include<fcntl.h>
#include<glib.h>
#include<sys/types.h>
#define _LARGEFILE64_SOURCE

#define BUFFER_SIZE 256

#define PAGEPRESENT   0x8000000000000000
//#define PAGEPRESENT   0x1
#define PAGESWAPPED   0x4000000000000000
//#define PAGESWAPPED   0x2
#define PFNBITS       0x007FFFFFFFFFFFFF
//#define PFNBITS       0xFFFFFFFFFFFFF000
#define PAGESHIFTBITS 0x1F80000000000000
//#define PAGESHIFTBITS 0x1F8

struct pagedata {
	int memmapped;
	int procmapped;
};

void * newkey(unsigned long long key){
	long long *temp = malloc(sizeof(*temp));
	*temp = key;
	return temp;
}
/* newval is deprecated because hashtable stores structs now.
void * newval(unsigned int val){
	unsigned int *temp =  malloc(sizeof(*temp));
	*temp = val;
	return temp;
}
*/
void destroyval(void *val){
	free(val);
}
/* Deprecated because hashtable isn't using int as value.
void printvals(void* key, void* value, void* userdata){
	unsigned long long *castkey = key;
	unsigned int *castvalue = value;
	printf("key= %Ld, value= %d\n", *castkey, *castvalue);
}
*/

int lookup_pageflags(unsigned long long PFN, int fdpageflags, struct pagedata* pData){
	unsigned long long index;
	unsigned long long offset;
	unsigned long long bitbuffer;
	int retval = 0;
	index = PFN*8;
	offset = lseek64(fdpageflags, index, SEEK_SET);
	if (offset != index){
		fprintf(stderr, "Error seeking to offset %Ld using index %Ld\n", offset, index);
		return errno;
	}
	retval = read(fdpageflags, &bitbuffer, sizeof(char)*8);
	if (retval < 0){
		fprintf(stderr, "Error occurred reading from file for pageflags\n");
		return errno;
	}
	printf("%016Lx|",bitbuffer);
	return 0;
}

int lookup_pagecount(unsigned long long PFN, int fdpagecount, struct pagedata *pData){
	unsigned long long index;
	unsigned long long offset;
	unsigned long long bitbuffer;
	int retval = 0;
	index = PFN*8;
	offset = lseek64(fdpagecount, index, SEEK_SET);
	if (offset != index){
		fprintf(stderr, "Error seeking to offset %Ld using index %Ld\n", offset, index);
		printf("errno : %d",errno);
		return errno;
	}
	retval = read(fdpagecount, &bitbuffer, sizeof(char)*8);
	if (retval < 0){
		fprintf(stderr, "Error occurred reading from file for pagecount\n");
		return errno;
	}
	printf("%d,%Ld|",pData->procmapped, bitbuffer);
	pData->memmapped = bitbuffer;
	if (pData->procmapped > bitbuffer){
		printf("Page mapped by same process more than once!");
	}
	return 0;
}

int lookup_page(unsigned long long index, int fdpagemap, GHashTable *pages, int fdpageflags, int fdpagecount){
	unsigned long long offset, bitbuffer;
	int retval, pageshift;
	struct pagedata *pData = NULL;
	//printf("index page : %Ld\n", index);
	offset = lseek64(fdpagemap, index, SEEK_SET);
	if (offset != index){
		fprintf(stderr, "Error seeking to offset %Ld using index %Ld\n", offset, index);
		return errno; 
	}
	retval = read(fdpagemap, &bitbuffer, sizeof(char)*8);
	if (retval < 0){
		fprintf(stderr, "Error occurred reading from file for pagemap\n");
		return errno;
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
		retval = lookup_pagecount(pfnbits, fdpagecount, pData);
		if (retval){
			fprintf(stderr, "Error code %d\n", errno);
			return retval;
		}
		retval = lookup_pageflags(pfnbits, fdpageflags, pData);
		if (retval){
			fprintf(stderr, "Error code %d\n", errno);
			return retval;
		}
	}
	printf("\n");
	return 0;
}

//lookup_pagemap looks up addresses from the start of the VMA to the end.
int lookup_pagemap(unsigned long from, unsigned long to, const char* path, GHashTable *pages, int fdpagemap, int fdpageflags, int fdpagecount){
	unsigned long long index,offset;
	int retval = 0;

	size_t pagesize = getpagesize();
	size_t stepsize = sizeof(char)*8;
	size_t fromsize = from/pagesize * stepsize;
	size_t tosize = to/pagesize*stepsize;
	//start looking in pagemap
/**/
	//printf("SA:%lx||FA:%lx\n",from, to);
	//printf("PS:%d||SS:%d||FS:%d||TS:%d||\n",pagesize,stepsize,fromsize,tosize);
/**/

	for(index=fromsize;index<tosize;index+= stepsize){
		//printf("index pagemap %Ld\n", index);
		retval = lookup_page(index, fdpagemap, pages, fdpageflags, fdpagecount);
		if (retval){
			fprintf(stderr, "Error occurred looking up %Ld in %s \n", index, path);
			break;
		}
	}

	return retval;
}

int main(int argc, char *argv[]){
	int fdmaps, fdpagemap, fdpageflags, fdpagecount;
	char pathbuf[PATH_MAX];
	FILE* filemaps;
	char buffer[BUFFER_SIZE];
	unsigned long addrstart, addrend, pagecount;
	int retval;
	size_t pagesize = getpagesize();


	GHashTable *pages = g_hash_table_new_full(g_int64_hash,
		 g_int64_equal, &destroyval, &destroyval);

	//g_hash_table_insert(hashtable, newkey(13), newval(1));
	//g_hash_table_insert(hashtable, newkey(5), newval(2));
	//int lookval = 5;
	//printf("value for key %d is %d\n", lookval, *(unsigned int*) g_hash_table_lookup(hashtable, &lookval));
	//g_hash_table_foreach(hashtable, &printvals, NULL);
	//g_hash_table_destroy(hashtable); Hash table not cleaned up, exists until end of program.

	//Check for valid input
	if (argc!= 2){
		printf("Format must be %s PID\n", argv[0]); //currently only supporting one process inspected at once.
		return 2;
	}
	// Open up maps procfile
	sprintf(pathbuf, "/proc/%s/maps", argv[1]);
	fdmaps = open64(pathbuf, O_RDONLY);
	if (fdmaps < 0){
		fprintf(stderr, "Error occurred opening %s\n", pathbuf);
		return errno;
	}
	
	filemaps = fdopen(fdmaps, "r");
	sprintf(pathbuf, "/proc/%s/pagemap", argv[1]);
	fdpagemap = open64(pathbuf, O_RDONLY);
	if (fdpagemap < 0){
		fprintf(stderr, "Error occurred opening %s\n", pathbuf);
		return errno;
	}
	sprintf(pathbuf, "/proc/kpageflags");
	fdpageflags = open64(pathbuf, O_RDONLY);
	if (fdpageflags < 0){
		fprintf(stderr, "Error occurred opening %s\n", pathbuf);
		return errno;
	}
	sprintf(pathbuf, "/proc/kpagecount");
	fdpagecount = open64(pathbuf, O_RDONLY);
	if (fdpagecount < 0){
		fprintf(stderr, "Error occurred opening %s\n", pathbuf);
		return errno;
	}


	sprintf(pathbuf, "/proc/%s/pagemap", argv[1]);//no important but this is the pathbuf

	while (fgets(buffer, BUFFER_SIZE, filemaps) != NULL){
		printf("%s", buffer);
		retval = sscanf(buffer, "%lx-%lx", &addrstart, &addrend);
		if (retval < 2){
			fprintf(stderr, "Error: Could not find both address start and address end\n");
			return errno;
		}
		pagecount = (addrend - addrstart)/pagesize;
		printf("VMA is %ld pages long\n", pagecount);
		if (pagecount < 1){
			fprintf(stderr, "Error: VMA contains no pages\n");
			return 1;
		}

		retval=lookup_pagemap(addrstart, addrend, pathbuf, pages, fdpagemap, fdpageflags, fdpagecount);
		if(retval != 0){
			fprintf(stderr, "Error occurred looking up pagemap\n");
			return retval;
		}
		//break;
	}

	return 0;
}
