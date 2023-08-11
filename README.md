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

The following commands will compile the *cdladm* utility.

```
$ sh ./autogen.sh
$ ./configure
$ make
```

To install the compiled executable file and the man page for the *cdladm*
utility, the following command can be used.

```
$ sudo make install
```

The default installation directory is /usr/bin. This default location can be
changed using the configure script. Executing the following command displays
the options used to control the installation path.

```
$ ./configure --help
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

Five RPM packages are built: a binary package providing *cdladm* executable
and its documentation, a source RPM package, a *debuginfo*
RPM package and a *debugsource* RPM package, and an RPM package containing the
test suite.

The source RPM package can be used to build the binary and debug RPM packages
outside of *cdl-tools* source tree using the following command.

```
$ rpmbuild --rebuild cdl-tools-<version>.src.rpm
```

## Contributing

Read the [CONTRIBUTING](CONTRIBUTING) file and send patches to:

	Damien Le Moal <damien.lemoal@wdc.com>
	Niklas Cassel <niklas.cassel@wdc.com>

# Using Command Duration Limits

## *cdladm* utility

The *cdladm* utility allows manipulating command duration limits configuration
of SAS and SATA hard-disks supporting this feature.

*cdladm* provide many functions. The usage is as follows:

```
$ cdladm --help
Usage:
  cdladm --help | -h
  cdladm --version
  cdladm <command> [options] <device>
Options common to all commands:
  --verbose | -v       : Verbose output
  --force-ata | -a     : Force the use of ATA passthrough commands
Commands:
  info            : Show device and system support information
  list            : List supported pages
  show            : Display one or all supported pages
  clear           : Clear one or all supported pages
  save            : Save one or all pages to a file
  upload          : Upload a page to the device
  enable          : Enable command duration limits
  disable         : Disable command duration limits
  enable-highpri  : Enable high priority enhancement
  disable-highpri : Disable high priority enhancement
Command options:
  --count
	Apply to the show command.
	Omit the descriptor details and only print the number of
	valid descriptors in a page
  --page <name>
	Apply to the show, clear and save commands.
	Specify the name of the page to show,clear or save. The
	page name tcan be: "A", "B", "T2A" or "T2B".
  --file <path>
	Apply to the save and upload commands.
	Specify the path of the page file to use.
	Using this option is mandatory with the upload command.
	If this option is not specified with the save command,
	the default file name <dev name>-<page name>.cdl is
	used.
  --permanent
	Apply to the upload command.
	Specify that the device should save the page in
	non-volatile memory in addition to updating the current
	page value.
  --raw
	Apply to the show command.
	Show the raw values of the CDL pages fields.
  --force-dev
	Apply to the enable and disable commands for ATA devices.
	Force enabling and disabling the CDL feature directly on
	the device without enabling/disabling the system.
	The use of this option is not recommended under normal
	use and is reserved for testing only.
See "man cdladm" for more information.
```

### Checking for command duration limits support

To check if a device supports command duration limits, use the "info" command.

```
$ cdladm info /dev/sda
Device: /dev/sdg
    Vendor: ATA
    Product: xxx  xxxxxxxxxxx
    Revision: xxxx
    42970644480 512-byte sectors (22.000 TB)
    Device interface: ATA
      SAT Vendor: linux
      SAT Product: libata
      SAT revision: 3.00
    Command duration limits: supported, disabled
    Command duration guidelines: supported
    High priority enhancement: supported, disabled
    Duration minimum limit: 20000000 ns
    Duration maximum limit: 4294967295000 ns
System:
    Node name: xxx
    Kernel: Linux 6.5.0-rc3+ #130 SMP PREEMPT_DYNAMIC Wed Jul 26 09:38:47 JST 2023
    Architecture: x86_64
    Command duration limits: supported, disabled
    Device sdg command timeout: 30 s
```

In the above example, the SATA device used supports command duration limits.

When applied to a device that does not support command duration limits, *cdladm*
displays the following output.

```
$ cdladm info /dev/sde
Device: /dev/sdh
    Vendor: ATA
    Product: xxx  xxxxxxxxxxx
    Revision: xxxx
    39063650304 512-byte sectors (20.000 TB)
    Device interface: ATA
      SAT Vendor: linux
      SAT Product: libata
      SAT revision: 3.00
    Command duration limits: not supported, disabled
