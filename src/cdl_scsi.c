// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 */

#include "cdl.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>

#define CDL_SCSI_VPD_PAGE_00_LEN	32
#define CDL_SCSI_VPD_PAGE_89_LEN	0x238

/*
 * Fill the buffer with the result of a VPD page INQUIRY command.
 */
static int cdl_scsi_vpd_inquiry(struct cdl_dev *dev, uint8_t page,
				void *buf, uint16_t buf_len)
{
	struct cdl_sg_cmd cmd;
	int ret;

	/* Get the requested page */
	cdl_init_cmd(&cmd, 6, SG_DXFER_FROM_DEV, buf_len);
	cmd.cdb[0] = 0x12;
	cmd.cdb[1] = 0x01;
	cmd.cdb[2] = page;
	cdl_sg_set_be16(&cmd.cdb[3], buf_len);

	/* Execute the SG_IO command */
	ret = cdl_exec_cmd(dev, &cmd);
	if (ret) {
		cdl_dev_err(dev, "Get VPD page 0x%02x failed\n", page);
		return -EIO;
	}

	memcpy(buf, cmd.buf, buf_len);

	return 0;
}

/*
 * Test if a VPD page is supported.
 */
static bool cdl_scsi_vpd_page_supported(struct cdl_dev *dev, uint8_t page)
{
	uint8_t buf[CDL_SCSI_VPD_PAGE_00_LEN];
	int vpd_len, i, ret;

	ret = cdl_scsi_vpd_inquiry(dev, 0x00, buf, CDL_SCSI_VPD_PAGE_00_LEN);
	if (ret != 0) {
		cdl_dev_err(dev, "Get VPD page 0x00 failed\n");
		return false;
	}

	if (buf[1] != 0x00) {
		cdl_dev_err(dev, "Invalid page code 0x%02x for VPD page 0x00\n",
			    (int)buf[1]);
		return false;
	}

	vpd_len = cdl_sg_get_be16(&buf[2]) + 4;
	if (vpd_len > CDL_SCSI_VPD_PAGE_00_LEN)
		vpd_len = CDL_SCSI_VPD_PAGE_00_LEN;
	for (i = 4; i < vpd_len; i++) {
		if (buf[i] == page)
			return true;
	}

	return false;
}

/*
 * If this the device is an ATA disk, get ATA information.
 */
void cdl_scsi_get_ata_information(struct cdl_dev *dev)
{
	uint8_t buf[CDL_SCSI_VPD_PAGE_89_LEN];
	int ret;

	/*
	 * See if VPD page 0x89 (ATA Information) is supported to determine
	 * if we are dealing with an ATA device.
	 */
	if (!cdl_scsi_vpd_page_supported(dev, 0x89))
		return;

	/* This is an ATA device: Get SAT information */
	ret = cdl_scsi_vpd_inquiry(dev, 0x89, buf, CDL_SCSI_VPD_PAGE_89_LEN);
	if (ret != 0) {
		cdl_dev_err(dev, "Get VPD page 0x89 failed\n");
		return;
	}

	if (buf[1] != 0x89) {
		cdl_dev_err(dev, "Invalid page code 0x%02x for VPD page 0x00\n",
			    (int)buf[1]);
		return;
	}

	cdl_sg_get_str(dev->sat_vendor, &buf[8], CDL_SAT_VENDOR_LEN - 1);
	cdl_sg_get_str(dev->sat_product, &buf[16], CDL_SAT_PRODUCT_LEN - 1);
	cdl_sg_get_str(dev->sat_rev, &buf[32], CDL_SAT_REV_LEN - 1);
	dev->flags |= CDL_ATA;
}

/*
 * Get the CDL page type used for a command.
 */
static int cdl_scsi_get_cmd_cdlp(struct cdl_dev *dev, enum cdl_cmd c)
{
	struct cdl_sg_cmd cmd;
	uint8_t cdlp;
	int ret;

	/* Check command support */
	cdl_init_cmd(&cmd, 12, SG_DXFER_FROM_DEV, 512);
	cmd.cdb[0] = 0xa3; /* MAINTENANCE_IN */
	cmd.cdb[1] = 0x0c; /* MI_REPORT_SUPPORTED_OPERATION_CODES */
	cmd.cdb[2] = 0x03; /* one command format with SA */
	cmd.cdb[3] = cdl_cmd_opcode(c);
	cdl_sg_set_be16(&cmd.cdb[4], cdl_cmd_sa(c));
	cdl_sg_set_be32(&cmd.cdb[6], 512);

	ret = cdl_exec_cmd(dev, &cmd);
	if (ret) {
		if (cdl_verbose(dev))
			cdl_dev_err(dev,
				"REPORT_SUPPORTED_OPERATION_CODES failed\n");
		return CDLP_NONE;
	}

	if ((cmd.buf[1] & 0x03) != 0x03) {
		/* Command not supported */
		return CDLP_NONE;
	}

	/* See SPC-6, one command format of REPORT SUPPORTED OPERATION CODES */
	cdlp = (cmd.buf[1] & 0x18) >> 3;
	if (cmd.buf[0] & 0x01) {
		/* rwcdlp == 1 */
		switch (cdlp) {
		case 0x01:
			return CDLP_T2A;
		case 0x02:
			return CDLP_T2B;
		}
	} else {
		/* rwcdlp == 0 */
		switch (cdlp) {
		case 0x01:
			return CDLP_A;
		case 0x02:
			return CDLP_B;
		}
	}

	return CDLP_NONE;
}

