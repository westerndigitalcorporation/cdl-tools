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
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

/*
 * Status codes.
 */
#define CDL_SG_CHECK_CONDITION	0x02

/*
 * Host status codes.
 */
#define CDL_SG_DID_OK		0x00 /* No error */
#define CDL_SG_DID_NO_CONNECT	0x01 /* Couldn't connect before timeout period */
#define CDL_SG_DID_BUS_BUSY	0x02 /* BUS stayed busy through time out period */
#define CDL_SG_DID_TIME_OUT	0x03 /* Timed out for other reason */
#define CDL_SG_DID_BAD_TARGET	0x04 /* Bad target, device not responding? */
#define CDL_SG_DID_ABORT	0x05 /* Told to abort for some other reason. */
#define CDL_SG_DID_PARITY	0x06 /* Parity error. */
#define CDL_SG_DID_ERROR	0x07 /* Internal error detected in the host adapter. */
#define CDL_SG_DID_RESET	0x08 /* The SCSI bus (or this device) has been reset. */
#define CDL_SG_DID_BAD_INTR	0x09 /* Got an unexpected interrupt */
#define CDL_SG_DID_PASSTHROUGH	0x0a /* Forced command past mid-layer. */
#define CDL_SG_DID_SOFT_ERROR	0x0b /* The low level driver wants a retry. */

static const char *cdl_host_status[] =
{
	"DID_OK",
	"DID_NO_CONNECT",
	"DID_BUS_BUSY",
	"DID_TIME_OUT",
	"DID_BAD_TARGET",
	"DID_ABORT",
	"DID_PARITY",
	"DID_ERROR",
	"DID_RESET",
	"DID_BAD_INTR",
	"DID_PASSTHROUGH",
	"DID_SOFT_ERROR"
};

static const char *cdl_host_status_str(uint8_t status)
{
	if (status <= CDL_SG_DID_SOFT_ERROR)
		return cdl_host_status[status];
	return "???";
}

/*
 * Driver status codes.
 */
#define CDL_SG_DRIVER_OK		0x00
#define CDL_SG_DRIVER_BUSY		0x01
#define CDL_SG_DRIVER_SOFT		0x02
#define CDL_SG_DRIVER_MEDIA		0x03
#define CDL_SG_DRIVER_ERROR		0x04
#define CDL_SG_DRIVER_INVALID		0x05
#define CDL_SG_DRIVER_TIMEOUT		0x06
#define CDL_SG_DRIVER_HARD		0x07
#define CDL_SG_DRIVER_SENSE		0x08
#define CDL_SG_DRIVER_STATUS_MASK	0x0f

/*
 * Driver status code flags ('or'ed with code)
 */
#define CDL_SG_DRIVER_SUGGEST_RETRY	0x10
#define CDL_SG_DRIVER_SUGGEST_ABORT	0x20
#define CDL_SG_DRIVER_SUGGEST_REMAP	0x30
#define CDL_SG_DRIVER_SUGGEST_DIE	0x40
#define CDL_SG_DRIVER_SUGGEST_SENSE	0x80
#define CDL_SG_DRIVER_FLAGS_MASK	0xf0

#define cdl_cmd_driver_status(cmd)	((cmd)->io_hdr.driver_status & \
					 CDL_SG_DRIVER_STATUS_MASK)
#define cdl_cmd_driver_flags(cmd)	((cmd)->io_hdr.driver_status &	\
					 CDL_SG_DRIVER_FLAGS_MASK)

static const char *cdl_driver_status[] =
{
	"DRIVER_OK",
	"DRIVER_BUSY",
	"DRIVER_SOFT",
	"DRIVER_MEDIA",
	"DRIVER_ERROR",
	"DRIVER_INVALID",
	"DRIVER_TIMEOUT",
	"DRIVER_INVALID",
	"DRIVER_TIMEOUT",
	"DRIVER_HARD",
	"DRIVER_SENSE",
};

static const char *cdl_driver_status_str(uint8_t status)
{
	if (status <= CDL_SG_DID_SOFT_ERROR)
		return cdl_driver_status[status];
	return "???";
}

/*
 * Set bytes in a SCSI command cdb or buffer.
 */
void cdl_sg_set_be16(uint8_t *buf, uint16_t val)
{
	uint16_t *v = (uint16_t *)buf;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	*v = __builtin_bswap16(val);
#else
	*v = val;
#endif
}

