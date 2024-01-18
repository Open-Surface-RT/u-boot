// SPDX-License-Identifier: GPL-2.0+

#include <common.h>
#include <dm.h>
#include <dm/devres.h>
#include <fdtdec.h>
#include <input.h>
#include <keyboard.h>
#include <log.h>
#include <stdio_dev.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/funcmux.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/types.h>
#include <i2c.h>
#include <asm/gpio.h>
#include <dm/device.h>

#include "hid.h"

#define I2C_HID_DESCRIPTOR_SIZE 30
#define I2C_HID_VERSION 0x0100

/* HID Descriptor according to spec */
struct hid_descriptor {
	uint16_t wHIDDescLength;
	uint16_t bcdVersion;
	uint16_t wReportDescLength;
	uint16_t wReportDescRegister;
	uint16_t wInputRegister;
	uint16_t wMaxInputLength;
	uint16_t wOutputRegister;
	uint16_t wMaxOutputLength;
	uint16_t wCommandRegister;
	uint16_t wDataRegister;
	uint16_t wVendorID;
	uint16_t wProductID;
	uint16_t wVersionID;
	uint32_t RESERVED;
} __attribute__((packed));

/* hid config and state */
struct hid_over_i2c_priv {
	struct input_config *input;	/* The input layer */
	struct gpio_desc irq_gpio;

	uint8_t* input_report;
	uint8_t* report_descriptor;

	struct hid_descriptor hid_descriptor;
};

/**
 * Read a Register from the I2C chip
 * \param dev as always
 * \param reg_addr 16Bit Register Address
 * \param out_buf output buffer
 * \param out_sz number of bytes that should be read
*/
static int hid_i2c_read_reg(struct udevice *dev, uint16_t reg_addr, u8 * out_buf, uint32_t out_sz ) {
 	struct dm_i2c_chip *chip = dev_get_parent_plat(dev);
 	struct i2c_msg i2c_msgs[2];
	int ret;
	
	printf("%s: %04x to %p with length %d\n", __func__, reg_addr, out_buf, out_sz);

	// Write Report Address (2 Bytes)
	i2c_msgs[0].addr = chip->chip_addr;
	i2c_msgs[0].flags = 0;
	i2c_msgs[0].len = sizeof(reg_addr);
	i2c_msgs[0].buf = (u8*)&reg_addr;

	// Read Report (n Bytes)
	i2c_msgs[1].addr = chip->chip_addr;
	i2c_msgs[1].flags = I2C_M_RD;
	i2c_msgs[1].len = out_sz;
	i2c_msgs[1].buf = out_buf;

	ret = dm_i2c_xfer(dev, i2c_msgs, 2);

	return ret;
}

static int get_hid_descriptor(struct udevice *dev, struct hid_descriptor* hid_descriptor) {
	uint16_t hid_descriptor_addr;
	int ret;
	uint32_t tmp;

	// Get HID Descriptor Address from devicetree
	ret = dev_read_u32(dev, "hid-descr-addr", &tmp);
	printf("ret: %d, hid-descr-addr: %04x\n", ret, tmp);
	hid_descriptor_addr = tmp;

	ret = hid_i2c_read_reg(dev, hid_descriptor_addr, (u8*)hid_descriptor, I2C_HID_DESCRIPTOR_SIZE);
	
	if (ret)
		return ret;

	// Verify the HID Descriptor
	if (hid_descriptor->wHIDDescLength != I2C_HID_DESCRIPTOR_SIZE ||
		hid_descriptor->bcdVersion != I2C_HID_VERSION) {
		return -EPROTO;
	}

	return 0;
}

static void print_hid_descriptor(struct hid_descriptor* hid_descriptor) {
	printf("wHIDDescLength: %#06x\n"
		"bcdVersion: %#06x\n"
		"wReportDescLength: %#06x\n"
		"wReportDescRegister: %#06x\n"
		"wInputRegister: %#06x\n"
		"wMaxInputLength: %#06x\n"
		"wOutputRegister: %#06x\n"
		"wMaxOutputLength: %#06x\n"
		"wCommandRegister: %#06x\n"
		"wDataRegister: %#06x\n"
		"wVendorID: %#06x\n"
		"wProductID: %#06x\n"
		"wVersionID: %#06x\n"
		"RESERVED: %#010x\n",
		hid_descriptor->wHIDDescLength,
		hid_descriptor->bcdVersion,
		hid_descriptor->wReportDescLength,
		hid_descriptor->wReportDescRegister,
		hid_descriptor->wInputRegister,
		hid_descriptor->wMaxInputLength,
		hid_descriptor->wOutputRegister,
		hid_descriptor->wMaxOutputLength,
		hid_descriptor->wCommandRegister,
		hid_descriptor->wDataRegister,
		hid_descriptor->wVendorID,
		hid_descriptor->wProductID,
		hid_descriptor->wVersionID,
		hid_descriptor->RESERVED);
}

