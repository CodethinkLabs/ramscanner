#include "ramscanner_display.h"

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

void
print_detail_format(FILE *file)
{
	fprintf(file,            DETAIL_ADDRSTARTTITLE  DELIMITER
	                         DETAIL_ADDRENDTITLE    DELIMITER
	                         DETAIL_PERMTITLE       DELIMITER 
	                         DETAIL_PATHTITLE       DELIMITER 
	                         DETAIL_PRESENTTITLE    DELIMITER 
	                         DETAIL_SIZETITLE       DELIMITER 
	                         DETAIL_SWAPTITLE       DELIMITER 
	                      /* DETAIL_PFNTITLE        DELIMITER PFN OMITTED */
	                         DETAIL_MAPPEDTITLE     DELIMITER 
	                         DETAIL_LOCKEDTITLE     DELIMITER
	                         DETAIL_REFDTITLE       DELIMITER
	                         DETAIL_DIRTYTITLE      DELIMITER
	                         DETAIL_ANONTITLE       DELIMITER
	                         DETAIL_SWAPCACHETITLE  DELIMITER
	                         DETAIL_SWAPBACKEDTITLE DELIMITER
	                         DETAIL_KSMTITLE        "\n"); 
}

void
print_page_detail_data(pagedetaildata *page, FILE *file, vmastats *vmst)
{


	fprintf(file, "%5s"DELIMITER,  vmst->permissions);
	fprintf(file, "%s" DELIMITER,  vmst->path);
	fprintf(file, "%s" DELIMITER, (page->present ? 
	                               DETAIL_YESPRESENT : 
	                               DETAIL_NOPRESENT ));
	fprintf(file, "%d" DELIMITER, (1 << page->pageshift));
	fprintf(file, "%s" DELIMITER, (page->swap ? 
	                               DETAIL_YESSWAP : 
	                               DETAIL_NOSWAP ));
	/* PFN OMITTED
	 * fprintf(file, "%llx"DELIMITER, (page->pfn ? page->pfn : ""));
	 */
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

void
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


void
write_detail_page(void *key, void *val, void *userdata)
{
	options *opt = userdata;
	pagedetaildata *page = val;
	vmastats *currentvmst = &(opt->vmas[page->vmaindex]);
	uint32_t i;
	size_t size = getpagesize();

	for (i = page->addrstart; i < page->addrend; i += size) {
		fprintf(opt->detailfile, "%x" DELIMITER, i);
		fprintf(opt->detailfile, "%x" DELIMITER, (i + size));
		print_page_detail_data(page, opt->detailfile, currentvmst);
		fprintf(opt->detailfile, "\n");
	}
}

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

