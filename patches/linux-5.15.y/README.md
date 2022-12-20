Copyright (C) 2022, Western Digital Corporation or its affiliates.

# <p align="center">Kernel 5.15.y Command Duration Limits Support Patches</p>

The patches in this directory add support for the Command Duration Limits
feature to the LTS Linux kernel version 5.15. The patches apply against the
linux-5.15.y branch of the stable linux tree available at:

https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git

or

git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git

This patch series adds command duration limits support to both SAS and SATA
hard disks.

For SCSI/SAS disks, read and write commands failed due to a duration limit being
exceeded with a fast-fail policy result in the application seeing ETIME errors
for the failed read/write operations.
