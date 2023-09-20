# system76-io-dkms
DKMS module for controlling the System76 Io board, which is used in System76's Thelio desktop line.

This driver provides hwmon interfaces for fan control, and tells the Io board
when the system is suspending. Decisions on fan speeds are made in
[system76-power](https://github.com/pop-os/system76-power).
