#ifndef RAMSCANNER_COLLECT_H
#define RAMSCANNER_COLLECT_H

#include "ramscanner_common.h"

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

static void
parse_smaps_file(FILE *file, sizestats *stats);

static void
countgss(void *key, void *value, void *data);

static void
countsss(void *key, void *value, void *data);

static void
lookup_smaps(pid_t PID, sizestats *stats);

static void
store_flags_in_page(uint64_t bitfield, pagedetaildata *currentdpage);

static void
use_pfn(uint64_t pfn, options *opt, FILE *filepageflags, FILE *filepagecount, 
        pagedetaildata *currentdpage);

static void
parse_bitfield(uint64_t bitfield, options *opt, FILE *filepageflags, 
               FILE *filepagecount, pagedetaildata *currentdpage);

static int
are_pages_identical_and_adjacent(pagedetaildata *prev, pagedetaildata *curr);

static void
lookup_pagemap_with_addresses(uint32_t addressfrom, uint32_t addressto, 
                              options *opt, FILE *filepageflags,
                              FILE *filepagecount, FILE *filepagemap,
                              uint16_t vmaindex);

static vmastats *
make_another_vmst_in_opt(options *opt);

static void
lookup_maps_with_PID(pid_t pid, options *opt, FILE *filepageflags, 
                     FILE *filepagecount);

void
inspect_processes(options *opt);


#endif
