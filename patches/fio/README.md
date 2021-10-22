Copyright (C) 2021, Western Digital Corporation or its affiliates.

# <p align="center">fio Command Duration Limits Support Patches</p>

The patches in this directory add support for the Command Duration Limits
feature to fio. The patches apply against fio latest tree available from:

https://github.com/axboe/fio

The first patch is the only patch needed to support command duration limits in
fio.

The second patch adds the cmdprio_split option to facilitate creating workloads
that more than two command duration limits for read, write and read/write IOs.