```

When using a kernel including support for command duration limits (kernel
version 6.5 and above), the sysfs attribute files *cdl_supported* and
*cdl_enable* will be present for any scsi device.

```
$ tree -L 1 /sys/block/sda/device/
/sys/block/sda/device/
|-- access_state
|-- blacklist
|-- block
|-- bsg
|-- cdl_enable
|-- cdl_supported
|-- delete
|-- device_blocked
|-- device_busy
|-- dh_state
|-- driver -> ../../../../../../../bus/scsi/drivers/sd
...
```

The *cdl_supported* attribute file indicates with a value of "1" if a device
supports cdl. A value of "0" indicates that the device does not implement the
CDL feature.

The *cdl_enable* attribute files allows enabling and disabling the CDL feature
set for a device supporting it.

### Checking Duration Limits Descriptors

The *cdladm show* command retreives from the device and displays in human
readable form the command limit descriptors configured on the target device.

```
$ cdladm show /dev/sdg
Device: /dev/sdg
    Vendor: ATA
    Product: xxx  xxxxxxxxxxx
    Revision: xxxx
    42970644480 512-byte sectors (22.000 TB)
    Device interface: ATA
      SAT Vendor: linux
      SAT Product: libata
      SAT revision: 3.00
    Command duration limits: supported, disabled
    Command duration guidelines: supported
    High priority enhancement: supported, disabled
    Duration minimum limit: 20000000 ns
    Duration maximum limit: 4294967295000 ns
System:
    Node name: xxx
    Kernel: Linux 6.5.0-rc3+ #130 SMP PREEMPT_DYNAMIC Wed Jul 26 09:38:47 JST 2023
    Architecture: x86_64
    Command duration limits: supported, disabled
    Device sdg command timeout: 30 s
Page T2A:
  perf_vs_duration_guideline : 20%
  Descriptor 1:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : 30 ms
    duration guideline policy: complete-earliest
  Descriptor 2:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : 50 ms
    duration guideline policy: complete-earliest
  Descriptor 3:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : 100 ms
    duration guideline policy: complete-earliest
  Descriptor 4:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : 500 ms
    duration guideline policy: complete-earliest
  Descriptor 5:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : no limit
  Descriptor 6:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : no limit
  Descriptor 7:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : no limit
Page T2B:
  Descriptor 1:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : no limit
  Descriptor 2:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : no limit
  Descriptor 3:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : no limit
  Descriptor 4:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : no limit
  Descriptor 5:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : no limit
  Descriptor 6:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : no limit
  Descriptor 7:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : no limit
```

In the above example, the disk ```/dev/sdg``` has the read descriptors 1 to 4
configured with a duration guideline limit of 30ms, 50ms, 100ms and 500ms.

### Modifying Duration Limits Descriptors

To modify the duration limit descriptors of a device, *cdladm* "save" and
"upload" commands can be used. These commands operate on either the read
descriptors or the write descriptors. The read and write descriptors cannot be
changed together in a single operation.

The "save" command will save the current duration limit descriptors of the disk
to a file.

```
$ cdladm save --page T2A /dev/sdg
Device: /dev/sdg
    Vendor: ATA
    Product: xxx  xxxxxxxxxxx
    Revision: xxxx
    42970644480 512-byte sectors (22.000 TB)
    Device interface: ATA
      SAT Vendor: linux
      SAT Product: libata
      SAT revision: 3.00
    Command duration limits: supported, disabled
    Command duration guidelines: supported
    High priority enhancement: supported, disabled
    Duration minimum limit: 20000000 ns
    Duration maximum limit: 4294967295000 ns
System:
    Node name: xxx
    Kernel: Linux 6.5.0-rc3+ #130 SMP PREEMPT_DYNAMIC Wed Jul 26 09:38:47 JST 2023
    Architecture: x86_64
    Command duration limits: supported, disabled
    Device sdg command timeout: 30 s
Saving page T2A to file sdg-T2A.cdl
```

The file can then be edited to modify the duration limit descriptors fields.

```
$ less sdg-T2A.cdl
# T2A page format:
# perf-vs-duration-guideline can be one of:
#   - 0%    : 0x0
#   - 0.5%  : 0x1
#   - 1.0%  : 0x2
#   - 1.5%  : 0x3
#   - 2.0%  : 0x4
#   - 2.5%  : 0x5
#   - 3%    : 0x6
#   - 4%    : 0x7
#   - 5%    : 0x8
#   - 8%    : 0x9
#   - 10%   : 0xa
#   - 15%   : 0xb
#   - 20%   : 0xc
# t2cdlunits can be one of:
#   - none   : 0x0
#   - 500ns  : 0x6
#   - 1us    : 0x8
#   - 10ms   : 0xa
#   - 500ms  : 0xe
# max-inactive-time-policy can be one of:
#   - complete-earliest    : 0x0
#   - complete-unavailable : 0xd
#   - abort                : 0xf
# max-active-time-policy can be one of:
#   - complete-earliest    : 0x0
#   - complete-unavailable : 0xd
#   - abort-recovery       : 0xe
#   - abort                : 0xf
# duration-guideline-policy can be one of:
#   - complete-earliest    : 0x0
#   - continue-next-limit  : 0x1
#   - continue-no-limit    : 0x2
#   - complete-unavailable : 0xd
#   - abort                : 0xf

