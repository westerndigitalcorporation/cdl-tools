Copyright (C) 2021, Western Digital Corporation or its affiliates.

# <p align="center">fio Command Duration Limits Support Patches</p>

The patches in this directory add support for the Command Duration Limits
feature to fio. The patches apply against fio latest tree available from:

https://github.com/axboe/fio

This is the only patch needed to support command duration limits in fio.

Note that since fio 3.29, completion latencies are reported per IO priority
level, that is, per CDL limit used.
