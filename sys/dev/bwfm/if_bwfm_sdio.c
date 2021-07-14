/* $OpenBSD: if_bwfm_sdio.c,v 1.39 2021/02/26 00:07:41 patrick Exp $ */
/*
 * Copyright (c) 2010-2016 Broadcom Corporation
 * Copyright (c) 2016,2017 Patrick Wildt <patrick@blueri.se>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/module.h>
#include <sys/endian.h>
#include <sys/firmware.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>

#include <dev/sdio/sdiob.h>
#include <dev/sdio/sdio_subr.h>

#include <dev/bwfm/bwfmvar.h>
#include <dev/bwfm/bwfmreg.h>
#include <dev/bwfm/if_bwfm_sdio.h>

#include "bwfm_if.h"
#include "sdio_if.h"

#define BWFM_SDIO_CCCR_BRCM_CARDCAP			0xf0
#define  BWFM_SDIO_CCCR_BRCM_CARDCAP_CMD14_SUPPORT	0x02
#define  BWFM_SDIO_CCCR_BRCM_CARDCAP_CMD14_EXT		0x04
#define  BWFM_SDIO_CCCR_BRCM_CARDCAP_CMD_NODEC		0x08
#define BWFM_SDIO_CCCR_BRCM_CARDCTRL			0xf1
#define  BWFM_SDIO_CCCR_BRCM_CARDCTRL_WLANRESET		0x02
#define BWFM_SDIO_CCCR_BRCM_SEPINT			0xf2

/* #define BWFM_DEBUG */
#ifdef BWFM_DEBUG
#define DPRINTF(x)	do { if (bwfm_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (bwfm_debug >= (n)) printf x; } while (0)
static int bwfm_debug = 1;
#else
#define DPRINTF(x)	do { ; } while (0)
#define DPRINTFN(n, x)	do { ; } while (0)
#endif

#undef DEVNAME
#define DEVNAME(sc)	((sc)->sc_sc.sc_dev.dv_xname)

enum bwfm_sdio_clkstate {
	CLK_NONE,
	CLK_SDONLY,
	CLK_PENDING,
	CLK_AVAIL,
};

struct bwfm_sdio_softc {
	device_t		  sc_dev;
	struct bwfm_softc	  sc_sc;
	struct sdio_func	*sc_sf[3];
#if 0
	struct rwlock		 *sc_lock;
#endif
	void			 *sc_ih;
	int			  sc_node;
	int			  sc_oob;

	int			  sc_initialized;

	uint32_t		  sc_bar0;
	int			  sc_clkstate;
	int			  sc_alp_only;
	int			  sc_sr_enabled;
	uint32_t		  sc_console_addr;

	char			 *sc_bounce_buf;
	size_t			  sc_bounce_size;

	char			 *sc_console_buf;
	size_t			  sc_console_buf_size;
	uint32_t		  sc_console_readidx;

	struct bwfm_core	 *sc_cc;

	uint8_t			  sc_tx_seq;
	uint8_t			  sc_tx_max_seq;
#if 0
	struct mbuf_list	  sc_tx_queue;
#endif
	int			  sc_tx_count;

#if 0
	struct task		  sc_task;
#endif
};

int		 bwfm_sdio_preinit(device_t, struct bwfm_softc *);
#ifdef NOTYET
int		 bwfm_sdio_detach(struct device *, int);

int		 bwfm_sdio_intr(void *);
int		 bwfm_sdio_oob_intr(void *);
void		 bwfm_sdio_task(void *);
#endif
int		 bwfm_sdio_load_microcode(struct bwfm_sdio_softc *);

void		 bwfm_sdio_clkctl(struct bwfm_sdio_softc *,
		    enum bwfm_sdio_clkstate, int);
void		 bwfm_sdio_htclk(struct bwfm_sdio_softc *, int, int);
void		 bwfm_sdio_readshared(struct bwfm_sdio_softc *);

void		 bwfm_sdio_backplane(struct bwfm_sdio_softc *, uint32_t);
uint8_t		 bwfm_sdio_read_1(struct bwfm_sdio_softc *, uint32_t);
uint32_t	 bwfm_sdio_read_4(struct bwfm_sdio_softc *, uint32_t);
void		 bwfm_sdio_write_1(struct bwfm_sdio_softc *, uint32_t,
		    uint8_t);
void		 bwfm_sdio_write_4(struct bwfm_sdio_softc *, uint32_t,
		    uint32_t);

int		 bwfm_sdio_buf_read(struct bwfm_sdio_softc *,
		    struct sdio_func *, uint32_t, char *, size_t);
int		 bwfm_sdio_buf_write(struct bwfm_sdio_softc *,
		    struct sdio_func *, uint32_t, char *, size_t);
uint32_t	 bwfm_sdio_ram_read_write(struct bwfm_sdio_softc *,
		    uint32_t, char *, size_t, int);
uint32_t	 bwfm_sdio_frame_read_write(struct bwfm_sdio_softc *,
		    char *, size_t, int);

uint32_t	 bwfm_sdio_dev_read(struct bwfm_sdio_softc *, uint32_t);
void		 bwfm_sdio_dev_write(struct bwfm_sdio_softc *, uint32_t,
		    uint32_t);

uint32_t	 bwfm_sdio_buscore_read(device_t, struct bwfm_softc *, uint32_t);
void		 bwfm_sdio_buscore_write(device_t, struct bwfm_softc *, uint32_t,
		    uint32_t);
int		 bwfm_sdio_buscore_prepare(device_t, struct bwfm_softc *);
void		 bwfm_sdio_buscore_activate(device_t, struct bwfm_softc *, uint32_t);

#ifdef NOTYET
struct mbuf *	 bwfm_sdio_newbuf(void);
int		 bwfm_sdio_tx_ok(struct bwfm_sdio_softc *);
void		 bwfm_sdio_tx_frames(struct bwfm_sdio_softc *);
void		 bwfm_sdio_tx_ctrlframe(struct bwfm_sdio_softc *, struct mbuf *);
void		 bwfm_sdio_tx_dataframe(struct bwfm_sdio_softc *, struct mbuf *);
void		 bwfm_sdio_rx_frames(struct bwfm_sdio_softc *);
void		 bwfm_sdio_rx_glom(struct bwfm_sdio_softc *, uint16_t *, int,
		    uint16_t *, struct mbuf_list *);

int		 bwfm_sdio_txcheck(struct bwfm_softc *);
int		 bwfm_sdio_txdata(struct bwfm_softc *, struct mbuf *);
int		 bwfm_sdio_txctl(struct bwfm_softc *, void *);

#ifdef BWFM_DEBUG
void		 bwfm_sdio_debug_console(struct bwfm_sdio_softc *);
#endif

struct bwfm_bus_ops bwfm_sdio_bus_ops = {
	.bs_preinit = bwfm_sdio_preinit,
	.bs_stop = NULL,
	.bs_txcheck = bwfm_sdio_txcheck,
	.bs_txdata = bwfm_sdio_txdata,
	.bs_txctl = bwfm_sdio_txctl,
};

struct bwfm_buscore_ops bwfm_sdio_buscore_ops = {
	.bc_read = bwfm_sdio_buscore_read,
	.bc_write = bwfm_sdio_buscore_write,
	.bc_prepare = bwfm_sdio_buscore_prepare,
	.bc_reset = NULL,
	.bc_setup = NULL,
	.bc_activate = bwfm_sdio_buscore_activate,
};
#endif	/* #if NOTYET */

