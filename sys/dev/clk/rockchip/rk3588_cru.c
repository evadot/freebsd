/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Emmanuel Vadot <manu@freebsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk_div.h>
#include <dev/clk/clk_fixed.h>
#include <dev/clk/clk_mux.h>

#include <dev/clk/rockchip/rk_cru.h>
#include <contrib/device-tree/include/dt-bindings/clock/rockchip,rk3588-cru.h>

/* CRU */
#define	RK3588_PLLSEL_CON(x)		((x) * 0x20 + 0x160)
#define	CRU_CLKSEL_CON(x)		((x) * 0x4 + 0x300)
#define	CRU_CLKGATE_CON(x)		((x) * 0x4 + 0x800)
#define	RK3588_SOFTRST_CON(x)		((x) * 0x4 + 0xA04)
#define	RK3588_MODE_CON			0x280

/* PHPTOPCRU */
#define	RK3588_PHPCRU_BASE		0x8000
#define	RK3588_PHPCRU_PLLSEL_CON(x)	((x) * 0x4 + RK3588_PHPCRU_BASE + 0x200)

/* PMU1CRU */
#define	RK3588_PMUCRU_BASE		0x30000
#define	RK3588_PMUCRU_CLKSEL_CON(x)	(x * 0x4 + RK3588_PMUCRU_BASE + 0x300)
#define	RK3588_PMUCRU_CLKGATE_CON(x)	(x * 0x4 + RK3588_PMUCRU_BASE + 0x800)

/* BIGCORE0CRU */
#define	RK3588_B0_CRU_BASE		0x50000
#define	RK3588_B0_PLL_MODE_CON0		(RK3588_B0_CRU_BASE + 0x280)
#define	RK3588_B0_PLLSEL_CON(x)		((x) * 0x4 + RK3588_B0_CRU_BASE)
#define	RK3588_B0_CLKSEL_CON(x)		((x) * 0x4 + RK3588_B0_CRU_BASE + 0x300)
#define	RK3588_B0_CLKGATE_CON(x)	((x) * 0x4 + RK3588_B0_CRU_BASE + 0x800)
#define	RK3588_B0_SOFTRST_CON(x)	((x) * 0x4 + RK3588_B0_CRU_BASE + 0xA00)

/* BIGCORE1CRU */
#define	RK3588_B1_CRU_BASE		0x52000
#define	RK3588_B1_PLL_MODE_CON0		(RK3588_B1_CRU_BASE + 0x280)
#define	RK3588_B1_PLLSEL_CON(x)		((x) * 0x4 + RK3588_B1_CRU_BASE + 0x20)
#define	RK3588_B1_CLKSEL_CON(x)		((x) * 0x4 + RK3588_B1_CRU_BASE + 0x300)
#define	RK3588_B1_CLKGATE_CON(x)	((x) * 0x4 + RK3588_B1_CRU_BASE + 0x800)
#define	RK3588_B1_SOFTRST_CON(x)	((x) * 0x4 + RK3588_B1_CRU_BASE + 0xA00)

/* DSU_CRU */
#define	RK3588_DSU_CRU_BASE		0x58000
#define	RK3588_LPLL_MODE_CON0		(RK3588_DSU_CRU_BASE + 0x280)
#define	RK3588_LPLL_PLLSEL_CON(x)	((x) * 0x4 + RK3588_DSU_CRU_BASE + 0x40)
#define	RK3588_LPLL_CLKSEL_CON(x)	((x) * 0x4 + RK3588_DSU_CRU_BASE + 0x300)
#define	RK3588_LPLL_CLKGATE_CON(x)	((x) * 0x4 + RK3588_DSU_CRU_BASE + 0x800)
#define	RK3588_LPLL_SOFTRST_CON(x)	((x) * 0x4 + RK3588_DSU_CRU_BASE + 0xA00)

#define	RK3588_PLL_RATE(_hz, _p, _m, _s, _k)	\
{						\
	.freq = _hz,				\
	.p = _p,				\
	.m = _m,				\
	.s = _s,				\
	.k = _k,				\
}

struct rk_clk_pll_rate rk3588_pll_rates[] = {
	/* k are magic values taken from Linux, CON2 isn't even documented */
	RK3588_PLL_RATE(2520000000, 2, 210, 0, 0),
	RK3588_PLL_RATE(2496000000, 2, 208, 0, 0),
	RK3588_PLL_RATE(2472000000, 2, 206, 0, 0),
	RK3588_PLL_RATE(2448000000, 2, 204, 0, 0),
	RK3588_PLL_RATE(2424000000, 2, 202, 0, 0),
	RK3588_PLL_RATE(2400000000, 2, 200, 0, 0),
	RK3588_PLL_RATE(2376000000, 2, 198, 0, 0),
	RK3588_PLL_RATE(2352000000, 2, 196, 0, 0),
	RK3588_PLL_RATE(2328000000, 2, 194, 0, 0),
	RK3588_PLL_RATE(2304000000, 2, 192, 0, 0),
	RK3588_PLL_RATE(2280000000, 2, 190, 0, 0),
	RK3588_PLL_RATE(2256000000, 2, 376, 1, 0),
	RK3588_PLL_RATE(2232000000, 2, 372, 1, 0),
	RK3588_PLL_RATE(2208000000, 2, 368, 1, 0),
	RK3588_PLL_RATE(2184000000, 2, 364, 1, 0),
	RK3588_PLL_RATE(2160000000, 2, 360, 1, 0),
	RK3588_PLL_RATE(2136000000, 2, 356, 1, 0),
	RK3588_PLL_RATE(2112000000, 2, 352, 1, 0),
	RK3588_PLL_RATE(2088000000, 2, 348, 1, 0),
	RK3588_PLL_RATE(2064000000, 2, 344, 1, 0),
	RK3588_PLL_RATE(2040000000, 2, 340, 1, 0),
	RK3588_PLL_RATE(2016000000, 2, 336, 1, 0),
	RK3588_PLL_RATE(1992000000, 2, 332, 1, 0),
	RK3588_PLL_RATE(1968000000, 2, 328, 1, 0),
	RK3588_PLL_RATE(1944000000, 2, 324, 1, 0),
	RK3588_PLL_RATE(1920000000, 2, 320, 1, 0),
	RK3588_PLL_RATE(1896000000, 2, 316, 1, 0),
	RK3588_PLL_RATE(1872000000, 2, 312, 1, 0),
	RK3588_PLL_RATE(1848000000, 2, 308, 1, 0),
	RK3588_PLL_RATE(1824000000, 2, 304, 1, 0),
	RK3588_PLL_RATE(1800000000, 2, 300, 1, 0),
	RK3588_PLL_RATE(1776000000, 2, 296, 1, 0),
	RK3588_PLL_RATE(1752000000, 2, 292, 1, 0),
	RK3588_PLL_RATE(1728000000, 2, 288, 1, 0),
	RK3588_PLL_RATE(1704000000, 2, 284, 1, 0),
	RK3588_PLL_RATE(1680000000, 2, 280, 1, 0),
	RK3588_PLL_RATE(1656000000, 2, 276, 1, 0),
	RK3588_PLL_RATE(1632000000, 2, 272, 1, 0),
	RK3588_PLL_RATE(1608000000, 2, 268, 1, 0),
	RK3588_PLL_RATE(1584000000, 2, 264, 1, 0),
	RK3588_PLL_RATE(1560000000, 2, 260, 1, 0),
	RK3588_PLL_RATE(1536000000, 2, 256, 1, 0),
	RK3588_PLL_RATE(1512000000, 2, 252, 1, 0),
	RK3588_PLL_RATE(1488000000, 2, 248, 1, 0),
	RK3588_PLL_RATE(1464000000, 2, 244, 1, 0),
	RK3588_PLL_RATE(1440000000, 2, 240, 1, 0),
	RK3588_PLL_RATE(1416000000, 2, 236, 1, 0),
	RK3588_PLL_RATE(1392000000, 2, 232, 1, 0),
	RK3588_PLL_RATE(1320000000, 2, 220, 1, 0),
	RK3588_PLL_RATE(1200000000, 2, 200, 1, 0),
	RK3588_PLL_RATE(1188000000, 2, 198, 1, 0),
	RK3588_PLL_RATE(1100000000, 3, 550, 2, 0),
	RK3588_PLL_RATE(1008000000, 2, 336, 2, 0),
	RK3588_PLL_RATE(1000000000, 3, 500, 2, 0),
	RK3588_PLL_RATE(983040000, 4, 655, 2, 23592),
	RK3588_PLL_RATE(955520000, 3, 477, 2, 49806),
	RK3588_PLL_RATE(903168000, 6, 903, 2, 11009),
	RK3588_PLL_RATE(900000000, 2, 300, 2, 0),
	RK3588_PLL_RATE(850000000, 3, 425, 2, 0),
	RK3588_PLL_RATE(816000000, 2, 272, 2, 0),
	RK3588_PLL_RATE(786432000, 2, 262, 2, 9437),
	RK3588_PLL_RATE(786000000, 1, 131, 2, 0),
	RK3588_PLL_RATE(785560000, 3, 392, 2, 51117),
	RK3588_PLL_RATE(722534400, 8, 963, 2, 24850),
	RK3588_PLL_RATE(600000000, 2, 200, 2, 0),
	RK3588_PLL_RATE(594000000, 2, 198, 2, 0),
	RK3588_PLL_RATE(408000000, 2, 272, 3, 0),
	RK3588_PLL_RATE(312000000, 2, 208, 3, 0),
	RK3588_PLL_RATE(216000000, 2, 288, 4, 0),
	RK3588_PLL_RATE(100000000, 3, 400, 5, 0),
	RK3588_PLL_RATE(96000000, 2, 256, 5, 0),
	{},
};

/* Big core 0 PLL */
#define	RK3588_PLL(_id, _name, _pnames, _off, _mode_reg, _mode_shift)	\
{								\
	.type = RK3588_CLK_PLL,					\
	.clk.pll = &(struct rk_clk_pll_def) {			\
		.clkdef.id = _id,				\
		.clkdef.name = _name,				\
		.clkdef.parent_names = _pnames,			\
		.clkdef.parent_cnt = nitems(_pnames),		\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,	\
		.base_offset = _off,				\
		.mode_reg = _mode_reg,				\
		.mode_shift = _mode_shift,			\
		.rates = rk3588_pll_rates,			\
	},							\
}

PLIST(xin24m_xin32k_p) = {"xin24m", "xin32k"};
PLIST(gpll_aupll_p) = {"gpll", "aupll"};
PLIST(gpll_cpll_p) = {"gpll", "cpll"};
PLIST(gpll_lpll_p) = {"gpll", "lpll"};
PLIST(gpll_spll_p) = {"gpll", "spll"};
PLIST(gpll_cpll_aupll_p) = {"gpll", "cpll", "aupll"};
PLIST(gpll_cpll_npll_v0pll_p) = {"gpll", "cpll", "npll", "v0pll"};
PLIST(gpll_cpll_xin24m_p) = {"gpll", "cpll", "xin24m"};

PLIST(clk100m_clk50m_xin24m_p) = {"clk_100m_src", "clk_50m_src", "xin24m"};
PLIST(clk200m_clk100m_clk50m_xin24m_p) = {"clk_200m_src", "clk_100m_src", "clk_50m_src", "xin24m"};
PLIST(clk300m_xin24m_p) = {"clk_300m_src", "xin24m"};
PLIST(clk300m_clk200m_clk100m_xin24m_p) = {"clk_300m_src", "clk_200m_src", "clk_100m_src", "xin24m"};
PLIST(clk400m_xin24m_p) = {"clk_400m_src", "xin24m"};
PLIST(clk400m_clk200m_clk100m_xin24m_p) = {"clk_400m_src", "clk_200m_src", "clk_100m_src", "xin24m"};
PLIST(clk500m_clk300m_clk100m_xin24m_p) = {"clk_500m_src", "clk_300m_src", "clk_100m_src", "xin24m"};

PLIST(pmu_200m_100m_p) = {"clk_pmu1_200m_src", "clk_pmu1_100m_src"};

/* CLOCKS */
static struct rk_clk rk3588_clks[] = {
	/* External clocks */
	LINK("xin24m"),
	LINK("xin32k"),
	LINK("spll"),

	FFACT(0, "xin_osc0_half", "xin24m", 1, 2),

	/* BIGCORE0CRU */
	RK3588_PLL(PLL_B0PLL, "b0pll", xin24m_xin32k_p, RK3588_B0_PLLSEL_CON(0), RK3588_B0_PLL_MODE_CON0, 0),

	/* BIGCORE1CRU */
	RK3588_PLL(PLL_B1PLL, "b1pll", xin24m_xin32k_p, RK3588_B1_PLLSEL_CON(0), RK3588_B1_PLL_MODE_CON0, 0),

	/* DSUCRU */
	RK3588_PLL(PLL_LPLL, "lpll", xin24m_xin32k_p, RK3588_LPLL_PLLSEL_CON(0), RK3588_LPLL_MODE_CON0, 0),

	/* CRU PLL */
	RK3588_PLL(PLL_V0PLL, "v0pll", xin24m_xin32k_p, RK3588_PLLSEL_CON(0), RK3588_MODE_CON, 4),
	RK3588_PLL(PLL_AUPLL, "aupll", xin24m_xin32k_p, RK3588_PLLSEL_CON(1), RK3588_MODE_CON, 6),
	RK3588_PLL(PLL_CPLL, "cpll", xin24m_xin32k_p, RK3588_PLLSEL_CON(2), RK3588_MODE_CON, 8),
	RK3588_PLL(PLL_GPLL, "gpll", xin24m_xin32k_p, RK3588_PLLSEL_CON(3), RK3588_MODE_CON, 2),
	RK3588_PLL(PLL_NPLL, "npll", xin24m_xin32k_p, RK3588_PLLSEL_CON(4), RK3588_MODE_CON, 0),

