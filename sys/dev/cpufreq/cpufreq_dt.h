/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 Emmanuel Vadot <manu@FreeBSD.org>
 */

#ifndef _CPUFREQ_DT_H_
#define _CPUFREQ_DT_H_

#include <sys/bus.h>
#include <sys/linker_set.h>
#include <sys/kobj.h>
#include <dev/ofw/openfirm.h>

#define	CPUFREQ_DT_MAX_LEVELS	4	/* Maximum for opp-supported-hw */

/*
 * What the SoC specific code tells about the hardware, used to select the
 * OPPs (opp-supported-hw) and their voltages (opp-microvolt-<name>).
 */
struct cpufreq_dt_hw {
	/* One bitmask per level of version, nlevels is 0 if there is none */
	uint32_t	levels[CPUFREQ_DT_MAX_LEVELS];
	int		nlevels;
	/* Suffix of the opp-microvolt-<name> property to use, "" for none */
	char		name[16];
};

/*
 * The SoC specific code is a class implementing the cpufreq_dt_vendor
 * interface (cpufreq_dt_vendor_if.m).
 *
 * For the OPP tables with a specific compatible string, register an array of
 * struct ofw_compat_data, ocd_data is the kobj_class_t.
 */
#define	CPUFREQ_DT_VENDOR_COMPAT_DATA(compat)				\
	DATA_SET(cpufreq_dt_compat_set, compat)

/*
 * For the SoCs with an OPP table that cannot be identified by its compatible
 * string (operating-points-v2), register the class, which must implement the
 * match method.
 */
#define	CPUFREQ_DT_VENDOR_CLASS(class)					\
	DATA_SET(cpufreq_dt_match_set, class)

#endif /* _CPUFREQ_DT_H_ */
