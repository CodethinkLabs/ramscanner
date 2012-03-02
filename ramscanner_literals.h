#ifndef RAMSCANNER_LITERALS_H
#define RAMSCANNER_LITERALS_H
/**
 * Macros used in parsing smaps.
 */
#define SMAPS_VSS "Size"
#define SMAPS_RSS "Rss"
#define SMAPS_PSS "Pss"
#define SMAPS_REFD "Referenced"
#define SMAPS_ANON "Anonymous"
#define SMAPS_SWAP "Swap"
#define SMAPS_LOCKED "Locked"

/**
 * Macros used in printing the summary.
 */
#define SUMMARY_VSS "Vss"
#define SUMMARY_RSS "Rss"
#define SUMMARY_PSS "Pss"
#define SUMMARY_USS "Uss"
#define SUMMARY_SSS "Sss"
#define SUMMARY_GSS "Gss"
#define SUMMARY_REFD "Referenced"
#define SUMMARY_SWAP "Swap"
#define SUMMARY_ANON "Anonymous"
#define SUMMARY_LOCKED "Locked"

/**
 * Path macros for accessing the proc files. Defined here so that changes are
 * easier to account for
 */
#define PROC_PATH "/proc"
#define KPAGEFLAGS_FILENAME "kpageflags"
#define KPAGECOUNT_FILENAME "kpagecount"
#define MAPS_FILENAME "maps"
#define SMAPS_FILENAME "smaps"
#define PAGEMAP_FILENAME "pagemap"

/**
 * Macros used in printing the details' title.
 */
#define DETAIL_ADDRSTARTTITLE "from"
#define DETAIL_ADDRENDTITLE "to"
#define DETAIL_PERMTITLE "permissions"
#define DETAIL_PATHTITLE "path"
#define DETAIL_PRESENTTITLE "present"
#define DETAIL_SIZETITLE "size B"
#define DETAIL_SWAPTITLE "swap"
#define DETAIL_PFNTITLE "pfn"
#define DETAIL_MAPPEDTITLE "times mapped"
#define DETAIL_LOCKEDTITLE "locked"
#define DETAIL_REFDTITLE "referenced"
#define DETAIL_DIRTYTITLE "dirty"
#define DETAIL_ANONTITLE "anonymous"
#define DETAIL_SWAPCACHETITLE "swapcache"
#define DETAIL_SWAPBACKEDTITLE "swapbacked"
#define DETAIL_KSMTITLE "KSM"

/**
 * Macros used in printing the details.
 */
#define DETAIL_YESPRESENT "present"
#define DETAIL_NOPRESENT "absent"

#define DETAIL_YESSWAP "swapped"
#define DETAIL_NOSWAP "unswapped"

#define DETAIL_YESLOCKED "locked"
#define DETAIL_NOLOCKED ""

#define DETAIL_YESREFD "referenced"
#define DETAIL_NOREFD ""

#define DETAIL_YESDIRTY "dirty"
#define DETAIL_NODIRTY ""

#define DETAIL_YESANON "anonymous"
#define DETAIL_NOANON ""

#define DETAIL_YESSWAPCACHE "swapcached"
#define DETAIL_NOSWAPCACHE ""

#define DETAIL_YESSWAPBACKED "swapbacked"
#define DETAIL_NOSWAPBACKED ""

#define DETAIL_YESKSM "ksm"
#define DETAIL_NOKSM ""

#define DELIMITER ","

#endif
