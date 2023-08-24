// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Western Digital Corporation or its affiliates.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 */

#include "cdl.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/utsname.h>

/*
 * Print usage.
 */
static void cdladm_usage(void)
{
	printf("Usage:\n"
	       "  cdladm --help | -h\n"
	       "  cdladm --version\n"
	       "  cdladm <command> [options] <device>\n");
	printf("Options common to all commands:\n"
	       "  --verbose | -v       : Verbose output\n"
	       "  --force-ata | -a     : Force the use of ATA passthrough commands\n");
	printf("Commands:\n"
	       "  info            : Show device and system support information\n"
	       "  list            : List supported pages\n"
	       "  show            : Display one or all supported pages\n"
	       "  clear           : Clear one or all supported pages\n"
	       "  save            : Save one or all pages to a file\n"
	       "  upload          : Upload a page to the device\n"
	       "  enable          : Enable command duration limits\n"
	       "  disable         : Disable command duration limits\n"
	       "  enable-highpri  : Enable high priority enhancement\n"
	       "  disable-highpri : Disable high priority enhancement\n"
	       "  stats-show      : Display CDL statistics configuration and values\n"
	       "  stats-reset     : Reset to 0 all CDL statistics values\n"
	       "  stats-save      : Save CDL statistics configuration to a file\n"
	       "  stats-upload    : Upload CDL statistics configuration\n"
	       "                    to the device\n");
	printf("Command options:\n");
	printf("  --count\n"
	       "\tApply to the show command.\n"
	       "\tOmit the descriptor details and only print the number of\n"
	       "\tvalid descriptors in a page\n");
	printf("  --page <name>\n"
	       "\tApply to the show, clear and save commands.\n"
	       "\tSpecify the name of the page to show,clear or save. The\n"
	       "\tpage name tcan be: \"A\", \"B\", \"T2A\" or \"T2B\".\n");
	printf("  --file <path>\n"
	       "\tApplies to the save, upload, stats-save and stats-upload\n"
	       "\tcommands to specify the path of the page file or statistics\n"
	       "\tconfiguration file to use.\n"
	       "\tUsing this option is mandatory with the upload and\n"
	       "\tstats-upload commands.\n"
	       "\tIf this option is not specified with the save command,\n"
	       "\tthe default file name <dev name>-<page name>.cdl is used.\n"
	       "\tIf this option is not specified with the stats-save command,\n"
	       "\tthe default file name <dev name>-cdl-stats.cfg is used.\n");
	printf("  --permanent\n"
	       "\tApply to the upload command.\n"
	       "\tSpecify that the device should save the page in\n"
	       "\tnon-volatile memory in addition to updating the current\n"
	       "\tpage value.\n");
	printf("  --raw\n"
	       "\tApply to the show command.\n"
	       "\tShow the raw values of the CDL pages fields.\n");
	printf("  --force-dev\n"
	       "\tApply to the enable and disable commands for ATA devices.\n"
	       "\tForce enabling and disabling the CDL feature directly on\n"
	       "\tthe device without enabling/disabling the system.\n"
	       "\tThe use of this option is not recommended under normal\n"
	       "\tuse and is reserved for testing only.\n");
	printf("See \"man cdladm\" for more information.\n");
}

static int cdladm_list(struct cdl_dev *dev)
{
	int i, j, n;

	printf("Supported pages:\n");

	for (i = 0; i < CDL_MAX_PAGES; i++) {
		if (!cdl_page_supported(dev, i))
			continue;

		printf("    %s (", cdl_page_name(i));
		for (j = 0, n = 0; j < CDL_CMD_MAX; j++) {
			if ((int)dev->cmd_cdlp[j] != i)
				continue;
			printf("%s%s",
			       n == 0 ? "" : ", ",
			       cdl_cmd_str(j));
			n++;
		}
		printf(")\n");
	}

	return 0;
}

