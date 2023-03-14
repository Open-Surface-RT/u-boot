.. SPDX-License-Identifier: GPL-2.0+

U-Boot for the ASUS Transformer device family
=============================================

Quick Start
-----------

- Build U-Boot
- Boot U-Boot by loading it into RAM (coldboot)
- Chainloading U-Boot from the vendor bootloader
- Boot

Build U-Boot
------------

U-Boot can be built in two forms: U-Boot with SPL, which is used for booting
by loading directly into RAM and U-Boot without SPL, which can be flashed
and chainloaded from the vendor bootloader.

To build U-Boot with SPL proseed:

.. code-block:: bash

    $ export CROSS_COMPILE=arm-linux-gnueabi-
    $ make tf701t_defconfig
    $ make

After the build succeeds, you will obtain the final ``u-boot-dtb-tegra.bin``
image, ready for booting by loading into RAM.

To build U-Boot without SPL proseed:

.. code-block:: bash

    $ export CROSS_COMPILE=arm-linux-gnueabi-
    $ make tf701t_defconfig chainload.config
    $ make

After the build succeeds, you will obtain the final ``u-boot-dtb.bin``
file, which can be further processed into a flashable image.

Boot U-Boot by loading it into RAM (coldboot)
---------------------------------------------

Done fairly simply by using fusee-tools (using run_uboot.sh) and
``u-boot-dtb-tegra.bin`` generated on the previous step. This method requires
constant access to the host PC or payloader and can fully eliminate influence
of the vendor bootloader onto the boot process.

Chainloading U-Boot from the vendor bootloader
----------------------------------------------

``u-boot-dtb.bin`` has to be further packed into Android boot image form,
where ``u-boot-dtb.bin`` acts as kernel, while dtb and ramdisk parts should
not be included. Then the generated boot image can be flashed into the /boot
partition of the tablet using Fastboot and will act as the bootloader of
the last stage.

Boot
----
In both cases after U-Boot obtains control it performs search of extlinux.conf
first on the dock USB device is available, then on MicroSD card if available.
If none of the devices above are present, then the device is turned off. eMMC
is not used and is not recommended as boot source since it contains TegraPT
which is not supported by U-Boot.

If during boot of U-Boot Volume Down button is pressed, the device will enter
U-Boot Fastboot mode with all the benefits it provides.