/*
 * Initialize handling of SCSI device.
 */
int cdl_scsi_init(struct cdl_dev *dev)
{
	bool enabled = false;
	int i;

	/*
	 * Command duration limits is supported only with READ 16, WRITE 16,
	 * READ 32 and WRITE 32. Go through all these commands one at a time
	 * and check if any support duration limits.
	 */
	for (i = 0; i < CDL_CMD_MAX; i++) {
		dev->cmd_cdlp[i] = cdl_scsi_get_cmd_cdlp(dev, i);
		if (dev->cmd_cdlp[i] != CDLP_NONE) {
			dev->flags |= CDL_DEV_SUPPORTED;
			if (dev->cmd_cdlp[i] == CDLP_T2A ||
			    dev->cmd_cdlp[i] == CDLP_T2B)
				dev->flags |= CDL_GUIDELINE_DEV_SUPPORTED;
		}
	}

	if (!(dev->flags & CDL_DEV_SUPPORTED))
		return 0;

	/* Set the minimum and maximum limits */
	dev->min_limit = 500;
	dev->max_limit = 65535ULL * 500000000ULL;

	/*
	 * There is no device level CDL feature enable/disable control.
	 * So align to the system setting.
	 */
	enabled = cdl_sysfs_get_ulong_attr(dev,
				"/sys/block/%s/device/duration_limits/enable",
				dev->name);
	if (enabled)
		dev->flags |= CDL_DEV_ENABLED;
	else
		dev->flags &= ~CDL_DEV_ENABLED;

	return 0;
}

/*
 * Read a CDL page from the device.
 */
int cdl_scsi_read_page(struct cdl_dev *dev, enum cdl_p cdlp,
		       struct cdl_page *page)
{
	struct cdl_sg_cmd cmd;
	struct cdl_desc *desc = &page->descs[0];
	uint8_t *buf;
	int i, ret;

	/* Get a CDL page */
	cdl_init_cmd(&cmd, 10, SG_DXFER_FROM_DEV, 512);
	cmd.cdb[0] = 0x5a; /* MODE SENSE 10 */
	cmd.cdb[1] = 0x08; /* DBD = 1 */
	cmd.cdb[2] = 0x0A;
	cmd.cdb[3] = cdl_page_code(cdlp);
	cdl_sg_set_be16(&cmd.cdb[7], 512);

	ret = cdl_exec_cmd(dev, &cmd);
	if (ret) {
		cdl_dev_err(dev, "MODE SENSE 10 failed\n");
		return ret;
	}

	/* Check that we do not have any block descriptor */
	if (cdl_sg_get_be16(&cmd.buf[6])) {
		fprintf(stderr,
			"%s: DBD = 1 but got %d B of block descriptors\n",
			dev->name, (int)cdl_sg_get_be16(&cmd.buf[6]));
		return -EIO;
	}

	/*
	 * Save the mode sense buffer as we will need it for mode select
	 * when changing the page descriptors.
	 */
	page->msbufsz = cmd.bufsz;
	if (!page->msbuf) {
		page->msbuf = malloc(page->msbufsz);
		if (!page->msbuf) {
			fprintf(stderr, "%s: No memory for page %s mode sense buffer\n",
				dev->name, cdl_page_name(cdlp));
			return -ENOMEM;
		}
	}
	memcpy(page->msbuf, cmd.buf, page->msbufsz);

	/* Skip header and block descriptors */
	buf = cmd.buf + 8 + cdl_sg_get_be16(&cmd.buf[6]);

	if ((buf[0] & 0x3f) != 0x0a ||
	    buf[1] != cdl_page_code(cdlp)) {
		fprintf(stderr, "%s: Invalid mode page codes for page %s\n",
			dev->name, cdl_page_name(cdlp));
		return -EINVAL;
	}

	page->cdlp = cdlp;