	/* PHPCRU PLL */
	RK3588_PLL(PLL_PPLL, "ppll", xin24m_xin32k_p, RK3588_PHPCRU_PLLSEL_CON(0), RK3588_MODE_CON, 10),

	/* CRU_CLKSEL_CON00 */
	COMP(0, "clk_50m_src_c", gpll_cpll_p, 0, 0, 0, 5, 5, 1),
	COMP(0, "clk_100m_src_c", gpll_cpll_p, 0, 0, 6, 5, 11, 1),

	/* CRU_CLKSEL_CON01 */
	COMP(0, "clk_150m_src_c", gpll_cpll_p, 0, 1, 0, 5, 5, 1),
	COMP(0, "clk_200m_src_c", gpll_cpll_p, 0, 1, 6, 5, 11, 1),

	/* CRU_CLKSEL_CON02 */
	COMP(0, "clk_250m_src_c", gpll_cpll_p, 0, 2, 0, 5, 5, 1),
	COMP(0, "clk_300m_src_c", gpll_cpll_p, 0, 2, 6, 5, 11, 1),

	/* CRU_CLKSEL_CON03 */
	COMP(0, "clk_350m_src_c", gpll_spll_p, 0, 3, 0, 5, 5, 1),
	COMP(0, "clk_400m_src_c", gpll_cpll_p, 0, 3, 6, 5, 11, 1),

	/* CRU_CLKSEL_CON04 */
	COMP(0, "clk_450m_src_c", gpll_cpll_p, 0, 4, 0, 5, 5, 1),
	COMP(0, "clk_500m_src_c", gpll_cpll_p, 0, 4, 6, 5, 11, 1),

	/* CRU_CLKSEL_CON05 */
	COMP(0, "clk_600m_src_c", gpll_cpll_p, 0, 5, 0, 5, 5, 1),
	COMP(0, "clk_650m_src_c", gpll_lpll_p, 0, 5, 6, 5, 11, 1),

	/* CRU_CLKSEL_CON06 */
	COMP(0, "clk_700m_src_c", gpll_spll_p, 0, 6, 0, 5, 5, 1),
	COMP(0, "clk_800m_src_c", gpll_aupll_p, 0, 6, 6, 5, 11, 1),

	/* CRU_CLKSEL_CON07 */
	COMP(0, "clk_1000m_src_c", gpll_cpll_npll_v0pll_p, 0, 7, 0, 5, 5, 2),
	COMP(0, "clk_1200m_src_c", gpll_cpll_p, 0, 7, 6, 5, 11, 1),

	/* CRU_CLKSEL_CON08 */
	COMP(0, "aclk_low_top_root_c", gpll_cpll_p, 0, 8, 9, 5, 14, 1),
	MUX(0, "pclk_top_root_m", clk100m_clk50m_xin24m_p, 0, 8, 7, 2),
	COMP(0, "aclk_top_root_c", gpll_cpll_aupll_p, 0, 8, 0, 5, 5, 2),

	/* CRU_CLKSEL_CON09 */
	MUX(0, "aclk_top_s400_root_m", clk400m_clk200m_clk100m_xin24m_p, 0, 9, 8, 2),
	MUX(0, "aclk_top_s200_root_m", clk200m_clk100m_clk50m_xin24m_p, 0, 9, 6, 2),
	MUX(0, "aclk_top_m400_root_m", clk400m_clk200m_clk100m_xin24m_p, 0, 9, 4, 2),
	MUX(0, "aclk_top_m500_root_m", clk500m_clk300m_clk100m_xin24m_p, 0, 9, 2, 2),
	MUX(0, "aclk_top_m300_root_m", clk300m_clk200m_clk100m_xin24m_p, 0, 9, 0, 2),

	/* CRU_CLKSEL_CON10 */
	/* clk_testout_grp0_sel 14:12 */
	/* clk_testout_sel 11:9 */
	/* clk_testout_top_sel 8:6 */
	/* clk_testout_top_div 5:0 */

	/* CRU_CLKSEL_CON11 Unused */
	/* CRU_CLKSEL_CON12 Unused */
	/* CRU_CLKSEL_CON13 Unused */
	/* CRU_CLKSEL_CON14 Unused */

	/* CRU_CLKSEL_CON15 */
	COMP(0, "refclk025m_eth0_out_c", gpll_cpll_p, 0, 15, 8, 6, 15, 1),
	COMP(0, "mclk_gmac0_out_c", gpll_cpll_p, 0, 15, 0, 7, 7, 1),

	/* CRU_CLKSEL_CON16 */
	COMP(0, "refclk025m_eth1_out_c", gpll_cpll_p, 0, 16, 0, 7, 7, 1),

	/* CRU_CLKSEL_CON17 */
	/* clk_cifout_out_sel 9:8 */
	/* clk_cifout_out_div 7:0 */

	/* CRU_CLKSEL_CON18 */
	/* clk_mipi_camaraout_m0_sel 9:8 */
	/* clk_mipi_camaraout_m0_div 7:0 */

	/* CRU_CLKSEL_CON19 */
	/* clk_mipi_camaraout_m1_sel 9:8 */
	/* clk_mipi_camaraout_m1_div 7:0 */

	/* CRU_CLKSEL_CON20 */
	/* clk_mipi_camaraout_m2_sel 9:8 */
	/* clk_mipi_camaraout_m2_div 7:0 */

	/* CRU_CLKSEL_CON21 */
	/* clk_mipi_camaraout_m3_sel 9:8 */
	/* clk_mipi_camaraout_m3_div 7:0 */

	/* CRU_CLKSEL_CON22 */
	/* clk_mipi_camaraout_m4_sel 9:8 */
	/* clk_mipi_camaraout_m4_div 7:0 */

	/* CRU_CLKSEL_CON23 */

	/* CRU_CLKSEL_CON24 */
	/* clk_i2s0_8ch_tx_src_sel 9 */
	/* clk_i2s0_8ch_tx_src_div 8:4 */
	/* pclk_audio_root_sel 3:2 */
	/* hclk_audio_root_sel 1:0 */

	/* CRU_CLKSEL_CON25 */
	/* clk_i2s0_8ch_tx_frac_div 31:0 */

	/* CRU_CLKSEL_CON26 */
	/* clk_i2s0_8ch_rx_src_sel 7 */
	/* clk_i2s0_8ch_rx_src_div 6:2 */
	/* mclk_i2s0_8ch_tx_sel 1:0 */

	/* CRU_CLKSEL_CON27 */
	/* clk_i2s0_8ch_rx_frac_div 31:0 */

	/* CRU_CLKSEL_CON28 */
	/* clk_i2s2_2ch_src_sel 9 */
	/* clk_i2s2_2ch_src_div 8:4 */
	/* i2s0_8ch_mclkout_sel 3:2 */
	/* mclk_i2s0_8ch_rx_sel 1:0 */

	/* CRU_CLKSEL_CON29 */
	/* clk_i2s2_2ch_frac_div 31:0 */

	/* CRU_CLKSEL_CON30 */
	/* clk_i2s3_2ch_src_sel 8 */
	/* clk_i2s3_2ch_src_div 7:3 */
	/* i2s2_2ch_mclkout_sel 2 */
	/* mclk_i2s2_2ch_sel 1:0 */

	/* CRU_CLKSEL_CON31 */
	/* clk_i2s3_2ch_frac_div 31:0 */

	/* CRU_CLKSEL_CON32 */
	/* clk_spdif0_src_sel 8 */
	/* clk_spdif0_src_div 7:3 */
	/* i2s3_2ch_mclkout_sel 2 */
	/* mclk_i2s3_2ch_sel 1:0 */

	/* CRU_CLKSEL_CON33 */
	/* clk_spdif0_frac_div 31:0 */

	/* CRU_CLKSEL_CON34 */
	/* clk_spdif1_src_sel 7 */
	/* clk_spdif1_src_div 6:2 */
	/* mclk_spdif0_sel 1:0 */

	/* CRU_CLKSEL_CON35 */
	/* clk_spdif1_frac_div 31:0 */

	/* CRU_CLKSEL_CON36 */
	/* mclk_pdm1_sel 8:7 */
	/* mclk_pdm1_div 6:2 */
	/* mclk_spdif1_sel 1:0 */

	/* CRU_CLKSEL_CON37 */

	/* CRU_CLKSEL_CON38 */
	COMP(0, "aclk_bus_root_c", gpll_cpll_p, 0, 38, 0, 5, 5, 1),
	MUX(0, "clk_i2c1_m", gpll_cpll_p, 0, 38, 6, 1),
	MUX(0, "clk_i2c2_m", gpll_cpll_p, 0, 38, 7, 1),
	MUX(0, "clk_i2c3_m", gpll_cpll_p, 0, 38, 8, 1),
	MUX(0, "clk_i2c4_m", gpll_cpll_p, 0, 38, 9, 1),
	MUX(0, "clk_i2c5_m", gpll_cpll_p, 0, 38, 10, 1),
	MUX(0, "clk_i2c6_m", gpll_cpll_p, 0, 38, 11, 1),
	MUX(0, "clk_i2c7_m", gpll_cpll_p, 0, 38, 12, 1),
	MUX(0, "clk_i2c8_m", gpll_cpll_p, 0, 38, 13, 1),

	/* CRU_CLKSEL_CON39 */
	/* clk_can1_sel 11 */
	/* clk_can1_div 10:6 */
	/* clk_can0_sel 5 */
	/* clk_can0_div 4:0 */

	/* CRU_CLKSEL_CON40 */
	/* clk_saradc_sel 14 */
	/* clk_saradc_div 13:6 */
	/* clk_can2_sel 5 */
	/* clk_can2_div 4:0 */

	/* CRU_CLKSEL_CON41 */
	/* clk_uart1_src_sel 14 */
	/* clk_uart1_src_div 13:9 */
	/* clk_tsadc_sel 8 */
	/* clk_tsadc_div 7:0 */

	/* CRU_CLKSEL_CON42 */
	/* clk_uart1_frac_div 31:0 */

	/* CRU_CLKSEL_CON43 */
	/* clk_uart2_src_sel 7 */
	/* clk_uart2_src_div 6:2 */
	/* sclk_uart1_sel 1:0 */

	/* CRU_CLKSEL_CON44 */
	/* clk_uart2_frac_div 31:0 */

	/* CRU_CLKSEL_CON45 */
	/* clk_uart3_src_sel 7 */
	/* clk_uart3_src_div 6:2 */
	/* sclk_uart2_sel 1:0 */

	/* CRU_CLKSEL_CON46 */
	/* clk_uart3_frac_div 31:0 */

	/* CRU_CLKSEL_CON47 */
	/* clk_uart4_src_sel 7 */
	/* clk_uart4_src_div 6:2 */
	/* sclk_uart3_sel 1:0 */

	/* CRU_CLKSEL_CON48 */
	/* clk_uart4_frac_div 31:0 */

	/* CRU_CLKSEL_CON49 */
	/* clk_uart5_src_sel 7 */
	/* clk_uart5_src_div 6:2 */
	/* sclk_uart4_sel 1:0 */

	/* CRU_CLKSEL_CON50 */
	/* clk_uart5_frac_div 31:0 */

	/* CRU_CLKSEL_CON51 */
	/* clk_uart6_src_sel 7 */
	/* clk_uart6_src_div 6:2 */
	/* sclk_uart5_sel 1:0 */

	/* CRU_CLKSEL_CON52 */
	/* clk_uart6_frac_div 31:0 */

	/* CRU_CLKSEL_CON53 */
	/* clk_uart7_src_sel 7 */
	/* clk_uart7_src_div 6:2 */
	/* sclk_uart6_sel 1:0 */

	/* CRU_CLKSEL_CON54 */
	/* clk_uart7_frac_div 31:0 */

	/* CRU_CLKSEL_CON55 */
	/* clk_uart8_src_sel 7 */
	/* clk_uart8_src_div 6:2 */
	/* sclk_uart7_sel 1:0 */

	/* CRU_CLKSEL_CON56 */
	/* clk_uart8_frac_div 31:0 */

	/* CRU_CLKSEL_CON57 */
	/* clk_uart9_src_sel 7 */
	/* clk_uart9_src_div 6:2 */
	/* sclk_uart8_sel 1:0 */

	/* CRU_CLKSEL_CON58 */
	/* clk_uart9_frac_div 31:0 */

	/* CRU_CLKSEL_CON59 */
	/* clk_pwm2_sel 15:14 */
	/* clk_pwm1_sel 13:12 */
	/* clk_spi4_sel 11:10 */
	/* clk_spi3_sel 9:8 */
	/* clk_spi2_sel 7:6 */
	/* clk_spi1_sel 5:4 */
	/* clk_spi0_sel 3:2 */
	/* sclk_uart9_sel 1:0 */

	/* CRU_CLKSEL_CON60 */
	/* dbclk_gpio2_sel 14 */
	/* dbclk_gpio2_div 13:9 */
	/* dbclk_gpio1_sel 8 */
	/* dbclk_gpio1_div 7:3 */
	/* clk_bus_timer_root_sel 2 */
	/* clk_pwm3_sel 1:0 */

	/* CRU_CLKSEL_CON61 */
	/* dbclk_gpio4_sel 11 */
	/* dbclk_gpio4_div 10:6 */
	/* dbclk_gpio3_sel 5 */
	/* dbclk_gpio3_div 4:0 */