cdlp: T2A

perf-vs-duration-guideline: 0xc

== descriptor: 1
t2cdlunits: 0xa
max-inactive-time: 0
max-inactive-time-policy: 0x0
max-active-time: 0
max-active-time-policy: 0x0
duration-guideline: 3
duration-guideline-policy: 0x0

== descriptor: 2
t2cdlunits: 0xa
max-inactive-time: 0
max-inactive-time-policy: 0x0
max-active-time: 0
max-active-time-policy: 0x0
duration-guideline: 5
duration-guideline-policy: 0x0

== descriptor: 3
t2cdlunits: 0xa
max-inactive-time: 0
max-inactive-time-policy: 0x0
max-active-time: 0
max-active-time-policy: 0x0
duration-guideline: 10
duration-guideline-policy: 0x0

== descriptor: 4
t2cdlunits: 0xa
max-inactive-time: 0
max-inactive-time-policy: 0x0
max-active-time: 0
max-active-time-policy: 0x0
duration-guideline: 50
duration-guideline-policy: 0x0

== descriptor: 5
t2cdlunits: 0x0
max-inactive-time: 0
max-inactive-time-policy: 0x0
max-active-time: 0
max-active-time-policy: 0x0
duration-guideline: 0
duration-guideline-policy: 0x0

== descriptor: 6
t2cdlunits: 0x0
max-inactive-time: 0
max-inactive-time-policy: 0x0
max-active-time: 0
max-active-time-policy: 0x0
duration-guideline: 0
duration-guideline-policy: 0x0

== descriptor: 7
t2cdlunits: 0x0
max-inactive-time: 0
max-inactive-time-policy: 0x0
max-active-time: 0
max-active-time-policy: 0x0
duration-guideline: 0
duration-guideline-policy: 0x0
```

The modified file can then be used to upload to the disk the modified
descriptors. In the example below, descriptor 5 is enabled to define a 1s
duration limit.

```
$ cdladm upload --file sdg-T2A.cdl /dev/sdg
Device: /dev/sdg
    Vendor: ATA
    Product: xxx  xxxxxxxxxxx
    Revision: xxxx
    42970644480 512-byte sectors (22.000 TB)
    Device interface: ATA
      SAT Vendor: linux
      SAT Product: libata
      SAT revision: 3.00
    Command duration limits: supported, disabled
    Command duration guidelines: supported
    High priority enhancement: supported, disabled
    Duration minimum limit: 20000000 ns
    Duration maximum limit: 4294967295000 ns
System:
    Node name: xxx
    Kernel: Linux 6.5.0-rc3+ #130 SMP PREEMPT_DYNAMIC Wed Jul 26 09:38:47 JST 2023
    Architecture: x86_64
    Command duration limits: supported, disabled
    Device sdg command timeout: 30 s
Parsing file sdg-T2A.cdl...
Uploading page T2A:
  perf_vs_duration_guideline : 20%
  Descriptor 1:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : 50 ms
    duration guideline policy: complete-earliest
  Descriptor 2:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : 20 ms
    duration guideline policy: continue-next-limit
  Descriptor 3:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : 50 ms
    duration guideline policy: complete-earliest
  Descriptor 4:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : 20 ms
    duration guideline policy: continue-no-limit
  Descriptor 5:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : 20 ms
    duration guideline policy: complete-unavailable
  Descriptor 6:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : 20 ms
    duration guideline policy: abort
  Descriptor 7:
    max inactive time        : no limit
    max active time          : no limit
    duration guideline       : no limit
