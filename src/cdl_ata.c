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
#include <dirent.h>

/*
 * Read a log page.
 */
static int cdl_ata_read_log(struct cdl_dev *dev, uint8_t log,
			    uint16_t page, bool initialize,
			    struct cdl_sg_cmd *cmd, size_t bufsz)
{
	/*
	 * READ LOG DMA EXT in ATA 16 passthrough command.
	 * +=============================================================+
	 * |  Bit|  7  |  6  |   5   |   4   |   3   |   2   |  1  |  0  |
	 * |Byte |     |     |       |       |       |       |     |     |
	 * |=====+===================+===================================|
	 * | 0   |              Operation Code (85h)                     |
	 * |-----+-------------------------------------------------------|
	 * | 1   |  Multiple count   |            Protocol         | ext |
	 * |-----+-------------------------------------------------------|
	 * | 2   |  off_line |ck_cond| t_type| t_dir |byt_blk| t_length  |
	 * |-----+-------------------------------------------------------|
	 * | 3   |                 features (15:8)                       |
	 * |-----+-------------------------------------------------------|
	 * | 4   |                 features (7:0)                        |
	 * |-----+-------------------------------------------------------|
	 * | 5   |                 count (15:8)                          |
	 * |-----+-------------------------------------------------------|
	 * | 6   |                 count (7:0)                           |
	 * |-----+-------------------------------------------------------|
	 * | 7   |                 LBA (31:24 (15:8 if ext == 0)         |
	 * |-----+-------------------------------------------------------|
	 * | 8   |                 LBA (7:0)                             |
	 * |-----+-------------------------------------------------------|
	 * | 9   |                 LBA (39:32)                           |
	 * |-----+-------------------------------------------------------|
	 * | 10  |                 LBA (15:8)                            |
	 * |-----+-------------------------------------------------------|
	 * | 11  |                 LBA (47:40)                           |
	 * |-----+-------------------------------------------------------|
	 * | 12  |                 LBA (23:16)                           |
	 * |-----+-------------------------------------------------------|
	 * | 13  |                 Device                                |
	 * |-----+-------------------------------------------------------|
	 * | 14  |                 Command (0x47)                        |
	 * |-----+-------------------------------------------------------|
	 * | 15  |                 Control                               |
	 * +=============================================================+
	 */
	cdl_init_cmd(cmd, 16, SG_DXFER_FROM_DEV, bufsz);
	cmd->cdb[0] = 0x85; /* ATA 16 */
	/* DMA protocol, ext=1 */
	cmd->cdb[1] = (0x6 << 1) | 0x01;
	/* off_line=0, ck_cond=0, t_type=0, t_dir=1, byt_blk=1, t_length=10 */
	cmd->cdb[2] = 0x0e;
	if (initialize)
		cmd->cdb[4] |= 0x1;
	cdl_sg_set_be16(&cmd->cdb[5], bufsz / 512);
	cmd->cdb[8] = log;
	cdl_sg_set_be16(&cmd->cdb[9], page);
	cmd->cdb[14] = 0x47; /* READ LOG DMA EXT */

	/* Execute the command */
	return cdl_exec_cmd(dev, cmd);
}

/*
 * Write a log page.
 */