static void print_report_descriptor(uint8_t* report_descriptor) {
	printf("Report Descriptor:\n");
	for (int i = 0; i < 0xeb; i++) { // TODO No hardcoded size!
		if (i % 16 == 0 && i != 0) {
			printf("\n");
		}
		printf("%02x ", report_descriptor[i]);
	}
	printf("\n");
}

static int get_report_descriptor(struct udevice *dev, uint8_t *report_descriptor_buf) {
	struct hid_over_i2c_priv *priv = dev_get_priv(dev);
	struct hid_descriptor *hid_descriptor = &priv->hid_descriptor;
	int ret;

	ret = hid_i2c_read_reg(dev, hid_descriptor->wReportDescRegister, report_descriptor_buf, hid_descriptor->wReportDescLength);

	return ret;
}

struct input_report {
	uint16_t length;
	uint8_t report_id;
	uint8_t* data;
} __attribute__((packed));

static int get_input_report(struct udevice *dev) {
	struct hid_over_i2c_priv *priv = dev_get_priv(dev);
	struct hid_descriptor *hid_descriptor = &priv->hid_descriptor;
	uint16_t read_len;
	int ret;

	printf("Get Input report\n");
	ret = hid_i2c_read_reg(dev, hid_descriptor->wMaxInputLength, (u8*)&read_len, sizeof(read_len));
	
	if(ret)
		return ret;
	printf("Report Size to read: %d\n", read_len);

	ret = hid_i2c_read_reg(dev, hid_descriptor->wInputRegister, priv->input_report, read_len);
	if(ret)
		return ret;

	// Verify Input Report
	struct input_report *in_rep = (struct input_report*)priv->input_report;
	printf("size: %d, id: %d", in_rep->length, in_rep->report_id);
	printf("Input Report data:\n");
	for (int i = 0; i < read_len-3; i++) {
		if (i % 16 == 0 && i != 0) {
			printf("\n");
		}
		printf("%02x ", in_rep->data[i]);
	}
	printf("\n");

	if (in_rep->length != read_len) {
		return -EPROTO;
	}

	return 0;
}

static int keys_check(struct input_config *input)
{
	struct hid_over_i2c_priv *priv = dev_get_priv(input->dev);
//	struct hid_descriptor *hid_descriptor = &priv->hid_descriptor;
	int ret;
	
	// Check if interrupt is asserted
	ret  = dm_gpio_get_value(&priv->irq_gpio);
	if (ret < 0) { // error
		return ret;
	}

	if (ret == 1) { // No interrupt asserted and thus no data available.
		return -1;
	}

	printf("hid: time to fetch a report\n");


	// fetch input report
	get_input_report(input->dev);

	// decode input report

	// write to stdin

	return 0;
}

static int hid_over_i2c_probe(struct udevice *dev)
{
	struct hid_over_i2c_priv *priv = dev_get_priv(dev);
	struct keyboard_priv *uc_priv = dev_get_uclass_priv(dev);
	struct stdio_dev *sdev = &uc_priv->sdev;
	struct input_config *input = &uc_priv->input;
	int ret;

	ret = get_hid_descriptor(dev, &priv->hid_descriptor);
	print_hid_descriptor(&priv->hid_descriptor); // TODO debug only
	if (ret)
		return ret;

	// Alloc memory for reports
	priv->report_descriptor = devm_kzalloc(dev, priv->hid_descriptor.wReportDescLength, 0);
	priv->input_report = devm_kzalloc(dev, priv->hid_descriptor.wMaxInputLength, 0);

	// Get Report Descriptor and print it.
	// Eventually skip ReportDescriptor as we only need BootMode.
	get_report_descriptor(dev, priv->report_descriptor);
	print_report_descriptor(priv->report_descriptor);
	//parse_report_descriptor();

	// Request GPIO
	gpio_request_by_name(dev, "irq", 0, &priv->irq_gpio, GPIOD_IS_IN);

	// Configure Input.
	input_set_delays(input, 240, 30);
	input_allow_repeats(input, false);
	input_add_tables(input, false);

	priv->input = input;
	input->dev = dev;
	input->read_keys = keys_check;
	strcpy(sdev->name, "hid-over-i2c");

//	ret = input_stdio_register(sdev);
	if (ret) {
		debug("%s: input_stdio_register() failed\n", __func__);
		return ret;
	}

	get_input_report(dev);

	return 0;
}

static const struct keyboard_ops hid_over_i2c_ops = {
};

static const struct udevice_id hid_over_i2c_ids[] = {
	{ .compatible = "hid-over-i2c", 0 },
	{ }
};

U_BOOT_DRIVER(hid_over_i2c_driver) = {
	.name	= "hid_over_i2c_input",
	.id	= UCLASS_KEYBOARD,
	.of_match = hid_over_i2c_ids,
	.probe = hid_over_i2c_probe,
	.ops	= &hid_over_i2c_ops,
	.priv_auto	= sizeof(struct hid_over_i2c_priv),
};
