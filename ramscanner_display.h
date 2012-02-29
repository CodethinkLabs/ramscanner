#ifndef _RAMSCANNER_DISPLAY_H_
#define _RAMSCANNER_DISPLAY_H_

#include "ramscanner_common.h"

void
write_summary(const sizestats *stats, FILE *summary);

void
print_detail_format(FILE *file);

void
print_page_detail_data(pagedetaildata *page, FILE *file, vmastats *vmst);

void
write_compact_detail_page(void *key, void *val, void *userdata);

void
write_detail_page(void *key, void *val, void *userdata);

void
write_any_detail(options *opt);


#endif
