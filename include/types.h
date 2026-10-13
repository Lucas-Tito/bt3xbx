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

/* A 64-bit value in a packet for the GS (GIF tag, register data, a DMA tag written as one number), as the PS2 holds
   it: two 32-bit words, the low one first. Wraps the value stored into a packet and a value read back from one.
   PS2_PKT64_FIELDS declares the bit-fields of such a value, those of the low 32 bits first. On the PS2 both are
   the plain thing; a big-endian host swaps the halves (port/include/port_compat.h). */
#define PS2_PKT64(x) (x)
#define PS2_PKT64_FIELDS(lo, hi) lo hi

#endif