	/* CRU_CLKSEL_CON62 */
	/* dclk_decom_sel 5 */
	/* dclk_decom_div 4:0 */

	/* CRU_CLKSEL_CON63 */
	/* clk_testout_ddr01_sel 6 */
	/* clk_testout_ddr01_div 5:0 */

	/* CRU_CLKSEL_CON64 */

	/* CRU_CLKSEL_CON65 */
	/* clk_testout_ddr23_sel 6 */
	/* clk_testout_ddr23_div 5:0 */

	/* CRU_CLKSEL_CON66 */

	/* CRU_CLKSEL_CON67 */
	/* clk_isp1_core_sel 15:14 */
	/* clk_isp1_core_div 13:9 */
	/* hclk_isp1_root_sel 8:7 */
	/* aclk_isp1_root_sel 6:5 */
	/* aclk_isp1_root_div 4:0 */

	/* CRU_CLKSEL_CON68 */
	/* CRU_CLKSEL_CON69 */
	/* CRU_CLKSEL_CON70 */
	/* CRU_CLKSEL_CON71 */
	/* CRU_CLKSEL_CON72 */

	/* CRU_CLKSEL_CON73 */
	/* clk_testout_npu_sel 15 */
	/* clk_testout_npu_div 14:10 */
	/* clk_rknn_dsu0_src_t_sel 9:7 */
	/* clk_rknn_dsu0_src_t_div 6:2 */
	/* hclk_rknn_root_sel 1:0 */

	/* CRU_CLKSEL_CON74 */
	/* clk_npu_cm0_rtc_sel 12 */
	/* clk_npu_cm0_rtc_div 11:7 */
	/* hclk_npu_cm0_root_sel 6:5 */
	/* clk_npu_pvtpll_sel 4 */
	/* clk_nputimer_root_sel 3 */
	/* pclk_nputop_root_sel 2:1 */
	/* clk_rknn_dsu0_sel 0 */

	/* CRU_CLKSEL_CON75 */
	/* CRU_CLKSEL_CON76 */

	/* CRU_CLKSEL_CON77 */
	MUX(0, "hclk_nvm_root_m", clk200m_clk100m_clk50m_xin24m_p, 0, 77, 0, 2),
	COMP(0, "aclk_nvm_root_c", gpll_cpll_p, 0, 77, 2, 5, 7, 1),
	COMP(0, "cclk_emmc_c", gpll_cpll_xin24m_p, 0, 77, 8, 6, 14, 2),

	/* CRU_CLKSEL_CON78 */
	/* sclk_sfc_sel 13:12 */
	/* sclk_sfc_div 11:6 */
	COMP(0, "bclk_emmc_c", gpll_cpll_p, 0, 78, 0, 5, 5, 1),

	/* CRU_CLKSEL_CON79 */

	/* CRU_CLKSEL_CON80 */
	/* aclk_php_root_sel 13 */
	/* aclk_php_root_div 12:8 */
	/* aclk_pcie_root_sel 7 */
	/* aclk_pcie_root_div 6:2 */
	/* pclk_php_root_sel 1:0 */

	/* CRU_CLKSEL_CON81 */
	/* clk_gmac1_ptp_ref_sel 13 */
	/* clk_gmac1_ptp_ref_div 12:7 */
	/* clk_gmac0_ptp_ref_sel 6 */
	/* clk_gmac0_ptp_ref_div 5:0 */

	/* CRU_CLKSEL_CON82 */
	/* clk_rxoob1_sel 15 */
	/* clk_rxoob1_div 14:8 */
	/* clk_rxoob0_sel 7 */
	/* clk_rxoob0_div 6:0 */

	/* CRU_CLKSEL_CON83 */
	/* clk_gmac_125m_cru_i_sel 15 */
	/* clk_gmac_125m_cru_i_div 14:8 */
	/* clk_rxoob2_sel 7 */
	/* clk_rxoob2_div 6:0 */

	/* CRU_CLKSEL_CON84 */
	/* clk_utmi_otg2_sel 13:12 */
	/* clk_utmi_otg2_div 11:8 */
	/* clk_gmac_50m_cru_i_sel 7 */
	/* clk_gmac_50m_cru_i_div 6:0 */

	/* CRU_CLKSEL_CON85 */
	/* clk_gmac1_tx_125m_o_div 11:6 */
	/* clk_gmac0_tx_125m_o_div 5:0 */

	/* CRU_CLKSEL_CON86 */
	/* CRU_CLKSEL_CON87 */
	/* CRU_CLKSEL_CON88 */

	/* CRU_CLKSEL_CON89 */
	/* aclk_rkvdec_ccu_sel 15:14 */
	/* aclk_rkvdec_ccu_div 13:9 */
	/* aclk_rkvdec0_root_sel 8:7 */
	/* aclk_rkvdec0_root_div 6:2 */
	/* hclk_rkvdec0_root_sel 1:0 */

	/* CRU_CLKSEL_CON90 */
	/* clk_rkvdec0_hevc_ca_sel 12:11 */
	/* clk_rkvdec0_hevc_ca_div 10:6 */
	/* clk_rkvdec0_ca_sel 5 */
	/* clk_rkvdec0_ca_div 4:0 */

	/* CRU_CLKSEL_CON91 */
	/* clk_rkvdec0_core_sel 5 */
	/* clk_rkvdec0_core_div 4:0 */

	/* CRU_CLKSEL_CON92 */

	/* CRU_CLKSEL_CON93 */
	/* clk_rkvdec1_ca_sel 14 */
	/* clk_rkvdec1_ca_div 13:9 */
	/* aclk_rkvdec1_root_sel 8:7 */
	/* aclk_rkvdec1_root_div 6:2 */
	/* hclk_rkvdec1_root_sel 1:0 */

	/* CRU_CLKSEL_CON94 */
	/* clk_rkvdec1_core_sel 12 */
	/* clk_rkvdec1_core_div 11:7 */
	/* clk_rkvdec1_hevc_ca_sel 6:5 */
	/* clk_rkvdec1_hevc_ca_div 4:0 */

	/* CRU_CLKSEL_CON95 */

	/* CRU_CLKSEL_CON96 */
	MUX(0, "hclk_usb_root_m", clk100m_clk50m_xin24m_p, 0, 96, 6, 2),
	COMP(0, "aclk_usb_root_c", gpll_cpll_p, 0, 96, 0, 5, 5, 1),

	/* CRU_CLKSEL_CON97 */

	/* CRU_CLKSEL_CON98 */
	/* hclk_vdpu_root_sel 10:9 */
	/* aclk_vdpu_low_root_sel 8:7 */
	/* aclk_vdpu_root_sel 6:5 */
	/* aclk_vdpu_root_div 4:0 */

	/* CRU_CLKSEL_CON99 */
	/* clk_iep2p0_core_sel 12 */
	/* clk_iep2p0_core_div 11:7 */
	/* aclk_jpeg_decoder_root_sel 6:5 */
	/* aclk_jpeg_decoder_root_div 4:0 */

	/* CRU_CLKSEL_CON100 */
	/* clk_rga3_0_core_sel 15:13 */
	/* clk_rga3_0_core_div 12:8 */
	/* clk_rga2_core_sel 7:5 */
	/* clk_rga2_core_div 4:0 */

	/* CRU_CLKSEL_CON101 */

	/* CRU_CLKSEL_CON102 */
	/* clk_rkvenc0_core_sel 15:14 */
	/* clk_rkvenc0_core_div 13:9 */
	/* aclk_rkvenc0_root_sel 8:7 */
	/* aclk_rkvenc0_root_div 6:2 */
	/* hclk_rkvenc0_root_sel 1:0 */

	/* CRU_CLKSEL_CON103 */

	/* CRU_CLKSEL_CON104 */
	/* clk_rkvenc1_core_sel 15:14 */
	/* clk_rkvenc1_core_div 13:9 */
	/* aclk_rkvenc1_root_sel 8:7 */
	/* aclk_rkvenc1_root_div 6:2 */
	/* hclk_rkvenc1_root_sel 1:0 */

	/* CRU_CLKSEL_CON105 */

	/* CRU_CLKSEL_CON106 */
	/* pclk_vi_root_sel 11:10 */
	/* hclk_vi_root_sel 9:8 */
	/* aclk_vi_root_sel 7:5 */
	/* aclk_vi_root_div 4:0 */

	/* CRU_CLKSEL_CON107 */
	/* clk_isp0_core_sel 12:11 */
	/* clk_isp0_core_div 10:6 */
	/* dclk_vicap_sel 5 */
	/* dclk_vicap_div 4:0 */

	/* CRU_CLKSEL_CON108 */
	/* iclk_csihost01_sel 15:14 */
	/* clk_fisheye1_core_sel 13:12 */
	/* clk_fisheye1_core_div 11:7 */
	/* clk_fisheye0_core_sel 6:5 */
	/* clk_fisheye0_core_div 4:0 */

	/* CRU_CLKSEL_CON109 */

	/* CRU_CLKSEL_CON110 */
	/* pclk_vop_root_sel 13:12 */
	/* hclk_vop_root_sel 11:10 */
	/* aclk_vop_low_root_sel 9:8 */
	/* aclk_vop_root_sel 7:5 */
	/* aclk_vop_root_div 4:0 */

	/* CRU_CLKSEL_CON111 */
	/* dclk_vp1_src_sel 15:14 */
	/* dclk_vp1_src_div 13:9 */
	/* dclk_vp0_src_sel 8:7 */
	/* dclk_vp0_src_div 6:0 */

	/* CRU_CLKSEL_CON112 */
	/* dclk_vp2_sel 12:11 */
	/* dclk_vp1_sel 10:9 */
	/* dclk_vp0_sel 8:7 */
	/* dclk_vp2_src_sel 6:5 */
	/* dclk_vp2_src_div 4:0 */

	/* CRU_CLKSEL_CON113 */
	/* dclk_vp3_sel 8:7 */
	/* dclk_vp3_div 6:0 */

	/* CRU_CLKSEL_CON114 */
	/* clk_dsihost0_sel 8:7 */
	/* clk_dsihost0_div 6:0 */

	/* CRU_CLKSEL_CON115 */
	/* aclk_vop_sub_src_sel 9 */
	/* clk_dsihost1_sel 8:7 */
	/* clk_dsihost1_div 6:0 */

	/* CRU_CLKSEL_CON116 */
	/* pclk_vo0_s_root_sel 13:12 */
	/* pclk_vo0_root_sel 11:10 */
	/* hclk_vo0_s_root_sel 9:8 */
	/* hclk_vo0_root_sel 7:6 */
	/* aclk_vo0_root_sel 5 */
	/* aclk_vo0_root_div 4:0 */

	/* CRU_CLKSEL_CON117 */
	/* clk_aux16mhz_1_div 15:8 */
	/* clk_aux16mhz_0_div 7:0 */

	/* CRU_CLKSEL_CON118 */
	/* clk_i2s4_8ch_tx_src_sel 5 */
	/* clk_i2s4_8ch_tx_src_div 4:0 */

	/* CRU_CLKSEL_CON119 */
	/* clk_i2s4_8ch_tx_frac_div 31:0 */

	/* CRU_CLKSEL_CON120 */
	/* clk_i2s8_8ch_tx_src_sel 8 */
	/* clk_i2s8_8ch_tx_src_div 7:3 */
	/* mclk_i2s4_8ch_tx_sel 1:0 */

	/* CRU_CLKSEL_CON121 */
	/* clk_i2s8_8ch_tx_frac_div 31:0 */

	/* CRU_CLKSEL_CON122 */
	/* clk_spdif2_dp0_src_sel 8 */
	/* clk_spdif2_dp0_src_div 7:3 */
	/* mclk_i2s8_8ch_tx_sel 1:0 */

	/* CRU_CLKSEL_CON123 */
	/* clk_spdif2_dp0_frac_div 31:0 */

	/* CRU_CLKSEL_CON124 */
	/* clk_spdif5_dp1_src_sel 7 */
	/* clk_spdif5_dp1_src_div 6:2 */
	/* mclk_4x_spdif2_dp0_sel 1:0 */

	/* CRU_CLKSEL_CON125 */
	/* clk_spdif5_dp1_frac_div 31:0 */

	/* CRU_CLKSEL_CON126 */
	/* mclk_4x_spdif5_dp1_sel 1:0 */

	/* CRU_CLKSEL_CON127 */

	/* CRU_CLKSEL_CON128 */
	/* hclk_vo1_root_sel 14:13 */
	/* aclk_hdmirx_root_sel 12 */
	/* aclk_hdmirx_root_div 11:7 */
	/* aclk_hdcp1_root_sel 6:5 */
	/* aclk_hdcp1_root_div 4:0 */

	/* CRU_CLKSEL_CON129 */
	/* clk_i2s7_8ch_rx_src_sel 11 */
	/* clk_i2s7_8ch_rx_src_div 10:6 */
	/* pclk_vo1_s_root_sel 5:4 */
	/* pclk_vo1_root_sel 3:2 */
	/* hclk_vo1_s_root_sel 1:0 */

	/* CRU_CLKSEL_CON130 */
	/* clk_i2s7_8ch_rx_frac_div 31:0 */

	/* CRU_CLKSEL_CON131 */
	/* mclk_i2s7_8ch_rx_sel 1:0 */

	/* CRU_CLKSEL_CON132 */

	/* CRU_CLKSEL_CON133 */
	/* clk_hdmitx0_earc_sel 6 */
	/* clk_hdmitx0_earc_div 5:1 */

	/* CRU_CLKSEL_CON134 */
	/* CRU_CLKSEL_CON135 */

