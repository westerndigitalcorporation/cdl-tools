#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

if [ $# == 0 ]; then
	echo "cdladm (save CDL descriptors)"
	exit 0
fi

dev="$(realpath $1)"
bdev="$(basename ${dev})"

echo "# cdladm save $1"
cdladm save $1 || exit_failed " --> FAILED"
mv "${bdev}-T2A.cdl" /tmp/cdl-save-all-t2a || \
	exit_failed " --> FAILED (missing T2A page)"
mv "${bdev}-T2B.cdl" /tmp/cdl-save-all-t2b || \
	exit_failed " --> FAILED (missing T2B page)"

echo "# cdladm save --page T2A $1"
cdladm save --page T2A --file /tmp/cdl-save-t2a "$1" || \
	exit_failed " --> FAILED"
diff -q  /tmp/cdl-save-all-t2a /tmp/cdl-save-t2a || \
	exit_failed " --> FAILED (save all and save one T2A pages differ)"

echo "# cdladm save --page T2B $1"
cdladm save --page T2B --file /tmp/cdl-save-t2b "$1" || \
	exit_failed " --> FAILED"
diff -q  /tmp/cdl-save-all-t2b /tmp/cdl-save-t2b || \
	exit_failed " --> FAILED (save all and save one T2B pages differ)"

if dev_is_ata "$1"; then
	echo "# cdladm save --force-ata $1"
	cdladm save --force-ata "$1" || \
		exit_failed " --> FAILED"
	mv "${bdev}-T2A.cdl" /tmp/cdl-save-all-t2a-ata || \
		exit_failed " --> FAILED (missing T2A page)"
	mv "${bdev}-T2B.cdl" /tmp/cdl-save-all-t2b-ata || \
		exit_failed " --> FAILED (missing T2B page)"

	echo "# cdladm save --force-ata --page T2A $1"
	cdladm save --force-ata --page T2A --file /tmp/cdl-save-t2a-ata "$1" || \
		exit_failed " --> FAILED"
	diff -q  /tmp/cdl-save-all-t2a-ata /tmp/cdl-save-t2a-ata || \
		exit_failed " --> FAILED (ATA save all and save one T2A pages differ)"
	diff -q  /tmp/cdl-save-t2a /tmp/cdl-save-t2a-ata || \
		exit_failed " --> FAILED (save SAT and save ATA T2A pages differ)"

	echo "# cdladm save --force-ata --page T2B $1"
	cdladm save --force-ata --page T2B --file /tmp/cdl-save-t2b-ata "$1" || \
		exit_failed " --> FAILED"
	diff -q  /tmp/cdl-save-all-t2b-ata /tmp/cdl-save-t2b-ata || \
		exit_failed " --> FAILED (ATA save all and save one T2B pages differ)"
	diff -q  /tmp/cdl-save-t2b /tmp/cdl-save-t2b-ata || \
		exit_failed " --> FAILED (save SAT and save ATA T2B pages differ)"

fi

exit 0