static int
bwfm_sdio_probe(device_t dev)
{

	if (sdio_get_vendor(dev) != 0x2d0)
		return (ENXIO);

	switch (sdio_get_device(dev)) {
	case 0x4359:
	case 0xa9a6:
		break;
	default:
		return (ENXIO);
	}

	if (sdio_get_nfunc(dev) <= 1)
		return (ENXIO);

	if (sdio_get_funcnum(dev) != 1)
		return (ENXIO);

	return (0);
}

static int
bwfm_sdio_attach(device_t dev)
{
	struct bwfm_sdio_softc *sc;
	struct bwfm_core *core;
	uint32_t reg;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_sc.sc_dev = dev;

	if (sdio_get_func(dev, 0, &sc->sc_sf[0]) != 0) {
		device_printf(dev, "Cannot get function 0\n");
		return (ENXIO);
	}
	sc->sc_sf[1] = sdio_get_function(dev);
	if (sdio_claim_func(dev, 2, &sc->sc_sf[2]) != 0) {
		device_printf(dev, "Cannot claim function 2\n");
		return (ENXIO);
	}

	sdio_set_block_size(sc->sc_sf[1], 64);
	sdio_set_block_size(sc->sc_sf[2], 512);

	if (sdio_enable_func(sc->sc_sf[1]) != 0) {
		device_printf(dev, "cannot enable function 1\n");
		goto err;
	}

	if (bootverbose)
		device_printf(dev, "F1 signature read @0x18000000=%x\n",
		  bwfm_sdio_read_4(sc, 0x18000000));

	sc->sc_bounce_size = 64 * 1024;
	sc->sc_bounce_buf = malloc(sizeof(char) * sc->sc_bounce_size, M_DEVBUF, M_WAITOK | M_ZERO);

	/* Force PLL off */
	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR,
	    BWFM_SDIO_FUNC1_CHIPCLKCSR_FORCE_HW_CLKREQ_OFF |
	    BWFM_SDIO_FUNC1_CHIPCLKCSR_ALP_AVAIL_REQ);

	if (bwfm_chip_attach(dev, &sc->sc_sc) != 0) {
		device_printf(dev, "cannot attach chip\n");
		goto err;
	}

	sc->sc_cc = bwfm_chip_get_core(&sc->sc_sc, BWFM_AGENT_CORE_CHIPCOMMON);
	if (sc->sc_cc == NULL) {
		device_printf(dev, "cannot find chipcommon core\n");
		goto err;
	}

	device_printf(dev, "Calling bwfm_chip_get_core\n");
	core = bwfm_chip_get_core(&sc->sc_sc, BWFM_AGENT_CORE_SDIO_DEV);
	if (core->co_rev >= 12) {
		reg = bwfm_sdio_read_1(sc, BWFM_SDIO_FUNC1_SLEEPCSR);
		if (!(reg & BWFM_SDIO_FUNC1_SLEEPCSR_KSO)) {
			reg |= BWFM_SDIO_FUNC1_SLEEPCSR_KSO;
			bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_SLEEPCSR, reg);
		}
	}

	bwfm_sdio_write_1(sc, BWFM_SDIO_CCCR_BRCM_CARDCTRL,
	    bwfm_sdio_read_1(sc, BWFM_SDIO_CCCR_BRCM_CARDCTRL) |
	    BWFM_SDIO_CCCR_BRCM_CARDCTRL_WLANRESET);

	device_printf(dev, "Calling bwfm_chip_get_pmu\n");
	core = bwfm_chip_get_pmu(&sc->sc_sc);
	device_printf(dev, "Called bwfm_chip_get_pmu\n");
	bwfm_sdio_write_4(sc, core->co_base + BWFM_CHIP_REG_PMUCONTROL,
	    bwfm_sdio_read_4(sc, core->co_base + BWFM_CHIP_REG_PMUCONTROL) |
	    (BWFM_CHIP_REG_PMUCONTROL_RES_RELOAD <<
	     BWFM_CHIP_REG_PMUCONTROL_RES_SHIFT));
	device_printf(dev, "pmu stuff done\n");

	device_printf(dev, "Disabling function 2\n");
	sdio_disable_func(sc->sc_sf[2]);

	device_printf(dev, "Setting BWFM_SDIO_FUNC1_CHIPCLKCSR to 0\n");
	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR, 0);
	sc->sc_clkstate = CLK_SDONLY;

#ifdef NOTYET
	bwfm_attach(&sc->sc_sc);
#endif
	config_intrhook_oneshot(bwfm_attachhook, &sc->sc_sc);

	device_printf(dev, "Attach ok\n");
	return (0);
err:
	free(sc->sc_sf, M_DEVBUF);
	return (ENXIO);
}

static device_method_t bwfm_sdio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			bwfm_sdio_probe),
	DEVMETHOD(device_attach,		bwfm_sdio_attach),

	/* bwfm buscore interface */
	DEVMETHOD(bwfm_buscore_read,		bwfm_sdio_buscore_read),
	DEVMETHOD(bwfm_buscore_write,		bwfm_sdio_buscore_write),
	DEVMETHOD(bwfm_buscore_prepare,		bwfm_sdio_buscore_prepare),
	DEVMETHOD(bwfm_buscore_activate,	bwfm_sdio_buscore_activate),

	/* bwfm bus interface */
	DEVMETHOD(bwfm_bus_preinit,		bwfm_sdio_preinit),
	DEVMETHOD_END
};

static driver_t bwfm_sdio_driver = {
	"sdiob",
	bwfm_sdio_methods,
	sizeof(struct bwfm_sdio_softc),
};

static devclass_t bwfm_sdio_devclass;

DRIVER_MODULE(bwfm_sdio, sdiob, bwfm_sdio_driver, bwfm_sdio_devclass, 0, 0);

static void
sdio_test(struct bwfm_sdio_softc *sc)
{
	struct sdio_func *sf;
	uint32_t sbaddr, sdaddr, off;
	uint8_t buf[16384];
	/* uint8_t data; */
	int err;

	device_printf(sc->sc_dev, "Testing sdio interface\n\n\n");
	sf = sc->sc_sf[1];

	bwfm_sdio_clkctl(sc, CLK_AVAIL, 0);

	for (int i = 0; i < 16384; i++)
		buf[i] = 0xAA;

	sbaddr = 0x160000;
	off = 0x0;

	device_printf(sc->sc_dev, "rambase=%x\n", sc->sc_sc.sc_chip.ch_rambase);
	sbaddr = sc->sc_sc.sc_chip.ch_rambase + off;
	bwfm_sdio_backplane(sc, sbaddr);
	sdaddr = sbaddr & BWFM_SDIO_SB_OFT_ADDR_MASK;
	sdaddr |= BWFM_SDIO_SB_ACCESS_2_4B_FLAG;

	err = SDIO_WRITE_EXTENDED_BLOCK(device_get_parent(sf->dev), sf->fn, sdaddr, 16384, buf, true);
	if (err != 0) {
		device_printf(sc->sc_dev, "err: %d\n", err);
		return;
	}
	sbaddr = sc->sc_sc.sc_chip.ch_rambase;
	off = 0x0;

	for (int i = 0; i < 16384; i++)
		buf[i] = 0x55;

	sbaddr = sc->sc_sc.sc_chip.ch_rambase + off;
	bwfm_sdio_backplane(sc, sbaddr);
	sdaddr = sbaddr & BWFM_SDIO_SB_OFT_ADDR_MASK;
	sdaddr |= BWFM_SDIO_SB_ACCESS_2_4B_FLAG;

	err = SDIO_READ_EXTENDED_BLOCK(device_get_parent(sf->dev), sf->fn, sdaddr, 16384, buf, true);
	if (err != 0) {
		device_printf(sc->sc_dev, "err: %d\n", err);
		return;
	}

	for (int i = 0; i < 10; i++)
		device_printf(sc->sc_dev, "buf[%d] = %x\n", i, buf[i]);
	for (int i = 16374; i < 16384; i++)
		device_printf(sc->sc_dev, "buf[%d] = %x\n", i, buf[i]);

	/* while (off != 4) { */
	/* 	sbaddr = 0x160000 + off; */
	/* 	bwfm_sdio_backplane(sc, sbaddr); */
	/* 	sdaddr = sbaddr & BWFM_SDIO_SB_OFT_ADDR_MASK; */
	/* 	sdaddr |= BWFM_SDIO_SB_ACCESS_2_4B_FLAG; */

	/* 	data = 0x00; */
	/* 	SDIO_READ_DIRECT(device_get_parent(sf->dev), sf->fn, sdaddr, &data); */
	/* 	device_printf(sc->sc_dev, "Read %x from %x\n", data, sdaddr); */
	/* 	off++; */
	/* } */

	/* off = 508; */

	/* while (off != 512) { */
	/* 	sbaddr = 0x160000 + off; */
	/* 	bwfm_sdio_backplane(sc, sbaddr); */
	/* 	sdaddr = sbaddr & BWFM_SDIO_SB_OFT_ADDR_MASK; */
	/* 	sdaddr |= BWFM_SDIO_SB_ACCESS_2_4B_FLAG; */

	/* 	data = 0x00; */
	/* 	SDIO_READ_DIRECT(device_get_parent(sf->dev), sf->fn, sdaddr, &data); */
	/* 	device_printf(sc->sc_dev, "Read %x from %d\n", data, sdaddr); */
	/* 	off++; */
	/* } */
}


