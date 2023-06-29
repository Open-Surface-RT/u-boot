/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Copyright (c) 2023, Svyatoslav Ryhel <clamor95@gmail.com>
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <linux/sizes.h>

#include "tegra114-common.h"

/* High-level configuration options */
#define CFG_TEGRA_BOARD_STRING		"NVIDIA TegraTab"

#define BOARD_EXTRA_ENV_SETTINGS \
	"button_cmd_0_name=Volume Down\0" \
	"button_cmd_0=fastboot usb 0\0" \
	"button_cmd_1_name=Hall Sensor\0" \
	"button_cmd_1=poweroff\0"

/* Board-specific serial config */
#define CFG_SYS_NS16550_COM1		NV_PA_APB_UARTD_BASE

#include "tegra-common-post.h"

#endif /* __CONFIG_H */