```

## Using Command Duration Limits

The Linux kernel support for command duration limits disables the CDL feature by
default. For command duration limits to be effective, CDL support must first be
enabled. Once enabled, fio can be use to exercise a device with read and write
commands with command duration limits.

### Enabling and Disabling CDL support

This can be done using the *cdl_enable* sysfs attribute file:

```
$ echo 1 > /sys/block/sdg/device/cdl_enable
```

For convenience, the enable command of *cdladm* can also be used:

```
$ cdladm enable /dev/sdg
Command duration limits is enabled
Device: /dev/sdg
    Vendor: ATA
    Product: xxx  xxxxxxxxxxx
    Revision: xxxx
    42970644480 512-byte sectors (22.000 TB)
    Device interface: ATA
      SAT Vendor: linux
      SAT Product: libata
      SAT revision: 3.00
    Command duration limits: supported, enabled
    Command duration guidelines: supported
    High priority enhancement: supported, disabled
    Duration minimum limit: 20000000 ns
    Duration maximum limit: 4294967295000 ns
System:
    Node name: xxx
    Kernel: Linux 6.5.0-rc3+ #130 SMP PREEMPT_DYNAMIC Wed Jul 26 09:38:47 JST 2023
    Architecture: x86_64
    Command duration limits: supported, enabled
    Device sdg command timeout: 30 s

$ cat /sys/block/sdg/device/cdl_enable
1
```

Conversely, CDL can be disabled either using sysfs:

```
$ echo 0 > /sys/block/sdg/device/cdl_enable
```

Or using *cdladm* disable command:

```
$ cdladm disable /dev/sdg
Command duration limits is disabled
Device: /dev/sdg
    Vendor: ATA
    Product: xxx  xxxxxxxxxxx
    Revision: xxxx
    42970644480 512-byte sectors (22.000 TB)
    Device interface: ATA
      SAT Vendor: linux
      SAT Product: libata
      SAT revision: 3.00
    Command duration limits: supported, disabled
    Command duration guidelines: supported
    High priority enhancement: supported, disabled
    Duration minimum limit: 20000000 ns
    Duration maximum limit: 4294967295000 ns
System:
    Node name: xxx
    Kernel: Linux 6.5.0-rc3+ #130 SMP PREEMPT_DYNAMIC Wed Jul 26 09:38:47 JST 2023
    Architecture: x86_64
    Command duration limits: supported, disabled
    Device sdg command timeout: 30 s
```

### Using *fio*

*fio* can be used to exercise a drive with workload using command duration
limits. The standard *fio* options *cmdprio_class*, *cmdprio_percentage* and
*cmdprio_hint* allow a job using the *libaio* or *io_uring* I/O engines to
specify the use of command duration limits.

For instance, the following fio script:

```
[global]
filename=/dev/sdg
random_generator=tausworthe64
continue_on_error=none
ioscheduler=none
direct=1
write_lat_log=randread.log
per_job_logs=0
log_prio=1

[randread]
rw=randread
bs=128k
ioengine=libaio
iodepth=32
cmdprio_class=2
cmdprio_hint=1
cmdprio_percentage=20
```

Will issue random 128KB read commands at a queue depth of 32 with 20% of the
commands using the best effort priority class and the duration limit descriptor
1. Assuming that this descriptor defines a duration guideline limit of 50ms,
fio run statistics will show the impact of the duration limits feature.

```
rndrd_qd32: (g=0): rw=randread, bs=(R) 128KiB-128KiB, (W) 128KiB-128KiB, (T) 128KiB-128KiB, ioengine=libaio, iodepth=32
fio-3.35-83-g0b47
Starting 1 process