int
bwfm_sdio_preinit(device_t dev, struct bwfm_softc *bwfm)
{
	struct bwfm_sdio_softc *sc;
	const char *chip = NULL;
#ifdef NOTYET
	uint32_t clk, reg;
#endif

	device_printf(dev, "%s: called\n", __func__);
	sc = device_get_softc(dev);

	if (sc->sc_initialized)
		return 0;

#ifdef NOTYET
	rw_enter_write(sc->sc_lock);
#endif

	switch (bwfm->sc_chip.ch_chip)
	{
	case BRCM_CC_4330_CHIP_ID:
		chip = "4330";
		break;
	case BRCM_CC_4334_CHIP_ID:
		chip = "4334";
		break;
	case BRCM_CC_4345_CHIP_ID:
		if (bwfm->sc_chip.ch_chiprev == 9)
			chip = "43456";
		else
			chip = "43455";
		break;
	case BRCM_CC_43340_CHIP_ID:
	case BRCM_CC_43341_CHIP_ID:
		chip = "43340";
		break;
	case BRCM_CC_4335_CHIP_ID:
		if (bwfm->sc_chip.ch_chiprev < 2)
			chip = "4335";
		else
			chip = "4339";
		break;
	case BRCM_CC_4339_CHIP_ID:
		chip = "4339";
		break;
	case BRCM_CC_43430_CHIP_ID:
		if (bwfm->sc_chip.ch_chiprev == 0)
			chip = "43430a0";
		else
			chip = "43430";
		break;
	case BRCM_CC_4356_CHIP_ID:
		chip = "4356";
		break;
	case BRCM_CC_4359_CHIP_ID:
		/* chip = "4359"; */
		chip = "4329";
		break;
	default:
		device_printf(sc->sc_dev, "unknown firmware for chip %s\n",
		    bwfm->sc_chip.ch_name);
		goto err;
	}

	sc->sc_alp_only = 1;
	sdio_test(sc);
	/* device_printf(sc->sc_dev, "Will load firmware %s-sdio\n", chip); */
	/* if (bwfm_loadfirmware(bwfm, chip, "-sdio") != 0) */
	/* 	goto err; */

	/* sc->sc_alp_only = 1; */
	/* if (bwfm_sdio_load_microcode(sc) != 0) { */
	/* 	device_printf(sc->sc_dev, "could not load microcode\n"); */
	/* 	goto err; */
	/* } */
	sc->sc_alp_only = 0;

#ifdef NOTYET
	bwfm_sdio_clkctl(sc, CLK_AVAIL, 0);
	if (sc->sc_clkstate != CLK_AVAIL)
		goto err;

	clk = bwfm_sdio_read_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR);
	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR,
	    clk | BWFM_SDIO_FUNC1_CHIPCLKCSR_FORCE_HT);

	bwfm_sdio_dev_write(sc, SDPCMD_TOSBMAILBOXDATA,
	    SDPCM_PROT_VERSION << SDPCM_PROT_VERSION_SHIFT);
	if (sdmmc_io_function_enable(sc->sc_sf[2]) != 0) {
		printf("%s: cannot enable function 2\n", DEVNAME(sc));
		goto err;
	}

	bwfm_sdio_dev_write(sc, SDPCMD_HOSTINTMASK,
	    SDPCMD_INTSTATUS_HMB_SW_MASK|SDPCMD_INTSTATUS_CHIPACTIVE);
	bwfm_sdio_write_1(sc, BWFM_SDIO_WATERMARK, 8);

	if (bwfm_chip_sr_capable(bwfm)) {
		reg = bwfm_sdio_read_1(sc, BWFM_SDIO_FUNC1_WAKEUPCTRL);
		reg |= BWFM_SDIO_FUNC1_WAKEUPCTRL_HTWAIT;
		bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_WAKEUPCTRL, reg);
		bwfm_sdio_write_1(sc, BWFM_SDIO_CCCR_CARDCAP,
		    BWFM_SDIO_CCCR_CARDCAP_CMD14_SUPPORT |
		    BWFM_SDIO_CCCR_CARDCAP_CMD14_EXT);
		bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR,
		    BWFM_SDIO_FUNC1_CHIPCLKCSR_FORCE_HT);
		sc->sc_sr_enabled = 1;
	} else {
		bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR, clk);
	}

#if defined(__HAVE_FDT)
	if (sc->sc_node) {
		sc->sc_ih = fdt_intr_establish(sc->sc_node,
		    IPL_NET, bwfm_sdio_oob_intr, sc, DEVNAME(sc));
		if (sc->sc_ih != NULL) {
			bwfm_sdio_write_1(sc, BWFM_SDIO_CCCR_SEPINT,
			    BWFM_SDIO_CCCR_SEPINT_MASK |
			    BWFM_SDIO_CCCR_SEPINT_OE |
			    BWFM_SDIO_CCCR_SEPINT_ACT_HI);
			sc->sc_oob = 1;
		}
	}
	if (sc->sc_ih == NULL)
#endif
	sc->sc_ih = sdmmc_intr_establish(bwfm->sc_dev.dv_parent,
	    bwfm_sdio_intr, sc, DEVNAME(sc));
	if (sc->sc_ih == NULL) {
		printf("%s: can't establish interrupt\n", DEVNAME(sc));
		bwfm_sdio_clkctl(sc, CLK_NONE, 0);
		goto err;
	}
	sdmmc_intr_enable(sc->sc_sf[1]);
	rw_exit(sc->sc_lock);
#endif /* NOTYET */

	sc->sc_initialized = 1;
	return 0;

err:
#ifdef NOTYET
rw_exit(sc->sc_lock);
#endif
	return 1;
}

