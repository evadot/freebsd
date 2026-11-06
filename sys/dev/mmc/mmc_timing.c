/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Emmanuel Vadot <manu@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/mmc/bridge.h>

static const char * const mmc_bus_timing_names[] = {
	[bus_timing_normal] = "normal",
	[bus_timing_hs] = "hs",
	[bus_timing_uhs_sdr12] = "sdr12",
	[bus_timing_uhs_sdr25] = "sdr25",
	[bus_timing_uhs_sdr50] = "sdr50",
	[bus_timing_uhs_ddr50] = "ddr50",
	[bus_timing_uhs_sdr104] = "sdr104",
	[bus_timing_mmc_ddr52] = "ddr52",
	[bus_timing_mmc_hs200] = "hs200",
	[bus_timing_mmc_hs400] = "hs400",
	[bus_timing_mmc_hs400es] = "hs400es",
};
_Static_assert(nitems(mmc_bus_timing_names) == bus_timing_max + 1,
    "mmc_bus_timing_names out of sync with enum mmc_bus_timing");

const char *
mmc_bus_timing_name(enum mmc_bus_timing timing)
{
	if ((u_int)timing > bus_timing_max)
		return ("unknown");
	return (mmc_bus_timing_names[timing]);
}

bool
mmc_bus_timing_from_name(const char *name, enum mmc_bus_timing *timing)
{
	u_int i;

	for (i = 0; i < nitems(mmc_bus_timing_names); i++) {
		if (strcmp(name, mmc_bus_timing_names[i]) == 0) {
			*timing = i;
			return (true);
		}
	}
	return (false);
}