void cdl_sg_set_be32(uint8_t *buf, uint32_t val)
{
	uint32_t *v = (uint32_t *)buf;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	*v = __builtin_bswap32(val);
#else
	*v = val;
#endif
}

void cdl_sg_set_be64(uint8_t *buf, uint64_t val)
{
	uint64_t *v = (uint64_t *)buf;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	*v = __builtin_bswap64(val);
#else
	*v = val;
#endif
}

void cdl_sg_set_le16(uint8_t *buf, uint16_t val)
{
	uint16_t *v = (uint16_t *)buf;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	*v = val;
#else
	*v = __builtin_bswap16(val);
#endif
}

void cdl_sg_set_le32(uint8_t *buf, uint32_t val)
{
	uint32_t *v = (uint32_t *)buf;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	*v = val;
#else
	*v = __builtin_bswap32(val);
#endif
}

void cdl_sg_set_le64(uint8_t *buf, uint64_t val)
{
	uint64_t *v = (uint64_t *)buf;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	*v = val;
#else
	*v = __builtin_bswap64(val);
#endif
}

/*
 * Get bytes from a SCSI command cdb or buffer.
 */
uint16_t cdl_sg_get_be16(uint8_t *buf)
{
	uint16_t val = *((uint16_t *)buf);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	return __builtin_bswap16(val);
#else
	return val;
#endif
}

uint32_t cdl_sg_get_be32(uint8_t *buf)
{
	uint32_t val = *((uint32_t *)buf);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	return __builtin_bswap32(val);
#else
	return val;
#endif
}

uint64_t cdl_sg_get_be64(uint8_t *buf)
{
	uint64_t val = *((uint64_t *)buf);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	return __builtin_bswap64(val);
#else
	return val;
#endif
}

uint16_t cdl_sg_get_le16(uint8_t *buf)
{
	uint16_t val = *((uint16_t *)buf);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	return val;
#else
	return __builtin_bswap16(val);
#endif
}

uint32_t cdl_sg_get_le32(uint8_t *buf)
{
	uint32_t val = *((uint32_t *)buf);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	return val;
#else
	return __builtin_bswap32(val);
#endif
}

uint64_t cdl_sg_get_le64(uint8_t *buf)
{
	uint64_t val = *((uint64_t *)buf);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	return val;
#else
	return __builtin_bswap64(val);
#endif
}

/*
 * Get a string from a SCSI command output buffer.
 */
void cdl_sg_get_str(char *dst, uint8_t *buf, int len)
{
	char *str = (char *) buf;
	int i;

	for (i = len - 1; i >= 0; i--) {
	       if (isalnum(str[i]))
		       break;
	}

	if (i >= 0)
		memcpy(dst, str, i + 1);
}

/*
 * Print an array of bytes.
 */
static void cdl_print_bytes(uint8_t *buf, unsigned int len)
{
	unsigned int l = 0, i;

	printf("  +----------+-------------------------------------------------+\n");
	printf("  |  OFFSET  | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F |\n");
	printf("  +----------+-------------------------------------------------+\n");

	while (l < len) {
		printf("  | %08x |", l);
		for (i = 0; i < 16; i++, l++) {
			if (l < len)
				printf(" %02x", (unsigned int)buf[l]);
			else
				printf("   ");
		}
		printf(" |\n");
	}

	printf("  +----------+-------------------------------------------------+\n");
}

static void cdl_print_cmd(struct cdl_sg_cmd *cmd, bool cdb, bool buffer)
{
	if (cdb) {
		printf("===========\n");
		printf("Command CDB (%d B):\n", cmd->io_hdr.cmd_len);
		cdl_print_bytes(cmd->cdb, cmd->io_hdr.cmd_len);
	}

	if (buffer && cmd->bufsz) {
		printf("Command buffer (%zu B):\n", cmd->bufsz);
		cdl_print_bytes(cmd->buf, cmd->bufsz);
	}
}

void cdl_init_cmd(struct cdl_sg_cmd *cmd, int cdb_len,
		  int direction, size_t bufsz)
{
	memset(cmd, 0, sizeof(struct cdl_sg_cmd));

	assert(bufsz <= CDL_SG_BUF_MAX_SIZE);

	/* Setup SGIO header */
	cmd->io_hdr.interface_id = 'S';
	cmd->io_hdr.timeout = 30000;
	cmd->io_hdr.flags = 0x10; /* At tail */
	if (cdb_len) {
		assert(cdb_len <= CDL_SG_CDB_MAX_SIZE);
		cmd->io_hdr.cmd_len = cdb_len;
	} else {
		cmd->io_hdr.cmd_len = CDL_SG_CDB_DEFAULT_SIZE;
	}
	cmd->io_hdr.cmdp = cmd->cdb;

	cmd->io_hdr.dxfer_direction = direction;

	cmd->bufsz = bufsz;
	cmd->io_hdr.dxferp = cmd->buf;
        cmd->io_hdr.dxfer_len = bufsz;
	cmd->io_hdr.mx_sb_len = CDL_SG_SENSE_MAX_LENGTH;
	cmd->io_hdr.sbp = cmd->sense_buf;
}

