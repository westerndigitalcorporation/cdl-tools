Name:		%{pkg_name}
Version:	%{pkg_version}
Release:	1%{?dist}
Summary:	Provides user utilities for block devices supporting command duration limits

License:	GPLv2+
URL:		https://github.com/westerndigitalcorporation/%{name}
Source0:	%{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:	autoconf
BuildRequires:	automake
BuildRequires:	libtool
BuildRequires:	make
BuildRequires:	gcc

%description
This package provides the cdladm user utility to inspect and modify
command duration limits of SCSI and ATA disks supporting this feature.

# Tests package
%package tests
Summary: CDL test scripts
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildArch: noarch

%description tests
This package provides scripts to test command duration limits of SCSI and ATA
disks supporting this feature.

%prep
%autosetup

%build
sh autogen.sh
%configure
%make_build

%install
%make_install
%make_install install-tests

%files
%{_bindir}/*
%{_mandir}/man8/*
%license COPYING.GPL
%doc README.md CONTRIBUTING

%files tests
%{_prefix}/local/cdl-tests/*

%changelog
* Tue Nov 7 2023 Damien Le Moal <damien.lemoal@wdc.com> 1.0.0-1
- Version 1.0.0 package
