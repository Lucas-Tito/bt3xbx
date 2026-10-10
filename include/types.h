#ifndef TYPES_H
#define TYPES_H

typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef long s64;
typedef unsigned long u64;
typedef float f32;

/* A variable read through a pointer of another width, as the PS2 (little-endian) reads it: the low T of x, and
   the 64-bit word that starts at x. The port bridges these for big-endian hosts (port/include/port_compat.h). */
#define PS2_LOW(T, x) (*(T *)&(x))
#define PS2_LD64(x) (*(u64 *)&(x))

#endif