	/* CRU_CLKSEL_CON136 */
	/* clk_hdmitx1_earc_sel 6 */
	/* clk_hdmitx1_earc_div 5:1 */

	/* CRU_CLKSEL_CON137 */

	/* CRU_CLKSEL_CON138 */
	/* clk_hdmirx_aud_src_sel 8 */
	/* clk_hdmirx_aud_src_div 7:0 */

	/* CRU_CLKSEL_CON139 */
	/* clk_hdmirx_aud_frac_div 31:0 */

	/* CRU_CLKSEL_CON140 */
	/* clk_i2s5_8ch_tx_src_sel 10 */
	/* clk_i2s5_8ch_tx_src_div 9:5 */
	/* clk_edp1_200m_sel 4:3 */
	/* clk_edp0_200m_sel 2:1 */
	/* clk_hdmirx_aud_sel 0 */

	/* CRU_CLKSEL_CON141 */
	/* clk_i2s5_8ch_tx_frac_div 31:0 */

	/* CRU_CLKSEL_CON142 */
	/* mclk_i2s5_8ch_tx_sel 1:0 */

	/* CRU_CLKSEL_CON143 */

	/* CRU_CLKSEL_CON144 */
	/* clk_i2s6_8ch_tx_src_sel 8 */
	/* clk_i2s6_8ch_tx_src_div 7:3 */

	/* CRU_CLKSEL_CON145 */
	/* clk_i2s6_8ch_tx_frac_div 31:0 */

	/* CRU_CLKSEL_CON146 */
	/* clk_i2s6_8ch_rx_src_sel 7 */
	/* clk_i2s6_8ch_rx_src_div 6:2 */
	/* mclk_i2s6_8ch_tx_sel 1:0 */

	/* CRU_CLKSEL_CON147 */
	/* clk_i2s6_8ch_rx_frac_div 31:0 */

	/* CRU_CLKSEL_CON148 */
	/* clk_spdif3_src_sel 9 */
	/* clk_spdif3_src_div 8:4 */
	/* i2s6_8ch_mclkout_sel 3:2 */
	/* mclk_i2s6_8ch_rx_sel 1:0 */

	/* CRU_CLKSEL_CON149 */
	/* clk_spdif3_frac_div 31:0 */

	/* CRU_CLKSEL_CON150 */
	/* clk_spdif4_src_sel 7 */
	/* clk_spdif4_src_div 6:2 */
	/* mclk_spdif3_sel 1:0 */

	/* CRU_CLKSEL_CON151 */
	/* clk_spdif4_frac_div 31:0 */

	/* CRU_CLKSEL_CON152 */
	/* mclk_spdifrx1_sel 15:14 */
	/* mclk_spdifrx1_div 13:9 */
	/* mclk_spdifrx0_sel 8:7 */
	/* mclk_spdifrx0_div 6:2 */
	/* mclk_spdif4_sel 1:0 */

	/* CRU_CLKSEL_CON153 */
	/* clk_i2s9_8ch_rx_src_sel 12 */
	/* clk_i2s9_8ch_rx_src_div 11:7 */
	/* mclk_spdifrx2_sel 6:5 */
	/* mclk_spdifrx2_div 4:0 */

	/* CRU_CLKSEL_CON154 */
	/* clk_i2s9_8ch_rx_frac_div 31:0 */

	/* CRU_CLKSEL_CON155 */
	/* clk_i2s10_8ch_rx_src_sel 8 */
	/* clk_i2s10_8ch_rx_src_div 7:3 */
	/* mclk_i2s9_8ch_rx_sel 1:0 */

	/* CRU_CLKSEL_CON156 */
	/* clk_i2s10_8ch_rx_frac_div 31:0 */

	/* CRU_CLKSEL_CON157 */
	/* clk_hdmitrx_refsrc_sel 7 */
	/* clk_hdmitrx_refsrc_div 6:2 */
	/* mclk_i2s10_8ch_rx_sel 1:0 */

	/* CRU_CLKSEL_CON158 */
	/* clk_gpu_src_sel 14 */
	/* clk_testout_gpu_sel 13 */
	/* clk_testout_gpu_div 12:8 */
	/* clk_gpu_src_t_sel 7:5 */
	/* clk_gpu_src_t_div 4:0 */

	/* CRU_CLKSEL_CON159 */
	/* aclk_m0_gpu_biu_div 14:10 */
	/* aclk_s_gpu_biu_div 9:5 */
	/* clk_gpu_stacks_div 4:0 */

	/* CRU_CLKSEL_CON160 */
	/* aclk_m3_gpu_biu_div 14:10 */
	/* aclk_m2_gpu_biu_div 9:5 */
	/* aclk_m1_gpu_biu_div 4:0 */

	/* CRU_CLKSEL_CON161 */
	/* clk_gpu_pvtpll_sel 2 */
	/* pclk_gpu_root_sel 1:0 */

	/* CRU_CLKSEL_CON162 */

	/* CRU_CLKSEL_CON163 */
	/* pclk_av1_root_sel 8:7 */
	/* aclk_av1_root_sel 6:5 */
	/* aclk_av1_root_div 4:0 */

	/* CRU_CLKSEL_CON164 */

	/* CRU_CLKSEL_CON165 */
	/* clk_ddr_timer_root_sel 12 */
	/* aclk_center_s400_root_sel 11:10 */
	/* aclk_center_s200_root_sel 9:8 */
	/* pclk_center_root_sel 7:6 */
	/* hclk_center_root_sel 5:4 */
	/* aclk_center_low_root_sel 3:2 */
	/* aclk_center_root_sel 1:0 */

	/* CRU_CLKSEL_CON166 */
	/* clk_ddr_cm0_rtc_sel 5 */
	/* clk_ddr_cm0_rtc_div 4:0 */

	/* CRU_CLKSEL_CON167 */
	/* CRU_CLKSEL_CON168 */
	/* CRU_CLKSEL_CON169 */

	/* CRU_CLKSEL_CON170 */
	/* hclk_vo1usb_top_root_sel 7:6 */
	/* aclk_vo1usb_top_root_sel 5 */
	/* aclk_vo1usb_top_root_div 4:0 */

	/* CRU_CLKSEL_CON171 */

	/* CRU_CLKSEL_CON172 */
	/* cclk_src_sdio_sel 9:8 */
	/* cclk_src_sdio_div 7:2 */
	/* hclk_sdio_root_sel 1:0 */

	/* CRU_CLKSEL_CON173 */

	/* CRU_CLKSEL_CON174 */
	/* clk_rga3_1_core_sel 15:14 */
	/* clk_rga3_1_core_div 13:9 */
	/* hclk_rga3_root_sel 8:7 */
	/* aclk_rga3_root_sel 6:5 */
	/* aclk_rga3_root_div 4:0 */

	/* CRU_CLKSEL_CON175 */

	/* CRU_CLKSEL_CON176 */
	/* clk_ref_pipe_phy1_pll_src_div 11:6 */
	/* clk_ref_pipe_phy0_pll_src_div 5:0 */

	/* CRU_CLKSEL_CON177 */
	/* clk_ref_pipe_phy2_sel 8 */
	/* clk_ref_pipe_phy1_sel 7 */
	/* clk_ref_pipe_phy0_sel 6 */
	/* clk_ref_pipe_phy2_pll_src_di 5:0 */

	/* PMU1CRU_CLKSEL_CON00 */
	CDIVRAW(0, "clk_pmu1_50m_src_c", "clk_pmu1_400m_src", 0, RK3588_PMUCRU_CLKSEL_CON(0), 0, 3),
	CDIVRAW(0, "clk_pmu1_100m_src_c", "clk_pmu1_400m_src", 0, RK3588_PMUCRU_CLKSEL_CON(0), 4, 3),
	CDIVRAW(0, "clk_pmu1_200m_src_c", "clk_pmu1_400m_src", 0, RK3588_PMUCRU_CLKSEL_CON(0), 7, 3),
	COMPRAW(0, "clk_pmu1_300m_src_c", clk300m_xin24m_p, 0, RK3588_PMUCRU_CLKSEL_CON(0), 10, 4, 15, 1),

	/* PMU1CRU_CLKSEL_CON01 */
	COMPRAW(0, "clk_pmu1_400m_src_c", clk400m_xin24m_p, 0, RK3588_PMUCRU_CLKSEL_CON(1), 0, 5, 5, 1),
	MUXRAW(0, "hclk_pmu1_root_m", clk200m_clk100m_clk50m_xin24m_p, 0, RK3588_PMUCRU_CLKSEL_CON(1), 6, 2),
	MUXRAW(0, "pclk_pmu1_root_m", clk100m_clk50m_xin24m_p, 0, RK3588_PMUCRU_CLKSEL_CON(1), 8, 2),
	MUXRAW(0, "hclk_pmu_cm0_root_m", clk400m_clk200m_clk100m_xin24m_p, 0, RK3588_PMUCRU_CLKSEL_CON(1), 10, 2),
	/* reserved 12:15 */

	/* PMU1CRU_CLKSEL_CON02 */
	/* clk_pmu_cm0_rtc_div 0:4 */
	/* clk_pmu_cm0_rtc_sel 5 */
	/* tclk_pmu1wdt_sel 6 */
	/* clk_pmu1timer_root_sel 7:8 */
	/* clk_pmu1pwm_sel 9:10 */
	/* reserved 11:15 */

	/* PMU1CRU_CLKSEL_CON03 */
	/* reserved 0:5 */
	MUXRAW(0, "clk_i2c0_m", pmu_200m_100m_p, 0, RK3588_PMUCRU_CLKSEL_CON(3), 6, 1),
	/* clk_uart0_src_div 7:11 */
	/* reserved 12:15 */

	/* PMU1CRU_CLKSEL_CON04 */
	/* clk_uart0_frac_div 0:31 */

	/* PMU1CRU_CLKSEL_CON05 */
	/* sclk_uart0_sel 0:1 */
	/* clk_i2s1_8ch_tx_src_div 2:6 */
	/* reserved 7:15 */

	/* PMU1CRU_CLKSEL_CON06 */
	/* clk_i2s1_8ch_tx_frac_div 0:31 */

	/* PMU1CRU_CLKSEL_CON07 */
	/* mclk_i2s1_8ch_tx_sel 0:1 */
	/* clk_i2s1_8ch_rx_src_div 2:6 */
	/* reserved 7:15 */

	/* PMU1CRU_CLKSEL_CON08 */
	/* clk_i2s1_8ch_rx_frac_div 0:31 */

	/* PMU1CRU_CLKSEL_CON09 */
	/* mclk_i2s1_8ch_rx_sel 0:1 */
	/* i2s1_8ch_mclkout_sel 2:3 */
	/* mclk_pdm0_sel 4 */
	/* clk_usbdp_combo_phy0_ref_xtal_div 5:9 */
	/* clk_usbdp_combo_phy0_ref_xtal_sel 10 */
	/* reserved 11:15 */

	/* PMU1CRU_CLKSEL_CON10 Unused */
	/* PMU1CRU_CLKSEL_CON11 Unused */

	/* PMU1CRU_CLKSEL_CON12 */
	/* reserved 0:5 */
	/* clk_hdptx0_ref_xtal_div 6:10 */
	/* clk_hdptx0_ref_xtal_sel 11 */
	/* reserved 12:15 */

	/* PMU1CRU_CLKSEL_CON13 Unused */

	/* PMU1CRU_CLKSEL_CON14 */
	/* clk_ref_mipi_dcphy0_div 0:6 */
	/* clk_ref_mipi_dcphy0_sel 7:8 */
	/* clk_otgphy_u3_0_div 9:13 */
	/* clk_otgphy_u3_0_sel 14 */
	/* reserved 15 */

	/* PMU1CRU_CLKSEL_CON15 */
	/* clk_cr_para_div 0:4 */
	/* clk_cr_para_sel 5:6 */
	/* reserved 7:15 */

	/* PMU1CRU_CLKSEL_CON16 Unused */

	/* PMU1CRU_CLKSEL_CON17 */
	/* dbclk_gpio0_sel 0 */
	MUXRAW(0, "dbclk_gpio0_m", xin24m_xin32k_p, 0, RK3588_PMUCRU_CLKSEL_CON(17), 0, 1),
	/* reserved 1:15 */
};

/* GATES */
static struct rk_cru_gate rk3588_gates[] = {
	/* CRU_GATE_CON00 */
	GATE(CLK_50M_SRC, "clk_50m_src", "clk_50m_src_c", 0, 0),
	GATE(CLK_100M_SRC, "clk_100m_src", "clk_100m_src_c", 0, 1),
	GATE(CLK_150M_SRC, "clk_150m_src", "clk_150m_src_c", 0, 2),
	GATE(CLK_200M_SRC, "clk_200m_src", "clk_200m_src_c", 0, 3),
	GATE(CLK_250M_SRC, "clk_250m_src", "clk_250m_src_c", 0, 4),
	GATE(CLK_300M_SRC, "clk_300m_src", "clk_300m_src_c", 0, 5),
	GATE(CLK_350M_SRC, "clk_350m_src", "clk_350m_src_c", 0, 6),
	GATE(CLK_400M_SRC, "clk_400m_src", "clk_400m_src_c", 0, 7),
	GATE(CLK_450M_SRC, "clk_450m_src", "clk_450m_src_c", 0, 8),
	GATE(CLK_500M_SRC, "clk_500m_src", "clk_500m_src_c", 0, 9),
	GATE(CLK_600M_SRC, "clk_600m_src", "clk_600m_src_c", 0, 10),
	GATE(CLK_650M_SRC, "clk_650m_src", "clk_650m_src_c", 0, 11),
	GATE(CLK_700M_SRC, "clk_700m_src", "clk_700m_src_c", 0, 12),
	GATE(CLK_800M_SRC, "clk_800m_src", "clk_800m_src_c", 0, 13),
	GATE(CLK_1000M_SRC, "clk_1000m_src", "clk_1000m_src_c", 0, 14),
	GATE(CLK_1200M_SRC, "clk_1200m_src", "clk_1200m_src_c", 0, 15),