static int cdladm_show(struct cdl_dev *dev, char *page)
{
	int cdlp = -1, i, n;

	if (page) {
		/* Show only the specified page */
		cdlp = cdl_page_name2cdlp(page);
		if (cdlp < 0)
			return 1;
	}

	/* Show all supported pages or only the requested page */
	for (i = 0; i < CDL_MAX_PAGES; i++) {
		if (!cdl_page_supported(dev, i)) {
			if (page && i == cdlp) {
				printf("Page %s is not supported\n", page);
				return 1;
			}
			continue;
		}

		if (page && i != cdlp)
			continue;

		printf("Page %s: %s descriptors\n",
		       cdl_page_name(i),
		       dev->cdl_pages[i].rw == CDL_READ ? "read" : "write");
		n = cdl_page_show(&dev->cdl_pages[i], dev->flags);
		if (dev->flags & CDL_SHOW_COUNT) {
			if (!n)
				printf("  No valid descriptors\n");
			else
				printf("  %d valid descriptors\n", n);
		}

		if (page && i == cdlp)
			break;

	}

	return 0;
}

static int cdladm_clear(struct cdl_dev *dev, char *page_name)
{
	struct cdl_page page;
	int cdlp = -1, i, ret;

	if (page_name) {
		/* Clear only the specified page */
		cdlp = cdl_page_name2cdlp(page_name);
		if (cdlp < 0)
			return 1;
	}

	/* Clear all supported pages or only the requested page */
	for (i = 0; i < CDL_MAX_PAGES; i++) {
		if (!cdl_page_supported(dev, i)) {
			if (page_name && i == cdlp) {
				printf("Page %s is not supported\n", page_name);
				return 1;
			}
			continue;
		}

		if (page_name && i != cdlp)
			continue;

		printf("Clearing page %s: %s descriptors\n",
		       cdl_page_name(i),
		       dev->cdl_pages[i].rw == CDL_READ ? "read" : "write");
		memset(&page, 0, sizeof(page));
		page.cdlp = i;
		ret = cdl_write_page(dev, &page);
		if (ret)
			return 1;

		if (page_name && i == cdlp)
			break;
	}

	return 0;
}

static int cdladm_save_page(struct cdl_dev *dev, enum cdl_p cdlp, char *path)
{
	char *fpath;
	int ret;
	FILE *f;

	if (!path) {
		ret = asprintf(&fpath, "%s-%s.cdl",
			       dev->name, cdl_page_name(cdlp));
		if (ret < 0) {
			fprintf(stderr, "Failed to allocate file path\n");
			return 1;
		}
	} else {
		fpath = path;
	}

	f = fopen(fpath, "w");
	if (!f) {
		fprintf(stderr, "Open page %s file %s failed (%s)\n",
			cdl_page_name(cdlp), fpath, strerror(errno));
		ret = 1;
		goto out;
	}

	printf("Saving page %s to file %s\n",
	       cdl_page_name(cdlp), fpath);

	cdl_page_save(&dev->cdl_pages[cdlp], f);
	ret = 0;

	fclose(f);

out:
	if (!path)
		free(fpath);

	return ret;
}

static int cdladm_save(struct cdl_dev *dev, char *page, char *path)
{
	enum cdl_p cdlp;
	int i, ret;

	if (page) {
		cdlp = cdl_page_name2cdlp(page);
		if (cdlp < 0)
			return 1;
		if (!cdl_page_supported(dev, cdlp)) {
			fprintf(stderr, "Page %s is not supported\n", page);
			return 1;
		}
		return cdladm_save_page(dev, cdlp, path);
	}

	/* Save all supported pages */
	for (i = 0; i < CDL_MAX_PAGES; i++) {
		if (!cdl_page_supported(dev, i))
			continue;
		ret = cdladm_save_page(dev, i, NULL);
		if (ret)
			return 1;
	}

	return 0;
}

