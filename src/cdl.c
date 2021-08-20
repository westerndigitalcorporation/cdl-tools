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

/*
 * Get the CDL page type used for a command.
 */
static int cdl_get_cmd_cdlp(struct cdl_dev *dev, enum cdl_cmd c)
{
	struct cdl_sg_cmd cmd;
	uint8_t cdlp;
	int ret;

	/* Check command support */
	cdl_init_cmd(&cmd, 16, SG_DXFER_FROM_DEV, 512);
	cmd.cdb[0] = 0xa3; /* MAINTENANCE_IN */
	cmd.cdb[1] = 0x0c; /* MI_REPORT_SUPPORTED_OPERATION_CODES */
	cmd.cdb[2] = 0x03; /* one command format with SA */
	cmd.cdb[3] = cdl_cmd_opcode(c);
	cdl_sg_set_int16(&cmd.cdb[4], cdl_cmd_sa(c));
	cdl_sg_set_int32(&cmd.cdb[6], 512);

	ret = cdl_exec_cmd(dev, &cmd);
	if (ret) {
		cdl_dev_err(dev, "REPORT_SUPPORTED_OPERATION_CODES failed\n");
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
 * Read a CDL page from the device.
 */
static int cdl_read_page(struct cdl_dev *dev, enum cdl_p cdlp,
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
	cdl_sg_set_int16(&cmd.cdb[7], 512);

	ret = cdl_exec_cmd(dev, &cmd);
	if (ret) {
		cdl_dev_err(dev, "MODE SENSE 10 failed\n");
		return ret;
	}

	/* Check that we do not have any block descriptor */
	if (cdl_sg_get_int16(&cmd.buf[6])) {
		fprintf(stderr,
			"%s: DBD = 1 but got %d B of block descriptors\n",
			dev->name, (int)cdl_sg_get_int16(&cmd.buf[6]));
		return -EIO;
	}

	/*
	 * Save the mode sense buffer as we will need it for mode select
	 * when changing the page descriptors.
	 */
	page->msbufsz = cmd.bufsz;
	page->msbuf = malloc(page->msbufsz);
	if (!page->msbuf) {
		fprintf(stderr, "%s: No memory for page %s mode sense buffer\n",
			dev->name, cdl_page_name(cdlp));
		return -ENOMEM;
	}
	memcpy(page->msbuf, cmd.buf, page->msbufsz);

	/* Skip header and block descriptors */
	buf = cmd.buf + 8 + cdl_sg_get_int16(&cmd.buf[6]);

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
			desc->duration = cdl_sg_get_int16(&buf[2]);
		}
	} else {
		/* T2A and T2B limits page */
		page->perf_vs_duration_guideline = buf[7] >> 4;
		buf += 8;
		for (i = 0; i < CDL_MAX_DESC; i++, buf += 32, desc++) {
			desc->cdltunit = buf[0] & 0x0f;
			desc->max_inactive_time = cdl_sg_get_int16(&buf[2]);
			desc->max_active_time = cdl_sg_get_int16(&buf[4]);
			desc->duration = cdl_sg_get_int16(&buf[10]);
			desc->max_inactive_policy =  (buf[6] >> 4) & 0x0f;
			desc->max_active_policy = buf[6] & 0x0f;
			desc->duration_policy = buf[14] & 0x0f;
		}
	}

	return 0;
}

/*
 * Read all supported pages.
 */
int cdl_read_pages(struct cdl_dev *dev)
{
	struct cdl_page *page;
	uint8_t cdlp;
	int i, ret;

	/* Read supported pages */
	for (i = 0; i < CDL_CMD_MAX; i++) {
		cdlp = dev->cmd_cdlp[i];
		if (cdlp == CDLP_NONE)
			continue;

		page = &dev->cdl_pages[cdlp];
		if (page->cdlp == cdlp) {
			/* We already read this one */
			continue;
		}

		ret = cdl_read_page(dev, cdlp, page);
		if (ret)
			return ret;
	}

	return 0;
}

/*
 * Check if a page type is supported.
 */
bool cdl_page_supported(struct cdl_dev *dev, enum cdl_p cdlp)
{
	int i;

	for (i = 0; i < CDL_CMD_MAX; i++) {
		if (dev->cmd_cdlp[i] == cdlp)
			return true;
	}

	return false;
}

/*
 * Check CDL support.
 */
bool cdl_check_support(struct cdl_dev *dev)
{
	bool cdl_supported = false;
	int i;

	/*
	 * Command duration limits is supported only with READ 16, WRITE 16,
	 * READ 32 and WRITE 32. Go through all these commands one at a time
	 * and check if any support duration limits.
	 */
	for (i = 0; i < CDL_CMD_MAX; i++) {
		dev->cmd_cdlp[i] = cdl_get_cmd_cdlp(dev, i);
		if (dev->cmd_cdlp[i] != CDLP_NONE)
			cdl_supported = true;
	}

	return cdl_supported;
}

/*
 * Write a CDL page to the device.
 */
int cdl_write_page(struct cdl_dev *dev, struct cdl_page *page)
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

	cdl_sg_set_int16(&buf[0], 0); /* Clear mode data length */
	buf[3] = 0; /* clear WP and DPOFUA */

	/* Skip the mode page header */
	buf += 8;

	/* Set the page values */
	buf[0] &= 0x7F; /* Clear PS */
	buf[1] = cdl_page_code(cdlp);

	if (cdlp == CDLP_A || cdlp == CDLP_B) {
		/* A and B limits page */
		cdl_sg_set_int16(&buf[2], 0x0020);
		buf += 8;
		for (i = 0; i < CDL_MAX_DESC; i++, buf += 4, desc++) {
			buf[0] = (desc->cdltunit & 0x3) << 5;
			cdl_sg_set_int16(&buf[2], desc->duration);
		}
	} else {
		/* T2A and T2B limits page */
		cdl_sg_set_int16(&buf[2], 0x00e4);
		buf[7] = page->perf_vs_duration_guideline << 4;
		buf += 8;
		for (i = 0; i < CDL_MAX_DESC; i++, buf += 32, desc++) {
			buf[0] = desc->cdltunit & 0x0f;
			cdl_sg_set_int16(&buf[2], desc->max_inactive_time);
			cdl_sg_set_int16(&buf[4], desc->max_active_time);
			buf[6] = ((desc->max_inactive_policy & 0x0f) << 4) |
				(desc->max_active_policy & 0x0f);
			cdl_sg_set_int16(&buf[10], desc->duration);
			buf[14] = desc->duration_policy & 0x0f;
		}
	}

	cmd.cdb[0] = 0x55; /* MODE SELECT 10 */
	cmd.cdb[1] = 0x10; /* PF = 1, RTD = 0, SP = 0 */
	if (dev->flags & CDL_USE_MS_SP)
		cmd.cdb[1] |= 0x01; /* SP = 1 */
	cdl_sg_set_int16(&cmd.cdb[7], bufsz);
	ret = cdl_exec_cmd(dev, &cmd);
	if (ret) {
		cdl_dev_err(dev, "MODE SELECT 10 failed\n");
		return ret;
	}

	return 0;
}