/*
 * Get comamnd ASC/ASCQ.
 */
static void cdl_sg_get_sense(struct cdl_sg_cmd *cmd)
{
	unsigned int sense_buf_len = cmd->io_hdr.sb_len_wr;
	uint8_t *sense_buf = cmd->sense_buf;

	if (sense_buf_len < 4) {
		cmd->sense_key = 0;
		cmd->asc_ascq = 0;
		return;
	}

	if ((sense_buf[0] & 0x7F) == 0x72 ||
	    (sense_buf[0] & 0x7F) == 0x73) {
		/* store sense key, ASC/ASCQ */
		cmd->sense_key = sense_buf[1] & 0x0F;
		cmd->asc_ascq = ((int)sense_buf[2] << 8) | (int)sense_buf[3];
		return;
	}

	if (sense_buf_len < 14) {
		cmd->sense_key = 0;
		cmd->asc_ascq = 0;
		return;
	}

	if ((sense_buf[0] & 0x7F) == 0x70 ||
	    (sense_buf[0] & 0x7F) == 0x71) {
		/* store sense key, ASC/ASCQ */
		cmd->sense_key = sense_buf[2] & 0x0F;
		cmd->asc_ascq = ((int)sense_buf[12] << 8) | (int)sense_buf[13];
	}
}

int cdl_exec_cmd(struct cdl_dev *dev, struct cdl_sg_cmd *cmd)
{
	int ret;

	if (cdl_verbose(dev))
		cdl_print_cmd(cmd, true,
			      cmd->io_hdr.dxfer_direction == SG_DXFER_TO_DEV);

	/* Send the SG_IO command */
	ret = ioctl(dev->fd, SG_IO, &cmd->io_hdr);
	if (ret != 0) {
		ret = -errno;
		cdl_dev_err(dev, "SG_IO ioctl failed %d (%s)\n",
			    errno, strerror(errno));
		return ret;
	}

	if (cmd->io_hdr.status ||
	    cmd->io_hdr.host_status != CDL_SG_DID_OK ||
	    (cdl_cmd_driver_status(cmd) &&
	     (cdl_cmd_driver_status(cmd) != CDL_SG_DRIVER_SENSE))) {
		if (cmd->io_hdr.host_status == CDL_SG_DID_TIME_OUT) {
			cdl_dev_err(dev, "SCSI command failed (timeout)\n");
			return -ETIMEDOUT;
		}

		if (cdl_verbose(dev))
			cdl_dev_err(dev,
				"SCSI command failed host %s, driver %s\n",
				cdl_host_status_str(cmd->io_hdr.status),
				cdl_driver_status_str(cdl_cmd_driver_status(cmd)));

		cdl_sg_get_sense(cmd);

		if (cdl_verbose(dev))
			cdl_dev_err(dev,
				"SCSI command sense key 0x%02x, asc/ascq 0x%04x\n",
				cmd->sense_key, cmd->asc_ascq);

		return -EIO;
	}

	if (cmd->io_hdr.resid)
		cmd->bufsz -= cmd->io_hdr.resid;

	if (cdl_verbose(dev))
		cdl_print_cmd(cmd, false,
			      cmd->io_hdr.dxfer_direction == SG_DXFER_FROM_DEV);

	return 0;
}

/*
 * Get a device information.
 */