	/* CRU_GATE_CON01 */
	GATE(ACLK_TOP_ROOT, "aclk_top_root", "aclk_top_root_c", 1, 0),
	GATE(PCLK_TOP_ROOT, "pclk_top_root", "pclk_top_root_m", 1, 1),
	GATE(ACLK_LOW_TOP_ROOT, "aclk_low_top_root", "aclk_low_top_root_c", 1, 2),
	/* aclk_top_biu_en 3 */
	/* pclk_top_biu_en 4 */
	/* reserved 5 */
	/* pclk_csiphy0_en 6 */
	/* reserved 7 */
	/* pclk_csiphy1_en 8 */
	/* reserved 9 */
	GATE(ACLK_TOP_M300_ROOT, "aclk_top_m300_root", "aclk_top_m300_root_m", 1, 10),
	GATE(ACLK_TOP_M500_ROOT, "aclk_top_m500_root", "aclk_top_m500_root_m", 1, 14),
	GATE(ACLK_TOP_M400_ROOT, "aclk_top_m400_root", "aclk_top_m400_root_m", 1, 14),
	GATE(ACLK_TOP_S200_ROOT, "aclk_top_s200_root", "aclk_top_s200_root_m", 1, 14),
	GATE(ACLK_TOP_S400_ROOT, "aclk_top_s400_root", "aclk_top_s400_root_m", 1, 14),
	/* aclk_top_m500_biu_en */

	/* CRU_GATE_CON02 */
	/* aclk_top_m400_biu_en 0 */
	/* aclk_top_s200_biu_en 1 */
	/* aclk_top_s400_biu_en 2 */
	/* aclk_top_m300_biu_en 3 */
	/* clk_testout_top_en 4 */
	/* reserved 5 */
	/* clk_testout_grp0_en 6 */
	/* reserved 7 */
	/* clk_usbdp_combo_phy0_immortal_en 8 */
	/* reserved 14:9 */
	/* clk_usbdp_combo_phy1_immortal_en 15 */

	/* CRU_GATE_CON03 */
	/* reserved 13:0 */
	/* pclk_mipi_dcphy0_en 14 */
	/* pclk_mipi_dcphy0_grf_en 15 */

	/* CRU_GATE_CON04 */
	/* reserved 2:0 */
	/* pclk_mipi_dcphy1_en 3 */
	/* pclk_mipi_dcphy1_grf_en 4 */
	/* pclk_apb2asb_slv_cdphy_en 5 */
	/* pclk_apb2asb_slv_csiphy_en 6 */
	/* pclk_apb2asb_slv_vccio3_5_en 7 */
	/* pclk_apb2asb_slv_vccio6_en 8 */
	/* pclk_apb2asb_slv_emmcio_en 9 */
	/* pclk_apb2asb_slv_ioc_top_en 10 */
	/* pclk_apb2asb_slv_ioc_right_en 11 */
	/* reserved 15:12 */

	/* CRU_GATE_CON05 */
	/* pclk_cru_en 0 */
	/* reserved 2:1 */
	GATE(MCLK_GMAC0_OUT, "mclk_gmac0_out", "mclk_gmac0_out_c", 5, 3),
	GATE(REFCLKO25M_ETH0_OUT, "refclk025m_eth0_out", "refclk025m_eth0_out_c", 5, 4),
	GATE(REFCLKO25M_ETH1_OUT, "refclk025m_eth1_out", "refclk025m_eth1_out_c", 5, 5),
	/* clk_cifout_out_en 6 */
	/* aclk_channel_secure2vo1usb_en 7 */
	/* aclk_channel_secure2center_en 8 */
	/* clk_mipi_cameraout_m0_en 9 */
	/* clk_mipi_cameraout_m1_en 10 */
	/* clk_mipi_cameraout_m2_en 11 */
	/* clk_mipi_cameraout_m3_en 12 */
	/* clk_mipi_cameraout_m4_en 13 */
	/* hclk_channel_secure2vo1usb_en 14 */
	/* hclk_channel_secure2center_en 15 */

	/* CRU_GATE_CON06 */
	/* pclk_channel_secure2vo1usb_en 0 */
	/* pclk_channel_secure2center_en 1 */
	/* reserved 15:2 */

	/* CRU_GATE_CON07 */
	/* hclk_audio_root_en 0 */
	/* pclk_audio_root_en 1 */
	/* hclk_audio_biu_en 2 */
	/* pclk_audio_biu_en 3 */
	/* hclk_i2s0_8ch_en 4 */
	/* clk_i2s0_8ch_tx_en 5 */
	/* clk_i2s0_8ch_frac_tx_en 6 */
	/* mclk_i2s0_8ch_tx_en 7 */
	/* clk_i2s0_8ch_rx_en 8 */
	/* clk_i2s0_8ch_frac_rx_en 9 */
	/* mclk_i2s0_8ch_rx_en 10 */
	/* pclk_acdcdig_en 11 */
	/* hclk_i2s2_2ch_en 12 */
	/* hclk_i2s3_2ch_en 13 */
	/* clk_i2s2_2ch_en 14 */
	/* clk_i2s2_2ch_frac_en 15 */

	/* CRU_GATE_CON08 */
	/* mclk_i2s2_2ch_en 0 */
	/* clk_i2s3_2ch_en 1 */
	/* clk_i2s3_2ch_frac_en 2 */
	/* mclk_i2s3_2ch_en 3 */
	/* clk_dac_acdcdig_en 4 */
	/* reserved 13:5 */
	/* hclk_spdif0_en 14 */
	/* clk_spdif0_en 15 */

	/* CRU_GATE_CON09 */
	/* clk_spdif0_frac_en 0 */
	/* mclk_spdif0_en 1 */
	/* hclk_spdif1_en 2 */
	/* clk_spdif1_en 3 */
	/* clk_spdif1_frac_en 4 */
	/* mclk_spdif1_en 5 */
	/* hclk_pdm1_en 6 */
	/* mclk_pdm1_en 7 */
	/* reserver 15:8 */

	/* CRU_GATE_CON10 */
	/* aclk_bus_root_en 0 */
	/* aclk_bus_biu_en 1 */
	/* pclk_bus_biu_en 2 */
	/* aclk_gic_en 3 */
	/* reserved 4 */
	/* aclk_dmac0_en 5 */
	/* aclk_dmac1_en 6 */
	/* aclk_dmac2_en 7 */
	GATE(PCLK_I2C1, "pclk_i2c1", "pclk_top_root", 10, 8),
	GATE(PCLK_I2C2, "pclk_i2c2", "pclk_top_root", 10, 9),
	GATE(PCLK_I2C3, "pclk_i2c3", "pclk_top_root", 10, 10),
	GATE(PCLK_I2C4, "pclk_i2c4", "pclk_top_root", 10, 11),
	GATE(PCLK_I2C5, "pclk_i2c5", "pclk_top_root", 10, 12),
	GATE(PCLK_I2C6, "pclk_i2c6", "pclk_top_root", 10, 13),
	GATE(PCLK_I2C7, "pclk_i2c7", "pclk_top_root", 10, 14),
	GATE(PCLK_I2C8, "pclk_i2c8", "pclk_top_root", 10, 15),

	/* CRU_GATE_CON11 */
	GATE(CLK_I2C1, "clk_i2c1", "clk_i2c1_m", 11, 0),
	GATE(CLK_I2C2, "clk_i2c2", "clk_i2c2_m", 11, 1),
	GATE(CLK_I2C3, "clk_i2c3", "clk_i2c3_m", 11, 2),
	GATE(CLK_I2C4, "clk_i2c4", "clk_i2c4_m", 11, 3),
	GATE(CLK_I2C5, "clk_i2c5", "clk_i2c5_m", 11, 4),
	GATE(CLK_I2C6, "clk_i2c6", "clk_i2c6_m", 11, 5),
	GATE(CLK_I2C7, "clk_i2c7", "clk_i2c7_m", 11, 6),
	GATE(CLK_I2C8, "clk_i2c8", "clk_i2c8_m", 11, 7),
	/* pclk_can0_en 8 */
	/* clk_can0_en 9 */
	/* pclk_can1_en 10 */
	/* clk_can1_en 11 */
	/* pclk_can2_en 12 */
	/* clk_can2_en 13 */
	/* pclk_saradc_en 14 */
	/* clk_saradc_en 15 */

	/* CRU_GATE_CON12 */
	/* pclk_tsadc_en 0 */
	/* clk_tsadc_en 1 */
	/* pclk_uart1_en 2 */
	/* pclk_uart2_en 3 */
	/* pclk_uart3_en 4 */
	/* pclk_uart4_en 5 */
	/* pclk_uart5_en 6 */
	/* pclk_uart6_en 7 */
	/* pclk_uart7_en 8 */
	/* pclk_uart8_en 9 */
	/* pclk_uart9_en 10 */
	/* clk_uart1_en 11 */
	/* clk_uart1_frac_en 12 */
	/* sclk_uart1_en 13 */
	/* clk_uart2_en 14 */
	/* clk_uart2_frac_en 15 */

	/* CRU_GATE_CON13 */
	/* sclk_uart2_en 0 */
	/* clk_uart3_en 1 */
	/* clk_uart3_frac_en 2 */
	/* sclk_uart3_en 3 */
	/* clk_uart4_en 4 */
	/* clk_uart4_frac_en 5 */
	/* sclk_uart4_en 6 */
	/* clk_uart5_en 7 */
	/* clk_uart5_frac_en 8 */
	/* sclk_uart5_en 9 */
	/* clk_uart6_en 10 */
	/* clk_uart6_frac_en 11 */
	/* sclk_uart6_en 12 */
	/* clk_uart7_en 13 */
	/* clk_uart7_frac_en 14 */
	/* sclk_uart7_en 15 */

	/* CRU_GATE_CON14 */
	/* clk_uart8_en 0 */
	/* clk_uart8_frac_en 1 */
	/* sclk_uart8_en 2 */
	/* clk_uart9_en 3 */
	/* clk_uart9_frac_en 4 */
	/* sclk_uart9_en 5 */
	/* pclk_spi0_en 6 */
	/* pclk_spi1_en 7 */
	/* pclk_spi2_en 8 */
	/* pclk_spi3_en 9 */
	/* pclk_spi4_en 10 */
	/* clk_spi0_en 11 */
	/* clk_spi1_en 12 */
	/* clk_spi2_en 13 */
	/* clk_spi3_en 14 */
	/* clk_spi4_en 15 */

	/* CRU_GATE_CON15 */
	/* pclk_wdt0_en 0 */
	/* tclk_wdt0_en 1 */
	/* pclk_sys_grf_en 2 */
	/* pclk_pwm1_en 3 */
	/* clk_pwm1_en 4 */
	/* clk_pwm1_capture_en 5 */
	/* pclk_pwm2_en 6 */
	/* clk_pwm2_en 7 */
	/* clk_pwm2_capture_en 8 */
	/* pclk_pwm3_en 9 */
	/* clk_pwm3_en 10 */
	/* clk_pwm3_capture_en 11 */
	/* pclk_bustimer0_en 12 */
	/* pclk_bustimer1_en 13 */
	/* clk_bustimer_root_en 14 */
	/* clk_bustimer0_en 15 */

	/* CRU_GATE_CON16 */
	/* clk_bustimer1_en 0 */
	/* clk_bustimer2_en 1 */
	/* clk_bustimer3_en 2 */
	/* clk_bustimer4_en 3 */
	/* clk_bustimer5_en 4 */
	/* clk_bustimer6_en 5 */
	/* clk_bustimer7_en 6 */
	/* clk_bustimer8_en 7 */
	/* clk_bustimer9_en 8 */
	/* clk_bustimer10_en 9 */
	/* clk_bustimer11_en 10 */
	/* pclk_mailbox0_en 11 */
	/* pclk_mailbox1_en 12 */
	/* pclk_mailbox2_en 13 */
	/* pclk_gpio1_en 14 */
	/* dbclk_gpio1_en 15 */

	/* CRU_GATE_CON17 */
	/* pclk_gpio2_en 0 */
	/* dbclk_gpio2_en 1 */
	/* pclk_gpio3_en 2 */
	/* dbclk_gpio3_en 3 */
	/* pclk_gpio4_en 4 */
	/* dbclk_gpio4_en 5 */
	/* aclk_decom_en 6 */
	/* pclk_decom_en 7 */
	/* dclk_decom_en 8 */
	/* pclk_top_en 9 */
	/* reserved 10 */
	/* aclk_gicadb_gic2core_bus_en 11 */
	/* pclk_dft2apb_en 12 */
	/* pclk_apb2asb_mst_top_en 13 */
	/* pclk_apb2asb_mst_cdphy_en 14 */
	/* pclk_apb2asb_mst_bot_right_en 15 */