rndrd_qd32: (groupid=0, jobs=1): err= 0: pid=3091: Tue Aug 15 15:11:00 2023
  read: IOPS=154, BW=19.3MiB/s (20.2MB/s)(5794MiB/300246msec)
    slat (usec): min=97, max=1882, avg=148.04, stdev=45.90
    clat (msec): min=6, max=544, avg=206.77, stdev=125.79
     lat (msec): min=6, max=544, avg=206.92, stdev=125.79
    clat prio 0/0/0 (msec): min=8, max=544, avg=250.42, stdev=101.62
    clat prio 2/0/1 (usec): min=6805, max=88200, avg=34077.20, stdev=14640.35
    clat percentiles (msec):
     |  1.00th=[   16],  5.00th=[   21], 10.00th=[   28], 20.00th=[   52],
     | 30.00th=[  115], 40.00th=[  182], 50.00th=[  228], 60.00th=[  266],
     | 70.00th=[  296], 80.00th=[  330], 90.00th=[  363], 95.00th=[  393],
     | 99.00th=[  430], 99.50th=[  439], 99.90th=[  460], 99.95th=[  464],
     | 99.99th=[  514]
    clat prio 0/0/0 (79.83% of IOs) percentiles (msec):
     |  1.00th=[   22],  5.00th=[   59], 10.00th=[   99], 20.00th=[  159],
     | 30.00th=[  201], 40.00th=[  236], 50.00th=[  266], 60.00th=[  292],
     | 70.00th=[  317], 80.00th=[  342], 90.00th=[  376], 95.00th=[  397],
     | 99.00th=[  435], 99.50th=[  443], 99.90th=[  460], 99.95th=[  464],
     | 99.99th=[  523]
    clat prio 2/0/1 (20.17% of IOs) percentiles (usec):
     |  1.00th=[13304],  5.00th=[15926], 10.00th=[17695], 20.00th=[20579],
     | 30.00th=[23200], 40.00th=[26346], 50.00th=[30016], 60.00th=[35390],
     | 70.00th=[41681], 80.00th=[49546], 90.00th=[55837], 95.00th=[60556],
     | 99.00th=[68682], 99.50th=[71828], 99.90th=[78119], 99.95th=[81265],
     | 99.99th=[88605]
   bw (  KiB/s): min=15872, max=23040, per=100.00%, avg=19774.78, stdev=1227.50, samples=600
   iops        : min=  124, max=  180, avg=154.39, stdev= 9.59, samples=600
  lat (msec)   : 10=0.02%, 20=4.21%, 50=15.23%, 100=8.89%, 250=27.77%
  lat (msec)   : 500=43.94%, 750=0.01%
  cpu          : usr=0.32%, sys=2.64%, ctx=46569, majf=0, minf=1555
  IO depths    : 1=0.1%, 2=0.1%, 4=0.1%, 8=0.1%, 16=0.2%, 32=99.7%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.1%, 64=0.0%, >=64=0.0%
     issued rwts: total=46320,0,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=32

Run status group 0 (all jobs):
   READ: bw=19.3MiB/s (20.2MB/s), 19.3MiB/s-19.3MiB/s (20.2MB/s-20.2MB/s), io=5794MiB (6075MB), run=300246-300246msec

Disk stats (read/write):
  sdg: ios=55498/0, sectors=14207488/0, merge=0/0, ticks=11493246/0, in_queue=11493246, util=100.00%
```

Looking at the 99th percentile of the completion latecies, we can see that the
20% of I/Os that were executed using the 50ms limit all complete under 68ms,
while I/Os that are not assigned a limit can take up to 435ms to complete.

Using the *cmdprio_bssplit* option, different duration limits can be combined
to create more complex workloads using multiple limits. For instance, the
following fio job definition:

```
[global]
filename=/dev/sdg
random_generator=tausworthe64
continue_on_error=none
ioscheduler=none
direct=1
write_lat_log=randread.log
per_job_logs=0
log_prio=1

[randread]
rw=randread
bs=128k
ioengine=libaio
iodepth=32
cmdprio_bssplit=128k/10/2/0/2:128k/20/2/0/3
```

will result in fio executing 10% of all I/Os using the best effor priority
class with the CDL descriptor 2 and 20% of all I/Os using again the best effort
priority class but with CDL descriptor 3. The remaining 70% of I/Os will not be
assigned any limit.

Assuming that descriptor 2 defines a duration guideline limit of 100ms and
descriptor 3 defines a duration guideline limit of 3, fio will produce
statistics similar to the following.

```
rndrd_qd32: (g=0): rw=randread, bs=(R) 128KiB-128KiB, (W) 128KiB-128KiB, (T) 128KiB-128KiB, ioengine=libaio, iodepth=32
fio-3.35-83-g0b47
Starting 1 process