static int cdladm_upload(struct cdl_dev *dev, char *path)
{
	struct cdl_page page;
	FILE *f;
	int ret;

	if (!path) {
		fprintf(stderr, "No file specified\n");
		return 1;
	}

	/* Open the file and parse it */
	f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "Open file %s failed (%s)\n",
			path, strerror(errno));
		return 1;
	}

	printf("Parsing file %s...\n", path);
	ret = cdl_page_parse_file(f, dev, &page);
	fclose(f);
	if (ret)
		return 1;

	printf("Uploading page %s:\n",
	       cdl_page_name(page.cdlp));
	cdl_page_show(&page, false);

	ret = cdl_write_page(dev, &page);
	if (ret)
		return 1;

	return 0;
}

static int cdladm_stats_show(struct cdl_dev *dev)
{
	int i, ret;

	if (!(dev->flags & CDL_STATISTICS_SUPPORTED)) {
		fprintf(stderr, "CDL statistics is not supported\n");
		return 1;
	}

	for (i = 0; i < CDL_MAX_PAGES; i++) {
		if (!cdl_page_supported(dev, i))
			continue;

		printf("Page %s: %s descriptors\n",
		       cdl_page_name(i),
		       dev->cdl_pages[i].rw == CDL_READ ? "read" : "write");

		ret = cdl_statistics_show(dev, i);
		if (ret)
			return 1;
	}

	return 0;
}

static int cdladm_stats_reset(struct cdl_dev *dev)
{
	int ret;

	if (!(dev->flags & CDL_STATISTICS_SUPPORTED)) {
		fprintf(stderr, "CDL statistics is not supported\n");
		return 1;
	}

	printf("Reset CDL statistics\n");

	ret = cdl_statistics_reset(dev);
	if (ret) {
		fprintf(stderr, "Reset CDL statistics failed\n");
		return 1;
	}

	return 0;
}

static int cdladm_stats_save(struct cdl_dev *dev, char *path)
{
	char *fpath;
	int ret;
	FILE *f;

	if (!path) {
		ret = asprintf(&fpath, "%s-cdl-stats.cfg", dev->name);
		if (ret < 0) {
			fprintf(stderr, "Failed to allocate file path\n");
			return 1;
		}
	} else {
		fpath = path;
	}

	f = fopen(fpath, "w");
	if (!f) {
		fprintf(stderr,
			"Open statistics configuration file file %s failed (%s)\n",
			fpath, strerror(errno));
		ret = 1;
		goto out;
	}

	printf("Saving CDL statistics configuration to file %s\n",
	       fpath);

	ret = cdl_statistics_save(dev, f);
	if (ret)
		return 1;

	fclose(f);

out:
	if (!path)
		free(fpath);

	return ret;
}

static int cdladm_stats_upload(struct cdl_dev *dev, char *path)
{
	FILE *f;
	int ret;

	if (!path) {
		fprintf(stderr, "No file specified\n");
		return 1;
	}

	printf("Uploading CDL statitics configuration from file %s\n",
	       path);

	f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "Open file %s failed (%s)\n",
			path, strerror(errno));
		return 1;
	}

	ret = cdl_statistics_upload(dev, f);
	fclose(f);

	if (ret)
		return 1;

	return 0;
}

static void cdladm_get_kernel_support(struct cdl_dev *dev)
{
	bool supported, enabled = false;

	supported = cdl_sysfs_exists(dev, "/sys/block/%s/device/cdl_supported",
				     dev->name);
	if (supported) {
		dev->flags |= CDL_SYS_SUPPORTED;
		enabled = cdl_sysfs_get_ulong_attr(dev,
					"/sys/block/%s/device/cdl_enable",
					dev->name);
		if (enabled)
			dev->flags |= CDL_SYS_ENABLED;
	}
}

static void cdladm_show_kernel_support(struct cdl_dev *dev)
{
	struct utsname buf;

	if (uname(&buf) != 0) {
		fprintf(stderr, "uname failed %d (%s)\n",
			errno, strerror(errno));
		return;
	}

	printf("System:\n");
	printf("    Node name: %s\n", buf.nodename);
	printf("    Kernel: %s %s %s\n",
	       buf.sysname, buf.release, buf.version);
	printf("    Architecture: %s\n", buf.machine);
	printf("    Command duration limits: %ssupported, %s\n",
	       dev->flags & CDL_SYS_SUPPORTED ? "" : "not ",
	       dev->flags & CDL_SYS_ENABLED ? "enabled" : "disabled");
	printf("    Device %s command timeout: %llu s\n",
	       dev->name, dev->cmd_timeout / 1000000000ULL);
}