	/* CRU_GATE_CON18 */
	/* pclk_apb2asb_mst_ioc_top_en 0 */
	/* pclk_apb2asb_mst_ioc_right_en 1 */
	/* pclk_apb2asb_mst_csiphy_en 2 */
	/* pclk_apb2asb_mst_vccio3_5_en 3 */
	/* pclk_apb2asb_mst_vccio6_en 4 */
	/* pclk_apb2asb_mst_emmcio_en 5 */
	/* aclk_spinlock_en 6 */
	/* reserved 8:7 */
	/* pclk_otpc_ns_en 9 */
	/* clk_otpc_ns_en 10 */
	/* clk_otpc_arb_en 11 */
	/* clk_otpc_auto_rd_en 12 */
	/* clk_otp_phy_en 13 */
	/* reserved 15:14 */

	/* CRU_GATE_CON19 */
	/* pclk_busioc_en 0 */
	/* clk_bisrintf_pllsrc_en 1 */
	/* clk_bisrintf_en 2 */
	/* pclk_pmu2_en 3 */
	/* pclk_pmucm0_intmux_en 4 */
	/* pclk_ddrcm0_intmux_en 5 */
	/* reserved 15:6 */

	/* CRU_GATE_CON20 */
	/* pclk_ddr_dfictl_ch0_en 0 */
	/* pclk_ddr_mon_ch0_en 1 */
	/* pclk_ddr_standby_ch0_en 2 */
	/* pclk_ddr_upctl_ch0_en 3 */
	/* tmclk_ddr_mon_ch0_en 4 */
	/* pclk_ddr_grf_ch01_en 5 */
	/* clk_dfi_ch0_en 6 */
	/* clk_sbr_ch0_en 7 */
	/* clk_ddr_upctl_ch0_en 8 */
	/* clk_ddr_dfictl_ch0_en 9 */
	/* clk_ddr_mon_ch0_en 10 */
	/* clk_ddr_standby_ch0_en 11 */
	/* aclk_ddr_upctl_ch0_en 12 */
	/* pclk_ddr_dfictl_ch1_en 13 */
	/* pclk_ddr_mon_ch1_en 14 */
	/* pclk_ddr_standby_ch1_en 15 */

	/* CRU_GATE_CON21 */
	/* pclk_ddr_upctl_ch1_en 0 */
	/* tmclk_ddr_mon_ch1_en 1 */
	/* clk_dfi_ch1_en 2 */
	/* clk_sbr_ch1_en 3 */
	/* clk_ddr_upctl_ch1_en 4 */
	/* clk_ddr_dfictl_ch1_en 5 */
	/* clk_ddr_mon_ch1_en 6 */
	/* clk_ddr_standby_ch1_en 7 */
	/* aclk_ddr_upctl_ch1_en 8 */
	/* reserved 12:9 */
	/* aclk_ddr_ddrsch0_en 13 */
	/* aclk_ddr_rs_ddrsch0_en 14 */
	/* aclk_ddr_frs_ddrsch0_en 15 */

	/* CRU_GATE_CON22 */
	/* aclk_ddr_scramble0_en 0 */
	/* aclk_ddr_frs_scramble0_en 1 */
	/* aclk_ddr_ddrsch1_en 2 */
	/* aclk_ddr_rs_ddrsch1_en 3 */
	/* aclk_ddr_frs_ddrsch1_en 4 */
	/* aclk_ddr_scramble1_en 5 */
	/* aclk_ddr_frs_scramble1_en 6 */
	/* pclk_ddr_ddrsch0_en 7 */
	/* pclk_ddr_ddrsch1_en 8 */
	/* clk_testout_ddr01_en 9 */
	/* reserved 15:10 */

	/* CRU_GATE_CON23 */
	/* pclk_ddr_dfictl_ch2_en 0 */
	/* pclk_ddr_mon_ch2_en 1 */
	/* pclk_ddr_standby_ch2_en 2 */
	/* pclk_ddr_upctl_ch2_en 3 */
	/* tmclk_ddr_mon_ch2_en 4 */
	/* pclk_ddr_grf_ch23_en 5 */
	/* clk_dfi_ch2_en 6 */
	/* clk_sbr_ch2_en 7 */
	/* clk_ddr_upctl_ch2_en 8 */
	/* clk_ddr_dfictl_ch2_en 9 */
	/* clk_ddr_mon_ch2_en 10 */
	/* clk_ddr_standby_ch2_en 11 */
	/* aclk_ddr_upctl_ch2_en 12 */
	/* pclk_ddr_dfictl_ch3_en 13 */
	/* pclk_ddr_mon_ch3_en 14 */
	/* pclk_ddr_standby_ch3_en 15 */

	/* CRU_GATE_CON24 */
	/* pclk_ddr_upctl_ch3_en 0 */
	/* tmclk_ddr_mon_ch3_en 1 */
	/* clk_dfi_ch3_en 2 */
	/* clk_sbr_ch3_en 3 */
	/* clk_ddr_upctl_ch3_en 4 */
	/* clk_ddr_dfictl_ch3_en 5 */
	/* clk_ddr_mon_ch3_en 6 */
	/* clk_ddr_standby_ch3_en 7 */
	/* aclk_ddr_upctl_ch3_en 8 */
	/* reserved 12:9 */
	/* aclk_ddr_ddrsch2_en 13 */
	/* aclk_ddr_rs_ddrsch2_en 14 */
	/* aclk_ddr_frs_ddrsch2_en 15 */

	/* CRU_GATE_CON25 */
	/* aclk_ddr_scramble2_en 0 */
	/* aclk_ddr_frs_scramble2_en 1 */
	/* aclk_ddr_ddrsch3_en 2 */
	/* aclk_ddr_rs_ddrsch3_en 3 */
	/* aclk_ddr_frs_ddrsch3_en 4 */
	/* aclk_ddr_scramble3_en 5 */
	/* aclk_ddr_frs_scramble3_en 6 */
	/* pclk_ddr_ddrsch2_en 7 */
	/* pclk_ddr_ddrsch3_enpclk_ddr_ddrsch3_en 8 */
	/* clk_testout_ddr23_en 9 */
	/* reserved 15:10 */

	/* CRU_GATE_CON26 */
	/* aclk_isp1_root_en 0 */
	/* hclk_isp1_root_en 1 */
	/* clk_isp1_core_en 2 */
	/* clk_isp1_core_marvin_en 3 */
	/* clk_isp1_core_vicap_en 4 */
	/* aclk_isp1_en 5 */
	/* aclk_isp1_biu_en 6 */
	/* hclk_isp1_en 7 */
	/* hclk_isp1_biu_en 8 */
	/* reserved 15:9 */

	/* CRU_GATE_CON27 */
	/* aclk_rknn1_en 0 */
	/* aclk_rknn1_biu_en 1 */
	/* hclk_rknn1_en 2 */
	/* hclk_rknn1_biu_en 3 */
	/* reserved 15:4 */

	/* CRU_GATE_CON28 */
	/* aclk_rknn2_en 0 */
	/* aclk_rknn2_biu_en 1 */
	/* hclk_rknn2_en 2 */
	/* hclk_rknn2_biu_en 3 */
	/* reserved 15:4 */

	/* CRU_GATE_CON29 */
	/* hclk_rknn_root_en 0 */
	/* clk_rknn_dsu0_df_en 1 */
	/* clk_testout_npu_en 2 */
	/* clk_rknn_dsu0_en 3 */
	/* pclk_nputop_root_en 4 */
	/* pclk_nputop_biu_en 5 */
	/* pclk_npu_timer_en 6 */
	/* clk_nputimer_root_en 7 */
	/* clk_nputimer0_en 8 */
	/* clk_nputimer1_en 9 */
	/* pclk_npu_wdt_en 10 */
	/* tclk_npu_wdt_en 11 */
	/* pclk_pvtm1_en 12 */
	/* pclk_npu_grf_en 13 */
	/* clk_pvtm1_en 14 */
	/* clk_npu_pvtm_en 15 */

	/* CRU_GATE_CON30 */
	/* clk_npu_pvtpll_en 0 */
	/* hclk_npu_cm0_root_en 1 */
	/* hclk_npu_cm0_biu_en 2 */
	/* fclk_npu_cm0_core_en 3 */
	/* reserved 4 */
	/* clk_npu_cm0_rtc_en 5 */
	/* aclk_rknn0_en 6 */
	/* aclk_rknn0_biu_en 7 */
	/* hclk_rknn0_en 8 */
	/* hclk_rknn0_biu_en 9 */
	/* reserved 15:10 */

	/* CRU_GATE_CON31 */
	GATE(HCLK_NVM_ROOT, "hclk_nvm_root", "hclk_nvm_root_m", 31, 0),
	GATE(ACLK_NVM_ROOT, "aclk_nvm_root", "aclk_nvm_root_c", 31, 1),
	/* hclk_nvm_biu_en 2 */
	/* aclk_nvm_biu_en 3 */
	/* hclk_emmc_en 4 */
	/* aclk_emmc_en 5 */
	GATE(CCLK_EMMC, "cclk_emmc", "cclk_emmc_c", 31, 6),
	GATE(BCLK_EMMC, "bclk_emmc", "bclk_emmc_c", 31, 7),
	GATE(TMCLK_EMMC, "tmclk_emmc", "xin24m", 31, 8),
	/* sclk_sfc_en 9 */
	/* hclk_sfc_en 10 */
	/* hclk_sfc_xip_en 11 */
	/* reserved 15:12 */

	/* CRU_GATE_CON32 */
	/* pclk_php_root_en 0 */
	/* pclk_grf_en 1 */
	/* pclk_dec_biu_en 2 */
	/* pclk_gmac0_en 3 */
	/* pclk_gmac1_en 4 */
	/* pclk_php_biu_en 5 */
	/* aclk_pcie_root_en 6 */
	/* aclk_php_root_en 7 */
	/* aclk_pcie_bridge_en 8 */
	/* aclk_php_biu_en 9 */
	/* aclk_gmac0_en 10 */
	/* aclk_gmac1_en 11 */
	/* aclk_pcie_biu_en 12 */
	/* aclk_pcie_4l_dbi_en 13 */
	/* aclk_pcie_2l_dbi_en 14 */
	/* aclk_pcie_1l0_dbi_en 15 */

	/* CRU_GATE_CON33 */
	/* aclk_pcie_1l1_dbi_en 0 */
	/* aclk_pcie_1l2_dbi_en 1 */
	/* aclk_pcie_4l_mstr_en 2 */
	/* aclk_pcie_2l_mstr_en 3 */
	/* aclk_pcie_1l0_mstr_en 4 */
	/* aclk_pcie_1l1_mstr_en 5 */
	/* aclk_pcie_1l2_mstr_en 6 */
	/* aclk_pcie_4l_slv_en 7 */
	/* aclk_pcie_2l_slv_en 8 */
	/* aclk_pcie_1l0_slv_en 9 */
	/* aclk_pcie_1l1_slv_en 10 */
	/* aclk_pcie_1l2_slv_en 11 */
	/* pclk_pcie_4l_en 12 */
	/* pclk_pcie_2l_en 13 */
	/* pclk_pcie_1l0_en 14 */
	/* pclk_pcie_1l1_en 15 */

	/* CRU_GATE_CON34 */
	/* pclk_pcie_1l2_en 0 */
	/* clk_pcie_4l_aux_en 1 */
	/* clk_pcie_2l_aux_en 2 */
	/* clk_pcie_1l0_aux_en 3 */
	/* clk_pcie_1l1_aux_en 4 */
	/* clk_pcie_1l2_aux_en 5 */
	/* aclk_php_gic_its_en 6 */
	/* aclk_mmu_pcie_en 7 */
	/* aclk_mmu_php_en 8 */
	/* aclk_mmu_biu_en 9 */
	/* clk_gmac0_ptp_ref_en 10 */
	/* clk_gmac1_ptp_ref_en 11 */
	/* reserved 15:12 */

	/* CRU_GATE_CON35 */
	/* reserved 4:0 */
	/* clk_gmac_125m_cru_en 5 */
	/* clk_gmac_50m_cru_en 6 */
	/* aclk_usb3otg2_en 7 */
	/* suspend_clk_usb3otg2_en 8 */
	/* ref_clk_usb3otg2_en 9 */
	/* clk_utmi_otg2_en 10 */
	/* reserved 15:11 */

	/* CRU_GATE_CON36 */

	/* CRU_GATE_CON37 */
	/* clk_pipephy0_ref_en 0 */
	/* clk_pipephy1_ref_en 1 */
	/* clk_pipephy2_ref_en 2 */
	/* reserved 3 */
	/* clk_pmalive0_en 4 */
	/* clk_pmalive1_en 5 */
	/* clk_pmalive2_en 6 */
	/* aclk_sata0_en 7 */
	/* aclk_sata1_en 8 */
	/* aclk_sata2_en 9 */
	/* clk_rxoob0_en 10 */
	/* clk_rxoob1_en 11 */
	/* clk_rxoob2_en 12 */
	/* reserved 15:13 */

	/* CRU_GATE_CON38 */
	/* reserved 2:0 */
	/* clk_pipephy0_pipe_g_en 3 */
	/* clk_pipephy1_pipe_g_en 4 */
	/* clk_pipephy2_pipe_g_en 5 */
	/* clk_pipephy0_pipe_asic_g_en 6 */
	/* clk_pipephy1_pipe_asic_g_en 7 */
	/* clk_pipephy2_pipe_asic_g_en 8 */
	/* clk_pipephy2_pipe_u3_g_en 9 */
	/* reserved 12:10 */
	/* clk_pcie_1l2_pipe_en 13 */
	/* clk_pcie_1l0_pipe_en 14 */
	/* clk_pcie_1l1_pipe_en 15 */

	/* CRU_GATE_CON39 */
	/* clk_pcie_4l_pipe_en 0 */
	/* clk_pcie_2l_pipe_en 1 */
	/* reserved 15:2 */

