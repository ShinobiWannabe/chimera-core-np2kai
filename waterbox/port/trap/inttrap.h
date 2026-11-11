/* trap/inttrap.h - takes the place of NP2kai's trap/inttrap.h. ENABLE_TRAP
 * (see steptrap.h beside this file) also calls softinttrap at each INT n on
 * the IA-32 CPU; NP2kai's trap/inttrap.c only logs to the host, so here it
 * is nothing. */
#pragma once

#define softinttrap(cs, eip, vect) do { } while (0)
