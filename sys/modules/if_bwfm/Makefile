.PATH:	${SRCTOP}/sys/dev/bwfm

KMOD=	if_bwfm
SRCS=	if_bwfm_sdio.c bwfm.c bwfm_if.h bwfm_if.c

CFLAGS.if_bwfm_sdio.c+=	-Wno-cast-qual
CFLAGS.bwfm.c+=	-Wno-cast-qual

.include <bsd.kmod.mk>