int
bwfm_sdio_load_microcode(struct bwfm_sdio_softc *sc)
{
	struct bwfm_softc *bwfm = &sc->sc_sc;
	char *verify = NULL;
	int err = 0;

	bwfm_sdio_clkctl(sc, CLK_AVAIL, 0);

	/* Upload firmware */
	err = bwfm_sdio_ram_read_write(sc, bwfm->sc_chip.ch_rambase,
	  (char *)bwfm->fw_bin->data, bwfm->fw_bin->datasize, 1);
	if (err)
		goto out;
	return -1;

	/* Verify firmware */
	verify = malloc(bwfm->fw_bin->datasize, M_TEMP, M_WAITOK | M_ZERO);
	err = bwfm_sdio_ram_read_write(sc, bwfm->sc_chip.ch_rambase,
	    verify, bwfm->fw_bin->datasize, 0);
	if (err || memcmp(verify, bwfm->fw_bin->data, bwfm->fw_bin->datasize)) {
		device_printf(sc->sc_dev, "firmware verification failed\n");
		free(verify, M_TEMP);
		goto out;
	}
	free(verify, M_TEMP);

#ifdef NOTYET
	/* Upload nvram */
	err = bwfm_sdio_ram_read_write(sc, bwfm->sc_chip.ch_rambase +
	    bwfm->sc_chip.ch_ramsize - nvlen, nvram, nvlen, 1);
	if (err)
		goto out;

	/* Verify nvram */
	verify = malloc(nvlen, M_TEMP, M_WAITOK | M_ZERO);
	err = bwfm_sdio_ram_read_write(sc, bwfm->sc_chip.ch_rambase +
	    bwfm->sc_chip.ch_ramsize - nvlen, verify, nvlen, 0);
	if (err || memcmp(verify, nvram, nvlen)) {
		printf("%s: nvram verification failed\n",
		    DEVNAME(sc));
		free(verify, M_TEMP, nvlen);
		goto out;
	}
	free(verify, M_TEMP, nvlen);
#endif

	/* Load reset vector from firmware and kickstart core. */
	bwfm_chip_set_active(bwfm, *(uint32_t *)bwfm->fw_bin->data);

out:
	bwfm_sdio_clkctl(sc, CLK_SDONLY, 0);
	return err;
}

void
bwfm_sdio_clkctl(struct bwfm_sdio_softc *sc, enum bwfm_sdio_clkstate newstate,
    int pendok)
{
	enum bwfm_sdio_clkstate oldstate;

	oldstate = sc->sc_clkstate;
	if (sc->sc_clkstate == newstate)
		return;

	switch (newstate) {
	case CLK_AVAIL:
		if (sc->sc_clkstate == CLK_NONE)
			sc->sc_clkstate = CLK_SDONLY;
		bwfm_sdio_htclk(sc, 1, pendok);
		break;
	case CLK_SDONLY:
		if (sc->sc_clkstate == CLK_NONE)
			sc->sc_clkstate = CLK_SDONLY;
		else if (sc->sc_clkstate == CLK_AVAIL)
			bwfm_sdio_htclk(sc, 0, 0);
		else
			device_printf(sc->sc_dev, "request for %d -> %d\n",
			    sc->sc_clkstate, newstate);
		break;
	case CLK_NONE:
		if (sc->sc_clkstate == CLK_AVAIL)
			bwfm_sdio_htclk(sc, 0, 0);
		sc->sc_clkstate = CLK_NONE;
		break;
	default:
		break;
	}

	/* DPRINTF(("%s: %d -> %d = %d\n", DEVNAME(sc), oldstate, newstate, */
	/*     sc->sc_clkstate)); */
}

void
bwfm_sdio_htclk(struct bwfm_sdio_softc *sc, int on, int pendok)
{
	uint32_t clkctl, devctl, req;
	int i;

	clkctl = bwfm_sdio_read_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR);
	device_printf(sc->sc_dev, "BWFM_SDIO_FUNC1_CHIPCLKCSR: %x\n", clkctl);

	if (sc->sc_sr_enabled) {
		if (on)
			sc->sc_clkstate = CLK_AVAIL;
		else
			sc->sc_clkstate = CLK_SDONLY;
		return;
	}

	if (on) {
		if (sc->sc_alp_only)
			req = BWFM_SDIO_FUNC1_CHIPCLKCSR_ALP_AVAIL_REQ;
		else
			req = BWFM_SDIO_FUNC1_CHIPCLKCSR_HT_AVAIL_REQ;
		bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR, req);

		clkctl = bwfm_sdio_read_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR);
		if (!BWFM_SDIO_FUNC1_CHIPCLKCSR_CLKAV(clkctl, sc->sc_alp_only)
		    && pendok) {
			devctl = bwfm_sdio_read_1(sc, BWFM_SDIO_DEVICE_CTL);
			devctl |= BWFM_SDIO_DEVICE_CTL_CA_INT_ONLY;
			bwfm_sdio_write_1(sc, BWFM_SDIO_DEVICE_CTL, devctl);
			sc->sc_clkstate = CLK_PENDING;
			return;
		} else if (sc->sc_clkstate == CLK_PENDING) {
			devctl = bwfm_sdio_read_1(sc, BWFM_SDIO_DEVICE_CTL);
			devctl &= ~BWFM_SDIO_DEVICE_CTL_CA_INT_ONLY;
			bwfm_sdio_write_1(sc, BWFM_SDIO_DEVICE_CTL, devctl);
		}

		for (i = 0; i < 5000; i++) {
			if (BWFM_SDIO_FUNC1_CHIPCLKCSR_CLKAV(clkctl,
			    sc->sc_alp_only))
				break;
			clkctl = bwfm_sdio_read_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR);
			DELAY(1000);
		}
		if (!BWFM_SDIO_FUNC1_CHIPCLKCSR_CLKAV(clkctl, sc->sc_alp_only)) {
			device_printf(sc->sc_dev, "HT avail timeout\n");
			return;
		}

		sc->sc_clkstate = CLK_AVAIL;
	} else {
		if (sc->sc_clkstate == CLK_PENDING) {
			devctl = bwfm_sdio_read_1(sc, BWFM_SDIO_DEVICE_CTL);
			devctl &= ~BWFM_SDIO_DEVICE_CTL_CA_INT_ONLY;
			bwfm_sdio_write_1(sc, BWFM_SDIO_DEVICE_CTL, devctl);
		}
		sc->sc_clkstate = CLK_SDONLY;
		bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR, 0);
	}
}

void
bwfm_sdio_readshared(struct bwfm_sdio_softc *sc)
{
	struct bwfm_softc *bwfm = (void *)sc;
	struct bwfm_sdio_sdpcm sdpcm;
	uint32_t addr, shaddr;
	int err;

	shaddr = bwfm->sc_chip.ch_rambase + bwfm->sc_chip.ch_ramsize - 4;
	if (!bwfm->sc_chip.ch_rambase && bwfm_chip_sr_capable(bwfm))
		shaddr -= bwfm->sc_chip.ch_srsize;

	err = bwfm_sdio_ram_read_write(sc, shaddr, (char *)&addr,
	    sizeof(addr), 0);
	if (err)
		return;

	addr = htole32(addr);
	if (addr == 0 || ((~addr >> 16) & 0xffff) == (addr & 0xffff))
		return;

	err = bwfm_sdio_ram_read_write(sc, addr, (char *)&sdpcm,
	    sizeof(sdpcm), 0);
	if (err)
		return;

	sc->sc_console_addr = htole32(sdpcm.console_addr);
}

#ifdef NOTYET
int
bwfm_sdio_intr(void *v)
{
	bwfm_sdio_task(v);
	return 1;
}

#if defined(__HAVE_FDT)
int
bwfm_sdio_oob_intr(void *v)
{
	struct bwfm_sdio_softc *sc = (void *)v;
	if (!sc->sc_oob)
		return 0;
	fdt_intr_disable(sc->sc_ih);
	task_add(systq, &sc->sc_task);
	return 1;
}
#endif

