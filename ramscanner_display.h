#ifndef RAMSCANNER_DISPLAY_H
#define RAMSCANNER_DISPLAY_H

#include "ramscanner_common.h"

void
write_summary(const sizestats *stats, FILE *summary);

static void
print_detail_format(FILE *file);

static void
print_page_detail_data(pagedetaildata *page, FILE *file, vmastats *vmst);

static void
write_compact_detail_page(void *key, void *val, void *userdata);

static void
write_detail_page(void *key, void *val, void *userdata);

void
write_any_detail(options *opt);

#endif
