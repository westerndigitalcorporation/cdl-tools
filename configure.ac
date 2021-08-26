# SPDX-License-Identifier: CC0-1.0
#
# Copyright (C) 2021 Western Digital Corporation or its affiliates.

AC_INIT([cdl-tools], [0.0.2],
	[damien.lemoal@wdc.com, niklas.cassel@wdc.com],
        [cdl-tools], [https://github.com/westerndigitalcorporation/cdl-tools])

AC_PREFIX_DEFAULT(/usr)
AC_CONFIG_HEADER([src/config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AM_SILENT_RULES([yes])

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_GNU_SOURCE

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_pattern_allow([AM_PROG_AR])
LT_INIT

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LN_S

# Checks for header files.
AC_CHECK_HEADER(scsi/scsi.h, [],
		[AC_MSG_ERROR([Couldn't find scsi/scsi.h])])
AC_CHECK_HEADER(scsi/sg.h, [],
		[AC_MSG_ERROR([Couldn't find scsi/sg.h])])
AC_CHECK_HEADER(linux/fs.h, [],
		[AC_MSG_ERROR([Couldn't find linux/fs.h])])

# Checks for rpm package builds
AC_PATH_PROG([RPMBUILD], [rpmbuild], [notfound])
AC_PATH_PROG([RPM], [rpm], [notfound])
AM_CONDITIONAL([BUILD_RPM],
		[test "x$RPMBUILD" != xnotfound && test "x$RPM" != xnotfound])

AC_CONFIG_FILES([
	Makefile
	man/Makefile
	src/Makefile
])

AC_OUTPUT