void
bwfm_sdio_task(void *v)
{
	struct bwfm_sdio_softc *sc = (void *)v;
	uint32_t clkctl, devctl, intstat, hostint;

	rw_enter_write(sc->sc_lock);

	if (!sc->sc_sr_enabled && sc->sc_clkstate == CLK_PENDING) {
		clkctl = bwfm_sdio_read_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR);
		if (BWFM_SDIO_FUNC1_CHIPCLKCSR_HTAV(clkctl)) {
			devctl = bwfm_sdio_read_1(sc, BWFM_SDIO_DEVICE_CTL);
			devctl &= ~BWFM_SDIO_DEVICE_CTL_CA_INT_ONLY;
			bwfm_sdio_write_1(sc, BWFM_SDIO_DEVICE_CTL, devctl);
			sc->sc_clkstate = CLK_AVAIL;
		}
	}

	intstat = bwfm_sdio_dev_read(sc, BWFM_SDPCMD_INTSTATUS);
	intstat &= (SDPCMD_INTSTATUS_HMB_SW_MASK|SDPCMD_INTSTATUS_CHIPACTIVE);
	/* XXX fc state */
	if (intstat)
		bwfm_sdio_dev_write(sc, BWFM_SDPCMD_INTSTATUS, intstat);

	if (intstat & SDPCMD_INTSTATUS_HMB_HOST_INT) {
		hostint = bwfm_sdio_dev_read(sc, SDPCMD_TOHOSTMAILBOXDATA);
		bwfm_sdio_dev_write(sc, SDPCMD_TOSBMAILBOX,
		    SDPCMD_TOSBMAILBOX_INT_ACK);
		if (hostint & SDPCMD_TOHOSTMAILBOXDATA_NAKHANDLED)
			intstat |= SDPCMD_INTSTATUS_HMB_FRAME_IND;
		if (hostint & SDPCMD_TOHOSTMAILBOXDATA_DEVREADY ||
		    hostint & SDPCMD_TOHOSTMAILBOXDATA_FWREADY)
			bwfm_sdio_readshared(sc);
	}

	/* FIXME: Might stall if we don't when not set. */
	if (1 || intstat & SDPCMD_INTSTATUS_HMB_FRAME_IND) {
		bwfm_sdio_rx_frames(sc);
	}

	if (!ml_empty(&sc->sc_tx_queue)) {
		bwfm_sdio_tx_frames(sc);
	}

#ifdef BWFM_DEBUG
	bwfm_sdio_debug_console(sc);
#endif

	rw_exit(sc->sc_lock);

#if defined(__HAVE_FDT)
	if (sc->sc_oob)
		fdt_intr_enable(sc->sc_ih);
#endif
}

int
bwfm_sdio_detach(struct device *self, int flags)
{
	struct bwfm_sdio_softc *sc = (struct bwfm_sdio_softc *)self;

	bwfm_detach(&sc->sc_sc, flags);

	dma_free(sc->sc_bounce_buf, sc->sc_bounce_size);
	free(sc->sc_sf, M_DEVBUF);

	return 0;
}
#endif /* NOTYET */

void
bwfm_sdio_backplane(struct bwfm_sdio_softc *sc, uint32_t bar0)
{
	if (sc->sc_bar0 == bar0)
		return;

	device_printf(sc->sc_dev, "%s: Setting %x as the dest address\n", __func__, bar0);
	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_SBADDRLOW,
	    (bar0 >>  8) & 0x80);
	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_SBADDRMID,
	    (bar0 >> 16) & 0xff);
	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_SBADDRHIGH,
	    (bar0 >> 24) & 0xff);
	sc->sc_bar0 = bar0;
	device_printf(sc->sc_dev, "%s: Done\n", __func__);
}

uint8_t
bwfm_sdio_read_1(struct bwfm_sdio_softc *sc, uint32_t addr)
{
	struct sdio_func *sf;
	uint8_t rv;
	int err;

	/*
	 * figure out how to read the register based on address range
	 * 0x00 ~ 0x7FF: function 0 CCCR and FBR
	 * 0x10000 ~ 0x1FFFF: function 1 miscellaneous registers
	 * The rest: function 1 silicon backplane core registers
	 */
	if ((addr & ~0x7ff) == 0)
		sf = sc->sc_sf[0];
	else
		sf = sc->sc_sf[1];

	rv = sdio_read_1(sf, addr, &err);
	return rv;
}

uint32_t
bwfm_sdio_read_4(struct bwfm_sdio_softc *sc, uint32_t addr)
{
	struct sdio_func *sf;
	uint32_t bar0 = addr & ~BWFM_SDIO_SB_OFT_ADDR_MASK;
	uint32_t rv;
	int err;

	bwfm_sdio_backplane(sc, bar0);

	addr &= BWFM_SDIO_SB_OFT_ADDR_MASK;
	addr |= BWFM_SDIO_SB_ACCESS_2_4B_FLAG;

	/*
	 * figure out how to read the register based on address range
	 * 0x00 ~ 0x7FF: function 0 CCCR and FBR
	 * 0x10000 ~ 0x1FFFF: function 1 miscellaneous registers
	 * The rest: function 1 silicon backplane core registers
	 */
	if ((addr & ~0x7ff) == 0)
		sf = sc->sc_sf[0];
	else
		sf = sc->sc_sf[1];

	rv = sdio_read_4(sf, addr, &err);
	return rv;
}

void
bwfm_sdio_write_1(struct bwfm_sdio_softc *sc, uint32_t addr, uint8_t data)
{
	struct sdio_func *sf;
	int err;

	/*
	 * figure out how to read the register based on address range
	 * 0x00 ~ 0x7FF: function 0 CCCR and FBR
	 * 0x10000 ~ 0x1FFFF: function 1 miscellaneous registers
	 * The rest: function 1 silicon backplane core registers
	 */
	if ((addr & ~0x7ff) == 0)
		sf = sc->sc_sf[0];
	else
		sf = sc->sc_sf[1];

	sdio_write_1(sf, addr, data, &err);
}

void
bwfm_sdio_write_4(struct bwfm_sdio_softc *sc, uint32_t addr, uint32_t data)
{
	struct sdio_func *sf;
	uint32_t bar0 = addr & ~BWFM_SDIO_SB_OFT_ADDR_MASK;
	int err;

	bwfm_sdio_backplane(sc, bar0);

	addr &= BWFM_SDIO_SB_OFT_ADDR_MASK;
	addr |= BWFM_SDIO_SB_ACCESS_2_4B_FLAG;

	/*
	 * figure out how to read the register based on address range
	 * 0x00 ~ 0x7FF: function 0 CCCR and FBR
	 * 0x10000 ~ 0x1FFFF: function 1 miscellaneous registers
	 * The rest: function 1 silicon backplane core registers
	 */
	if ((addr & ~0x7ff) == 0)
		sf = sc->sc_sf[0];
	else
		sf = sc->sc_sf[1];

	sdio_write_4(sf, addr, data, &err);
}

#include "sdio_if.h"

int
bwfm_sdio_buf_read(struct bwfm_sdio_softc *sc, struct sdio_func *sf,
    uint32_t reg, char *data, size_t size)
{
	int err;

	/* KASSERT(((vaddr_t)data & 0x3) == 0, "Bad address"); */
	KASSERT(((size & 0x3) == 0), ("Bad size"));

	if (sf == sc->sc_sf[1]) {
		err = SDIO_READ_EXTENDED_BLOCK(device_get_parent(sf->dev), sf->fn, reg, size, data, true);
	} else {
		err = SDIO_READ_EXTENDED_BYTE(device_get_parent(sf->dev), sf->fn, reg, size, data, false);
	}

	if (err)
		printf("%s: error %d\n", __func__, err);

	return err;
}