static int cdl_ata_write_log(struct cdl_dev *dev, uint8_t log,
			     uint16_t page, uint8_t *buf, size_t bufsz)
{
	struct cdl_sg_cmd cmd;

	/*
	 * WRITE LOG DMA EXT in ATA 16 passthrough command.
	 * +=============================================================+
	 * |  Bit|  7  |  6  |   5   |   4   |   3   |   2   |  1  |  0  |
	 * |Byte |     |     |       |       |       |       |     |     |
	 * |=====+===================+===================================|
	 * | 0   |              Operation Code (85h)                     |
	 * |-----+-------------------------------------------------------|
	 * | 1   |  Multiple count   |            Protocol         | ext |
	 * |-----+-------------------------------------------------------|
	 * | 2   |  off_line |ck_cond| t_type| t_dir |byt_blk| t_length  |
	 * |-----+-------------------------------------------------------|
	 * | 3   |                 features (15:8)                       |
	 * |-----+-------------------------------------------------------|
	 * | 4   |                 features (7:0)                        |
	 * |-----+-------------------------------------------------------|
	 * | 5   |                 count (15:8)                          |
	 * |-----+-------------------------------------------------------|
	 * | 6   |                 count (7:0)                           |
	 * |-----+-------------------------------------------------------|
	 * | 7   |                 LBA (31:24 (15:8 if ext == 0)         |
	 * |-----+-------------------------------------------------------|
	 * | 8   |                 LBA (7:0)                             |
	 * |-----+-------------------------------------------------------|
	 * | 9   |                 LBA (39:32)                           |
	 * |-----+-------------------------------------------------------|
	 * | 10  |                 LBA (15:8)                            |
	 * |-----+-------------------------------------------------------|
	 * | 11  |                 LBA (47:40)                           |
	 * |-----+-------------------------------------------------------|
	 * | 12  |                 LBA (23:16)                           |
	 * |-----+-------------------------------------------------------|
	 * | 13  |                 Device                                |
	 * |-----+-------------------------------------------------------|
	 * | 14  |                 Command (0x57)                        |
	 * |-----+-------------------------------------------------------|
	 * | 15  |                 Control                               |
	 * +=============================================================+
	 */
	cdl_init_cmd(&cmd, 16, SG_DXFER_TO_DEV, bufsz);
	memcpy(cmd.buf, buf, bufsz);

	cmd.cdb[0] = 0x85; /* ATA 16 */
	/* DMA protocol, ext=1 */
	cmd.cdb[1] = (0x6 << 1) | 0x01;
	/* off_line=0, ck_cond=0, t_type=0, t_dir=1, byt_blk=1, t_length=10 */
	cmd.cdb[2] = 0x0e;
	cdl_sg_set_be16(&cmd.cdb[5], bufsz / 512);
	cmd.cdb[8] = log;
	cdl_sg_set_be16(&cmd.cdb[9], page);
	cmd.cdb[14] = 0x57; /* WRITE LOG DMA EXT */

	/* Execute the command */
	return cdl_exec_cmd(dev, &cmd);
}

/*
 * Return the number of pages for @log, if it is supported, 0, if @log
 * is not supported, and a negative error code in case of error.
 */
int cdl_ata_log_nr_pages(struct cdl_dev *dev, uint8_t log)
{
	struct cdl_sg_cmd cmd;
	int ret;

	/* Read general purpose log directory */
	ret = cdl_ata_read_log(dev, 0x00, 0x00, false, &cmd, 512);
	if (ret) {
		cdl_dev_err(dev,
			    "Read general purpose log directory failed\n");
		return ret;
	}

	return cdl_sg_get_le16(&cmd.buf[log * 2]);
}

/*
 * Issue a SET FEATURES comamnd.
 */
static int cdl_ata_set_features(struct cdl_dev *dev, uint8_t feature,
				uint16_t val)
{
	struct cdl_sg_cmd cmd;

	/*
	 * SET FEATURES in ATA 16 passthrough command.
	 * +=============================================================+
	 * |  Bit|  7  |  6  |   5   |   4   |   3   |   2   |  1  |  0  |
	 * |Byte |     |     |       |       |       |       |     |     |
	 * |=====+===================+===================================|
	 * | 0   |              Operation Code (85h)                     |
	 * |-----+-------------------------------------------------------|
	 * | 1   |  Multiple count   |            Protocol         | ext |
	 * |-----+-------------------------------------------------------|
	 * | 2   |  off_line |ck_cond| t_type| t_dir |byt_blk| t_length  |
	 * |-----+-------------------------------------------------------|
	 * | 3   |                 features (15:8)                       |
	 * |-----+-------------------------------------------------------|
	 * | 4   |                 features (7:0)                        |
	 * |-----+-------------------------------------------------------|
	 * | 5   |                 count (15:8)                          |
	 * |-----+-------------------------------------------------------|
	 * | 6   |                 count (7:0)                           |
	 * |-----+-------------------------------------------------------|
	 * | 7   |                 LBA (31:24 (15:8 if ext == 0)         |
	 * |-----+-------------------------------------------------------|
	 * | 8   |                 LBA (7:0)                             |
	 * |-----+-------------------------------------------------------|
	 * | 9   |                 LBA (39:32)                           |
	 * |-----+-------------------------------------------------------|
	 * | 10  |                 LBA (15:8)                            |
	 * |-----+-------------------------------------------------------|
	 * | 11  |                 LBA (47:40)                           |
	 * |-----+-------------------------------------------------------|
	 * | 12  |                 LBA (23:16)                           |
	 * |-----+-------------------------------------------------------|
	 * | 13  |                 Device                                |
	 * |-----+-------------------------------------------------------|
	 * | 14  |                 Command (0xef)                        |
	 * |-----+-------------------------------------------------------|
	 * | 15  |                 Control                               |
	 * +=============================================================+
	 */
	cdl_init_cmd(&cmd, 16, SG_DXFER_NONE, 0);

	cmd.cdb[0] = 0x85; /* ATA 16 */
	/* Non-data protocol, ext=0 */
	cmd.cdb[1] = (0x3 << 1);
	cmd.cdb[4] = feature;
	cdl_sg_set_be16(&cmd.cdb[5], val);
	cmd.cdb[14] = 0xef; /* SET FEATURES */

	/* Execute the command */
	return cdl_exec_cmd(dev, &cmd);
}

