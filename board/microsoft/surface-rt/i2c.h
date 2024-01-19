#ifndef _SURFACE_RT_I2C_H_
#define _SURFACE_RT_I2C_H_

typedef unsigned char u8;
typedef unsigned int u32;

int i2c_recv_buf_small(u8 *buf, u32 size, u32 idx, u32 x, u32 y);



#endif
