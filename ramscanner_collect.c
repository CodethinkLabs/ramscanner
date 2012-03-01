#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "ramscanner_collect.h"


void
parse_smaps_file(FILE *file, sizestats *stats)
{
/*
 * Extracts numerical data from the smaps file. Depends on the format staying as
 * Type:   [val] kB
 * Warning! May get confused by paths which contain text of format Type:[val] kB
 */
	char buffer[BUFSIZ];
	char *pt = buffer;
	char *pos = NULL;
	size_t n = 0;
	uint32_t temp;
	uint32_t arg1;
	uint32_t arg2;
	while (getline(&pt, &n, file) > 0) {
		if (sscanf(pt, "%x-%x", &arg1, &arg2) == 2) {
			/* First line of a vma detected, do nothing. */
		} else if (strstr(pt, SMAPS_VSS) != NULL) {
			pos = strchr(pt, ':');
			sscanf(pos + 1, "%d kB", &temp);
			stats->vss += temp;
		} else if (strstr(pt, SMAPS_RSS) != NULL) {
			pos = strchr(pt, ':');
			sscanf(pos + 1, "%d kB", &temp);
			stats->rss += temp;
		} else if (strstr(pt, SMAPS_PSS) != NULL) {
			pos = strchr(pt, ':');
			sscanf(pos + 1, "%d kB", &temp);
			stats->pss += temp;
		} else if (strstr(pt, SMAPS_REFD) != NULL) {
			pos = strchr(pt, ':');
			sscanf(pos + 1, "%d kB", &temp);
			stats->refd += temp;
		} else if (strstr(pt, SMAPS_ANON) != NULL) {
			pos = strchr(pt, ':');
			sscanf(pos + 1, "%d kB", &temp);
			stats->anon += temp;
		} else if (strstr(pt, SMAPS_SWAP) != NULL) {
			pos = strchr(pt, ':');
			sscanf(pos + 1, "%d kB", &temp);
			stats->swap += temp;
		} else if (strstr(pt, SMAPS_LOCKED) != NULL) {
			pos = strchr(pt, ':');
			sscanf(pos + 1, "%d kB", &temp);
			stats->locked += temp;
		}
	}
}

void
countgss(void *key, void *value, void *data)
{
	size_t pagesize = getpagesize();
	pagesummarydata *page = value;
	sizestats *stats = data;
	if (page->memmapped == page->procmapped)
		stats->gss += pagesize/KBSIZE;
}

void
countsss(void *key, void *value, void *data)
{
	size_t pagesize = getpagesize();
	pagesummarydata *page = value;
	sizestats *stats = data;
	if (page->memmapped == page->procmapped)
		stats->sss += pagesize/KBSIZE;
}

void
lookup_smaps(pid_t PID, sizestats *stats)
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

/**
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
void
use_pfn(uint64_t pfn, options *opt, FILE *filepageflags, FILE *filepagecount, 
        pagedetaildata *currentdpage)
{

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
	ret = fseek(filepagecount, index, SEEK_SET);
	if (ret == -1) {
		perror("Error seeking in kpagecount");
		cleanup_and_exit(EXIT_FAILURE);
	}

	errno = 0;
	ret = fread(&bitfield, sizeof(bitfield), 1, filepagecount);
	if (ret != 1) {
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
	ret = fseek(filepageflags, index, SEEK_SET);
	if (ret == -1) {
		perror("Error seeking in kpageflags");
		cleanup_and_exit(EXIT_FAILURE);
	}
	errno = 0;
	ret = fread(&bitfield, sizeof(bitfield), 1, filepageflags);
	if (ret != 1) {
		perror("Error reading kpageflags");
		cleanup_and_exit(EXIT_FAILURE);
	}
	store_flags_in_page(bitfield, currentdpage);
}

void parse_bitfield(uint64_t bitfield, options *opt, FILE *filepageflags, 
                    FILE *filepagecount, pagedetaildata *currentdpage)
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

	use_pfn(pfnbits, opt, filepageflags, filepagecount, currentdpage);
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

void
lookup_pagemap_with_addresses(uint32_t addressfrom, uint32_t addressto, 
                              options *opt, FILE *filepageflags,
                              FILE *filepagecount, FILE *filepagemap,
                              uint16_t vmaindex)
{
/*
 * Looks up every entry over the index range in pagemap and reads the 64-bit
 * bitfield into a uint64_t.
 * Calls parse_bitfield to interpret the meanings of each bit
 */

	size_t entrysize = sizeof(uint64_t);
	size_t pagesize = getpagesize();
	pagedetaildata *previousdpage = NULL;
	pagedetaildata *currentdpage;
	errno = 0;
	currentdpage = calloc(1, sizeof(*currentdpage));
	if (currentdpage == NULL) {
		perror("Error occurred allocating memory for detail page");
		cleanup_and_exit(EXIT_FAILURE);
	}

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
		o = fseek(filepagemap, i, SEEK_SET);
		if (o == -1) {
			perror("Error seeking in pagemap");
			cleanup_and_exit(EXIT_FAILURE);
		}
		errno = 0;
		ret = fread(&bitfield, sizeof(bitfield), 1, filepagemap);
		if (ret != 1) {
			perror("Error reading pagemap");
			cleanup_and_exit(EXIT_FAILURE);
		}

		currentdpage->vmaindex = vmaindex;
		currentdpage->addrstart = inum * pagesize;
		currentdpage->addrend = (inum + 1) * pagesize;

		parse_bitfield(bitfield, opt, 
		               filepageflags, filepagecount, currentdpage);

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
			errno = 0;
			currentdpage = calloc(1, sizeof(*currentdpage));
			if (currentdpage == NULL) {
				perror("Error occurred allocating memory for"
				       " detail page");
				cleanup_and_exit(EXIT_FAILURE);
			}
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