int
bwfm_sdio_buf_write(struct bwfm_sdio_softc *sc, struct sdio_func *sf,
    uint32_t reg, char *data, size_t size)
{
	int err;

	/* KASSERT(((vaddr_t)data & 0x3) == 0); */
	KASSERT(((size & 0x3) == 0), ("Bad size"));

	err = SDIO_WRITE_EXTENDED_BLOCK(device_get_parent(sf->dev), sf->fn, reg, size, data, true);

	if (err)
		printf("%s: error %d\n", __func__, err);

	return err;
}

uint32_t
bwfm_sdio_ram_read_write(struct bwfm_sdio_softc *sc, uint32_t reg,
    char *data, size_t left, int write)
{
	uint32_t sbaddr, sdaddr, off;
	size_t size;
	int err;

	device_printf(sc->sc_dev, "%s: reg=%x, totalsize=%zu\n", __func__, reg, left);
	err = off = 0;
	while (left > 0) {
		sbaddr = reg + off;
		bwfm_sdio_backplane(sc, sbaddr);

		sdaddr = sbaddr & BWFM_SDIO_SB_OFT_ADDR_MASK;
		size = min(left, (BWFM_SDIO_SB_OFT_ADDR_PAGE - sdaddr));
		sdaddr |= BWFM_SDIO_SB_ACCESS_2_4B_FLAG;

		device_printf(sc->sc_dev, "%s: size=%zu\n", __func__, size);
		if (write) {
			memcpy(sc->sc_bounce_buf, data + off, size);
			if (roundup(size, 4) != size)
				memset(sc->sc_bounce_buf + size, 0,
				    roundup(size, 4) - size);
			err = bwfm_sdio_buf_write(sc, sc->sc_sf[1], sdaddr,
			    sc->sc_bounce_buf, roundup(size, 4));
		} else {
			err = bwfm_sdio_buf_read(sc, sc->sc_sf[1], sdaddr,
			    sc->sc_bounce_buf, roundup(size, 4));
			memcpy(data + off, sc->sc_bounce_buf, size);
		}
		if (err)
			break;

		off += size;
		left -= size;
	}

	return err;
}

uint32_t
bwfm_sdio_frame_read_write(struct bwfm_sdio_softc *sc,
    char *data, size_t size, int write)
{
	uint32_t addr;
	int err;

	addr = sc->sc_cc->co_base;
	bwfm_sdio_backplane(sc, addr);

	addr &= BWFM_SDIO_SB_OFT_ADDR_MASK;
	addr |= BWFM_SDIO_SB_ACCESS_2_4B_FLAG;

	if (write)
		err = bwfm_sdio_buf_write(sc, sc->sc_sf[2], addr, data, size);
	else
		err = bwfm_sdio_buf_read(sc, sc->sc_sf[2], addr, data, size);

	return err;
}

uint32_t
bwfm_sdio_dev_read(struct bwfm_sdio_softc *sc, uint32_t reg)
{
	struct bwfm_core *core;
	core = bwfm_chip_get_core(&sc->sc_sc, BWFM_AGENT_CORE_SDIO_DEV);
	return bwfm_sdio_read_4(sc, core->co_base + reg);
}

void
bwfm_sdio_dev_write(struct bwfm_sdio_softc *sc, uint32_t reg, uint32_t val)
{
	struct bwfm_core *core;
	core = bwfm_chip_get_core(&sc->sc_sc, BWFM_AGENT_CORE_SDIO_DEV);
	bwfm_sdio_write_4(sc, core->co_base + reg, val);
}

uint32_t
bwfm_sdio_buscore_read(device_t dev, struct bwfm_softc *bwfm, uint32_t reg)
{
	struct bwfm_sdio_softc *sc;

	sc = device_get_softc(dev);
	return bwfm_sdio_read_4(sc, reg);
}

void
bwfm_sdio_buscore_write(device_t dev, struct bwfm_softc *bwfm, uint32_t reg, uint32_t val)
{
	struct bwfm_sdio_softc *sc;

	sc = device_get_softc(dev);
	bwfm_sdio_write_4(sc, reg, val);
}

int
bwfm_sdio_buscore_prepare(device_t dev, struct bwfm_softc *bwfm)
{
	struct bwfm_sdio_softc *sc;
	uint8_t clkval, clkset, clkmask;
	int i;

	sc = device_get_softc(dev);

	clkset = BWFM_SDIO_FUNC1_CHIPCLKCSR_ALP_AVAIL_REQ |
	    BWFM_SDIO_FUNC1_CHIPCLKCSR_FORCE_HW_CLKREQ_OFF;
	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR, clkset);

	clkmask = BWFM_SDIO_FUNC1_CHIPCLKCSR_ALP_AVAIL |
	    BWFM_SDIO_FUNC1_CHIPCLKCSR_HT_AVAIL;
	clkval = bwfm_sdio_read_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR);

	if ((clkval & ~clkmask) != clkset) {
		device_printf(sc->sc_dev, "wrote 0x%02x read 0x%02x\n",
		    clkset, clkval);
		/* return 1; */
	}

	for (i = 1000; i > 0; i--) {
		clkval = bwfm_sdio_read_1(sc,
		    BWFM_SDIO_FUNC1_CHIPCLKCSR);
		if (clkval & clkmask)
			break;
	}
	if (i == 0) {
		device_printf(sc->sc_dev, "timeout on ALPAV wait, clkval 0x%02x\n",
		    clkval);
		return 1;
	}

	clkset = BWFM_SDIO_FUNC1_CHIPCLKCSR_FORCE_HW_CLKREQ_OFF |
	    BWFM_SDIO_FUNC1_CHIPCLKCSR_FORCE_ALP;
	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR, clkset);
	DELAY(65);

	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_SDIOPULLUP, 0);

	return 0;
}

void
bwfm_sdio_buscore_activate(device_t dev, struct bwfm_softc *bwfm, uint32_t rstvec)
{
#ifdef NOTYET
	struct bwfm_sdio_softc *sc = (void *)bwfm;

	bwfm_sdio_dev_write(sc, BWFM_SDPCMD_INTSTATUS, 0xFFFFFFFF);

	if (rstvec)
		bwfm_sdio_ram_read_write(sc, 0, (char *)&rstvec,
		    sizeof(rstvec), 1);
#endif
}

#ifdef NOTYET
struct mbuf *
bwfm_sdio_newbuf(void)
{
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);

	MCLGET(m, M_DONTWAIT);
	if (!(m->m_flags & M_EXT)) {
		m_freem(m);
		return (NULL);
	}

	m->m_len = m->m_pkthdr.len = MCLBYTES;

	return (m);
}

int
bwfm_sdio_tx_ok(struct bwfm_sdio_softc *sc)
{
	return (uint8_t)(sc->sc_tx_max_seq - sc->sc_tx_seq) != 0 &&
	    ((uint8_t)(sc->sc_tx_max_seq - sc->sc_tx_seq) & 0x80) == 0;
}

void
bwfm_sdio_tx_frames(struct bwfm_sdio_softc *sc)
{
	struct ifnet *ifp = &sc->sc_sc.sc_ic.ic_if;
	struct mbuf *m;
	int i;

	if (!bwfm_sdio_tx_ok(sc))
		return;

	i = min((uint8_t)(sc->sc_tx_max_seq - sc->sc_tx_seq), 32);
	while (i--) {
		m = ml_dequeue(&sc->sc_tx_queue);
		if (m == NULL)
			break;

		if (m->m_type == MT_CONTROL)
			bwfm_sdio_tx_ctrlframe(sc, m);
		else
			bwfm_sdio_tx_dataframe(sc, m);

		m_freem(m);
	}

	if (sc->sc_tx_count < 64)
		ifq_restart(&ifp->if_snd);
}

