/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 Emmanuel Vadot <manu@FreeBSD.org>
 */

/*
 * Hardware version of the Allwinner SoCs for the generic DT cpufreq driver.
 *
 * The voltage of the OPPs (and the list of OPPs for some SoCs) depends on the
 * speed bin of the chip, which is stored in an efuse referenced by the
 * nvmem-cells property of the OPP table.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>

#include <dev/nvmem/nvmem.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "cpufreq_dt.h"
#include "cpufreq_dt_vendor_if.h"
#include "nvmem_if.h"

/* Read the efuse cell of the OPP table, up to 4 bytes, little endian. */
static int
aw_cpufreq_read_efuse(phandle_t table, uint32_t *efuse)
{
	uint32_t offset, size;
	uint8_t buf[4];
	device_t provider;
	int error;

	error = nvmem_cell_get_by_ofw_idx(table, 0, &provider, &offset,
	    &size);
	if (error != 0)
		return (error);
	if (size == 0 || size > sizeof(buf))
		return (EINVAL);

	memset(buf, 0, sizeof(buf));
	error = NVMEM_READ(provider, offset, size, buf);
	if (error != 0)
		return (error);

	*efuse = le32dec(buf);

	return (0);
}

static void
aw_cpufreq_set_bin(struct cpufreq_dt_hw *hw, uint32_t bin)
{
	hw->nlevels = 1;
	hw->levels[0] = 1U << bin;
	snprintf(hw->name, sizeof(hw->name), "speed%u", bin);
}

/*
 * The speed bin is in the bits 7:5, the values 1 to 3 are the bins 0 to 2, from
 * the slowest to the fastest. Handle the unexpected values as the slowest bin.
 */
static int
aw_cpufreq_h6_get_hw(kobj_t vendor __unused, device_t dev,
    phandle_t cpu __unused, phandle_t table, struct cpufreq_dt_hw *hw)
{
	uint32_t efuse, bin;
	int error;

	error = aw_cpufreq_read_efuse(table, &efuse);
	if (error != 0)
		return (error);

	bin = (efuse >> 5) & 0x7;
	bin = (bin >= 1 && bin <= 3) ? bin - 1 : 0;
	if (bootverbose)
		device_printf(dev, "efuse 0x%x, speed bin %u\n", efuse, bin);
	aw_cpufreq_set_bin(hw, bin);

	return (0);
}

static kobj_method_t aw_cpufreq_h6_methods[] = {
	KOBJMETHOD(cpufreq_dt_vendor_get_hw,	aw_cpufreq_h6_get_hw),
	KOBJMETHOD_END
};

DEFINE_CLASS_0(aw_cpufreq_h6, aw_cpufreq_h6_class, aw_cpufreq_h6_methods,
    sizeof(struct kobj));

static const struct ofw_compat_data aw_cpufreq_compat_data[] = {
	{ "allwinner,sun50i-h6-operating-points",
	    (uintptr_t)&aw_cpufreq_h6_class },
	{ NULL, 0 }
};
CPUFREQ_DT_VENDOR_COMPAT_DATA(aw_cpufreq_compat_data);
