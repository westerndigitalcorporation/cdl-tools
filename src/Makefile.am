# SPDX-License-Identifier: CC0-1.0
#
# Copyright (C) 2021 Western Digital Corporation or its affiliates.

AM_CFLAGS = -O2 -Wall -Wextra -Wno-unused-parameter -D_GNU_SOURCE

bin_PROGRAMS = cdladm

CFILES = cdl_dev.c \
	 cdl_scsi.c \
	 cdl_ata.c \
	 cdl.c \
	 cdladm.c
HFILES = cdl.h

cdladm_SOURCES = ${CFILES} ${HFILES}