static int cdladm_enable(struct cdl_dev *dev)
{
	int ret;

	if (!(dev->flags & CDL_SYS_SUPPORTED)) {
		fprintf(stderr, "System lacks CDL support\n");
		return 1;
	}

	/* Special case (for tests only !): enable CDL on the device only */
	if (dev->flags & CDL_FORCE_DEV) {
		if (!cdl_dev_is_ata(dev)) {
			fprintf(stderr,
				"--force-dev applies to ATA devices only\n");
			return 1;
		}
		ret = cdl_ata_enable(dev, true, false);
		if (ret)
			return 1;
		return 0;
	}

	/* Enable system: this should enable the device too */
	ret = cdl_sysfs_set_attr(dev, "1", "/sys/block/%s/device/cdl_enable",
				 dev->name);
	if (ret)
		return 1;

	dev->flags |= CDL_SYS_ENABLED;
	printf("Command duration limits is enabled\n");

	cdl_check_enabled(dev, true);
	if (!(dev->flags & CDL_DEV_ENABLED))
		printf("WARNING: Command duration limits is disabled "
		       "on the device\n");

	if ((dev->flags & CDL_DEV_ENABLED) &&
	    (dev->flags & CDL_HIGHPRI_DEV_ENABLED))
		printf("WARNING: Command duration limits and high "
		       "priority enhancement are both enabled on "
		       " the device\n");

	return 0;
}

static int cdladm_disable(struct cdl_dev *dev)
{
	int ret;

	if (!(dev->flags & CDL_SYS_SUPPORTED)) {
		fprintf(stderr, "System lacks CDL support\n");
		return 1;
	}

	/* Special case (for tests only !): disable CDL on the device only */
	if (dev->flags & CDL_FORCE_DEV) {
		if (!cdl_dev_is_ata(dev)) {
			fprintf(stderr,
				"--force-dev applies to ATA devices only\n");
			return 1;
		}
		ret = cdl_ata_enable(dev, false, false);
		if (ret)
			return 1;
		return 0;
	}

	/* Enable system: this should enable the device too */
	ret = cdl_sysfs_set_attr(dev, "0",
			"/sys/block/%s/device/cdl_enable",
			dev->name);
	if (ret)
		return 1;

	dev->flags &= ~CDL_SYS_ENABLED;
	printf("Command duration limits is disabled\n");

	cdl_check_enabled(dev, false);
	if (dev->flags & CDL_DEV_ENABLED)
		printf("WARNING: Command duration limits is still enabled "
		       "on the device\n");

	return 0;
}

static int cdladm_enable_highpri(struct cdl_dev *dev)
{
	int ret;

	if (!(dev->flags & CDL_HIGHPRI_DEV_SUPPORTED)) {
		fprintf(stderr,
			"Device does not support high priority enhancement\n");
		return 1;
	}

	ret = cdl_ata_enable(dev, true, true);
	if (ret)
		return 1;

	cdl_check_enabled(dev, true);

	if (dev->flags & CDL_HIGHPRI_DEV_ENABLED) {
		printf("High priority enhancement is enabled\n");
		if (dev->flags & CDL_DEV_ENABLED)
			printf("WARNING: Command duration limits and high "
			       "priority enhancement are both enabled on "
			       " the device\n");
	} else {
		printf("WARNING: high priority enhancement is disabled "
		       "on the device\n");
	}

	return 0;
}

static int cdladm_disable_highpri(struct cdl_dev *dev)
{
	int ret;

	if (!(dev->flags & CDL_HIGHPRI_DEV_SUPPORTED)) {
		fprintf(stderr,
			"Device does not support high priority enhancement\n");
		return 1;
	}

	ret = cdl_ata_enable(dev, false, true);
	if (ret)
		return 1;

	cdl_check_enabled(dev, false);

	if (dev->flags & CDL_HIGHPRI_DEV_ENABLED)
		printf("WARNING: Command duration limits is still enabled "
		       "on the device\n");
	else
		printf("High priority enhancement is disabled\n");

	return 0;
}

