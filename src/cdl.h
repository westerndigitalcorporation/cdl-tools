// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 */
#ifndef CDL_H
#define CDL_H

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <scsi/scsi.h>
#include <scsi/sg.h>

/*
 * Commands supporting duration limits.
 */
enum cdl_cmd {
	CDL_READ_16,
	CDL_WRITE_16,
	CDL_READ_32,
	CDL_WRITE_32,

	CDL_CMD_MAX,
};

/*
 * Command duration limits sub-pages for the control mode page 0Ah.
 */
enum cdl_p {
	CDLP_A,
	CDLP_B,
	CDLP_T2A,
	CDLP_T2B,
	CDLP_NONE,

	CDL_MAX_PAGES = CDLP_NONE,
};

struct cdl_desc {
	uint8_t		cdltunit;
	uint64_t	max_inactive_time;
	uint64_t	max_active_time;
	uint64_t	duration;
	uint8_t		max_inactive_policy;
	uint8_t		max_active_policy;
	uint8_t		duration_policy;
};

#define CDL_MAX_DESC	7

struct cdl_page {
	enum cdl_p		cdlp;
	uint8_t			perf_vs_duration_guideline;
	struct cdl_desc		descs[CDL_MAX_DESC];
	uint8_t			*msbuf;
	size_t			msbufsz;
};

/*
 * Device flags.
 */
#define CDL_VERBOSE	(1ULL << 0)

#define CDL_VENDOR_LEN	9
#define CDL_ID_LEN	17
#define CDL_REV_LEN	5

struct cdl_dev {
	/* Device file path and basename */
	char		*path;
	char		*name;

	/* Device file descriptor */
	int		fd;

	/* Device info */
	unsigned long long	flags;
	char			vendor[CDL_VENDOR_LEN];
	char			id[CDL_ID_LEN];
	char			rev[CDL_REV_LEN];
	unsigned long long	capacity;
	bool			cdl_supported;
	enum cdl_p		cmd_cdlp[CDL_CMD_MAX];
	struct cdl_page		cdl_pages[CDL_MAX_PAGES];
};

/*
 * For SG commands.
 */
#define CDL_SG_SENSE_MAX_LENGTH		64
#define CDL_SG_BUF_MAX_SIZE		4096
#define CDL_SG_CDB_MAX_SIZE		32
#define CDL_SG_CDB_DEFAULT_SIZE		16

struct cdl_sg_cmd {
	uint8_t		buf[CDL_SG_BUF_MAX_SIZE];
	size_t		bufsz;
	uint8_t		cdb[CDL_SG_CDB_MAX_SIZE];
	uint8_t		sense_buf[CDL_SG_SENSE_MAX_LENGTH];
	sg_io_hdr_t	io_hdr;
	uint8_t		sense_key;
	uint16_t	asc_ascq;
};

/*
 * Converter structure.
 */
union converter {
	uint8_t		val_buf[8];
	uint16_t	val16;
	uint32_t	val32;
	uint64_t	val64;
};

/* In cdl_dev.c */
int cdl_open_dev(struct cdl_dev *dev);
void cdl_close_dev(struct cdl_dev *dev);
void cdl_init_cmd(struct cdl_sg_cmd *cmd, int cdb_len,
		  int direction, size_t bufsz);
int cdl_exec_cmd(struct cdl_dev *dev, struct cdl_sg_cmd *cmd);
void cdl_sg_set_bytes(uint8_t *cmd, void *buf, int bytes);
void cdl_sg_get_bytes(uint8_t *val, union converter *conv, int bytes);
void cdl_sg_get_str(char *dst, uint8_t *buf, int len);

/* In cdl_utils.c */
const char *cdl_page_name(enum cdl_p cdlp);
uint8_t cdl_page_code(enum cdl_p cdlp);
int cdl_page_name2cdlp(char *page);

const char *cdl_cmd_str(enum cdl_cmd cmd);
uint8_t cdl_cmd_opcode(enum cdl_cmd cmd);
uint16_t cdl_cmd_sa(enum cdl_cmd cmd);

void cdl_print_page(struct cdl_page *page, FILE *f);
int cdl_parse_page_file(FILE *f, struct cdl_page *page);

/* In cdl.c */
int cdl_read_pages(struct cdl_dev *dev);
bool cdl_page_supported(struct cdl_dev *dev, enum cdl_p cdlp);
bool cdl_check_support(struct cdl_dev *dev);
int cdl_write_page(struct cdl_dev *dev, struct cdl_page *page);

static inline bool cdl_verbose(struct cdl_dev *dev)
{
	return dev->flags & CDL_VERBOSE;
}

#define cdl_dev_info(dev,format,args...)		\
	printf("%s: " format, (dev)->name, ##args)

#define cdl_dev_err(dev,format,args...)			\
	fprintf(stderr, "[ERROR] %s: " format, (dev)->name, ##args)

/*
 * Set a 64 bits integer in a command cdb.
 */
static inline void cdl_sg_set_int64(uint8_t *buf, uint64_t val)
{
	cdl_sg_set_bytes(buf, &val, 8);
}

/*
 * Set a 32 bits integer in a command cdb.
 */
static inline void cdl_sg_set_int32(uint8_t *buf, uint32_t val)
{
	cdl_sg_set_bytes(buf, &val, 4);
}

/*
 * Set a 16 bits integer in a command cdb.
 */
static inline void cdl_sg_set_int16(uint8_t *buf, uint16_t val)
{
	cdl_sg_set_bytes(buf, &val, 2);
}

/*
 * Get a 64 bits integer from a command output buffer.
 */
static inline uint64_t cdl_sg_get_int64(uint8_t *buf)
{
	union converter conv;

	cdl_sg_get_bytes(buf, &conv, 8);

	return conv.val64;
}

/*
 * Get a 32 bits integer from a command output buffer.
 */
static inline uint32_t cdl_sg_get_int32(uint8_t *buf)
{
	union converter conv;

	cdl_sg_get_bytes(buf, &conv, 4);

	return conv.val32;
}

/*
 * Get a 16 bits integer from a command output buffer.
 */
static inline uint16_t cdl_sg_get_int16(uint8_t *buf)
{
	union converter conv;

	cdl_sg_get_bytes(buf, &conv, 2);

	return conv.val16;
}

#endif /* CDL_H */
