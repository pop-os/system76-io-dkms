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
#include <linux/suspend.h>
#include <linux/usb.h>

#define IO_VENDOR 0x1209
#define IO_DEVICE 0x1776
#define IO_INTF_CTRL 0
#define IO_EP_CTRL 0x00
#define IO_INTF_DATA 1
#define IO_EP_IN 0x83
#define IO_EP_OUT 0x04
#define IO_MSG_SIZE 32
#define IO_TIMEOUT 1000

#include "system76-io_dev.c"
#include "system76-io_hwmon.c"

#define BAUD 1000000

static u8 line_encoding[7] = {
    (u8)BAUD,
    (u8)(BAUD >> 8),
    (u8)(BAUD >> 16),
    (u8)(BAUD >> 24),
    0,
    0,
    8
};

static ssize_t show_bootloader(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", 0);
}

static ssize_t set_bootloader(struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {
    struct io_dev * io_dev = dev_get_drvdata(dev);

    unsigned int val;
    int ret;

    ret = kstrtouint(buf, 10, &val);
    if (ret) {
        return ret;
    }

    if (val) {
        ret = io_dev_bootloader(io_dev, IO_TIMEOUT);
        if(ret) {
            return ret;
        }
    }

    return size;
}

static DEVICE_ATTR(bootloader, S_IRUGO | S_IWUSR, show_bootloader, set_bootloader);

static ssize_t show_revision(struct device *dev, struct device_attribute *attr, char *buf) {
    struct io_dev * io_dev = dev_get_drvdata(dev);

    return io_dev_revision(io_dev, buf, PAGE_SIZE, IO_TIMEOUT);
}

static DEVICE_ATTR(revision, S_IRUGO, show_revision, NULL);

#ifdef CONFIG_PM_SLEEP
static int io_pm(struct notifier_block *nb, unsigned long action, void *data) {
    struct io_dev * io_dev = container_of(nb, struct io_dev, pm_notifier);

    switch (action) {
        case PM_HIBERNATION_PREPARE:
        case PM_SUSPEND_PREPARE:
            io_dev_set_suspend(io_dev, 1, IO_TIMEOUT);
            break;

        case PM_POST_HIBERNATION:
        case PM_POST_SUSPEND:
            io_dev_set_suspend(io_dev, 0, IO_TIMEOUT);
            break;

        case PM_POST_RESTORE:
        case PM_RESTORE_PREPARE:
        default:
            break;
    }

    return NOTIFY_DONE;
}
#endif

static int io_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    int retry;
    int result;
    struct io_dev * io_dev;

    dev_info(&interface->dev, "id %04X:%04X interface %d probe\n", id->idVendor, id->idProduct, id->bInterfaceNumber);

    if (id->bInterfaceNumber == IO_INTF_CTRL) {
        result = usb_control_msg(
            interface_to_usbdev(interface),
            usb_sndctrlpipe(interface_to_usbdev(interface), IO_EP_CTRL),
            0x22,
            0x21,
            0x03,
            0,
            NULL,
            0,
            IO_TIMEOUT
        );
        if (result < 0) {
            dev_err(&interface->dev, "set line state failed: %d\n", -result);
            return result;
        }


        result = usb_control_msg(
            interface_to_usbdev(interface),
            usb_sndctrlpipe(interface_to_usbdev(interface), IO_EP_CTRL),
            0x20,
            0x21,
            0,
            0,
            line_encoding,
            7,
            IO_TIMEOUT
        );
        if (result < 0) {
            dev_err(&interface->dev, "set line encoding failed: %d\n", -result);
            return result;
        }

        return 0;
    } else if (id->bInterfaceNumber == IO_INTF_DATA) {
        io_dev = kmalloc(sizeof(struct io_dev), GFP_KERNEL);
        if (IS_ERR_OR_NULL(io_dev)) {
            dev_err(&interface->dev, "kmalloc failed\n");
            return -ENOMEM;
        }

        memset(io_dev, 0, sizeof(struct io_dev));

        io_dev->usb_dev = usb_get_dev(interface_to_usbdev(interface));

        usb_set_intfdata(interface, io_dev);

        for(retry = 0; retry < 8; retry++) {
            dev_info(&interface->dev, "trying reset: %d\n", retry);
            result = io_dev_reset(io_dev, IO_TIMEOUT);
            if (result != -ETIMEDOUT) {
                break;
            }
        }
        if (result) {
            dev_err(&interface->dev, "io_dev_reset failed: %d\n", result);
            goto fail1;
        }

        result = device_create_file(&interface->dev, &dev_attr_bootloader);
        if (result) {
            dev_err(&interface->dev, "device_create_file failed: %d\n", result);
            goto fail1;
        }

        result = device_create_file(&interface->dev, &dev_attr_revision);
        if (result) {
            dev_err(&interface->dev, "device_create_file failed: %d\n", result);
            goto fail2;
        }

        io_dev->hwmon_dev = hwmon_device_register_with_groups(&interface->dev, "system76_io", io_dev, io_groups);
        if (IS_ERR(io_dev->hwmon_dev)) {
            result = PTR_ERR(io_dev->hwmon_dev);

            dev_err(&interface->dev, "hwmon_device_register_with_groups failed: %d\n", result);
            goto fail3;
        }

#ifdef CONFIG_PM_SLEEP
        io_dev->pm_notifier.notifier_call = io_pm;
        register_pm_notifier(&io_dev->pm_notifier);
#endif

        return 0;

    fail3:
        device_remove_file(&interface->dev, &dev_attr_revision);
    fail2:
        device_remove_file(&interface->dev, &dev_attr_bootloader);
    fail1:
        usb_set_intfdata(interface, NULL);
        usb_put_dev(io_dev->usb_dev);
        kfree(io_dev);
        return result;
    } else {
        return -ENODEV;
    }
}

static void io_disconnect(struct usb_interface *interface) {
    struct io_dev * io_dev;

    dev_info(&interface->dev, "disconnect\n");

    io_dev = usb_get_intfdata(interface);

    if (io_dev) {
#ifdef CONFIG_PM_SLEEP
        unregister_pm_notifier(&io_dev->pm_notifier);
#endif

        hwmon_device_unregister(io_dev->hwmon_dev);

        device_remove_file(&interface->dev, &dev_attr_revision);

        device_remove_file(&interface->dev, &dev_attr_bootloader);

        usb_set_intfdata(interface, NULL);

        usb_put_dev(io_dev->usb_dev);

        kfree(io_dev);
    }
}

static struct usb_device_id io_table[] = {
        { USB_DEVICE_INTERFACE_NUMBER(IO_VENDOR, IO_DEVICE, IO_INTF_CTRL) },
        { USB_DEVICE_INTERFACE_NUMBER(IO_VENDOR, IO_DEVICE, IO_INTF_DATA) },
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
