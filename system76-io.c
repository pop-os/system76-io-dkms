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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>

static struct usb_device_id io_table[] = {
        { USB_DEVICE(0x7676, 0x7676) },
        { }
};
MODULE_DEVICE_TABLE(usb, io_table);

static int io_probe(struct usb_interface *interface, const struct usb_device_id *id) {
	printk(KERN_INFO "system76-io: %04X:%04X probe\n", id->idVendor, id->idProduct);
	return 0;
}

static void io_disconnect(struct usb_interface *interface) {
	printk(KERN_INFO "system76-io: disconnect\n");
}

static struct usb_driver io_driver = {
	.name        = "system76-io",
	.id_table    = io_table,
	.probe       = io_probe,
	.disconnect  = io_disconnect,
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
