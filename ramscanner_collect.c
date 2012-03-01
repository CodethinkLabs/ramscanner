#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "ramscanner_collect.h"

/**
 * Extracts data from the process' smaps file (see "man proc" for details on 
 * smaps) and stores it in the sizestats struct. It is dependent on the format
 * of the output of smaps remaining as: [Address]-[Address] for the first line
 * of an entry, and Label:  [size] kB for each size stat. 
 * 
 * For example:
 * b77f3000-b77f4000 r--p 002a1000 08:01 15343049 /usr/lib/locale/locale-archive
 * Size:                  4 kB
 * Rss:                   4 kB
 * Pss:                   0 kB
 * Shared_Clean:          4 kB
 * Shared_Dirty:          0 kB
 * Private_Clean:         0 kB
 * Private_Dirty:         0 kB
 * Referenced:            4 kB
 * Anonymous:             0 kB
 * Swap:                  0 kB
 * KernelPageSize:        4 kB
 * MMUPageSize:           4 kB
 * Locked:                0 kB
 *
 * It will extract Size, Rss, Pss, Referenced, Anonymous, Swap and Locked for
 * each entry.
 */
static void
parse_smaps_file(FILE *file, sizestats *stats)
{
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

/**
 * A function called in a g_hash_table_foreach(). If the page is mapped only by
 * the primary and secondary processes, it increments the Group Set Size by the
 * size of the page in kB.
 */
static void
countgss(void *key, void *value, void *data)
{
	size_t pagesize = getpagesize();
	pagesummarydata *page = value;
	sizestats *stats = data;
	if (page->memmapped == page->procmapped)
		stats->gss += pagesize/KBSIZE;
}

/**
 * A function called in a g_hash_table_foreach(). If the page is mapped only by
 * the primary process, it increments the Self Set Size by the size of the page
 * in kB.
 */
static void
countsss(void *key, void *value, void *data)
{
	size_t pagesize = getpagesize();
	pagesummarydata *page = value;
	sizestats *stats = data;
	if (page->memmapped == page->procmapped)
		stats->sss += pagesize/KBSIZE;
}

/**
 * A function to open the /proc/PID/smaps file, then call parse_smaps_file() to
 * read the smaps file and store the results.
 */
static void
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
		errno = 0;
	}
}

/**
 * A function to inspect the page flags extracted and store the results in the
 * page detail data struct.
 */
static void
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
 * /proc/kpagecount. If it has only been mapped once then the page's size will
 * be added to the Unique Set Size (Uss).
 * If the program has been told to look for details, it will look up 
 * /proc/kpageflags and retrieve a 64-bit bitfield of the flags set on that 
 * process, and pass it to the function store_flags_in_page().
 */