/*
 * Possible command codes.
 */
enum cdladm_cmd_code {
	CDLADM_NONE,
	CDLADM_INFO,
	CDLADM_LIST,
	CDLADM_SHOW,
	CDLADM_CLEAR,
	CDLADM_SAVE,
	CDLADM_UPLOAD,
	CDLADM_ENABLE,
	CDLADM_DISABLE,
	CDLADM_ENABLE_HIGHPRI,
	CDLADM_DISABLE_HIGHPRI,
	CDLADM_STATS_SHOW,
	CDLADM_STATS_RESET,
	CDLADM_STATS_SAVE,
	CDLADM_STATS_UPLOAD,

	CDLADM_CMD_MAX,
};

/*
 * Command codes and the device open mode needed.
 */
static struct {
	const char *opt;
	enum cdladm_cmd_code code;
	mode_t mode;
} cdladm_cmd[CDLADM_CMD_MAX + 1] =
{
	{ "",			CDLADM_NONE,		0	 },
	{ "info",		CDLADM_INFO,		O_RDONLY },
	{ "list",		CDLADM_LIST,		O_RDONLY },
	{ "show",		CDLADM_SHOW,		O_RDONLY },
	{ "clear",		CDLADM_CLEAR,		O_RDWR	 },
	{ "save",		CDLADM_SAVE,		O_RDONLY },
	{ "upload",		CDLADM_UPLOAD,		O_RDWR	 },
	{ "enable",		CDLADM_ENABLE,		O_RDWR   },
	{ "disable",		CDLADM_DISABLE,		O_RDWR   },
	{ "enable-highpri",	CDLADM_ENABLE_HIGHPRI,	O_RDWR   },
	{ "disable-highpri",	CDLADM_DISABLE_HIGHPRI,	O_RDWR   },
	{ "stats-show",		CDLADM_STATS_SHOW,	O_RDWR   },
	{ "stats-reset",	CDLADM_STATS_RESET,	O_RDWR   },
	{ "stats-save",		CDLADM_STATS_SAVE,	O_RDWR   },
	{ "stats-upload",	CDLADM_STATS_UPLOAD,	O_RDWR   },
	{ NULL,			CDLADM_CMD_MAX,		0        }
};

static int cdladm_get_command(char *opt)
{
	int i;

	for (i = 1; i < CDLADM_CMD_MAX; i++) {
		if (strcmp(opt, cdladm_cmd[i].opt) == 0)
			return cdladm_cmd[i].code;
	}

	return CDLADM_CMD_MAX;
}

/*
 * Main function.
 */
