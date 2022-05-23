/*
 * system76-io_hwmon.c
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

#define IO_FAN(N, I)

#define IO_FANS \
    IO_FAN(CPUF, 1) \
    IO_FAN(INTF, 2)

static const char * io_fan_name(int index) {
    switch (index) {
        #undef IO_FAN
        #define IO_FAN(N, I) \
            case I: \
                return #N;
        IO_FANS
        default:
            return NULL;
    }
}

static ssize_t io_fan_input_show(struct device *dev, struct device_attribute *attr, char *buf) {
    const char *name;
    u16 value;
    int ret;

    struct io_dev * io_dev = dev_get_drvdata(dev);

    mutex_lock(&io_dev->lock);

    if ((name = io_fan_name(to_sensor_dev_attr(attr)->index))) {
        ret = io_dev_tach(io_dev, name, &value, IO_TIMEOUT);
        if (!ret) {
            ret = sprintf(buf, "%i\n", value * 30);
        }
    } else {
        ret = -ENOENT;
    }

    mutex_unlock(&io_dev->lock);

    return ret;
}

static ssize_t io_fan_label_show(struct device *dev, struct device_attribute *attr, char *buf) {
    int ret;

    const char * name = io_fan_name(to_sensor_dev_attr(attr)->index);
    if (name) {
        ret = sprintf(buf, "%s\n", name);
    } else {
        ret = -ENOENT;
    }

    return ret;
}

static ssize_t io_pwm_show(struct device *dev, struct device_attribute *attr, char *buf) {
    const char *name;
    u16 value;
    int ret;

    struct io_dev * io_dev = dev_get_drvdata(dev);

    mutex_lock(&io_dev->lock);

    if ((name = io_fan_name(to_sensor_dev_attr(attr)->index))) {
        ret = io_dev_duty(io_dev, name, &value, IO_TIMEOUT);
        if (!ret) {
            ret = sprintf(buf, "%i\n", (((u32)value) * 255) / 10000);
        }
    } else {
        ret = -ENOENT;
    }

    mutex_unlock(&io_dev->lock);

    return ret;
}

static ssize_t io_pwm_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	const char *name;
  	u32 value;
  	int ret;

    struct io_dev * io_dev = dev_get_drvdata(dev);

    mutex_lock(&io_dev->lock);

    if ((name = io_fan_name(to_sensor_dev_attr(attr)->index))) {
      	ret = kstrtou32(buf, 10, &value);
      	if (!ret) {
            if (value <= 255) {
                ret = io_dev_set_duty(io_dev, name, (u16)((value * 10000) / 255), IO_TIMEOUT);
                if (!ret) {
                    ret = count;
                }
            } else {
                ret = -EINVAL;
            }
        }
    } else {
        ret = -ENOENT;
    }

    mutex_unlock(&io_dev->lock);

    return ret;
}

static ssize_t io_pwm_enable_show(struct device *dev, struct device_attribute *attr, char *buf) {
    int ret;

    const char * name = io_fan_name(to_sensor_dev_attr(attr)->index);
    if (name) {
        ret = sprintf(buf, "%i\n", 1);
    } else {
        ret = -ENOENT;
    }

    return ret;
}

static ssize_t io_pwm_enable_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
  	u8 value;
  	int ret;

    const char * name = io_fan_name(to_sensor_dev_attr(attr)->index);
    if (name) {
        ret = kstrtou8(buf, 10, &value);
      	if (!ret) {
            if (value == 1) {
                ret = count;
            } else {
                ret = -EINVAL;
            }
        }
    } else {
        ret = -ENOENT;
    }

    return ret;
}

#undef IO_FAN
#define IO_FAN(N, I) \
    static SENSOR_DEVICE_ATTR(fan ## I ## _input, S_IRUGO, io_fan_input_show, NULL, I); \
    static SENSOR_DEVICE_ATTR(fan ## I ## _label, S_IRUGO, io_fan_label_show, NULL, I); \
    static SENSOR_DEVICE_ATTR(pwm ## I, S_IRUGO |  S_IWUSR, io_pwm_show, io_pwm_set, I); \
    static SENSOR_DEVICE_ATTR(pwm ## I ## _enable, S_IRUGO |  S_IWUSR, io_pwm_enable_show, io_pwm_enable_set, I);
IO_FANS

static struct attribute *io_attrs[] = {
    #undef IO_FAN
    #define IO_FAN(N, I) \
        &sensor_dev_attr_fan ## I ## _input.dev_attr.attr, \
        &sensor_dev_attr_fan ## I ## _label.dev_attr.attr, \
        &sensor_dev_attr_pwm ## I.dev_attr.attr, \
        &sensor_dev_attr_pwm ## I ## _enable.dev_attr.attr,
	IO_FANS
	NULL
};

ATTRIBUTE_GROUPS(io);