rndrd_qd32: (groupid=0, jobs=1): err= 0: pid=3427: Tue Aug 15 15:53:06 2023
  read: IOPS=155, BW=19.5MiB/s (20.4MB/s)(5841MiB/300204msec)
    slat (usec): min=120, max=2029, avg=180.03, stdev=50.81
    clat (msec): min=8, max=504, avg=205.05, stdev=87.40
     lat (msec): min=8, max=504, avg=205.23, stdev=87.40
    clat prio 0/0/0 (msec): min=10, max=504, avg=235.55, stdev=70.42
    clat prio 2/0/2 (msec): min=9, max=219, avg=47.12, stdev=25.02
    clat prio 2/0/4 (msec): min=8, max=339, avg=179.51, stdev=63.85
    clat percentiles (msec):
     |  1.00th=[   18],  5.00th=[   35], 10.00th=[   62], 20.00th=[  127],
     | 30.00th=[  169], 40.00th=[  199], 50.00th=[  222], 60.00th=[  243],
     | 70.00th=[  262], 80.00th=[  284], 90.00th=[  309], 95.00th=[  326],
     | 99.00th=[  359], 99.50th=[  368], 99.90th=[  380], 99.95th=[  388],
     | 99.99th=[  418]
    clat prio 0/0/0 (69.42% of IOs) percentiles (msec):
     |  1.00th=[   37],  5.00th=[  100], 10.00th=[  138], 20.00th=[  182],
     | 30.00th=[  209], 40.00th=[  228], 50.00th=[  247], 60.00th=[  262],
     | 70.00th=[  279], 80.00th=[  296], 90.00th=[  317], 95.00th=[  334],
     | 99.00th=[  363], 99.50th=[  372], 99.90th=[  384], 99.95th=[  393],
     | 99.99th=[  422]
    clat prio 2/0/2 (10.10% of IOs) percentiles (msec):
     |  1.00th=[   14],  5.00th=[   17], 10.00th=[   20], 20.00th=[   24],
     | 30.00th=[   29], 40.00th=[   35], 50.00th=[   43], 60.00th=[   51],
     | 70.00th=[   59], 80.00th=[   70], 90.00th=[   84], 95.00th=[   96],
     | 99.00th=[  109], 99.50th=[  111], 99.90th=[  116], 99.95th=[  120],
     | 99.99th=[  220]
    clat prio 2/0/3 (20.48% of IOs) percentiles (msec):
     |  1.00th=[   24],  5.00th=[   58], 10.00th=[   87], 20.00th=[  126],
     | 30.00th=[  150], 40.00th=[  171], 50.00th=[  188], 60.00th=[  203],
     | 70.00th=[  220], 80.00th=[  236], 90.00th=[  257], 95.00th=[  275],
     | 99.00th=[  300], 99.50th=[  305], 99.90th=[  313], 99.95th=[  317],
     | 99.99th=[  338]
   bw (  KiB/s): min=14336, max=22784, per=100.00%, avg=19935.45, stdev=1124.89, samples=600
   iops        : min=  112, max=  178, avg=155.65, stdev= 8.78, samples=600
  lat (msec)   : 10=0.01%, 20=1.59%, 50=6.36%, 100=7.96%, 250=48.57%
  lat (msec)   : 500=35.57%, 750=0.01%
  cpu          : usr=0.34%, sys=3.16%, ctx=46932, majf=0, minf=1555
  IO depths    : 1=0.1%, 2=0.1%, 4=0.1%, 8=0.1%, 16=0.2%, 32=99.7%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.1%, 64=0.0%, >=64=0.0%
     issued rwts: total=46694,0,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=32

Run status group 0 (all jobs):
   READ: bw=19.5MiB/s (20.4MB/s), 19.5MiB/s-19.5MiB/s (20.4MB/s-20.4MB/s), io=5841MiB (6124MB), run=300204-300204msec

Disk stats (read/write):
  sdg: ios=55999/0, sectors=14335744/0, merge=0/0, ticks=11494014/0, in_queue=11494014, util=100.00%