int main(int argc, char **argv)
{
	struct cdl_dev dev;
	char *page = NULL;
	char *path = NULL;
	int command = CDLADM_NONE;
	int i, ret;

	/* Initialize */
	memset(&dev, 0, sizeof(dev));
	dev.fd = -1;
	for (i = 0; i < CDL_CMD_MAX; i++)
		dev.cmd_cdlp[i] = CDLP_NONE;
	for (i = 0; i < CDL_MAX_PAGES; i++)
		dev.cdl_pages[i].cdlp = CDLP_NONE;

	if (argc == 1) {
		cdladm_usage();
		return 0;
	}

	/* Generic options */
	if (strcmp(argv[1], "--version") == 0) {
		printf("cdladm, version %s\n", PACKAGE_VERSION);
		printf("Copyright (C) 2021, Western Digital Corporation"
		       " or its affiliates.\n");
		return 0;
	}

	if (strcmp(argv[1], "--help") == 0 ||
	    strcmp(argv[1], "-h") == 0) {
		cdladm_usage();
		return 0;
	}

	/* Get the command */
	command = cdladm_get_command(argv[1]);
	if (command >= CDLADM_CMD_MAX) {
		fprintf(stderr, "Invalid command %s\n", argv[1]);
		return 1;
	}

	/* Parse options */
	for (i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--verbose") == 0 ||
		    strcmp(argv[i], "-v") == 0) {
			dev.flags |= CDL_VERBOSE;
			continue;
		}

		if (strcmp(argv[i], "--force-ata") == 0 ||
		    strcmp(argv[i], "-a") == 0) {
			dev.flags |= CDL_USE_ATA;
			continue;
		}

		if (strcmp(argv[i], "--count") == 0) {
			if (command != CDLADM_SHOW)
				goto err_cmd_line;
			dev.flags |= CDL_SHOW_COUNT;
			continue;
		}

		if (strcmp(argv[i], "--page") == 0) {
			if (command != CDLADM_SHOW &&
			    command != CDLADM_CLEAR &&
			    command != CDLADM_SAVE)
				goto err_cmd_line;
			i++;
			if (i >= argc - 1)
				goto err_cmd_line;
			page = argv[i];
			continue;
		}

		if (strcmp(argv[i], "--file") == 0) {
			if (command != CDLADM_SAVE &&
			    command != CDLADM_UPLOAD &&
			    command != CDLADM_STATS_SAVE &&
			    command != CDLADM_STATS_UPLOAD)
				goto err_cmd_line;
			i++;
			if (i >= argc - 1)
				goto err_cmd_line;
			path = argv[i];
			continue;
		}

		if (strcmp(argv[i], "--permanent") == 0) {
			if (command != CDLADM_UPLOAD)
				goto err_cmd_line;
			dev.flags |= CDL_USE_MS_SP;
			continue;
		}

		if (strcmp(argv[i], "--raw") == 0) {
			if (command != CDLADM_SHOW)
				goto err_cmd_line;
			dev.flags |= CDL_SHOW_RAW_VAL;
			continue;
		}

		if (strcmp(argv[i], "--force-dev") == 0) {
			if (command != CDLADM_ENABLE &&
			    command != CDLADM_DISABLE)
				goto err_cmd_line;
			dev.flags |= CDL_FORCE_DEV;
			continue;
		}

		if (argv[i][0] != '-')
			break;

		fprintf(stderr, "Invalid option '%s'\n", argv[i]);
		return 1;
	}

	if (i != argc - 1) {
err_cmd_line:
		fprintf(stderr, "Invalid command line\n");
		return 1;
	}

	/* Get device path */
	dev.path = realpath(argv[i], NULL);
	if (!dev.path) {
		fprintf(stderr, "Failed to get device real path\n");
		return 1;
	}

	/* Open the device and printf some information about it */
	ret = cdl_open_dev(&dev, cdladm_cmd[command].mode);
	if (ret)
		return 1;

	cdladm_get_kernel_support(&dev);

	/* Execute enable/disable first to display updated information */
	if (command == CDLADM_ENABLE || command == CDLADM_DISABLE) {
		if (command == CDLADM_ENABLE) {
			ret = cdladm_enable(&dev);
			if (ret) {
				fprintf(stderr, "Enabling CDL failed\n");
				goto out;
			}
		} else {
			ret = cdladm_disable(&dev);
			if (ret) {
				fprintf(stderr, "Disabling CDL failed\n");
				goto out;
			}
		}

		/* Close and re-open the device to get updated information */
		cdl_close_dev(&dev);
		ret = cdl_open_dev(&dev, cdladm_cmd[command].mode);
		if (ret)
			return 1;
	}

	printf("Device: /dev/%s\n", dev.name);
	printf("    Vendor: %s\n", dev.vendor);
	printf("    Product: %s\n", dev.id);
	printf("    Revision: %s\n", dev.rev);
	printf("    %llu 512-byte sectors (%llu.%03llu TB)\n",
	       dev.capacity,
	       (dev.capacity << 9) / 1000000000000,
	       ((dev.capacity << 9) % 1000000000000) / 1000000000);
	printf("    Device interface: %s\n",
	       cdl_dev_is_ata(&dev) ? "ATA" : "SAS");
	if (cdl_dev_is_ata(&dev)) {
		printf("      SAT Vendor: %s\n", dev.sat_vendor);
		printf("      SAT Product: %s\n", dev.sat_product);
		printf("      SAT revision: %s\n", dev.sat_rev);
	}

	printf("    Command duration limits: %ssupported, %s\n",
	       dev.flags & CDL_DEV_SUPPORTED ? "" : "not ",
	       dev.flags & CDL_DEV_ENABLED ? "enabled" : "disabled");
	if (!(dev.flags & CDL_DEV_SUPPORTED)) {
		ret = 1;
		goto out;
	}

	printf("    Command duration guidelines: %ssupported\n",
	       dev.flags & CDL_GUIDELINE_DEV_SUPPORTED ? "" : "not ");
	printf("    High priority enhancement: %ssupported, %s\n",
	       dev.flags & CDL_HIGHPRI_DEV_SUPPORTED ? "" : "not ",
	       dev.flags & CDL_HIGHPRI_DEV_ENABLED ? "enabled" : "disabled");
	printf("    Statistics: %ssupported\n",
	       dev.flags & CDL_STATISTICS_SUPPORTED ? "" : "not ");


	printf("    Duration minimum limit: %llu ns\n", dev.min_limit);
	if (!dev.max_limit)
		printf("    Duration maximum limit: none\n");
	else
		printf("    Duration maximum limit: %llu ns\n", dev.max_limit);

	cdladm_show_kernel_support(&dev);

	/* Some paranoia checks */
	if (!(dev.flags & CDL_SYS_SUPPORTED))
		printf("WARNING: System does not support command duration limits\n");
	if (command != CDLADM_ENABLE && command != CDLADM_DISABLE) {
		if ((dev.flags & CDL_SYS_ENABLED) &&
		    !(dev.flags & CDL_DEV_ENABLED))
			printf("WARNING: Command duration limits is enabled on "
			       "the system but disabled on the device\n");
		if ((dev.flags & CDL_DEV_ENABLED) &&
		    !(dev.flags & CDL_SYS_ENABLED))
			printf("WARNING: Command duration limits is disabled "
			       "on the system but enabled on the device\n");
		if ((dev.flags & CDL_DEV_ENABLED) &&
		    (dev.flags & CDL_HIGHPRI_DEV_ENABLED))
			printf("WARNING: Command duration limits and high "
			       "priority enhancement are both enabled on "
			       " the device\n");
	}

	if (command == CDLADM_INFO) {
		ret = 0;
		goto out;
	}

	ret = cdl_read_pages(&dev);
	if (ret)
		goto out;

	/* Execute the command */
	switch (command) {
	case CDLADM_LIST:
		ret = cdladm_list(&dev);
		break;
	case CDLADM_SHOW:
		ret = cdladm_show(&dev, page);
		break;
	case CDLADM_CLEAR:
		ret = cdladm_clear(&dev, page);
		break;
	case CDLADM_SAVE:
		ret = cdladm_save(&dev, page, path);
		break;
	case CDLADM_UPLOAD:
		ret = cdladm_upload(&dev, path);
		break;
	case CDLADM_ENABLE:
	case CDLADM_DISABLE:
		break;
	case CDLADM_ENABLE_HIGHPRI:
		ret = cdladm_enable_highpri(&dev);
		break;
	case CDLADM_DISABLE_HIGHPRI:
		ret = cdladm_disable_highpri(&dev);
		break;
	case CDLADM_STATS_SHOW:
		ret = cdladm_stats_show(&dev);
		break;
	case CDLADM_STATS_RESET:
		ret = cdladm_stats_reset(&dev);
		break;
	case CDLADM_STATS_SAVE:
		ret = cdladm_stats_save(&dev, path);
		break;
	case CDLADM_STATS_UPLOAD:
		ret = cdladm_stats_upload(&dev, path);
		break;
	case CDLADM_NONE:
	default:
		fprintf(stderr, "No command specified\n");
		ret = 1;
	}

out:
	cdl_close_dev(&dev);

	if (ret)
		return 1;

	return 0;
}

