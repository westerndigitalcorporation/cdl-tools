Copyright (C) 2021, Western Digital Corporation or its affiliates.

# <p align="center">CDL tools</p>

This project provides the *cdladm* command line utility which allows
inspecting and configuring command duration limits for block devices
supporting this feature.

## License

The *cdl-tools* project source code is distributed under the terms of the
GNU General Public License v2.0 or later
([GPL-v2](https://opensource.org/licenses/GPL-2.0)).
A copy of this license with *cdl-tools* copyright can be found in the files
[LICENSES/GPL-2.0-or-later.txt](LICENSES/GPL-2.0-or-later.txt) and
[COPYING.GPL](COPYING.GPL).

All source files in *cdl-tools* contain the SPDX short identifier for the
GPL-2.0-or-later license in place of the full license text.

```
SPDX-License-Identifier: GPL-2.0-or-later
```

Some files such as the `Makefile.am` files and the `.gitignore` file are public
domain specified by the [CC0 1.0 Universal (CC0 1.0) Public Domain
Dedication](https://creativecommons.org/publicdomain/zero/1.0/).
These files are identified with the following SPDX short identifier header.

```
SPDX-License-Identifier: CC0-1.0
```

See [LICENSES/CC0-1.0.txt](LICENSES/CC0-1.0.txt) for the full text of this
license.

## Requirements

The following packages must be installed prior to compiling *cdladm*.

* autoconf
* autoconf-archive
* automake
* libtool

## Compilation and Installation

The following commands will compile the *cdladm* tool.

```
$ sh ./autogen.sh
$ ./configure
$ make
```

To install the compiled executable files, simply execute as root the following
command.

```
$ sudo make install
```

The default installation directory is /usr/bin. This default location can be
changed using the configure script. Executing the following command displays
the options used to control the installation path.

```
> ./configure --help
```

## Building RPM Packages

The *rpm* and *rpmbuild* utilities are necessary to build *cdl-tools* RPM
packages. Once these utilities are installed, the RPM packages can be built
using the following command.

```
$ sh ./autogen.sh
$ ./configure
$ make rpm
```

Four RPM packages are built: a binary package providing *cdladm* executable
and its documentation and license files, a source RPM package, a *debuginfo*
RPM package and a *debugsource* RPM package.

The source RPM package can be used to build the binary and debug RPM packages
outside of *cdl-tools* source tree using the following command.

```
$ rpmbuild --rebuild cdl-tools-<version>.src.rpm
```

## Usage

*cdladm* detailed usage is as follows:

```
> cdladm -h 
Usage: cdladm [options] <device path>
Options:
  --help | -h   : General help message
  -v            : Verbose output
  ...
See "man cdladm" for more information
```

## Contributing

Read the [CONTRIBUTING](CONTRIBUTING) file and send patches to:

	Damien Le Moal <damien.lemoal@wdc.com>
	Niklas Cassel <niklas.cassel@wdc.com>

