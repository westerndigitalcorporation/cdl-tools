Name:		cdl-tools
Version:	0.0.1
Release:	1%{?dist}
Summary:	Provides user utilities for block devices supporting command duration limits

License:	GPLv2+
URL:		https://github.com/westerndigitalcorporation/%{name}
Source0:	%{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:	autoconf
BuildRequires:	autoconf-archive
BuildRequires:	automake
BuildRequires:	libtool
BuildRequires:	make
BuildRequires:	gcc

%description
This package provides the cdladm user utility to inspect and modify
command duration limits for block devices supporting this feature.

%prep
%autosetup

%build
sh autogen.sh
%configure
%make_build

%install
%make_install

%files
%{_bindir}/*
%{_mandir}/man8/*

%license COPYING.GPL
%doc README.md CONTRIBUTING

%changelog
* Mon Aug 16 2021 Damien Le Moal <damien.lemoal@wdc.com> 0.0.1-1
- Version 0.0.1 initial package