void
lookup_maps_with_PID(pid_t pid, options *opt, FILE *filepageflags, 
                     FILE *filepagecount)
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
	FILE *filepagemap;
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
	filepagemap = fopen(pathbuf, "r");
	if (filepagemap == NULL) {
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
		                              filepageflags, filepagecount,
		                              filepagemap, vmaindex);
	}

	errno = 0;
	ret = fclose(filemaps);
	if (ret == EOF) {
		perror("Error closing maps file");
		errno = 0;
	}
	errno = 0;
	ret = fclose(filepagemap);
	if (ret == EOF) {
		perror("Error closing pagemap file");
		errno = 0;
	}
}

void
inspect_processes(options *opt)
{
/* 
 * Function to do most of the hard work. flags tell it what to do, and pids
 * tells it which processes to use. The first PID (primary PID) is inspected
 * in detail, finding out information about each page. The other PIDs (secondary
 * PIDs) are used to calculate Gss
 */

	char pathbuf[PATH_MAX]; /* Buffer for storing path to open files*/

	FILE *filepageflags;/* file descriptor for /proc/kpageflags */
	FILE *filepagecount;/* ditto for /proc/kpagecount */
	int i;
	int ret;

	if (opt->detail || opt->compactdetail) {
		sprintf(pathbuf, "%s/%s", PROC_PATH, KPAGEFLAGS_FILENAME);
		errno = 0;
		filepageflags = fopen(pathbuf, "r");
		if (filepageflags == NULL) {
			perror("Error occurred opening kpageflags");
			cleanup_and_exit(EXIT_FAILURE);
		}
	}

	sprintf(pathbuf, "%s/%s", PROC_PATH, KPAGECOUNT_FILENAME);
	errno = 0;
	filepagecount = fopen(pathbuf, "r");
	if (filepagecount == NULL) {
		perror("Error occurred opening kpagecount");
		cleanup_and_exit(EXIT_FAILURE);
	}

	lookup_maps_with_PID(opt->pids[0], opt,
	                     filepageflags, filepagecount);

	if (opt->summary) {
		options opt2 = *opt;
		g_hash_table_foreach(opt->summarypages, countsss,
		                     &(opt->summarystats));

		opt2.summary = 0;
		opt2.detail = 0;
		opt2.compactdetail = 0;
		for (i = 1; i < opt->pidcount; i++) {
			lookup_maps_with_PID(opt->pids[i], &opt2,
			                     filepageflags,
			                     filepagecount);		
		}


		g_hash_table_foreach(opt->summarypages, countgss,
		                     &(opt->summarystats));
		lookup_smaps(opt->pids[0], &(opt->summarystats));

	}
	if (opt->detail || opt->compactdetail) {
		errno = 0;
		ret = fclose(filepageflags);
		if (ret == EOF) {
			perror("Error closing kpageflags file");
			errno = 0;
		}
	}
	errno = 0;
	ret = fclose(filepagecount);
	if (ret == EOF) {
		perror("Error closing kpagecount file");
		errno = 0;
	}
	
}

