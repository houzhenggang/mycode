#ifndef __DPI_H
#define __DPI_H
#include <stdint.h>
#include "list.h"
#include "config.h"

int do_dpi_entry(struct rte_mbuf *m, unsigned portid, unsigned lcore_id);

#endif
