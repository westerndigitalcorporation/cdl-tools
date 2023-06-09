# SPDX-License-Identifier: CC0-1.0
#
# Copyright (C) 2021 Western Digital Corporation or its affiliates.

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src man

EXTRA_DIST = autogen.sh README.md COPYING.GPL CONTRIBUTING

TESTSDIR = $(DESTDIR)$(prefix)/local/cdl-tests

install-tests:
	rm -rf ${TESTSDIR}
	install -m 755 -d ${TESTSDIR}
	install -m 755 -d ${TESTSDIR}/scripts
	install -m 755 $(top_srcdir)/tests/scripts/*.sh ${TESTSDIR}/scripts
	install -m 644 $(top_srcdir)/tests/scripts/test_lib ${TESTSDIR}/scripts
	install -m 755 -d ${TESTSDIR}/scripts/cdl
	install -m 644 $(top_srcdir)/tests/scripts/cdl/* ${TESTSDIR}/scripts/cdl
	install -m 755 $(top_srcdir)/tests/cdl-tests.sh ${TESTSDIR}

uninstall-tests:
	rm -rf ${TESTSDIR}

uninstall-hook:
	rm -rf ${TESTSDIR}

if BUILD_RPM

rpmdir = $(abs_top_builddir)/rpmbuild

EXTRA_DIST += cdl-tools.spec tests
RPMARCH=`$(RPM) --eval %_target_cpu`

rpm: dist
	@echo "Building RPM package..."
	@mkdir -p $(rpmdir)
	$(RPMBUILD) -ta --clean \
		-D "_topdir $(rpmdir)" \
		-D "pkg_name cdl-tools" \
		-D "pkg_version $(PACKAGE_VERSION)" \
		cdl-tools-$(PACKAGE_VERSION).tar.gz
	@mv -f $(rpmdir)/RPMS/$(RPMARCH)/*.rpm $(abs_top_builddir)
	@mv -f $(rpmdir)/RPMS/noarch/*.rpm $(abs_top_builddir)
	@mv -f $(rpmdir)/SRPMS/*.rpm $(abs_top_builddir)
	@rm -rf $(rpmdir)
	@rm -f cdl-tools-$(PACKAGE_VERSION).tar.gz
else
rpm:
	@echo "Building RPM packages requires rpmbuild and rpm utilities"
	exit 1
endif