```

Again observing the 99th percentile completion latency for each group of I/O
priority, we can see that I/Os using descriptor 2 all complete within 109ms, I/O
using descriptor 3 all complete within 300ms, while the remaining 70% of I/Os
with no limit may see completion times up to 363ms.

### CDL Benchmark Scripts

The *benchmark* directory contains a set of shell scripts allowing to easily run
various CDL workloads to evaluate a device. See the [README
file](benchmark/README.md) in the *benchmark* directory for more information on
how to use the scripts.

## Testing a system Command Duration Limits Support

The *cdl-tools* project includes a test suite to exercise a device supporting
command duration limits. Executing the test suite also allows testing the
host-bus-adapter and kernel being used on the test system.

> **Warning**: cdl-tools test suite is destructive. This means that it will
> overwrite the CDL descriptors in the T2A and T2B pages.
>
> If you want to keep your original limit descriptor settings set by your
> system administrator or HDD vendor, you must back up the T2A and T2B page
> manually, before running the test suite using:
>
> ```
> $ cdladm save --page T2A --file original_T2A.cdl /dev/sda
> $ cdladm save --page T2B --file original_T2B.cdl /dev/sda
> ```

*cdl-tools* test suite is written as a collection of bash scripts and can be
installed as follows as root.

```
$ sudo make install-tests
```

The default installation path is ```/usr/local/cdl-tests```. This installation
path can be changed using the ```--prefix``` option of the configure script.

The top script to use for executing tests is ```cdl-tests.sh```

```
$ cd /usr/local/cdl-tests
$ ./cdl-tests.sh --help
Usage: cdl-tests.sh [Options] <block device file>
Options:
  --help | -h             : This help message
  --list | -l             : List all tests
  --test | -t <test num>  : Execute only the specified test case. This
                            option can be specified multiple times.
  --group | -g <group num>: Execute only the tests belonging to the
                            specified test group. This option can be
                            specified multiple times.
  --repeat | -r <num>     : Repeat the execution of the selected test cases
                            <num> times (default: tests are executed once).
  --run-time <seconds>    : Specify fio run time in seconds. Short run
                            times can result in less reliable test results.
                            Default: 60 seconds
  --stop-on-error         : For test cases that expect an error, tell
                            fio to stop immediately when an IO error
                            is detected. Default: continue running
  --quick | -q            : Same as "--run-time 20 --stop-on-error"
  --logdir <log dir>      : Use this directory to store test log files.
                            default: logs/<bdev name>
```

The test cases can be listed using the option "--list".

```
$ cd /usr/local/cdl-tests
$ ./cdl-tests.sh --list
Group 00: cdladm and sysfs tests
  Test 0001: cdladm (get device information)
  Test 0002: cdladm (unsupported devices)
  Test 0010: cdladm (list CDL descriptors)
  Test 0011: cdladm (show CDL descriptors)
  Test 0012: cdladm (save CDL descriptors)
  Test 0013: cdladm (upload CDL descriptors)
  Test 0020: CDL sysfs (all attributes present)
  Test 0021: CDL (enable/disable)
Group 01: NCQ CDL reads single limit tests
  Test 0100: CDL dur. guideline (0x0 complete-earliest policy) reads
  Test 0101: CDL dur. guideline (0x1 continue-next-limit policy) reads
  Test 0102: CDL dur. guideline (0x2 continue-no-limit policy) reads
  Test 0103: CDL dur. guideline (0xd complete-unavailable policy) reads
  Test 0104: CDL dur. guideline (0xf abort policy) reads
  Test 0110: CDL active time (0x0 complete-earliest policy) reads
  Test 0111: CDL active time (0xd complete-unavailable policy) reads
  Test 0112: CDL active time (0xe abort-recovery policy) reads
  Test 0113: CDL active time (0xf abort policy) reads
  Test 0120: CDL inactive time (0x0 complete-earliest policy) reads
  Test 0121: CDL inactive time (0xd complete-unavailable policy) reads
  Test 0122: CDL inactive time (0xf abort policy) reads
Group 02: NCQ CDL reads multiple limits tests
  Test 0200: CDL active (0x0 policy) + inactive (0x0 policy) reads
  Test 0201: CDL active (0xd policy) + inactive (0xd policy) reads
  Test 0202: CDL active (0xd policy) + inactive (0xf policy) reads
  Test 0203: CDL active (0xf policy) + inactive (0xd policy) reads
  Test 0204: CDL active (0xf policy) + inactive (0xf policy) reads
  Test 0210: CDL active (0x0 policy) + CDL inactive (0x0 policy) reads
  Test 0211: CDL active (0xd policy) + CDL active (0xf policy) reads
  Test 0212: CDL active (0xd policy) + CDL inactive (0xd policy) reads
  Test 0213: CDL active (0xd policy) + CDL inactive (0xf policy) reads
  Test 0214: CDL active (0xf policy) + CDL inactive (0xd policy) reads
  Test 0215: CDL active (0xf policy) + CDL inactive (0xf policy) reads
  Test 0216: CDL inactive (0xd policy) + CDL inactive (0xf policy) reads
Group 03: NCQ CDL reads + no CDL writes tests
  Test 0300: CDL active (0x0 policy) reads + no CDL writes
  Test 0301: CDL active (0xd policy) reads + no CDL writes
  Test 0302: CDL active (0xe policy) reads + no CDL writes
  Test 0303: CDL active (0xf policy) reads + no CDL writes
  Test 0310: CDL inactive (0x0 policy) reads + no CDL writes
  Test 0311: CDL inactive (0xd policy) reads + no CDL writes
  Test 0312: CDL inactive (0xf abort policy) reads + no CDL writes