int cdl_ata_get_acs_ver(struct cdl_dev *dev)
{
	struct cdl_sg_cmd cmd;
	int major_ver_num, i, ret;

	ret = cdl_ata_read_log(dev, 0x30, 0x01, false, &cmd, 512);
	if (ret) {
		cdl_dev_err(dev,
			    "Read identify device data log page failed\n");
		return ret;
	}

	/* Get the ACS version supported */
	major_ver_num = cdl_sg_get_le16(&cmd.buf[80 * 2]);
	for (i = 8; i < 14; i++) {
		if (major_ver_num & (1 << i))
			dev->acs_ver = i + 1 - 8;
	}

	if (dev->acs_ver < 1) {
		cdl_dev_err(dev, "Invalid major version number\n");
		return -1;
	}

	if (dev->acs_ver > 6) {
		cdl_dev_err(dev, "Unknown ACS major version number 0x%02x\n",
			    major_ver_num);
		return -1;
	}

	return 0;
}

static const char *acs_ver_name[] =
{
	NULL,		/* 0 */
	"ATA8-ACS",	/* 1 */
	"ACS-2",	/* 2 */
	"ACS-3",	/* 3 */
	"ACS-4",	/* 4 */
	"ACS-5",	/* 5 */
	"ACS-6",	/* 6 */
};

const char *cdl_ata_acs_ver(struct cdl_dev *dev)
{
	return acs_ver_name[dev->acs_ver];
}

int cdl_ata_get_limits(struct cdl_dev *dev, struct cdl_sg_cmd *cmd)
{
	struct cdl_sg_cmd _cmd;
	uint64_t qword;
	int ret;

	if (!cmd) {
		cmd = &_cmd;
		ret = cdl_ata_read_log(dev, 0x30, 0x03, false, cmd, 512);
		if (ret) {
			cdl_dev_err(dev,
				    "Read supported capabilities log page failed\n");
			return ret;
		}
	}

	/* Get the minimum and maximum limits */
	qword = cdl_sg_get_le64(&cmd->buf[176]);
	if (qword & (1ULL << 63))
		dev->min_limit = (qword & 0xffffffff) * 1000;
	qword = cdl_sg_get_le64(&cmd->buf[184]);
	if (qword & (1ULL << 63))
		dev->max_limit = (qword & 0xffffffff) * 1000;
	if (!dev->max_limit) {
		/*
		 * The natural maximum limit is imposed by the time unit
		 * (microseconds) and the time limit fields size (32-bits).
		 */
		dev->max_limit = (unsigned long long)UINT_MAX * 1000;
	}

	return 0;
}

int cdl_ata_get_statistics_supported(struct cdl_dev *dev)
{
	struct cdl_sg_cmd cmd;
	int i, ret;

	/* Check if device statistics log is supported */
	ret = cdl_ata_log_nr_pages(dev, 0x04);
	if (ret < 0) {
		cdl_dev_err(dev,
			    "Check for device statistics log page failed\n");
		return ret;
	}
	if (!ret)
		return 0;

	/* Get list of supported statistics */
	ret = cdl_ata_read_log(dev, 0x04, 0x00, false, &cmd, 512);
	if (ret) {
		cdl_dev_err(dev,
			    "Read supported statistics log page failed\n");
		return ret;
	}

	for (i = 0; i < cmd.buf[8]; i++) {
		if (cmd.buf[i + 9] == 0x09) {
			dev->flags |= CDL_STATISTICS_SUPPORTED;
			break;
		}
	}

	return 0;
}

/*
 * Initialize handling of ATA device.
 */
