# $FreeBSD$
#
# Common rules for building firmware.  Note this gets auto-included
# by the subdir Makefile's as a consequence of included bsd.kmod.mk.

_FIRM=	${IMG}.bin

CLEANFILES+=	${_FIRM}

FIRMWS=	${_FIRM}:${KMOD}

#
# Note that a license ack is not needed for bwfm.
#
#FIRMWARE_LICENSE=

${_FIRM}: ${SRCTOP}/sys/contrib/dev/bwfm/${_FIRM}.uu
	uudecode -p ${.ALLSRC} > ${.TARGET}
