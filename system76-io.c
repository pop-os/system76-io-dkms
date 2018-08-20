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

struct io_dev {
    struct usb_device * usb_dev;
    struct device * hwmon_dev;
};

static ssize_t s76_hwmon_show_name(struct device *dev, struct device_attribute *attr, char *buf) {
	return sprintf(buf, "system76-io\n");
}

static ssize_t io_fan_input_show(struct device *dev, struct device_attribute *attr, char *buf) {
	int index = to_sensor_dev_attr(attr)->index;
	return sprintf(buf, "%i\n", 1000);
}

static ssize_t io_fan_label_show(struct device *dev, struct device_attribute *attr, char *buf) {
	switch (to_sensor_dev_attr(attr)->index) {
	case 0:
		return sprintf(buf, "CPU fan\n");
	case 1:
		return sprintf(buf, "Intake fan\n");
	case 2:
		return sprintf(buf, "Exhaust fan\n");
	}
	return 0;
}

static ssize_t io_pwm_show(struct device *dev, struct device_attribute *attr, char *buf) {
	int index = to_sensor_dev_attr(attr)->index;
	return sprintf(buf, "%i\n", 255);
}

static ssize_t io_pwm_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	u32 value;
	int err;
	int index = to_sensor_dev_attr(attr)->index;

	err = kstrtou32(buf, 10, &value);
	if (err) {
        return err;
    }

    if (value > 255) {
        return -EINVAL;
    }

	return count;
}

static ssize_t io_pwm_enable_show(struct device *dev, struct device_attribute *attr, char *buf) {
	int index = to_sensor_dev_attr(attr)->index;
	return sprintf(buf, "%i\n", 0);
}

static ssize_t io_pwm_enable_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	u32 value;
	int err;
	int index = to_sensor_dev_attr(attr)->index;

	err = kstrtou32(buf, 10, &value);
	if (err) {
        return err;
    }

	if (value == 0) {

	} else if (value == 1) {

	} else if (value == 2) {

	} else {
        return -EINVAL;
    }

	return count;
}

static SENSOR_DEVICE_ATTR(name, S_IRUGO, s76_hwmon_show_name, NULL, 0);
// CPU Fan
static SENSOR_DEVICE_ATTR(fan1_input, S_IRUGO, io_fan_input_show, NULL, 0);
static SENSOR_DEVICE_ATTR(fan1_label, S_IRUGO, io_fan_label_show, NULL, 0);
static SENSOR_DEVICE_ATTR(pwm1, S_IRUGO |  S_IWUSR, io_pwm_show, io_pwm_set, 0);
static SENSOR_DEVICE_ATTR(pwm1_enable, S_IRUGO |  S_IWUSR, io_pwm_enable_show, io_pwm_enable_set, 0);
// Intake Fan
static SENSOR_DEVICE_ATTR(fan2_input, S_IRUGO, io_fan_input_show, NULL, 1);
static SENSOR_DEVICE_ATTR(fan2_label, S_IRUGO, io_fan_label_show, NULL, 1);
static SENSOR_DEVICE_ATTR(pwm2, S_IRUGO |  S_IWUSR, io_pwm_show, io_pwm_set, 1);
static SENSOR_DEVICE_ATTR(pwm2_enable, S_IRUGO |  S_IWUSR, io_pwm_enable_show, io_pwm_enable_set, 1);
// Exhaust Fan
static SENSOR_DEVICE_ATTR(fan3_input, S_IRUGO, io_fan_input_show, NULL, 2);
static SENSOR_DEVICE_ATTR(fan3_label, S_IRUGO, io_fan_label_show, NULL, 2);
static SENSOR_DEVICE_ATTR(pwm3, S_IRUGO |  S_IWUSR, io_pwm_show, io_pwm_set, 2);
static SENSOR_DEVICE_ATTR(pwm3_enable, S_IRUGO |  S_IWUSR, io_pwm_enable_show, io_pwm_enable_set, 2);

static struct attribute *io_attrs[] = {
	&sensor_dev_attr_name.dev_attr.attr,
	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_fan1_label.dev_attr.attr,
	&sensor_dev_attr_pwm1.dev_attr.attr,
	&sensor_dev_attr_pwm1_enable.dev_attr.attr,
	&sensor_dev_attr_fan2_input.dev_attr.attr,
	&sensor_dev_attr_fan2_label.dev_attr.attr,
	&sensor_dev_attr_pwm2.dev_attr.attr,
	&sensor_dev_attr_pwm2_enable.dev_attr.attr,
	&sensor_dev_attr_fan3_input.dev_attr.attr,
	&sensor_dev_attr_fan3_label.dev_attr.attr,
	&sensor_dev_attr_pwm3.dev_attr.attr,
	&sensor_dev_attr_pwm3_enable.dev_attr.attr,
	NULL
};

static const struct attribute_group io_group = {
    .attrs = io_attrs
};

static const struct attribute_group * io_groups[] = {
    &io_group,
    NULL
};

static int io_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    struct io_dev * io_dev;

	dev_info(&interface->dev, "%04X:%04X probe\n", id->idVendor, id->idProduct);

	io_dev = kmalloc(sizeof(struct io_dev), GFP_KERNEL);
	if (io_dev == NULL) {
		dev_err(&interface->dev, "kmalloc failed\n");
        return -ENOMEM;
	}

	memset(io_dev, 0, sizeof(struct io_dev));

    io_dev->usb_dev = usb_get_dev(interface_to_usbdev(interface));
    io_dev->hwmon_dev = hwmon_device_register_with_groups(&io_dev->usb_dev->dev, "system76-io", io_dev, io_groups);

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
        { USB_DEVICE(0x7676, 0x7676) },
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
