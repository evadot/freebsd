#
# Copyright (c) 2012 Adrian Chadd, Xenion Pty Ltd
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer,
#    without modification.
# 2. Redistributions in binary form must reproduce at minimum a disclaimer
#    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
#    redistribution must be conditioned upon including a substantially
#    similar Disclaimer requirement for further binary redistribution.
#
# NO WARRANTY
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
# AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGES.
#
#

.PATH:	${SRCTOP}/sys/dev/gpio/

KMOD=	gpiobus
SRCS=	gpiobus.c gpioc.c
SRCS+=	gpio_if.c gpio_if.h gpiobus_if.c gpiobus_if.h ofw_bus_if.h
SRCS+=	device_if.h bus_if.h opt_platform.h

.if !empty(OPT_FDT)
SRCS+=	ofw_gpiobus.c
.endif
.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "aarch64"
SRCS+=	acpi_gpiobus.c opt_acpi.h acpi_if.h
.endif

CFLAGS+=  -I. -I${SRCTOP}/sys/dev/gpio/

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