	if (cdlp == CDLP_A || cdlp == CDLP_B) {
		buf += 8;
		for (i = 0; i < CDL_MAX_DESC; i++, buf += 4, desc++) {
			desc->cdltunit = (buf[0] & 0xe0) >> 5;
			desc->duration = cdl_sg_get_be16(&buf[2]);
		}
	} else {
		/* T2A and T2B limits page */
		if (cdlp == CDLP_T2A)
			page->perf_vs_duration_guideline = (buf[7] >> 4) & 0x0f;
		buf += 8;
		for (i = 0; i < CDL_MAX_DESC; i++, buf += 32, desc++) {
			desc->cdltunit = buf[0] & 0x0f;
			desc->max_inactive_time = cdl_sg_get_be16(&buf[2]);
			desc->max_active_time = cdl_sg_get_be16(&buf[4]);
			desc->duration = cdl_sg_get_be16(&buf[10]);
			desc->max_inactive_policy =  (buf[6] >> 4) & 0x0f;
			desc->max_active_policy = buf[6] & 0x0f;
			desc->duration_policy = buf[14] & 0x0f;
		}
	}

	return 0;
}

/*
 * Write a CDL page to the device.
 */
int cdl_scsi_write_page(struct cdl_dev *dev, struct cdl_page *page)
{
	struct cdl_desc *desc = &page->descs[0];
	uint8_t cdlp = page->cdlp;
	struct cdl_sg_cmd cmd;
	uint8_t *buf = cmd.buf;
	size_t bufsz;
	int i, ret;

	/*
	 * Initialize MODE SELECT 10 command: use the mode sense use the
	 * mode sense buffer to initialize the command buffer.
	 */
	bufsz = dev->cdl_pages[cdlp].msbufsz;
	cdl_init_cmd(&cmd, 10, SG_DXFER_TO_DEV, bufsz);
	memcpy(buf, dev->cdl_pages[cdlp].msbuf, bufsz);

	cdl_sg_set_be16(&buf[0], 0); /* Clear mode data length */
	buf[3] = 0; /* clear WP and DPOFUA */

	/* Skip the mode page header */
	buf += 8;

	/* Set the page values */
	buf[0] &= 0x7F; /* Clear PS */
	buf[1] = cdl_page_code(cdlp);

	if (cdlp == CDLP_A || cdlp == CDLP_B) {
		/* A and B limits page */
		cdl_sg_set_be16(&buf[2], 0x0020);
		buf += 8;
		for (i = 0; i < CDL_MAX_DESC; i++, buf += 4, desc++) {
			buf[0] = (desc->cdltunit & 0x3) << 5;
			cdl_sg_set_be16(&buf[2], desc->duration);
		}
	} else {
		/* T2A and T2B limits page */
		cdl_sg_set_be16(&buf[2], 0x00e4);
		if (cdlp == CDLP_T2A)
			buf[7] = (page->perf_vs_duration_guideline & 0x0f) << 4;
		buf += 8;
		for (i = 0; i < CDL_MAX_DESC; i++, buf += 32, desc++) {
			buf[0] = desc->cdltunit & 0x0f;
			cdl_sg_set_be16(&buf[2], desc->max_inactive_time);
			cdl_sg_set_be16(&buf[4], desc->max_active_time);
			buf[6] = ((desc->max_inactive_policy & 0x0f) << 4) |
				(desc->max_active_policy & 0x0f);
			cdl_sg_set_be16(&buf[10], desc->duration);
			buf[14] = desc->duration_policy & 0x0f;
		}
	}

	cmd.cdb[0] = 0x55; /* MODE SELECT 10 */
	cmd.cdb[1] = 0x10; /* PF = 1, RTD = 0, SP = 0 */
	if (dev->flags & CDL_USE_MS_SP)
		cmd.cdb[1] |= 0x01; /* SP = 1 */
	cdl_sg_set_be16(&cmd.cdb[7], bufsz);
	ret = cdl_exec_cmd(dev, &cmd);
	if (ret) {
		cdl_dev_err(dev, "MODE SELECT 10 failed\n");
		return ret;
	}

	return 0;
}

/*
 * Check the device CDL enable status.
 */
int cdl_scsi_check_enabled(struct cdl_dev *dev, bool enabled)
{
	/*
	 * There is no device level CDL feature enable/disable control.
	 * So align to the system setting.
	 */
	if (enabled)
		dev->flags |= CDL_DEV_ENABLED;
	else
		dev->flags &= ~CDL_DEV_ENABLED;

	return 0;
}

/*
 * Force device revalidation so that sysfs exposes updated command
 * duration limits.
 */
void cdl_scsi_revalidate(struct cdl_dev *dev)
{
	int ret;

	/* Rescan the device */
	ret = cdl_sysfs_set_attr(dev, "1",
			"/sys/block/%s/device/rescan",
			dev->name);
	if (ret)
		cdl_dev_err(dev, "Revalidate device failed\n");
}