static void
use_pfn(uint64_t pfn, options *opt, FILE *filepageflags, FILE *filepagecount, 
        pagedetaildata *currentdpage)
{
	int ret;
	uint64_t bitfield;

	size_t elementsize = sizeof(uint64_t);
	uint64_t index = pfn * elementsize;

	pagesummarydata *pData = NULL;
	if (opt->summarypages) {
		pData = g_hash_table_lookup(opt->summarypages, &pfn);

		if (pData == NULL) {
			if (opt->summary) {
				errno = 0;
				pData = malloc(sizeof(*pData));
				if (pData == NULL) {
					perror("Error allocating page summary"
					       " data");
					cleanup_and_exit(EXIT_FAILURE);
				}
				pData->procmapped = 1;
				pData->memmapped = 0; /*Indicates a new page*/
				g_hash_table_insert(opt->summarypages, 
				                    newkey(pfn), pData);
			}
		} else {
			pData->procmapped += 1;
		}
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

	if (opt->detail || opt->compactdetail) {
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
}

/**
 * Is passed the 64-bit bitfield retrieved from the process' pagemap, and parses
 * this information. If the process is not swapped then it has a PFN, which it
 * will pass to the function use_pfn() to extract information from 
 * /proc/kpagemaps and /proc/kpagecount.
 */
static void
parse_bitfield(uint64_t bitfield, options *opt, FILE *filepageflags, 
               FILE *filepagecount, pagedetaildata *currentdpage)
{
	uint64_t pfnbits;

	if (opt->detail || opt->compactdetail) {
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
		/* Omitting swap type and swap offset information. */
		if (opt->detail || opt->compactdetail)
			currentdpage->swap = 1;
		return;
	}
	if (opt->detail || opt->compactdetail)
		currentdpage->swap = 0;
	pfnbits = bitfield & PFNBITS;

	if (opt->detail || opt->compactdetail)
		currentdpage->pfn = pfnbits;

	use_pfn(pfnbits, opt, filepageflags, filepagecount, currentdpage);
}

/**
 * Checks whether two page detail data structs are identical enough to merge 
 * together.
 * Checks whether the two page detail data structs are adjacent, assuming that
 * prev is always before curr.
 * It ignores the PFN from identity checks because checking PFN would check if
 * it is the same page, rather than one with the exact same flags.
 */
static int
are_pages_identical_and_adjacent(pagedetaildata *prev, pagedetaildata *curr)
{
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

/**
 * Looks up every entry in /proc/PID/pagemap over the range addressfrom to 
 * addressto, creating a pagedetaildata struct and filling it by using
 * parse_bitfield(). If that page is both identical and adjacent to the previous
 * page made, then it merges the two pages together by changing the address of
 * the end of the previous page to the end address of the new page, then
 * discarding the new page. If they are not adjacent and identical, it stores
 * the new page in the GHashTable opt->detailpages.
 */
static void
lookup_pagemap_with_addresses(uint32_t addressfrom, uint32_t addressto, 
                              options *opt, FILE *filepageflags,
                              FILE *filepagecount, FILE *filepagemap,
                              uint16_t vmaindex)
{
	size_t entrysize = sizeof(uint64_t);
	size_t pagesize = getpagesize();
	pagedetaildata *previousdpage = NULL;
	pagedetaildata *currentdpage;

	if (opt->detail || opt->compactdetail) {
		errno = 0;
		currentdpage = calloc(1, sizeof(*currentdpage));
		if (currentdpage == NULL) {
			perror("Error occurred allocating memory for detail page");
			cleanup_and_exit(EXIT_FAILURE);
		}
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
		if (opt->detail || opt->compactdetail) {
			currentdpage->vmaindex = vmaindex;
			currentdpage->addrstart = inum * pagesize;
			currentdpage->addrend = (inum + 1) * pagesize;
		}
		parse_bitfield(bitfield, opt, 
		               filepageflags, filepagecount, currentdpage);

		if (opt->detail || opt->compactdetail) {
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
					perror("Error occurred allocating "
					       "memory for detail page");
					cleanup_and_exit(EXIT_FAILURE);
				}
			}
		}
	}
}

/**
 * opt stores an array of vmastats structs so that they persist, can be access,
 * and cleaned up at the end of the program. This makes another vmastats
 * available and returns a pointer to it. The vmastats pointer will not persist,
 * as realloc does not guarantee memory stays in the same place, so persistent
 * access to this struct requires storing an index, which is most easily
 * accessed for the newest vmastats struct by getting (opt->vmacount - 1).
 */
static vmastats *
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

/**
 * Uses the PID to access the appropriate /proc/PID/maps file, which contains
 * permissions and path information about each Virtual Memory Area (VMA), 
 * and the region that memory addresses exist for. Assumes constant-sized pages
 * to pass the range of indexes to be accessed in /proc/PID/pagemap.
 * Calls the function lookup_pagemap_with_addresses() with the range of indexes
 * to interrogate each listing in pagemap
 */
static void
lookup_maps_with_PID(pid_t pid, options *opt, FILE *filepageflags, 
                     FILE *filepagecount)
{
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

/**
 * Calculates all the information for ramscanner, storing it in the GHashTables
 * opt->summarypages and opt->detailpages, the vmastats array opt->vmas, and the
 * sizestats struct opt->summarystats, using opt->summary, opt->detail and
 * opt->compactdetail to identify how much work it needs to do.
 */
void
inspect_processes(options *opt)
{
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

