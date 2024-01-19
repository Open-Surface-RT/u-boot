#include "i2c.h"

#define vu32 volatile u32

static void _i2c_wait(vu32 *base)
{
	u32 i;
	base[0x23] = 0x25;
	for (i = 0; i < 20; i++)
	{
		if (!(base[0x23] & 1))
			break;
	}
}

/**
 * @brief i2c send package
 * Sends a package to the I2C Bus
 * @param idx i2c controller
 * @param x slave address
 * @param buf data to be transmitted
 * @param size data size
 * @return int error code
 */
static int _i2c_send_pkt(u32 idx, u32 x, u8 *buf, u32 size)
{
	vu32 *base = (vu32 *) 0x7000d000; //i2c_addrs[idx];
	u32 tmp = 0;
	if (size > 4)
		return 0;

	//memcpy(&tmp, buf, size);
	tmp = *((unsigned int*)buf);

	base[1] = x << 1; //Set x (send mode). x = address + 0=write
	base[3] = tmp; //Set value. data register
	base[0] = (2 * size - 2) | 0x2800; //Set size and send mode.
	_i2c_wait(base); //Kick transaction.

	base[0] = (base[0] & 0xFFFFFDFF) | 0x200;
	while (base[7] & 0x100)
		;

	if (base[7] << 28)
		return 0;

	return 1;
}

static int _i2c_recv_pkt(u32 idx, u8 *buf, u32 size, u32 x)
{
	vu32 *base = (vu32 *) 0x7000d000; //i2c_addrs[idx];
	u32 tmp;

	if (size > 4)
		return 0;

	base[1] = (x << 1) | 1; //Set x (recv mode).
	base[0] = (2 * size - 2) | 0x2840; //Set size and recv mode.
	_i2c_wait(base); //Kick transaction.

	base[0] = (base[0] & 0xFFFFFDFF) | 0x200;
	while (base[7] & 0x100)
		;

	if (base[7] << 28)
		return 0;

	tmp = base[3]; //Get value.
	//memcpy(buf, &tmp, size);
	
	*((unsigned int*)buf) = tmp;

	return 1;
}

int i2c_recv_buf_small(u8 *buf, u32 size, u32 idx, u32 x, u32 y)
{
	int res = _i2c_send_pkt(idx, x, (u8 *)&y, 1);
	if (res)
		res = _i2c_recv_pkt(idx, buf, size, x);
	return res;
}