static int cdl_get_dev_info(struct cdl_dev *dev)
{
	struct cdl_sg_cmd cmd;
	uint64_t capacity, timeout;
	uint32_t lba_size;
	int ret;

	/* Get device model, vendor and version (INQUIRY) */
	cdl_init_cmd(&cmd, 16, SG_DXFER_FROM_DEV, 64);
	cmd.cdb[0] = 0x12; /* INQUIRY */
	cdl_sg_set_be16(&cmd.cdb[3], 64);

	ret = cdl_exec_cmd(dev, &cmd);
	if (ret) {
		cdl_dev_err(dev, "INQUIRY failed\n");
		return -1;
	}

	cdl_sg_get_str(dev->vendor, &cmd.buf[8], CDL_VENDOR_LEN - 1);
	cdl_sg_get_str(dev->id, &cmd.buf[16], CDL_ID_LEN - 1);
	cdl_sg_get_str(dev->rev, &cmd.buf[32], CDL_REV_LEN - 1);

	/* Get capacity (READ CAPACITY 16) */
	cdl_init_cmd(&cmd, 16, SG_DXFER_FROM_DEV, 32);
	cmd.cdb[0] = 0x9e;
	cmd.cdb[1] = 0x10;
	cdl_sg_set_be32(&cmd.cdb[10], 32);

	ret = cdl_exec_cmd(dev, &cmd);
	if (ret) {
		cdl_dev_err(dev, "READ CAPACITY failed\n");
		return -1;
	}

	capacity = cdl_sg_get_be64(&cmd.buf[0]) + 1;
	lba_size = cdl_sg_get_be32(&cmd.buf[8]);
	dev->capacity = (capacity * lba_size) >> 9;

	/* Get the device command timeout */
	timeout = cdl_sysfs_get_ulong_attr(dev, "/sys/block/%s/device/timeout",
					   dev->name);
	dev->cmd_timeout = timeout * 1000000000ULL;

	return 0;
}

/*
 * Initialize the device CDL handling.
 */
static int cdl_dev_init(struct cdl_dev *dev)
{
	cdl_scsi_get_ata_information(dev);
	if (cdl_dev_is_ata(dev)) {
		/*
		 * If we are dealing with a device whose SAT is provided by the
		 * kernel libata, force the use of ATA PASSTHROUGH commands,
		 * always, as writing CDL descriptor pages is not translated by
		 * libata.
		 */
		if (strcmp(dev->sat_vendor, "linux") == 0 &&
		    strcmp(dev->sat_product, "libata") == 0)
			dev->flags |= CDL_USE_ATA;
	} else {
		dev->flags &= ~CDL_USE_ATA;
	}

	/*
	 * If we detected an ATA device handled by the kernel libata, or if the
	 * user explicitely requested ATA command use, use ATA passthrough
	 * commands to bypass the SAT implementation (kernel or HBA) used for
	 * the device.
	 */
	if (dev->flags & CDL_USE_ATA)
		return cdl_ata_init(dev);

	return cdl_scsi_init(dev);
}

/*
 * Open a device.
 */
int cdl_open_dev(struct cdl_dev *dev, mode_t mode)
{
	struct stat st;
	int ret = 0;

	dev->name = basename(dev->path);

	/* Check that this is a block device */
	if (stat(dev->path, &st) < 0) {
		fprintf(stderr,
			"Get %s stat failed %d (%s)\n",
			dev->path,
			errno, strerror(errno));
		return -1;
	}

	if (!S_ISBLK(st.st_mode) && !S_ISCHR(st.st_mode)) {
		fprintf(stderr,
			"Invalid device file %s\n",
			dev->path);
		return -1;
	}

	/* Open device */
	dev->fd = open(dev->path, mode | O_EXCL);
	if (dev->fd < 0) {
		fprintf(stderr,
			"Open %s failed %d (%s)\n",
			dev->path,
			errno, strerror(errno));
		return -1;
	}

	ret = cdl_get_dev_info(dev);
	if (ret)
		goto err;

	ret = cdl_dev_init(dev);
	if (ret)
		goto err;

	return 0;

err:
	cdl_close_dev(dev);
	return ret;
}

/*
 * Close an open device.
 */
void cdl_close_dev(struct cdl_dev *dev)
{
	int i;

	if (dev->fd < 0)
		return;

	for (i = 0; i < CDL_MAX_PAGES; i++) {
		free(dev->cdl_pages[i].msbuf);
		dev->cdl_pages[i].msbuf = NULL;
	}

	free(dev->ata_cdl_log);
	dev->ata_cdl_log = NULL;

	close(dev->fd);
	dev->fd = -1;
}

/*
 * Revalidate a device: scsi device rescan does not trigger a revalidate in
 * libata. So for ATA devices managed with libata, always force a separate
 * ATA revalidate.
 */
void cdl_revalidate_dev(struct cdl_dev *dev)
{
	if (cdl_dev_use_ata(dev))
		cdl_ata_revalidate(dev);

	cdl_scsi_revalidate(dev);
}
