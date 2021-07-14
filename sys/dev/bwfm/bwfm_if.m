#include <sys/types.h>

INTERFACE bwfm;

HEADER {
	struct bwfm_softc;
	struct mbuf;
};

CODE {
	static void
	null_buscore_reset(device_t dev __unused,
	    struct bwfm_softc *sc __unused)
	{

	}

	static void
	null_buscore_setup(device_t dev __unused,
	    struct bwfm_softc *sc __unused)
	{

	}

	static int
	null_bus_preinit(device_t dev __unused,
	    struct bwfm_softc *sc __unused)
	{

		return (0);
	}

	static int
	null_bus_stop(device_t dev __unused,
	    struct bwfm_softc *sc __unused)
	{

		return (0);
	}
}

METHOD uint32_t buscore_read {
	device_t		dev;
	struct bwfm_softc	*bwfm;
	uint32_t		reg;
};

METHOD void buscore_write {
	device_t		dev;
	struct bwfm_softc	*bwfm;
	uint32_t		reg;
	uint32_t		val;
};

METHOD int buscore_prepare {
	device_t		dev;
	struct bwfm_softc	*bwfm;
};

METHOD void buscore_activate {
	device_t		dev;
	struct bwfm_softc	*bwfm;
	uint32_t		rstvec;
};

METHOD void buscore_reset {
	device_t		dev;
	struct bwfm_softc	*bwfm;
} DEFAULT null_buscore_reset;

METHOD void buscore_setup {
	device_t		dev;
	struct bwfm_softc	*bwfm;
} DEFAULT null_buscore_setup;

METHOD int bus_preinit {
	device_t		dev;
	struct bwfm_softc	*bwfm;
} DEFAULT null_bus_preinit;

METHOD int bus_stop {
	device_t		dev;
	struct bwfm_softc	*bwfm;
} DEFAULT null_bus_stop;

METHOD int bus_txcheck {
	device_t		dev;
	struct bwfm_softc	*bwfm;
};

METHOD int bus_txdata {
	device_t		dev;
	struct bwfm_softc	*bwfm;
	struct mbuf		*m;
};

METHOD int bus_txctl {
	device_t		dev;
	struct bwfm_softc	*bwfm;
	void			*arg;
};