int cdl_ata_init(struct cdl_dev *dev)
{
	struct cdl_sg_cmd cmd;
	uint64_t qword;
	int ret;

	/* This is an ATA device */
	dev->flags |= CDL_ATA;

	/* Get the ACS version supported */
	ret = cdl_ata_get_acs_ver(dev);
	if (ret)
		return ret;

	if (dev->acs_ver < 5) {
		cdl_dev_info(dev, "ACS version lower than ACS-5\n");
		return -1;
	}

	/* Check CDL features bits using the supported capabilities log page */
	ret = cdl_ata_read_log(dev, 0x30, 0x03, false, &cmd, 512);
	if (ret) {
		cdl_dev_err(dev,
			    "Read supported capabilities log page failed\n");
		return ret;
	}

	qword = cdl_sg_get_le64(&cmd.buf[168]);
	if (qword & (1ULL << 63)) {
		/* QWord content is valid: check CDL feature */
		if (qword & (1 << 0))
			dev->flags |= CDL_DEV_SUPPORTED;
		/* Check CDL guideline feature */
		if (qword & (1 << 1))
			dev->flags |= CDL_GUIDELINE_DEV_SUPPORTED;
		/* Check CDL high-priority enhancement feature */
		if (qword & (1 << 2))
			dev->flags |= CDL_HIGHPRI_DEV_SUPPORTED;
	}

	if (!(dev->flags & CDL_DEV_SUPPORTED))
		return 0;

	/* Get the minimum and maximum limits */
	ret = cdl_ata_get_limits(dev, &cmd);
	if (ret)
		return ret;

	/* Check if CDL statistics is supported */
	ret = cdl_ata_get_statistics_supported(dev);
	if (ret)
		return ret;

	/* Set the CDL page type used for a command */
	dev->cmd_cdlp[CDL_READ_16] = CDLP_T2A;
	dev->cmd_cdlp[CDL_WRITE_16] = CDLP_T2B;
	dev->cmd_cdlp[CDL_READ_32] = CDLP_NONE;
	dev->cmd_cdlp[CDL_WRITE_32] = CDLP_NONE;

	/* Check CDL current settings */
	ret = cdl_ata_read_log(dev, 0x30, 0x04, false, &cmd, 512);
	if (ret) {
		cdl_dev_err(dev,
			    "Read current settings log page failed\n");
		return ret;
	}

	qword = cdl_sg_get_le64(&cmd.buf[8]);
	if (qword & (1ULL << 21))
		dev->flags |= CDL_DEV_ENABLED;
	if (qword & (1ULL << 22))
		dev->flags |= CDL_HIGHPRI_DEV_ENABLED;

	return 0;
}

static uint16_t cdl_ata_a2s_limit(uint8_t *buf)
{
	uint32_t limit = cdl_sg_get_le32(buf);

	/* SCSI mode page limits are 2-bytes only, so take care of overflows */
	if (limit / 10000 > 65535)
		return 65535;
	return limit / 10000;
}

/*
 * Read the device CDL descriptor log.
 */
static int cdl_ata_read_cdl_log(struct cdl_dev *dev)
{
	struct cdl_sg_cmd cmd;
	int ret;

	/* Command duration limits log */
	ret = cdl_ata_read_log(dev, 0x18, 0, false, &cmd, CDL_ATA_LOG_SIZE);
	if (ret) {
		cdl_dev_err(dev,
			    "Read command duration limits log page failed\n");
		return ret;
	}

	/* Save the log */
	memcpy(dev->ata_cdl_log, cmd.buf, CDL_ATA_LOG_SIZE);

	return 0;
}

/*
 * Read a CDL page from the device.
 */
int cdl_ata_read_page(struct cdl_dev *dev, enum cdl_p cdlp,
		      struct cdl_page *page)
{
	struct cdl_desc *desc = &page->descs[0];
	uint8_t *buf = dev->ata_cdl_log;
	uint32_t policy;
	int i, ret;

	/* Command duration limits log */
	ret = cdl_ata_read_cdl_log(dev);
	if (ret)
		return ret;

	/* T2A and T2B limits page */
	page->cdlp = cdlp;
	if (cdlp == CDLP_T2A) {
		/* Read descriptors */
		page->rw = CDL_READ;
		page->perf_vs_duration_guideline = buf[0] & 0x0f;
		buf += 64;
	} else {
		/* Write descriptors */
		page->rw = CDL_WRITE;
		buf += 288;
	}

	for (i = 0; i < CDL_MAX_DESC; i++, buf += 32, desc++) {
		policy = cdl_sg_get_le32(buf);
		desc->max_inactive_policy = (policy >> 8) & 0x0f;
		desc->max_active_policy = (policy >> 4) & 0x0f;
		desc->duration_policy = policy & 0x0f;
		desc->max_active_time = cdl_ata_a2s_limit(&buf[4]);
		desc->max_inactive_time = cdl_ata_a2s_limit(&buf[8]);
		desc->duration = cdl_ata_a2s_limit(&buf[16]);

		/*
		 * SATL defines t2cdlunits as fixed to 0xa, even for an empty
		 * descriptor, so do the same here.
		 */
		desc->cdltunit = 0x0a; /* 10ms */

		if (cdl_dev_statistics_supported(dev)) {
			/* Save the satistics configuration (selectors) */
			if (cdlp == CDLP_T2A) {
				dev->cdl_stats.ata.reads_a[i].selector = buf[12];
				dev->cdl_stats.ata.reads_b[i].selector = buf[13];
			} else {
				dev->cdl_stats.ata.writes_a[i].selector = buf[12];
				dev->cdl_stats.ata.writes_b[i].selector = buf[13];
			}
		}
	}

