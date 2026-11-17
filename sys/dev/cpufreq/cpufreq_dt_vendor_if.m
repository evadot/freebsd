#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright 2026 Emmanuel Vadot <manu@FreeBSD.org>
#

#include <sys/bus.h>
#include <dev/ofw/openfirm.h>

#
# Interface for the SoC specific code of the generic DT cpufreq driver.
#
INTERFACE cpufreq_dt_vendor;

HEADER {
	struct cpufreq_dt_hw;
}

#
# Default implementations of some methods.
#
CODE {
	static bool
	cpufreq_dt_vendor_default_match(kobj_t vendor __unused,
	    phandle_t cpu __unused, phandle_t table __unused)
	{

		return (false);
	}
}

#
# Is this OPP table (used by the cpu node) handled by the vendor ?
# Only needed for the SoCs where the OPP table cannot be found from its
# compatible string.
#
METHOD bool match {
	kobj_t			vendor;
	phandle_t		cpu;
	phandle_t		table;
} DEFAULT cpufreq_dt_vendor_default_match;

#
# Get the hardware version of the SoC, 0 on success.
#
METHOD int get_hw {
	kobj_t			vendor;
	device_t		dev;
	phandle_t		cpu;
	phandle_t		table;
	struct cpufreq_dt_hw	*hw;
};