Group 04: NCQ CDL writes single limit tests
  Test 0400: CDL dur. guideline (0x0 complete-earliest policy) writes
  Test 0401: CDL dur. guideline (0x1 continue-next-limit policy) writes
  Test 0402: CDL dur. guideline (0x2 continue-no-limit policy) writes
  Test 0403: CDL dur. guideline (0xd complete-unavailable policy) writes
  Test 0404: CDL dur. guideline (0xf abort policy) writes
  Test 0410: CDL active time (0x0 complete-earliest policy) writes
  Test 0411: CDL active time (0xd complete-unavailable policy) writes
  Test 0412: CDL active time (0xe abort-recovery policy) writes
  Test 0413: CDL active time (0xf abort policy) writes
  Test 0420: CDL inactive time (0x0 complete-earliest policy) writes
  Test 0421: CDL inactive time (0xd complete-unavailable policy) writes
  Test 0422: CDL inactive time (0xf abort policy) writes
Group 05: Non-NCQ CDL reads tests
  Test 0500: CDL dur. guideline (0x0 complete-earliest policy) reads ncq=off
  Test 0501: CDL dur. guideline (0x1 continue-next-limit policy) reads ncq=off
  Test 0502: CDL dur. guideline (0x2 continue-no-limit policy) reads ncq=off
  Test 0503: CDL dur. guideline (0xd complete-unavailable policy) reads ncq=off
  Test 0504: CDL dur. guideline (0xf abort policy) reads ncq=off
  Test 0510: CDL active time (0x0 complete-earliest policy) reads ncq=off
  Test 0511: CDL active time (0xd complete-unavailable policy) reads ncq=off
  Test 0512: CDL active time (0xe abort-recovery policy) reads ncq=off
  Test 0513: CDL active time (0xf abort policy) reads ncq=off
  Test 0520: CDL inactive time (0x0 complete-earliest policy) reads ncq=off
  Test 0521: CDL inactive time (0xd complete-unavailable policy) reads ncq=off
  Test 0522: CDL inactive time (0xf abort policy) reads ncq=off
Group 06: Non-NCQ CDL writes tests
  Test 0600: CDL dur. guideline (0x0 complete-earliest policy) writes ncq=off
  Test 0601: CDL dur. guideline (0x1 continue-next-limit policy) writes ncq=off
  Test 0602: CDL dur. guideline (0x2 continue-no-limit policy) writes ncq=off
  Test 0603: CDL dur. guideline (0xd complete-unavailable policy) writes ncq=off
  Test 0604: CDL dur. guideline (0xf abort policy) writes ncq=off
  Test 0610: CDL active time (0x0 complete-earliest policy) writes ncq=off
  Test 0611: CDL active time (0xd complete-unavailable policy) writes ncq=off
  Test 0612: CDL active time (0xe abort-recovery policy) writes ncq=off
  Test 0613: CDL active time (0xf abort policy) writes ncq=off
  Test 0620: CDL inactive time (0x0 complete-earliest policy) writes ncq=off
  Test 0621: CDL inactive time (0xd complete-unavailable policy) writes ncq=off
  Test 0622: CDL inactive time (0xf abort policy) writes ncq=off
```

Executing the test suite requires root access rights.

```
$ cd /usr/local/cdl-tests
$ sudo ./cdl-tests.sh /dev/sdg
Running CDL tests on cmr /dev/sdg:
    Product: xxx  xxxxxxxxxxx
    Revision: WXYZ
    Device adapter driver: ahci
    Using cdl-tools version 0.5.0
fio: 60s run time, stop on error disabled

Group 00: cdladm and sysfs
  Test 0001:  cdladm (get device information)                                      ... PASS
  Test 0002:  cdladm (unsupported devices)                                         ... PASS
  Test 0010:  cdladm (list CDL descriptors)                                        ... PASS
  Test 0011:  cdladm (show CDL descriptors)                                        ... PASS
  Test 0012:  cdladm (save CDL descriptors)                                        ... PASS
  Test 0013:  cdladm (upload CDL descriptors)                                      ... PASS
  Test 0020:  CDL sysfs (all attributes present)                                   ... PASS
  Test 0021:  CDL (enable/disable)                                                 ... PASS
Group 01: NCQ CDL reads single limit
  Test 0100:  CDL dur. guideline (0x0 complete-earliest policy) reads              ... PASS
  ...
```

Log files for each test case are written by default in the "logs"
directory in the current working directory.