	return 0;
}

/*
 * Convert SCSI limit to ATA microseconds.
 */
static uint32_t cdl_ata_s2a_limit(uint16_t limit, uint8_t t2cdlunit)
{
	uint64_t l = cdl_t2time(limit, t2cdlunit) / 1000;

	/* Take care of overflows */
	if (l > (~0U))
		return ~0U;
	return l;
}

/*
 * Force device revalidation so that sysfs exposes updated command
 * duration limits.
 */
void cdl_ata_revalidate(struct cdl_dev *dev)
{
	const char *scan = "- - -";
	char path[PATH_MAX];
	char host[32], *h;
	struct dirent *dirent;
	FILE *f = NULL;
	DIR *d;

	sprintf(path, "/sys/block/%s/device/scsi_device", dev->name);
	d = opendir(path);
	if (!d) {
		cdl_dev_err(dev, "Open %s failed\n", path);
		return;
	}

	while ((dirent = readdir(d))) {
		if (dirent->d_name[0] != '.')
			break;
	}
	if (!dirent) {
		cdl_dev_err(dev, "Read %s failed\n", path);
		goto close;
	}

	strcpy(host, dirent->d_name);
	h = strchr(host, ':');
	if (!h) {
		cdl_dev_err(dev, "Parse %s entry failed\n", path);
		goto close;
	}
	*h = '\0';

	sprintf(path, "/sys/class/scsi_host/host%s/scan", host);

	f = fopen(path, "w");
	if (!f) {
		cdl_dev_err(dev, "Open %s failed\n", path);
		goto close;
	}

	if (!fwrite(scan, strlen(scan), 1, f))
		cdl_dev_err(dev, "Write %s failed\n", path);

close:
	if (d)
		closedir(d);
	if (f)
		fclose(f);
}

/*
 * Write the device CDL descriptor log.
 */
static int cdl_ata_write_cdl_log(struct cdl_dev *dev)
{
	int ret;

	ret = cdl_ata_write_log(dev, 0x18, 0,
				dev->ata_cdl_log, CDL_ATA_LOG_SIZE);
	if (ret) {
		cdl_dev_err(dev,
			    "Write command duration limits log failed\n");
		return ret;
	}

	return 0;
}

/*
 * Write a CDL page to the device.
 */
int cdl_ata_write_page(struct cdl_dev *dev, struct cdl_page *page)
{
	struct cdl_desc *desc = &page->descs[0];
	uint8_t *buf = dev->ata_cdl_log;
	uint8_t cdlp = page->cdlp;
	int i, ret;

	/* T2A and T2B limits page */
	if (cdlp == CDLP_T2A) {
		/* Read descriptors */
		buf[0] = page->perf_vs_duration_guideline & 0x0f;
		buf += 64;
	} else {
		/* Write descriptors */
		buf += 288;
	}

	for (i = 0; i < CDL_MAX_DESC; i++, buf += 32, desc++) {
		cdl_sg_set_le32(&buf[0],
				(desc->max_inactive_policy & 0x0f) << 8 |
				(desc->max_active_policy & 0x0f) << 4 |
				(desc->duration_policy & 0x0f));
		cdl_sg_set_le32(&buf[4],
				cdl_ata_s2a_limit(desc->max_active_time,
						  desc->cdltunit));
		cdl_sg_set_le32(&buf[8],
				cdl_ata_s2a_limit(desc->max_inactive_time,
						  desc->cdltunit));
		cdl_sg_set_le32(&buf[16],
				cdl_ata_s2a_limit(desc->duration,
						  desc->cdltunit));
	}

	ret = cdl_ata_write_cdl_log(dev);
	if (ret)
		return ret;

	return 0;
}

/*
 * Check the device CDL enable status.
 */
int cdl_ata_check_enabled(struct cdl_dev *dev, bool enabled)
{
	struct cdl_sg_cmd cmd;
	uint64_t qword;
	int ret;

	/* Check CDL current settings */
	ret = cdl_ata_read_log(dev, 0x30, 0x04, false, &cmd, 512);
	if (ret) {
		cdl_dev_err(dev,
			    "Read current settings log page failed\n");
		return ret;
	}

	qword = cdl_sg_get_le64(&cmd.buf[8]);
	if (qword & (1ULL << 21))
		dev->flags |= CDL_DEV_ENABLED;
	else
		dev->flags &= ~CDL_DEV_ENABLED;
	if (qword & (1ULL << 22))
		dev->flags |= CDL_HIGHPRI_DEV_ENABLED;
	else
		dev->flags &= ~CDL_HIGHPRI_DEV_ENABLED;

	return 0;
}

