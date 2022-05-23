/*
 * system76-io_dev.c
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

struct io_dev {
    struct mutex lock;
    struct usb_device * usb_dev;
    struct device * hwmon_dev;
#ifdef CONFIG_PM_SLEEP
    struct notifier_block pm_notifier;
#endif
    char command[IO_MSG_SIZE];
    char partial[IO_MSG_SIZE];
    char lines[2][IO_MSG_SIZE];
    char response[IO_MSG_SIZE];
};

static ssize_t io_dev_read(struct io_dev * io_dev, char * buf, size_t len, int timeout) {
    int result;
    int count;

    result = usb_bulk_msg(
        io_dev->usb_dev,
        usb_rcvbulkpipe(io_dev->usb_dev, IO_EP_IN),
        (void *)buf,
        len,
        &count,
        timeout
    );
    if (result) {
        return result;
    }

    return count;
}

static ssize_t io_dev_write(struct io_dev * io_dev, const char * buf, size_t len, int timeout) {
    int result;
    int count;

    result = usb_bulk_msg(
        io_dev->usb_dev,
        usb_sndbulkpipe(io_dev->usb_dev, IO_EP_OUT),
        (void *)buf,
        len,
        &count,
        timeout
    );
    if (result) {
        return result;
    }

    return count;
}

static int io_dev_command(struct io_dev * io_dev, const char * command, size_t clen, char * response, size_t rlen, int timeout) {
    int result;
    bool cr;
    bool lf;
    int i;
    char c;
    int lines_i;
    int line_i;
    bool error;

    memset(response, 0, rlen);

    result = io_dev_write(io_dev, command, clen, timeout);
    if (result < 0) {
        snprintf(response, rlen, "io_dev_write");
        return result;
    }

    cr = 0;
    lf = 0;
    lines_i = 0;
    line_i = 0;
    for (;;) {
        result = io_dev_read(io_dev, io_dev->partial, IO_MSG_SIZE, timeout);
        if (result < 0) {
            snprintf(response, rlen, "io_dev_read");
            return result;
        }

        for (i = 0; i < result; i++) {
            c = io_dev->partial[i];
            if (c == '\r') {
                if (!cr) {
                    cr = 1;
                } else {
                    // Unexpected \r, return error
                    snprintf(response, rlen, "Unexpected CR");
                    return -EINVAL;
                }
            } else if (c == '\n') {
                if (cr) {
                    cr = 0;
                    if (lf) {
                        // Received a response in full
                        if (lines_i < 2 && line_i < IO_MSG_SIZE) {
                            io_dev->lines[lines_i++][line_i] = 0;
                            line_i = 0;
                        } else {
                            snprintf(response, rlen, "Too many lines");
                            return -EINVAL;
                        }
                    }
                    lf = !lf;
                } else {
                    // Unexpected \n, return error
                    snprintf(response, rlen, "Unexpected LF");
                    return -EINVAL;
                }
            } else if (!cr && lf) {
                // Received a response byte
                if (lines_i < 2 && line_i < IO_MSG_SIZE) {
                    io_dev->lines[lines_i][line_i++] = c;
                } else {
                    // Response too long
                    snprintf(response, rlen, "Too many chars");
                    return -EINVAL;
                }
            } else {
                // Unexpected data, return error
                snprintf(response, rlen, "Unexpected char");
                return -EINVAL;
            }
        }

        if (lines_i > 0) {
            if (strcmp(io_dev->lines[lines_i - 1], "OK") == 0) {
                error = 0;
                break;
            } else if (strcmp(io_dev->lines[lines_i - 1], "ERROR") == 0) {
                error = 1;
            }
        }
    }

    if (lines_i > 1) {
        snprintf(response, rlen, "%s", io_dev->lines[lines_i - 2]);
    }

    if (error) {
        return -EIO;
    } else {
        return 0;
    }
}

static int io_dev_bootloader(struct io_dev * io_dev, int timeout) {
    int len;
    int result;

    len = snprintf(io_dev->command, IO_MSG_SIZE, "IoBOOT\r");
    if (len >= IO_MSG_SIZE) {
        return -EINVAL;
    }

    result = io_dev_command(io_dev, io_dev->command, len, io_dev->response, IO_MSG_SIZE, timeout);
    if (result) {
        dev_err(&io_dev->usb_dev->dev, "io_dev_boot failed: %d: %s\n", -result, io_dev->response);
        return result;
    }

    return 0;
}

static int io_dev_reset(struct io_dev * io_dev, int timeout) {
    int len;
    int result;

    len = snprintf(io_dev->command, IO_MSG_SIZE, "IoRSET\r");
    if (len >= IO_MSG_SIZE) {
        return -EINVAL;
    }

    result = io_dev_command(io_dev, io_dev->command, len, io_dev->response, IO_MSG_SIZE, timeout);
    if (result) {
        dev_err(&io_dev->usb_dev->dev, "io_dev_reset failed: %d: %s\n", -result, io_dev->response);
        return result;
    }

    return 0;
}

static int io_dev_tach(struct io_dev * io_dev, const char * device, u16 * value, int timeout) {
    int len;
    int result;

    if (strlen(device) != 4) {
        return -EINVAL;
    }

    len = snprintf(io_dev->command, IO_MSG_SIZE, "IoTACH%s\r", device);
    if (len >= IO_MSG_SIZE) {
        return -EINVAL;
    }

    result = io_dev_command(io_dev, io_dev->command, len, io_dev->response, IO_MSG_SIZE, timeout);
    if (result) {
        dev_err(&io_dev->usb_dev->dev, "io_dev_tach failed: %d: %s\n", -result, io_dev->response);
        return result;
    }

    return kstrtou16(io_dev->response, 16, value);
}

static int io_dev_duty(struct io_dev * io_dev, const char * device, u16 * value, int timeout) {
    int len;
    int result;

    if (strlen(device) != 4) {
        return -EINVAL;
    }

    len = snprintf(io_dev->command, IO_MSG_SIZE, "IoDUTY%s\r", device);
    if (len >= IO_MSG_SIZE) {
        return -EINVAL;
    }

    result = io_dev_command(io_dev, io_dev->command, len, io_dev->response, IO_MSG_SIZE, timeout);
    if (result) {
        dev_err(&io_dev->usb_dev->dev, "io_dev_duty failed: %d: %s\n", -result, io_dev->response);
        return result;
    }

    return kstrtou16(io_dev->response, 16, value);
}

static int io_dev_set_duty(struct io_dev * io_dev, const char * device, u16 value, int timeout) {
    int len;
    int result;

    if (strlen(device) != 4) {
        return -EINVAL;
    }

    if (value > 10000) {
        return -EINVAL;
    }

    len = snprintf(io_dev->command, IO_MSG_SIZE, "IoDUTY%s%04X\r", device, value);
    if (len >= IO_MSG_SIZE) {
        return -EINVAL;
    }

    result = io_dev_command(io_dev, io_dev->command, len, io_dev->response, IO_MSG_SIZE, timeout);
    if (result) {
        dev_err(&io_dev->usb_dev->dev, "io_dev_set_duty failed: %d: %s\n", -result, io_dev->response);
        return result;
    }

    return 0;
}

static int io_dev_set_suspend(struct io_dev * io_dev, u16 value, int timeout) {
    int len;
    int result;

    if (value > 1) {
        return -EINVAL;
    }

    len = snprintf(io_dev->command, IO_MSG_SIZE, "IoSUSP%04X\r", value);
    if (len >= IO_MSG_SIZE) {
        return -EINVAL;
    }

    result = io_dev_command(io_dev, io_dev->command, len, io_dev->response, IO_MSG_SIZE, timeout);
    if (result) {
        dev_err(&io_dev->usb_dev->dev, "io_dev_set_suspend failed: %d: %s\n", -result, io_dev->response);
        return result;
    }

    return 0;
}

static int io_dev_revision(struct io_dev * io_dev, char * value, int value_len, int timeout) {
    int len;
    int result;

    len = snprintf(io_dev->command, IO_MSG_SIZE, "IoREVISION\r");
    if (len >= IO_MSG_SIZE) {
        return -EINVAL;
    }

    result = io_dev_command(io_dev, io_dev->command, len, value, value_len, timeout);
    if (result) {
        dev_err(&io_dev->usb_dev->dev, "io_dev_revision failed: %d: %s\n", -result, value);
        return result;
    }

    return strlen(value);
}
