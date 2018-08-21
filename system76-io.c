/*
 * system76-io.c
 *
 * Copyright (C) 2018 Jeremy Soller <jeremy@system76.com>
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the  GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is  distributed in the hope that it  will be useful, but
 * WITHOUT  ANY   WARRANTY;  without   even  the  implied   warranty  of
 * MERCHANTABILITY  or FITNESS FOR  A PARTICULAR  PURPOSE.  See  the GNU
 * General Public License for more details.
 *
 * You should  have received  a copy of  the GNU General  Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>

#define IO_VENDOR 0x7676
#define IO_DEVICE 0x7676
#define IO_INTERFACE 1
#define IO_EP_IN 0x83
#define IO_EP_OUT 0x04
#define IO_MSG_SIZE 16
#define IO_TIMEOUT 1000

#include "system76-io_dev.c"
#include "system76-io_hwmon.c"

static int io_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    struct io_dev * io_dev;

	dev_info(&interface->dev, "id %04X:%04X interface %d probe\n", id->idVendor, id->idProduct, id->bInterfaceNumber);

	io_dev = kmalloc(sizeof(struct io_dev), GFP_KERNEL);
	if (IS_ERR_OR_NULL(io_dev)) {
		dev_err(&interface->dev, "kmalloc failed\n");

        return -ENOMEM;
	}

	memset(io_dev, 0, sizeof(struct io_dev));

    io_dev->usb_dev = usb_get_dev(interface_to_usbdev(interface));

    io_dev->hwmon_dev = hwmon_device_register_with_groups(&interface->dev, "system76_io", io_dev, io_groups);
    if (IS_ERR(io_dev->hwmon_dev)) {
		dev_err(&interface->dev, "hwmon_device_register_with_groups failed\n");

        usb_put_dev(io_dev->usb_dev);

        kfree(io_dev);

        return PTR_ERR(io_dev->hwmon_dev);
    }

    usb_set_intfdata(interface, io_dev);

	return 0;
}

static void io_disconnect(struct usb_interface *interface) {
    struct io_dev * io_dev;

	dev_info(&interface->dev, "disconnect\n");

    io_dev = usb_get_intfdata(interface);

    usb_set_intfdata(interface, NULL);

    hwmon_device_unregister(io_dev->hwmon_dev);

    usb_put_dev(io_dev->usb_dev);

    kfree(io_dev);
}

static struct usb_device_id io_table[] = {
        { USB_DEVICE_INTERFACE_NUMBER(IO_VENDOR, IO_DEVICE, IO_INTERFACE) },
        { }
};

MODULE_DEVICE_TABLE(usb, io_table);

static struct usb_driver io_driver = {
	.name        = "system76-io",
	.probe       = io_probe,
	.disconnect  = io_disconnect,
	.id_table    = io_table,
};

static int __init io_init(void) {
	return usb_register(&io_driver);
}

static void __exit io_exit(void) {
	usb_deregister(&io_driver);
}

module_init(io_init);
module_exit(io_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeremy Soller <jeremy@system76.com>");
MODULE_DESCRIPTION("System76 Io driver");