	/* CRU_GATE_CON40 */
	/* hclk_rkvdec0_root_en 0 */
	/* aclk_rkvdec0_root_en 1 */
	/* aclk_rkvdec_ccu_en 2 */
	/* hclk_rkvdec0_en 3 */
	/* aclk_rkvdec0_en 4 */
	/* hclk_rkvdec0_biu_en 5 */
	/* aclk_rkvdec0_biu_en 6 */
	/* clk_rkvdec0_ca_en 7 */
	/* clk_rkvdec0_hevc_ca_en 8 */
	/* clk_rkvdec0_core_en 9 */
	/* reserved 15:10 */

	/* CRU_GATE_CON41 */
	/* hclk_rkvdec1_root_en 0 */
	/* aclk_rkvdec1_root_en 1 */
	/* hclk_rkvdec1_en 2 */
	/* aclk_rkvdec1_en 3 */
	/* hclk_rkvdec1_biu_en 4 */
	/* aclk_rkvdec1_biu_en 5 */
	/* clk_rkvdec1_ca_en 6 */
	/* clk_rkvdec1_hevc_ca_en 7 */
	/* clk_rkvdec1_core_en 8 */
	/* reserved 15:9 */

	/* CRU_GATE_CON42 */
	GATE(ACLK_USB_ROOT, "aclk_usb_root", "aclk_usb_root_c", 42, 0),
	GATE(HCLK_USB_ROOT, "hclk_usb_root", "hclk_usb_root_m", 42, 1),
	/* aclk_usb_biu_en 2 */
	/* hclk_usb_biu_en 3 */
	/* aclk_usb3otg0_en 4 */
	/* suspend_clk_usb3otg0_en 5 */
	/* ref_clk_usb3otg0_en 6 */
	/* aclk_usb3otg1_en 7 */
	/* suspend_clk_usb3otg1_en 8 */
	/* ref_clk_usb3otg1_en 9 */
	/* hclk_host0_en 10 */
	/* hclk_host_arb0_en 11 */
	/* hclk_host1_en 12 */
	/* hclk_host_arb1_en 13 */
	/* aclk_usb_grf_en 14 */
	/* utmi_ohci_clk48_host0_en 15 */

	/* CRU_GATE_CON43 */
	/* utmi_ohci_clk48_host1_en 0 */
	/* reserved 15:1 */

	/* CRU_GATE_CON44 */
	/* aclk_vdpu_root_en 0 */
	/* aclk_vdpu_low_root_en 1 */
	/* hclk_vdpu_root_en 2 */
	/* aclk_jpeg_decoder_root_en 3 */
	/* aclk_vdpu_biu_en 4 */
	/* aclk_vdpu_low_biu_en 5 */
	/* hclk_vdpu_biu_en 6 */
	/* aclk_jpeg_decoder_biu_en 7 */
	/* aclk_vpu_en 8 */
	/* hclk_vpu_en 9 */
	/* aclk_jpeg_encoder0_en 10 */
	/* hclk_jpeg_encoder0_en 11 */
	/* aclk_jpeg_encoder1_en 12 */
	/* hclk_jpeg_encoder1_en 13 */
	/* aclk_jpeg_encoder2_en 14 */
	/* hclk_jpeg_encoder2_en 15 */

	/* CRU_GATE_CON45 */
	/* aclk_jpeg_encoder3_en 0 */
	/* hclk_jpeg_encoder3_en 1 */
	/* aclk_jpeg_decoder_en 2 */
	/* hclk_jpeg_decoder_en 3 */
	/* hclk_iep2p0_en 4 */
	/* aclk_iep2p0_en 5 */
	/* clk_iep2p0_core_en 6 */
	/* hclk_rga2_en 7 */
	/* aclk_rga2_en 8 */
	/* clk_rga2_core_en 9 */
	/* hclk_rga3_0_en 10 */
	/* aclk_rga3_0_en 11 */
	/* clk_rga3_0_core_en 12 */
	/* reserved 15:13 */

	/* CRU_GATE_CON46 */

	/* CRU_GATE_CON47 */
	/* hclk_rkvenc0_root_en 0 */
	/* aclk_rkvenc0_root_en 1 */
	/* hclk_rkvenc0_biu_en 2 */
	/* aclk_rkvenc0_biu_en 3 */
	/* hclk_rkvenc0_en 4 */
	/* aclk_rkvenc0_en 5 */
	/* clk_rkvenc0_core_en 6 */
	/* reserved 15:7 */

	/* CRU_GATE_CON48 */
	/* hclk_rkvenc1_root_en 0 */
	/* aclk_rkvenc1_root_en 1 */
	/* hclk_rkvenc1_biu_en 2 */
	/* aclk_rkvenc1_biu_en 3 */
	/* hclk_rkvenc1_en 4 */
	/* aclk_rkvenc1_en 5 */
	/* clk_rkvenc1_core_en 6 */
	/* reserved 15:7 */

	/* CRU_GATE_CON49 */
	/* aclk_vi_root_en 0 */
	/* hclk_vi_root_en 1 */
	/* pclk_vi_root_en 2 */
	/* aclk_vi_biu_en 3 */
	/* hclk_vi_biu_en 4 */
	/* pclk_vi_biu_en 5 */
	/* dclk_vicap_en 6 */
	/* aclk_vicap_en 7 */
	/* hclk_vicap_en 8 */
	/* clk_isp0_core_en 9 */
	/* clk_isp0_core_marvin_en 10 */
	/* clk_isp0_core_vicap_en 11 */
	/* aclk_isp0_en 12 */
	/* hclk_isp0_en 13 */
	/* aclk_fisheye0_en 14 */
	/* hclk_fisheye0_en 15 */

	/* CRU_GATE_CON50 */
	/* clk_fisheye0_core_en 0 */
	/* aclk_fisheye1_en 1 */
	/* hclk_fisheye1_en 2 */
	/* clk_fisheye1_core_en 3 */
	/* pclk_csi_host_0_en 4 */
	/* pclk_csi_host_1_en 5 */
	/* pclk_csi_host_2_en 6 */
	/* pclk_csi_host_3_en 7 */
	/* pclk_csi_host_4_en 8 */
	/* pclk_csi_host_5_en 9 */
	/* reserved 15:10 */

	/* CRU_GATE_CON51 */
	/* reserved 3:0 */
	/* clk_csihost0_vicap_en 4 */
	/* clk_csihost1_vicap_en 5 */
	/* clk_csihost2_vicap_en 6 */
	/* clk_csihost3_vicap_en 7 */
	/* clk_csihost4_vicap_en 8 */
	/* clk_csihost5_vicap_en 9 */
	/* iclk_csihost01_en 10 */
	/* iclk_csihost0_en 11 */
	/* iclk_csihost1_en 12 */
	/* reserved 15:13 */

	/* CRU_GATE_CON52 */
	/* aclk_vop_root_en 0 */
	/* aclk_vop_low_root_en 1 */
	/* hclk_vop_root_en 2 */
	/* pclk_vop_root_en 3 */
	/* aclk_vop_biu_en 4 */
	/* aclk_vop_low_biu_en 5 */
	/* hclk_vop_biu_en 6 */
	/* pclk_vop_biu_en 7 */
	/* hclk_vop_en 8 */
	/* aclk_vop_en 9 */
	/* dclk_vp0_src_en 10 */
	/* dclk_vp1_src_en 11 */
	/* dclk_vp2_src_en 12 */
	/* dclk_vp0_en 13 */
	/* reserved 15:14 */

	/* CRU_GATE_CON53 */
	/* dclk_vp1_en 0 */
	/* dclk_vp2_en 1 */
	/* dclk_vp3_en 2 */
	/* pclk_vopgrf_en 3 */
	/* pclk_dsihost0_en 4 */
	/* pclk_dsihost1_en 5 */
	/* clk_dsihost0_en 6 */
	/* clk_dsihost1_en 7 */
	/* clk_vop_pmu_en 8 */
	/* pclk_vop_channel_biu_en 9 */
	/* aclk_vop_doby_en 10 */
	/* reserved 15:11 */

	/* CRU_GATE_CON54 */

	/* CRU_GATE_CON55 */
	/* aclk_vo0_root_en 0 */
	/* hclk_vo0_root_en 1 */
	/* hclk_vo0_s_root_en 2 */
	/* pclk_vo0_root_en 3 */
	/* pclk_vo0_s_root_en 4 */
	/* hclk_vo0_biu_en 5 */
	/* hclk_vo0_s_biu_en 6 */
	/* pclk_vo0_biu_en 7 */
	/* pclk_vo0_s_biu_en 8 */
	/* aclk_hdcp0_biu_en 9 */
	/* pclk_vo0grf_en 10 */
	/* hclk_hdcp_key0_en 11 */
	/* aclk_hdcp0_en 12 */
	/* hclk_hdcp0_en 13 */
	/* pclk_hdcp0_en 14 */
	/* reserved 15 */

	/* CRU_GATE_CON56 */
	/* aclk_trng0_en 0 */
	/* pclk_trng0_en 1 */
	/* clk_aux16mhz_0_en 2 */
	/* clk_aux16mhz_1_en 3 */
	/* pclk_dp0_en 4 */
	/* pclk_dp1_en 5 */
	/* pclk_s_dp0_en 6 */
	/* pclk_s_dp1_en 7 */
	/* clk_dp0_en 8 */
	/* clk_dp1_en 9 */
	/* hclk_i2s4_8ch_en 10 */
	/* clk_i2s4_8ch_tx_en 11 */
	/* clk_i2s4_8ch_frac_tx_en 12 */
	/* mclk_i2s4_8ch_tx_en 13 */
	/* hclk_i2s8_8ch_en 14 */
	/* clk_i2s8_8ch_tx_en 15 */

	/* CRU_GATE_CON57 */
	/* clk_i2s8_8ch_frac_tx_en 0 */
	/* mclk_i2s8_8ch_tx_en 1 */
	/* hclk_spdif2_dp0_en 2 */
	/* clk_spdif2_dp0_en 3 */
	/* clk_spdif2_dp0_frac_en 4 */
	/* mclk_spdif2_dp0_en 5 */
	/* mclk_spdif2_en 6 */
	/* hclk_spdif5_dp1_en 7 */
	/* clk_spdif5_dp1_en 8 */
	/* clk_spdif5_dp1_frac_en 9 */
	/* mclk_spdif5_dp1_en 10 */
	/* mclk_spdif5_en 11 */
	/* reserved 15:12 */

	/* CRU_GATE_CON58 */

	/* CRU_GATE_CON59 */
	/* aclk_hdcp1_root_en 0 */
	/* aclk_hdmirx_root_en 1 */
	/* hclk_vo1_root_en 2 */
	/* hclk_vo1_s_root_en 3 */
	/* pclk_vo1_root_en 4 */
	/* pclk_vo1_s_root_en 5 */
	/* aclk_hdcp1_biu_en 6 */
	/* reserved 7 */
	/* aclk_vo1_biu_en 8 */
	/* hclk_vo1_biu_en 9 */
	/* hclk_vo1_s_biu_en 10 */
	/* pclk_vo1_biu_en 11 */
	/* pclk_vo1grf_en 12 */
	/* pclk_vo1_s_biu_en 13 */
	/* pclk_s_edp0_en 14 */
	/* pclk_s_edp1_en 15 */

	/* CRU_GATE_CON60 */
	/* hclk_i2s7_8ch_en 0 */
	/* clk_i2s7_8ch_rx_en 1 */
	/* clk_i2s7_8ch_frac_rx_en 2 */
	/* mclk_i2s7_8ch_rx_en 3 */
	/* hclk_hdcp_key1_en 4 */
	/* aclk_hdcp1_en 5 */
	/* hclk_hdcp1_en 6 */
	/* pclk_hdcp1_en 7 */
	/* reserved 8 */
	/* aclk_trng1_en 9 */
	/* pclk_trng1_en 10 */
	/* pclk_hdmitx0_en 11 */
	/* reserved 14:12 */
	/* clk_hdmitx0_earc_en 15 */

	/* CRU_GATE_CON61 */
	/* clk_hdmitx0_ref_en 0 */
	/* reserved 1 */
	/* pclk_hdmitx1_en 2 */
	/* reserved 5:3 */
	/* clk_hdmitx1_earc_en 6 */
	/* clk_hdmitx1_ref_en 7 */
	/* reserved 8 */
	/* aclk_hdmirx_en 9 */
	/* pclk_hdmirx_en 10 */
	/* clk_hdmirx_ref_en 11 */
	/* clk_hdmirx_aud_src_en 12 */
	/* clk_hdmirx_aud_frac_en 13 */
	/* clk_hdmirx_aud_en 14 */
	/* clk_hdmirx_tmdsqp_en 15 */

	/* CRU_GATE_CON62 */
	/* pclk_edp0_en 0 */
	/* clk_edp0_24m_en 1 */
	/* clk_edp0_200m_en 2 */
	/* pclk_edp1_en 3 */
	/* clk_edp1_24m_en 4 */
	/* clk_edp1_200m_en 5 */
	/* clk_i2s5_8ch_tx_en 6 */
	/* clk_i2s5_8ch_frac_tx_en 7 */
	/* mclk_i2s5_8ch_tx_en 8 */
	/* reserved 11:9 */
	/* hclk_i2s5_8ch_en 12 */
	/* clk_i2s6_8ch_tx_en 13 */
	/* clk_i2s6_8ch_frac_tx_en 14 */
	/* mclk_i2s6_8ch_tx_en 15 */