void
bwfm_sdio_tx_ctrlframe(struct bwfm_sdio_softc *sc, struct mbuf *m)
{
	struct bwfm_sdio_hwhdr *hwhdr;
	struct bwfm_sdio_swhdr *swhdr;
	size_t len, roundto;

	len = sizeof(*hwhdr) + sizeof(*swhdr) + m->m_len;

	/* Zero-pad to either block-size or 4-byte alignment. */
	if (len > 512 && (len % 512) != 0)
		roundto = 512;
	else
		roundto = 4;

	KASSERT(roundup(len, roundto) <= sc->sc_bounce_size);

	hwhdr = (void *)sc->sc_bounce_buf;
	hwhdr->frmlen = htole16(len);
	hwhdr->cksum = htole16(~len);

	swhdr = (void *)&hwhdr[1];
	swhdr->seqnr = sc->sc_tx_seq++;
	swhdr->chanflag = BWFM_SDIO_SWHDR_CHANNEL_CONTROL;
	swhdr->nextlen = 0;
	swhdr->dataoff = sizeof(*hwhdr) + sizeof(*swhdr);
	swhdr->maxseqnr = 0;

	m_copydata(m, 0, m->m_len, (caddr_t)&swhdr[1]);

	if (roundup(len, roundto) != len)
		memset(sc->sc_bounce_buf + len, 0,
		    roundup(len, roundto) - len);

	bwfm_sdio_frame_read_write(sc, sc->sc_bounce_buf,
	    roundup(len, roundto), 1);
}

void
bwfm_sdio_tx_dataframe(struct bwfm_sdio_softc *sc, struct mbuf *m)
{
	struct bwfm_sdio_hwhdr *hwhdr;
	struct bwfm_sdio_swhdr *swhdr;
	struct bwfm_proto_bcdc_hdr *bcdc;
	size_t len, roundto;

	len = sizeof(*hwhdr) + sizeof(*swhdr) + sizeof(*bcdc)
	    + m->m_pkthdr.len;

	/* Zero-pad to either block-size or 4-byte alignment. */
	if (len > 512 && (len % 512) != 0)
		roundto = 512;
	else
		roundto = 4;

	KASSERT(roundup(len, roundto) <= sc->sc_bounce_size);

	hwhdr = (void *)sc->sc_bounce_buf;
	hwhdr->frmlen = htole16(len);
	hwhdr->cksum = htole16(~len);

	swhdr = (void *)&hwhdr[1];
	swhdr->seqnr = sc->sc_tx_seq++;
	swhdr->chanflag = BWFM_SDIO_SWHDR_CHANNEL_DATA;
	swhdr->nextlen = 0;
	swhdr->dataoff = sizeof(*hwhdr) + sizeof(*swhdr);
	swhdr->maxseqnr = 0;

	bcdc = (void *)&swhdr[1];
	bcdc->data_offset = 0;
	bcdc->priority = ieee80211_classify(&sc->sc_sc.sc_ic, m);
	bcdc->flags = BWFM_BCDC_FLAG_VER(BWFM_BCDC_FLAG_PROTO_VER);
	bcdc->flags2 = 0;

	m_copydata(m, 0, m->m_pkthdr.len, (caddr_t)&bcdc[1]);

	if (roundup(len, roundto) != len)
		memset(sc->sc_bounce_buf + len, 0,
		    roundup(len, roundto) - len);

	bwfm_sdio_frame_read_write(sc, sc->sc_bounce_buf,
	    roundup(len, roundto), 1);

	sc->sc_tx_count--;
}

void
bwfm_sdio_rx_frames(struct bwfm_sdio_softc *sc)
{
	struct ifnet *ifp = &sc->sc_sc.sc_ic.ic_if;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct bwfm_sdio_hwhdr *hwhdr;
	struct bwfm_sdio_swhdr *swhdr;
	uint16_t *sublen, nextlen = 0;
	struct mbuf *m;
	size_t flen;
	char *data;
	off_t off;
	int nsub;

	hwhdr = (struct bwfm_sdio_hwhdr *)sc->sc_bounce_buf;
	swhdr = (struct bwfm_sdio_swhdr *)&hwhdr[1];
	data = (char *)&swhdr[1];

	for (;;) {
		/* If we know the next size, just read ahead. */
		if (nextlen) {
			if (bwfm_sdio_frame_read_write(sc, sc->sc_bounce_buf,
			    nextlen, 0))
				break;
		} else {
			if (bwfm_sdio_frame_read_write(sc, sc->sc_bounce_buf,
			    sizeof(*hwhdr) + sizeof(*swhdr), 0))
				break;
		}

		hwhdr->frmlen = letoh16(hwhdr->frmlen);
		hwhdr->cksum = letoh16(hwhdr->cksum);

		if (hwhdr->frmlen == 0 && hwhdr->cksum == 0)
			break;

		if ((hwhdr->frmlen ^ hwhdr->cksum) != 0xffff) {
			printf("%s: checksum error\n", DEVNAME(sc));
			break;
		}

		if (hwhdr->frmlen < sizeof(*hwhdr) + sizeof(*swhdr)) {
			printf("%s: length error\n", DEVNAME(sc));
			break;
		}

		if (nextlen && hwhdr->frmlen > nextlen) {
			printf("%s: read ahead length error (%u > %u)\n",
			    DEVNAME(sc), hwhdr->frmlen, nextlen);
			break;
		}

		sc->sc_tx_max_seq = swhdr->maxseqnr;

		flen = hwhdr->frmlen - (sizeof(*hwhdr) + sizeof(*swhdr));
		if (flen == 0) {
			nextlen = swhdr->nextlen << 4;
			continue;
		}

		if (!nextlen) {
			KASSERT(roundup(flen, 4) <= sc->sc_bounce_size -
			    (sizeof(*hwhdr) + sizeof(*swhdr)));
			if (bwfm_sdio_frame_read_write(sc, data,
			    roundup(flen, 4), 0))
				break;
		}

		if (swhdr->dataoff < (sizeof(*hwhdr) + sizeof(*swhdr)))
			break;

		off = swhdr->dataoff - (sizeof(*hwhdr) + sizeof(*swhdr));
		if (off > flen)
			break;

		switch (swhdr->chanflag & BWFM_SDIO_SWHDR_CHANNEL_MASK) {
		case BWFM_SDIO_SWHDR_CHANNEL_CONTROL:
			sc->sc_sc.sc_proto_ops->proto_rxctl(&sc->sc_sc,
			    data + off, flen - off);
			nextlen = swhdr->nextlen << 4;
			break;
		case BWFM_SDIO_SWHDR_CHANNEL_EVENT:
		case BWFM_SDIO_SWHDR_CHANNEL_DATA:
			m = bwfm_sdio_newbuf();
			if (m == NULL)
				break;
			if (flen - off > m->m_len) {
				printf("%s: frame bigger than anticipated\n",
				    DEVNAME(sc));
				m_freem(m);
				break;
			}
			m->m_len = m->m_pkthdr.len = flen - off;
			memcpy(mtod(m, char *), data + off, flen - off);
			sc->sc_sc.sc_proto_ops->proto_rx(&sc->sc_sc, m, &ml);
			nextlen = swhdr->nextlen << 4;
			break;
		case BWFM_SDIO_SWHDR_CHANNEL_GLOM:
			if ((flen % sizeof(uint16_t)) != 0)
				break;
			nsub = flen / sizeof(uint16_t);
			sublen = mallocarray(nsub, sizeof(uint16_t),
			    M_DEVBUF, M_WAITOK | M_ZERO);
			memcpy(sublen, data, nsub * sizeof(uint16_t));
			bwfm_sdio_rx_glom(sc, sublen, nsub, &nextlen, &ml);
			free(sublen, M_DEVBUF, nsub * sizeof(uint16_t));
			break;
		default:
			printf("%s: unknown channel\n", DEVNAME(sc));
			break;
		}
	}

	if_input(ifp, &ml);
}