/*
 * Enable or disable CDL or the high priority enhancement feature on the device.
 */
int cdl_ata_enable(struct cdl_dev *dev, bool enable, bool highpri)
{
	uint16_t val = 0;
	int ret;

	if (enable) {
		if (highpri)
			val = 0x10;
		else
			val = 0x01;
	}

	/*
	 * Enabling CDL always disables the high priority
	 * enhancement feature. Enabling the high priority enhancement
	 * feature will fail if the CDL feature is enabled.
	 */
	ret = cdl_ata_set_features(dev, 0xD, val);
	if (ret) {
		cdl_dev_err(dev, "Set features (%sable %s) failed\n",
			    enable ? "en" : "dis",
			    highpri ? "highpri" : "CDL");
		return ret;
	}

	if (enable) {
		if (highpri) {
			dev->flags |= CDL_HIGHPRI_DEV_ENABLED;
		} else {
			dev->flags |= CDL_DEV_ENABLED;
			dev->flags &= ~CDL_HIGHPRI_DEV_ENABLED;
		}
	} else {
		dev->flags &= ~CDL_DEV_ENABLED;
		dev->flags &= ~CDL_HIGHPRI_DEV_ENABLED;
	}

	return 0;
}

static void cdl_ata_get_stats_desc_vals(struct cdl_ata_stats_desc *desc,
					uint8_t *buf)
{
	uint64_t qword = cdl_sg_get_le64(buf);

	/* Flags may not be valid */
	if (qword & (1ULL << 63))
		desc->flags = qword >> 56;
	else
		desc->flags = 0;
	desc->val = qword & 0xFFFFFFFF;
}

/*
 * Get a device CDL statistics.
 */
static int cdl_ata_get_stats(struct cdl_dev *dev)
{
	struct cdl_sg_cmd cmd;
	int i, ofst, ret;
	uint64_t qword;

	/* Check CDL current settings */
	ret = cdl_ata_read_log(dev, 0x04, 0x09, false, &cmd, 512);
	if (ret) {
		cdl_dev_err(dev,
			    "Read CDL statistics log page failed\n");
		return ret;
	}

	/* Check the page */
	qword = cdl_sg_get_le64(&cmd.buf[0]);
	if (((qword >> 16) & 0xFF) != 0x09) {
		cdl_dev_err(dev,
			    "Invlaid CDL statistics log page number\n");
		return ret;
	}
	if ((qword & 0xFFFF) != 0x01) {
		cdl_dev_err(dev,
			    "Invlaid CDL statistics log page revision\n");
		return ret;
	}

	/* Stats A for reads */
	ofst = 16;
	for (i = 0; i < CDL_MAX_DESC; i++, ofst += 8)
		cdl_ata_get_stats_desc_vals(&dev->cdl_stats.ata.reads_a[i],
					    &cmd.buf[ofst]);

	/* Stats A for writes */
	for (i = 0; i < CDL_MAX_DESC; i++, ofst += 8)
		cdl_ata_get_stats_desc_vals(&dev->cdl_stats.ata.writes_a[i],
					    &cmd.buf[ofst]);

	/* Stats B for reads */
	for (i = 0; i < CDL_MAX_DESC; i++, ofst += 8)
		cdl_ata_get_stats_desc_vals(&dev->cdl_stats.ata.reads_b[i],
					    &cmd.buf[ofst]);

	/* Stats B for writes */
	for (i = 0; i < CDL_MAX_DESC; i++, ofst += 8)
		cdl_ata_get_stats_desc_vals(&dev->cdl_stats.ata.writes_b[i],
					    &cmd.buf[ofst]);

	return 0;
}

/*
 * Accessors for statistics flags. Note that the flags are shifted by
 * 56 bits from the original qword of the log page.
 */
#define cdl_ata_stat_supported(sdesc)	((sdesc)->flags & 0x7)
#define cdl_ata_stat_valid(sdesc)	((sdesc)->flags & 0x6)
#define cdl_ata_stat_normalized(sdesc)	((sdesc)->flags & 0x5)
#define cdl_ata_stat_dsn(sdesc)		((sdesc)->flags & 0x4)
#define cdl_ata_stat_cond_met(sdesc)	((sdesc)->flags & 0x3)
#define cdl_ata_stat_init_sup(sdesc)	((sdesc)->flags & 0x2)

static const char *stat_val_type[] =
{
	/* 0h */ "Disabled",
	/* 1h */ "Inactive time limit met",
	/* 2h */ "Active time limit met",
	/* 3h */ "Inactive and active time limit met",
	/* 4h */ "Number of commands"
		 "Unknown statistic type"
};

