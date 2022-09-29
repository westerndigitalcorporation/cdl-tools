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
	       "  --verbose | -v       : Verbose output\n");
	printf("Commands:\n"
	       "  info    : Show device and system support information\n"
	       "  list    : List supported pages\n"
	       "  show    : Display one or all supported pages\n"
	       "  save    : Save one or all pages to a file\n"
	       "  upload  : Upload a page to the device\n"
	       "  enable  : Enable command duration limits\n"
	       "  disable : Disable command duration limits\n");
	printf("Command options:\n");
	printf("  --count\n"
	       "\tApply to the show command.\n"
	       "\tOmit the descriptor details and only print the number of\n"
	       "\tdescriptors valid in a page\n");
	printf("  --page <name>\n"
	       "\tApply to the show and save commands.\n"
	       "\tSpecify the target page name. The page name can be:\n"
	       "\t\"A\", \"B\", \"T2A\" or \"T2B\".\n");
	printf("  --file <path>\n"
	       "\tApply to the save and upload commands.\n"
	       "\tSpecify the path of the page file to use.\n"
	       "\tUsing this option is mandatory with the upload command.\n"
	       "\tIf this option is not specified with the save command,\n"
	       "\tthe default file name <dev name>-<page name>.cdl is\n"
	       "\tused.\n");
	printf("  --permanent\n"
	       "\tApply to the upload command.\n"
	       "\tSpecify that the device should save the page in\n"
	       "\tnon-volatile memory in addition to updating the current\n"
	       "\tpage value.\n");
	printf("  --raw\n"
	       "\tApply to the show command.\n"
	       "\tShow the raw values of the CDL pages fields\n");
}

static int cdladm_list(struct cdl_dev *dev)
{
	int i, j, n;

	cdl_dev_info(dev, "Supported pages:\n");
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

		printf("Page %s:\n", cdl_page_name(i));
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

static void cdladm_check_kernel_support(struct cdl_dev *dev)
{
	struct utsname buf;
	bool supported, enabled = false;

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

	supported = cdl_sysfs_exists(dev,
				     "/sys/block/%s/device/duration_limits",
				     dev->name);
	if (supported) {
		dev->flags |= CDL_SYS_SUPPORTED;
		enabled = cdl_sysfs_get_ulong_attr(dev,
				"/sys/block/%s/device/duration_limits/enable",
				dev->name);
		if (enabled)
			dev->flags |= CDL_SYS_ENABLED;
	}

	printf("    Command duration limits: %ssupported, %s\n",
	       supported ? "" : "not ",
	       enabled ? "enabled" : "disabled");
	printf("    Device %s command timeout: %llu s\n",
	       dev->name, dev->cmd_timeout / 1000000000ULL);
}

static int cdladm_enable(struct cdl_dev *dev)
{
	int ret;

	if (!(dev->flags & CDL_SYS_SUPPORTED)) {
		fprintf(stderr, "System lacks support\n");
		return 1;
	}

	/* Enable system: this should enable the device too */
	ret = cdl_sysfs_set_attr(dev, "1",
			"/sys/block/%s/device/duration_limits/enable",
			dev->name);
	if (ret)
		return 1;

	dev->flags |= CDL_SYS_ENABLED;
	printf("Command duration limits is enabled\n");

	cdl_check_enabled(dev, true);
	if (!(dev->flags & CDL_DEV_ENABLED))
		printf("WARNING: Command duration limits is disabled "
		       "on the device\n");

	return 0;
}

static int cdladm_disable(struct cdl_dev *dev)
{
	int ret;

	if (!(dev->flags & CDL_SYS_SUPPORTED)) {
		fprintf(stderr, "System lacks support\n");
		return 1;
	}

	/* Enable system: this should enable the device too */
	ret = cdl_sysfs_set_attr(dev, "0",
			"/sys/block/%s/device/duration_limits/enable",
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

/*
 * Possible commands.
 */
enum cdladm_cmd {
	CDLADM_NONE,
	CDLADM_INFO,
	CDLADM_LIST,
	CDLADM_SHOW,
	CDLADM_SAVE,
	CDLADM_UPLOAD,
	CDLADM_ENABLE,
	CDLADM_DISABLE,
};

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

	/* Parse options */
	for (i = 1; i < argc; i++) {
		/* Generic options */
		if (strcmp(argv[i], "--version") == 0) {
			printf("cdladm, version %s\n", PACKAGE_VERSION);
			printf("Copyright (C) 2021, Western Digital Corporation"
			       " or its affiliates.\n");
			return 0;
		}

		if (strcmp(argv[i], "--help") == 0 ||
		    strcmp(argv[i], "-h") == 0) {
			cdladm_usage();
			printf("See \"man cdladm\" for more information\n");
			return 0;
		}

		/* Commands */
		if (strcmp(argv[i], "info") == 0) {
			if (command != CDLADM_NONE)
				goto err_cmd_line;
			command = CDLADM_INFO;
			continue;
		}

		if (strcmp(argv[i], "list") == 0) {
			if (command != CDLADM_NONE)
				goto err_cmd_line;
			command = CDLADM_LIST;
			continue;
		}

		if (strcmp(argv[i], "show") == 0) {
			if (command != CDLADM_NONE)
				goto err_cmd_line;
			command = CDLADM_SHOW;
			continue;
		}

		if (strcmp(argv[i], "save") == 0) {
			if (command != CDLADM_NONE)
				goto err_cmd_line;
			command = CDLADM_SAVE;
			continue;
		}

		if (strcmp(argv[i], "upload") == 0) {
			if (command != CDLADM_NONE)
				goto err_cmd_line;
			command = CDLADM_UPLOAD;
			continue;
		}

		if (strcmp(argv[i], "enable") == 0) {
			if (command != CDLADM_NONE)
				goto err_cmd_line;
			command = CDLADM_ENABLE;
			continue;
		}

		if (strcmp(argv[i], "disable") == 0) {
			if (command != CDLADM_NONE)
				goto err_cmd_line;
			command = CDLADM_DISABLE;
			continue;
		}

		/* Options */
		if (strcmp(argv[i], "--verbose") == 0 ||
		    strcmp(argv[i], "-v") == 0) {
			dev.flags |= CDL_VERBOSE;
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
			    command != CDLADM_UPLOAD)
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

		if (argv[i][0] == '-') {
			fprintf(stderr, "Invalid option '%s'\n", argv[i]);
			return 1;
		} else {
			break;
		}
	}

	if (i != argc - 1) {
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
	if (cdl_open_dev(&dev) < 0)
		return 1;

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

	printf("    Duration minimum limit: %llu ns\n", dev.min_limit);
	if (!dev.max_limit)
		printf("    Duration maximum limit: none\n");
	else
		printf("    Duration maximum limit: %llu ns\n", dev.max_limit);

	cdladm_check_kernel_support(&dev);

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
	case CDLADM_SAVE:
		ret = cdladm_save(&dev, page, path);
		break;
	case CDLADM_UPLOAD:
		ret = cdladm_upload(&dev, path);
		break;
	case CDLADM_ENABLE:
		ret = cdladm_enable(&dev);
		break;
	case CDLADM_DISABLE:
		ret = cdladm_disable(&dev);
		break;
	case CDLADM_NONE:
	default:
		fprintf(stderr, "No command specified\n");
		ret = 1;
	}

out:
	cdl_close_dev(&dev);

	return ret;

err_cmd_line:
	fprintf(stderr, "Invalid command line\n");

	return 1;
}

