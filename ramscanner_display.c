#include <stdio.h>
#include <unistd.h>

#include "ramscanner_display.h"

/**
 * Write the summary data into the file specified as "summary".
 */
void
write_summary(const sizestats *stats, FILE *summary)
{
	fprintf(summary,"Type" DELIMITER "         Size(kB)\n");
	fprintf(summary,SUMMARY_VSS    DELIMITER "          %8u\n",stats->vss);
	fprintf(summary,SUMMARY_RSS    DELIMITER "          %8u\n",stats->rss);
	fprintf(summary,SUMMARY_PSS    DELIMITER "          %8u\n",stats->pss);
	fprintf(summary,SUMMARY_USS    DELIMITER "          %8u\n",stats->uss);
	fprintf(summary,SUMMARY_SSS    DELIMITER "          %8u\n",stats->sss);
	fprintf(summary,SUMMARY_GSS    DELIMITER "          %8u\n",stats->gss);
	fprintf(summary,SUMMARY_REFD   DELIMITER "   %8u\n",stats->refd);
	fprintf(summary,SUMMARY_SWAP   DELIMITER "         %8u\n",stats->swap);
	fprintf(summary,SUMMARY_ANON   DELIMITER "    %8u\n",stats->anon);
	fprintf(summary,SUMMARY_LOCKED DELIMITER "       %8u\n",stats->locked);
		
}

/**
 * Write the head of the details in a detail file. Valid for compact detail 
 * and detail.
 */
static void
print_detail_format(FILE *file)
{
	fprintf(file,            DETAIL_ADDRSTARTTITLE  DELIMITER
	                         DETAIL_ADDRENDTITLE    DELIMITER
	                         DETAIL_PERMTITLE       DELIMITER 
	                         DETAIL_PATHTITLE       DELIMITER 
	                         DETAIL_PRESENTTITLE    DELIMITER 
	                         DETAIL_SIZETITLE       DELIMITER 
	                         DETAIL_SWAPTITLE       DELIMITER 
	                         DETAIL_MAPPEDTITLE     DELIMITER 
	                         DETAIL_LOCKEDTITLE     DELIMITER
	                         DETAIL_REFDTITLE       DELIMITER
	                         DETAIL_DIRTYTITLE      DELIMITER
	                         DETAIL_ANONTITLE       DELIMITER
	                         DETAIL_SWAPCACHETITLE  DELIMITER
	                         DETAIL_SWAPBACKEDTITLE DELIMITER
	                         DETAIL_KSMTITLE        "\n"); 
}

/**
 * Write the details data taken from the pagedetaildata struct into the file
 * provided, taking some additional information from the vmastats.
 */
static void
print_page_detail_data(pagedetaildata *page, FILE *file, vmastats *vmst)
{


	fprintf(file, "%4s"DELIMITER,  vmst->permissions);
	fprintf(file, "%s" DELIMITER,  vmst->path);
	fprintf(file, "%s" DELIMITER, (page->present ? 
	                               DETAIL_YESPRESENT : 
	                               DETAIL_NOPRESENT ));
	fprintf(file, "%d" DELIMITER, (1 << page->pageshift));
	fprintf(file, "%s" DELIMITER, (page->swap ? 
	                               DETAIL_YESSWAP : 
	                               DETAIL_NOSWAP ));
	fprintf(file, "%d"DELIMITER, page->timesmapped);
	fprintf(file, "%s"DELIMITER, (page->locked ? 
	                              DETAIL_YESLOCKED : 
	                              DETAIL_NOLOCKED ));
	fprintf(file, "%s"DELIMITER, (page->referenced ? 
	                              DETAIL_YESREFD : 
	                              DETAIL_NOREFD ));
	fprintf(file, "%s"DELIMITER, (page->dirty ? 
	                              DETAIL_YESDIRTY : 
	                              DETAIL_NODIRTY ));
	fprintf(file, "%s"DELIMITER, (page->anonymous ? 
	                              DETAIL_YESANON : 
	                              DETAIL_NOANON ));
	fprintf(file, "%s"DELIMITER, (page->swapcache ? 
	                              DETAIL_YESSWAPCACHE : 
	                              DETAIL_NOSWAPCACHE ));
	fprintf(file, "%s"DELIMITER, (page->swapbacked ? 
	                              DETAIL_YESSWAPBACKED : 
	                              DETAIL_NOSWAPBACKED ));
	fprintf(file, "%s"DELIMITER, (page->ksm ? 
	                              DETAIL_YESKSM : 
	                              DETAIL_NOKSM ));

}

/**
 * Function passed into a g_hash_table_foreach(). Takes the details from the 
 * page and prints the information with the help of the print_page_detail_data()
 * function.
 */
static void
write_compact_detail_page(void *key, void *val, void *userdata)
{
	options *opt = userdata;
	pagedetaildata *page = val;
	vmastats *currentvmst = &(opt->vmas[page->vmaindex]);
	uint32_t pagecount = (page->addrend - page->addrstart) / getpagesize();
	fprintf(opt->compactdetailfile, "%08x" DELIMITER, page->addrstart);
	fprintf(opt->compactdetailfile, "%08x" DELIMITER, page->addrend);
	print_page_detail_data(page, opt->compactdetailfile, currentvmst);			
	fprintf(opt->compactdetailfile, "%d" DELIMITER" identical pages\n",
	        pagecount);
}

/**
 * Like write_compact_detail_page(), retrieves information from the page.
 * This function expands the information into one line per page mapped.
 */
static void
write_detail_page(void *key, void *val, void *userdata)
{
	options *opt = userdata;
	pagedetaildata *page = val;
	vmastats *currentvmst = &(opt->vmas[page->vmaindex]);
	uint32_t i;
	size_t size = getpagesize();

	for (i = page->addrstart; i < page->addrend; i += size) {
		fprintf(opt->detailfile, "%zx" DELIMITER, i);
		fprintf(opt->detailfile, "%zx" DELIMITER, (i + size));
		print_page_detail_data(page, opt->detailfile, currentvmst);
		fprintf(opt->detailfile, "\n");
	}
}

/**
 * This function is called if either opt->compactdetail or opt->detail are set
 * to true in opt. It prints the information stored within the GHashTable
 * detailpages to the files specified by opt.
 */
void
write_any_detail(options *opt)
{
	if (opt->compactdetail) {
		print_detail_format(opt->compactdetailfile);
		g_hash_table_foreach(opt->detailpages,
		                     &write_compact_detail_page, opt);
	}
	if (opt->detail) {
		print_detail_format(opt->detailfile);
		g_hash_table_foreach(opt->detailpages, &write_detail_page, opt);
	}
}