	/* CRU_GATE_CON63 */
	/* clk_i2s6_8ch_rx_en 0 */
	/* clk_i2s6_8ch_frac_rx_en 1 */
	/* mclk_i2s6_8ch_rx_en 2 */
	/* hclk_i2s6_8ch_en 3 */
	/* hclk_spdif3_en 4 */
	/* clk_spdif3_en 5 */
	/* clk_spdif3_frac_en 6 */
	/* mclk_spdif3_en 7 */
	/* hclk_spdif4_en 8 */
	/* clk_spdif4_en 9 */
	/* clk_spdif4_frac_en 10 */
	/* mclk_spdif4_en 11 */
	/* hclk_spdifrx0_en 12 */
	/* mclk_spdifrx0_en 13 */
	/* hclk_spdifrx1_en 14 */
	/* mclk_spdifrx1_en 15 */

	/* CRU_GATE_CON64 */
	/* hclk_spdifrx2_en 0 */
	/* mclk_spdifrx2_en 1 */
	/* reserved 13:2 */
	/* dclk_vp2hdmi_bridge0_vo1_en 14 */
	/* dclk_vp2hdmi_bridge1_vo1_en 15 */

	/* CRU_GATE_CON65 */
	/* hclk_i2s9_8ch_en 0 */
	/* clk_i2s9_8ch_rx_en 1 */
	/* clk_i2s9_8ch_frac_rx_en 2 */
	/* mclk_i2s9_8ch_rx_en 3 */
	/* hclk_i2s10_8ch_en 4 */
	/* clk_i2s10_8ch_rx_en 5 */
	/* clk_i2s10_8ch_frac_rx_en 6 */
	/* mclk_i2s10_8ch_rx_en 7 */
	/* pclk_s_hdmirx_en 8 */
	/* clk_hdmitrx_refsrc_en 9 */
	/* reserved 15:10 */

	/* CRU_GATE_CON66 */
	/* reserved 0 */
	/* clk_gpu_src_df_en 1 */
	/* clk_testout_gpu_en 2 */
	/* clk_gpu_src_en 3 */
	/* clk_gpu_en 4 */
	/* reserved 5 */
	/* clk_gpu_coregroup_en 6 */
	/* clk_gpu_stacks_en 7 */
	/* aclk_s_gpu_biu_en 8 */
	/* aclk_m0_gpu_biu_en 9 */
	/* aclk_m1_gpu_biu_en 10 */
	/* aclk_m2_gpu_biu_en 11 */
	/* aclk_m3_gpu_biu_en 12 */
	/* pclk_gpu_root_en 13 */
	/* pclk_gpu_biu_en 14 */
	/* pclk_pvtm2_en 15 */

	/* CRU_GATE_CON67 */
	/* clk_pvtm2_en 0 */
	/* clk_gpu_pvtm_en 1 */
	/* pclk_gpu_grf_en 2 */
	/* clk_gpu_pvtpll_en 3 */
	/* reserved 15:4 */

	/* CRU_GATE_CON68 */
	/* aclk_av1_root_en 0 */
	/* aclk_av1_biu_en 1 */
	/* aclk_av1_en 2 */
	/* pclk_av1_root_en 3 */
	/* pclk_av1_biu_en 4 */
	/* pclk_av1_en 5 */
	/* reserved 15:6 */

	/* CRU_GATE_CON69 */
	/* aclk_center_root_en 0 */
	/* aclk_center_low_root_en 1 */
	/* hclk_center_root_en 2 */
	/* pclk_center_root_en 3 */
	/* aclk_ddr_biu_en 4 */
	/* aclk_dma2ddr_en 5 */
	/* aclk_ddr_sharemem_en 6 */
	/* aclk_ddr_sharemem_biu_en 7 */
	/* aclk_center_s200_root_en 8 */
	/* aclk_center_s400_root_en 9 */
	/* aclk_center_s200_biu_en 10 */
	/* aclk_center_s400_biu_en 11 */
	/* hclk_ahb2apb_en 12 */
	/* hclk_center_biu_en 13 */
	/* fclk_ddr_cm0_core_en 14 */
	/* clk_ddr_timer_root_en 15 */

	/* CRU_GATE_CON70 */
	/* clk_ddr_timer0_en 0 */
	/* clk_ddr_timer1_en 1 */
	/* tclk_wdt_ddr_en 2 */
	/* reserved 3 */
	/* clk_ddr_cm0_rtc_en 4 */
	/* pclk_center_grf_en 5 */
	/* pclk_ahb2apb_en 6 */
	/* pclk_wdt_en 7 */
	/* pclk_timer_en 8 */
	/* pclk_dma2ddr_en 9 */
	/* pclk_sharemem_en 10 */
	/* pclk_center_biu_en 11 */
	/* pclk_center_channel_biu_en 12 */
	/* reserved 15:13 */

	/* CRU_GATE_CON71 */

	/* CRU_GATE_CON72 */
	/* reserved 0 */
	/* pclk_usbdpgrf0_en 1 */
	/* pclk_usbdpphy0_en 2 */
	/* pclk_usbdpgrf1_en 3 */
	/* pclk_usbdpphy1_en 4 */
	/* pclk_hdptx0_en 5 */
	/* pclk_hdptx1_en 6 */
	/* pclk_apb2asb_slv_bot_right_en 7 */
	/* pclk_usb2phy_u3_0_grf0_en 8 */
	/* pclk_usb2phy_u3_1_grf0_en 9 */
	/* pclk_usb2phy_u2_0_grf0_en 10 */
	/* pclk_usb2phy_u2_1_grf0_en 11 */
	/* reserved 15:12 */

	/* CRU_GATE_CON73 */
	/* reserved 11:0 */
	/* clk_hdmihdp0_en 12 */
	/* clk_hdmihdp1_en 13 */
	/* reserved 15:14 */

	/* CRU_GATE_CON74 */
	/* aclk_vo1usb_top_root_en 0 */
	/* aclk_vo1usb_top_biu_en 1 */
	/* hclk_vo1usb_top_root_en 2 */
	/* hclk_vo1usb_top_biu_en 3 */
	/* reserved 15:4 */

	/* CRU_GATE_CON75 */
	/* hclk_sdio_root_en 0 */
	/* hclk_sdio_biu_en 1 */
	/* hclk_sdio_en 2 */
	/* cclk_src_sdio_en 3 */
	/* reserved 15:4 */

	/* CRU_GATE_CON76 */
	/* aclk_rga3_root_en 0 */
	/* hclk_rga3_root_en 1 */
	/* hclk_rga3_biu_en 2 */
	/* aclk_rga3_biu_en 3 */
	/* hclk_rga3_1_en 4 */
	/* aclk_rga3_1_en 5 */
	/* clk_rga3_1_core_en 6 */
	/* reserved 15:7 */

	/* CRU_GATE_CON77 */
	/* clk_ref_pipe_phy0_osc_src_en 0 */
	/* clk_ref_pipe_phy1_osc_src_en 1 */
	/* clk_ref_pipe_phy2_osc_src_en 2 */
	/* clk_ref_pipe_phy0_pll_src_en 3 */
	/* clk_ref_pipe_phy1_pll_src_en 4 */
	/* clk_ref_pipe_phy2_pll_src_en 5 */
	/* reserved 15:6 */

	/* PMU1CRU_GATE_CON00 */
	GATERAW(CLK_PMU1_50M_SRC, "clk_pmu1_50m_src", "clk_pmu1_50m_src_c", RK3588_PMUCRU_CLKGATE_CON(0), 0),
	GATERAW(CLK_PMU1_100M_SRC, "clk_pmu1_100m_src", "clk_pmu1_100m_src_c", RK3588_PMUCRU_CLKGATE_CON(0), 1),
	GATERAW(CLK_PMU1_200M_SRC, "clk_pmu1_200m_src", "clk_pmu1_200m_src_c", RK3588_PMUCRU_CLKGATE_CON(0), 2),
	GATERAW(CLK_PMU1_300M_SRC, "clk_pmu1_300m_src", "clk_pmu1_300m_src_c", RK3588_PMUCRU_CLKGATE_CON(0), 3),
	GATERAW(CLK_PMU1_400M_SRC, "clk_pmu1_400m_src", "clk_pmu1_400m_src_c", RK3588_PMUCRU_CLKGATE_CON(0), 4),
	/* hclk_pmu1_root_i_en 5 */
	/* hclk_pmu1_root_en 6 */
	/* pclk_pmu1_root_i_en 7 */
	GATERAW(PCLK_PMU1_ROOT, "pclk_pmu1_root", "pclk_pmu1_root_m", RK3588_PMUCRU_CLKGATE_CON(0), 7),
	/* hclk_pmu_cm0_root_i_en 8 */
	/* hclk_pmu_cm0_root_en 9 */
	/* hclk_pmu1_biu_en 10 */
	/* pclk_pmu1_biu_en 11 */
	/* hclk_pmu_cm0_biu_en 12 */
	/* fclk_pmu_cm0_core_en 13 */
	/* reserved 14 */
	/* clk_pmu_cm0_rtc_en 15 */

	/* PMU1CRU_GATE_CON01 */
	/* pclk_pmu1_en 0 */
	/* clk_ddr_fail_safe_en 1 */
	/* pclk_pmu1_cru_en 2 */
	/* clk_pmu1_en 3 */
	/* pclk_pmu1_grf_en 4 */
	/* pclk_pmu1_ioc_en 5 */
	/* pclk_pmu1wdt_en 6 */
	/* tclk_pmu1wdt_en 7 */
	/* pclk_pmu1timer_en 8 */
	/* clk_pmu1timer_root_en 9 */
	/* clk_pmu1timer0_en 10 */
	/* clk_pmu1timer1_en 11 */
	/* pclk_pmu1pwm_en 12 */
	/* clk_pmu1pwm_en 13 */
	/* clk_pmu1pwm_capture_en 14 */
	/* reserved 15 */

	/* PMU1CRU_GATE_CON02 */
	/* reserved 0 */
	/* pclk_i2c0_en 1 */
	GATERAW(PCLK_I2C0, "pclk_i2c0", "pclk_pmu0_root", RK3588_PMUCRU_CLKGATE_CON(2), 1),
	/* clk_i2c0_en 2 */
	GATERAW(CLK_I2C0, "clk_i2c0", "clk_i2c0_m", RK3588_PMUCRU_CLKGATE_CON(2), 2),
	/* clk_uart0_en 3 */
	/* clk_uart0_frac_en 4 */
	/* sclk_uart0_en 5 */
	/* pclk_uart0_en 6 */
	/* hclk_i2s1_8ch_en 7 */
	/* clk_i2s1_8ch_tx_en 8 */
	/* clk_i2s1_8ch_frac_tx_en 9 */
	/* mclk_i2s1_8ch_tx_en 10 */
	/* clk_i2s1_8ch_rx_en 11 */
	/* clk_i2s1_8ch_frac_rx_en 12 */
	/* mclk_i2s1_8ch_rx_en 13 */
	/* hclk_pdm0_en 14 */
	/* mclk_pdm0_en 15 */

	/* PMU1CRU_GATE_CON03 */
	/* hclk_vad_en 0 */
	/* reserved 1:4 */
	/* clk_usbdp_combo_phy0_ref_xtal_en 5 */
	/* reserved 6:10 */
	/* clk_hdptx0_ref_xtal_en 11 */
	/* reserved 12:15 */

	/* PMU1CRU_GATE_CON04 */
	/* reserved 0:2 */
	/* clk_ref_mipi_dcphy0_en 3 */
	/* reserved 4:6 */
	/* clk_otgphy_u3_0_en 7 */
	/* reserved 8:10 */
	/* clk_cr_para_en 11 */
	/* reserved 12:15 */

	/* PMU1CRU_GATE_CON05 */
	GATERAW(PCLK_PMU0_ROOT, "pclk_pmu0_root", "pclk_pmu1_root", RK3588_PMUCRU_CLKGATE_CON(5), 0),
	GATERAW(CLK_PMU0, "clk_pmu0", "xin24m", RK3588_PMUCRU_CLKGATE_CON(5), 1),
	GATERAW(PCLK_PMU0, "pclk_pmu0", "pclk_pmu0_root", RK3588_PMUCRU_CLKGATE_CON(5), 2),
	/* pclk_pmu0grf_en 3 */
	GATERAW(PCLK_PMU0IOC, "pclk_pmu0ioc", "pclk_pmu0_root", RK3588_PMUCRU_CLKGATE_CON(5), 4),
	GATERAW(PCLK_GPIO0, "pclk_gpio0", "pclk_pmu0_root", RK3588_PMUCRU_CLKGATE_CON(5), 5),
	GATERAW(DBCLK_GPIO0, "dbclk_gpio0", "dbclk_gpio0_m", RK3588_PMUCRU_CLKGATE_CON(5), 6),
	/* reserved 7:15 */
};

static int
rk3588_cru_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "rockchip,rk3588-cru")) {
		device_set_desc(dev, "Rockchip RK3588 Clock & Reset Unit");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
rk3588_cru_attach(device_t dev)
{
	struct rk_cru_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->clks = rk3588_clks;
	sc->nclks = nitems(rk3588_clks);
	sc->gates = rk3588_gates;
	sc->ngates = nitems(rk3588_gates);
	sc->reset_offset = 0xA04;
	sc->reset_num = 1236;

	return (rk_cru_attach(dev));
}

static device_method_t methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk3588_cru_probe),
	DEVMETHOD(device_attach,	rk3588_cru_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(rk3588_cru, rk3588_cru_driver, methods,
    sizeof(struct rk_cru_softc), rk_cru_driver);

EARLY_DRIVER_MODULE(rk3588_cru, simplebus, rk3588_cru_driver,
    0, 0, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