static const char *cdl_ata_stat_val_type(struct cdl_ata_stats_desc *sdesc)
{
	if (sdesc->selector >= 0x1 && sdesc->selector <= 0x4)
		return stat_val_type[sdesc->selector];
	return stat_val_type[5];
}

static void cdl_ata_stat_desc_show(struct cdl_dev *dev,
				   struct cdl_ata_stats_desc *sdesc,
				   const char *ab)
{
	printf("    Statistic %s: ", ab);
	fflush(stdout);

	if (!sdesc->selector) {
		printf("Disabled\n");
		return;
	}

	if (!cdl_ata_stat_supported(sdesc)) {
		printf("Not supported\n");
		return;
	}

	if (!cdl_ata_stat_valid(sdesc)) {
		printf("Not valid\n");
		return;
	}

	if (dev->flags & CDL_SHOW_RAW_VAL) {
		printf("\n");
		printf("        Selector = 0x%02x\n", sdesc->selector);
		printf("        Value = 0x%08x\n", sdesc->val);
	} else {
		printf("%s, value = %u\n",
		       cdl_ata_stat_val_type(sdesc),
		       sdesc->val);
	}
}

int cdl_ata_statistics_show(struct cdl_dev *dev, int cdlp)
{
	struct cdl_page page = {};
	struct cdl_ata_stats_desc *sdesc_a, *sdesc_b;
	int ret, i;

	/* Get the statistics configuration */
	ret = cdl_ata_read_page(dev, cdlp, &page);
	if (ret)
		return ret;

	/* Get the statistics values */
	ret = cdl_ata_get_stats(dev);
	if (ret)
		return ret;

	for (i = 0; i < CDL_MAX_DESC; i++) {
		printf("  Descriptor %d:\n", i + 1);

		if (cdlp == CDLP_T2A) {
			sdesc_a = &dev->cdl_stats.ata.reads_a[i];
			sdesc_b = &dev->cdl_stats.ata.reads_b[i];
		} else {
			sdesc_a = &dev->cdl_stats.ata.writes_a[i];
			sdesc_b = &dev->cdl_stats.ata.writes_b[i];
		}

		cdl_ata_stat_desc_show(dev, sdesc_a, "A");
		cdl_ata_stat_desc_show(dev, sdesc_b, "B");
	}

	return 0;
}

int cdl_ata_statistics_reset(struct cdl_dev *dev)
{
	struct cdl_sg_cmd cmd;
	int ret;

	/*
	 * Reset CDL statistics by reading the statistics log page with the
	 * read then initialize bit set.
	 */
	ret = cdl_ata_read_log(dev, 0x04, 0x09, true, &cmd, 512);
	if (ret) {
		cdl_dev_err(dev,
			    "Read then initialize CDL statistics log page failed\n");
		return ret;
	}

	return 0;
}

int cdl_ata_statistics_save(struct cdl_dev *dev, FILE *f)
{
	uint8_t *buf;
	int i, ret;

	/* Command duration limits log */
	ret = cdl_ata_read_cdl_log(dev);
	if (ret)
		return ret;

	/* Get read descriptors statistics selectors */
	buf = dev->ata_cdl_log + 64;
	for (i = 0; i < CDL_MAX_DESC; i++, buf += 32) {
		dev->cdl_stats.ata.reads_a[i].selector = buf[12];
		dev->cdl_stats.ata.reads_b[i].selector = buf[13];
	}

	/* Get write descriptors statistics selectors */
	buf = dev->ata_cdl_log + 288;
	for (i = 0; i < CDL_MAX_DESC; i++, buf += 32) {
		dev->cdl_stats.ata.writes_a[i].selector = buf[12];
		dev->cdl_stats.ata.writes_b[i].selector = buf[13];
	}

	/* File legend */
	fprintf(f, "# CDL statistics configuration format:\n");
	fprintf(f,
		"# selector_a and selector_b of a descriptor can be one of:\n"
		"#   - 0 : Disable statistic\n"
		"#   - 1 : Increment the statistics value if the device\n"
		"#         processes the inactive time limit policy\n"
		"#         requirements set for the descriptor\n"
		"#   - 2 : Increment the statistics value if the device\n"
		"#         processes the active time limit policy\n"
		"#         requirements set for the descriptor\n"
		"#   - 3 : Increment the statistics value if the device\n"
		"#         processes the requirements of the inactive time\n"
		"#         limit policy or of the active time limit policy\n"
		"#         set for the descriptor\n"
		"#   - 4 : Increment the statistics value if the device\n"
		"#         processes a command using this descriptor\n"
		"#         (Valid only for devices supporting at least\n"
		"#         the ACS-6 specifications)\n");
	fprintf(f, "\n");

	for (i = 0; i < CDL_MAX_DESC; i++) {
		fprintf(f, "== read descriptor: %d\n", i + 1);
		fprintf(f, "selector_a: %u\n",
			dev->cdl_stats.ata.reads_a[i].selector);
		fprintf(f, "selector_b: %u\n",
			dev->cdl_stats.ata.reads_b[i].selector);
		fprintf(f, "\n");
	}

	for (i = 0; i < CDL_MAX_DESC; i++) {
		fprintf(f, "== write descriptor: %d\n", i + 1);
		fprintf(f, "selector_a: %u\n",
			dev->cdl_stats.ata.writes_a[i].selector);
		fprintf(f, "selector_b: %u\n",
			dev->cdl_stats.ata.writes_b[i].selector);
		fprintf(f, "\n");
	}

	return 0;
}