void
bwfm_sdio_rx_glom(struct bwfm_sdio_softc *sc, uint16_t *sublen, int nsub,
    uint16_t *nextlen, struct mbuf_list *ml)
{
	struct bwfm_sdio_hwhdr hwhdr;
	struct bwfm_sdio_swhdr swhdr;
	struct mbuf_list glom, drop;
	struct mbuf *m;
	size_t flen;
	off_t off;
	int i;

	ml_init(&glom);
	ml_init(&drop);

	if (nsub == 0)
		return;

	for (i = 0; i < nsub; i++) {
		m = bwfm_sdio_newbuf();
		if (m == NULL) {
			ml_purge(&glom);
			return;
		}
		ml_enqueue(&glom, m);
		if (letoh16(sublen[i]) > m->m_len) {
			ml_purge(&glom);
			return;
		}
		if (bwfm_sdio_frame_read_write(sc, mtod(m, char *),
		    letoh16(sublen[i]), 0)) {
			ml_purge(&glom);
			return;
		}
		m->m_len = m->m_pkthdr.len = letoh16(sublen[i]);
	}

	/* TODO: Verify actual superframe header */
	m = MBUF_LIST_FIRST(&glom);
	if (m->m_len >= sizeof(hwhdr) + sizeof(swhdr)) {
		m_copydata(m, 0, sizeof(hwhdr), (caddr_t)&hwhdr);
		m_copydata(m, sizeof(hwhdr), sizeof(swhdr), (caddr_t)&swhdr);
		*nextlen = swhdr.nextlen << 4;
		m_adj(m, sizeof(struct bwfm_sdio_hwhdr) +
		    sizeof(struct bwfm_sdio_swhdr));
	}

	while ((m = ml_dequeue(&glom)) != NULL) {
		if (m->m_len < sizeof(hwhdr) + sizeof(swhdr))
			goto drop;

		m_copydata(m, 0, sizeof(hwhdr), (caddr_t)&hwhdr);
		m_copydata(m, sizeof(hwhdr), sizeof(swhdr), (caddr_t)&swhdr);

		hwhdr.frmlen = letoh16(hwhdr.frmlen);
		hwhdr.cksum = letoh16(hwhdr.cksum);

		if (hwhdr.frmlen == 0 && hwhdr.cksum == 0)
			goto drop;

		if ((hwhdr.frmlen ^ hwhdr.cksum) != 0xffff) {
			printf("%s: checksum error\n", DEVNAME(sc));
			goto drop;
		}

		if (hwhdr.frmlen < sizeof(hwhdr) + sizeof(swhdr)) {
			printf("%s: length error\n", DEVNAME(sc));
			goto drop;
		}

		flen = hwhdr.frmlen - (sizeof(hwhdr) + sizeof(swhdr));
		if (flen == 0)
			goto drop;
		if (m->m_len < flen)
			goto drop;

		if (swhdr.dataoff < (sizeof(hwhdr) + sizeof(swhdr)))
			goto drop;

		off = swhdr.dataoff - (sizeof(hwhdr) + sizeof(swhdr));
		if (off > flen)
			goto drop;

		switch (swhdr.chanflag & BWFM_SDIO_SWHDR_CHANNEL_MASK) {
		case BWFM_SDIO_SWHDR_CHANNEL_CONTROL:
			printf("%s: control channel not allowed in glom\n",
			    DEVNAME(sc));
			goto drop;
		case BWFM_SDIO_SWHDR_CHANNEL_EVENT:
		case BWFM_SDIO_SWHDR_CHANNEL_DATA:
			m_adj(m, swhdr.dataoff);
			sc->sc_sc.sc_proto_ops->proto_rx(&sc->sc_sc, m, ml);
			break;
		case BWFM_SDIO_SWHDR_CHANNEL_GLOM:
			printf("%s: glom not allowed in glom\n",
			    DEVNAME(sc));
			goto drop;
		default:
			printf("%s: unknown channel\n", DEVNAME(sc));
			goto drop;
		}

		continue;
drop:
		ml_enqueue(&drop, m);
	}

	ml_purge(&drop);
}

int
bwfm_sdio_txcheck(struct bwfm_softc *bwfm)
{
	struct bwfm_sdio_softc *sc = (void *)bwfm;

	if (sc->sc_tx_count >= 64)
		return ENOBUFS;

	return 0;
}

int
bwfm_sdio_txdata(struct bwfm_softc *bwfm, struct mbuf *m)
{
	struct bwfm_sdio_softc *sc = (void *)bwfm;

	if (sc->sc_tx_count >= 64)
		return ENOBUFS;

	sc->sc_tx_count++;
	ml_enqueue(&sc->sc_tx_queue, m);
	task_add(systq, &sc->sc_task);
	return 0;
}

int
bwfm_sdio_txctl(struct bwfm_softc *bwfm, void *arg)
{
	struct bwfm_sdio_softc *sc = (void *)bwfm;
	struct bwfm_proto_bcdc_ctl *ctl = arg;
	struct mbuf *m;

	KASSERT(ctl->len <= MCLBYTES);

	MGET(m, M_DONTWAIT, MT_CONTROL);
	if (m == NULL)
		goto fail;
	if (ctl->len > MLEN) {
		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			m_freem(m);
			goto fail;
		}
	}
	memcpy(mtod(m, char *), ctl->buf, ctl->len);
	m->m_len = ctl->len;

	TAILQ_INSERT_TAIL(&sc->sc_sc.sc_bcdc_rxctlq, ctl, next);
	ml_enqueue(&sc->sc_tx_queue, m);
	task_add(systq, &sc->sc_task);
	return 0;

fail:
	free(ctl->buf, M_TEMP, ctl->len);
	free(ctl, M_TEMP, sizeof(*ctl));
	return 1;
}

#ifdef BWFM_DEBUG
void
bwfm_sdio_debug_console(struct bwfm_sdio_softc *sc)
{
	struct bwfm_sdio_console c;
	uint32_t newidx;
	int err;

	if (!sc->sc_console_addr)
		return;

	err = bwfm_sdio_ram_read_write(sc, sc->sc_console_addr,
	    (char *)&c, sizeof(c), 0);
	if (err)
		return;

	c.log_buf = letoh32(c.log_buf);
	c.log_bufsz = letoh32(c.log_bufsz);
	c.log_idx = letoh32(c.log_idx);

	if (sc->sc_console_buf == NULL) {
		sc->sc_console_buf = malloc(c.log_bufsz, M_DEVBUF,
		    M_WAITOK|M_ZERO);
		sc->sc_console_buf_size = c.log_bufsz;
	}

	newidx = c.log_idx;
	if (newidx >= sc->sc_console_buf_size)
		return;

	err = bwfm_sdio_ram_read_write(sc, c.log_buf, sc->sc_console_buf,
	    sc->sc_console_buf_size, 0);
	if (err)
		return;

	if (newidx != sc->sc_console_readidx)
		DPRINTFN(3, ("BWFM CONSOLE: "));
	while (newidx != sc->sc_console_readidx) {
		uint8_t ch = sc->sc_console_buf[sc->sc_console_readidx];
		sc->sc_console_readidx++;
		if (sc->sc_console_readidx == sc->sc_console_buf_size)
			sc->sc_console_readidx = 0;
		if (ch == '\r')
			continue;
		DPRINTFN(3, ("%c", ch));
	}
}
#endif
#endif
