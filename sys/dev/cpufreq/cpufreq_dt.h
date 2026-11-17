/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 Emmanuel Vadot <manu@FreeBSD.org>
 */

#ifndef _CPUFREQ_DT_H_
#define _CPUFREQ_DT_H_

#include <sys/bus.h>
#include <dev/ofw/openfirm.h>

/*
 * Resolve the idx-th entry of the nvmem-cells property of node (the OPP table
 * or the CPU node, depending on the SoC). Return the attached nvmem provider
 * and the offset and size of the cell in it. The caller reads the cell with
 * NVMEM_READ() as how to read and decode it depends on the SoC.
 */
int cpufreq_dt_get_nvmem_cell(phandle_t node, int idx, device_t *provider,
    uint32_t *offset, uint32_t *size);

#endif /* _CPUFREQ_DT_H_ */