static int cdl_ata_statistics_parse_desc(struct cdl_dev *dev, FILE *f,
					 char *line, bool read, int index)
{
	int idx, selector_a, selector_b;
	char *type, *str;

	/* Parse descriptor header */
	if (read)
		type = "read";
	else
		type = "write";

	str = cdl_get_line(f, line);
	if (!str || strncmp(str, "==", 2) != 0)
		goto err;

	str = cdl_skip_spaces(str, 2);
	if (!str || strncmp(str, type, strlen(type)) != 0)
		goto err;

	str = cdl_skip_spaces(str, strlen(type));
	if (!str || strncmp(str, "descriptor:", 11) != 0)
		goto err;

	str = cdl_skip_spaces(str, 11);
	if (!str)
		goto err;

	idx = atoi(str);
	if (idx != index)
		goto err;

	/* Parse selector_a */
	str = cdl_get_line(f, line);
	if (!str || strncmp(str, "selector_a:", 11) != 0)
		goto err;
	str = cdl_skip_spaces(str, 11);
	if (!str)
		goto err;

	selector_a = atoi(str);
	if (selector_a < 0 || selector_a > 4) {
		fprintf(stderr, "Invalid %s descriptor %d selector_a value\n",
			type, index);
		return 1;
	}

	if (read)
		dev->cdl_stats.ata.reads_a[index].selector = selector_a;
	else
		dev->cdl_stats.ata.writes_a[index].selector = selector_a;

	/* Parse selector_b */
	str = cdl_get_line(f, line);
	if (!str || strncmp(str, "selector_b:", 11) != 0)
		goto err;
	str = cdl_skip_spaces(str, 11);
	if (!str)
		goto err;

	selector_b = atoi(str);
	if (selector_b < 0 || selector_b > 4) {
		fprintf(stderr, "Invalid %s descriptor %d selector_b value\n",
			type, index);
		return 1;
	}

	if (read)
		dev->cdl_stats.ata.reads_b[index].selector = selector_b;
	else
		dev->cdl_stats.ata.writes_b[index].selector = selector_b;

	printf("  %s descriptor %d:\tselector_a = %u,\tselector_b = %u\n",
	       type, index,
	       selector_a, selector_b);

	return 0;

err:
	fprintf(stderr, "Invalid %s descriptor %d\n",
		type, index);

	return 1;
}

/*
 * Parse a CDL statistics configuration file.
 */

int cdl_ata_statistics_upload(struct cdl_dev *dev, FILE *f)
{
	char line[CDL_LINE_MAX_LEN];
	uint8_t *buf;
	int i, ret;

	/* Get the command duration limits log */
	ret = cdl_ata_read_cdl_log(dev);
	if (ret)
		return ret;

	/* Parse the file and update the CDL log with the new configuration */
	buf = dev->ata_cdl_log + 64;
	for (i = 0; i < CDL_MAX_DESC; i++, buf += 32) {
		ret = cdl_ata_statistics_parse_desc(dev, f, line, true, i + 1);
		if (ret)
			return ret;
		buf[12] = dev->cdl_stats.ata.reads_a[i].selector;
		buf[13] = dev->cdl_stats.ata.reads_b[i].selector;
	}

	buf = dev->ata_cdl_log + 288;
	for (i = 0; i < CDL_MAX_DESC; i++, buf += 32) {
		ret = cdl_ata_statistics_parse_desc(dev, f, line, false, i + 1);
		if (ret)
			return ret;
		buf[12] = dev->cdl_stats.ata.writes_a[i].selector;
		buf[13] = dev->cdl_stats.ata.writes_b[i].selector;
	}

	/* Update the CDL log on the device */
	ret = cdl_ata_write_cdl_log(dev);
	if (ret)
		return ret;

	return 0;
}
