/*
 * Forced into every game source of the PC build (-include port_compat.h), in front of the game's own headers.
 * The game sources under src/ and include/ are shared with the matching decompilation and are not edited for the
 * port unless there is no other way; differences between the PS2 compiler and a host compiler are bridged here.
 */
#ifndef PORT_COMPAT_H
#define PORT_COMPAT_H

#define PORT 1

/* The game's types: on the PS2 `long` is 64 bits. Take over include/types.h. */
#define TYPES_H
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef long long s64;
typedef unsigned long long u64;
typedef float f32;

/* include/types.h: a variable read through a pointer of another width, as the PS2 (little-endian) reads it. On a
   big-endian host the low part of x is at its end, and the two words of a 64-bit load come swapped. */
#ifdef PORT_BIG_ENDIAN
#define PS2_LOW(T, x) (*(T *)((u8 *)&(x) + sizeof(x) - sizeof(T)))
#define PS2_LD64(x) ((u64)((u32 *)&(x))[1] << 32 | ((u32 *)&(x))[0])
#else
#define PS2_LOW(T, x) (*(T *)&(x))
#define PS2_LD64(x) (*(u64 *)&(x))
#endif

/* Functions the matching build takes from assembly: the port supplies them elsewhere (see docs/port/). */
#define INCLUDE_ASM_H
#define INCLUDE_ASM(FOLDER, NAME)
#define INCLUDE_RODATA(FOLDER, NAME)
#define RODATA_ALIGN16()
#define LIT4_WORD(NAME, HEX)
#define ASM_STUB_BEGIN() extern int asm_stub_begin_
#define ASM_STUB_END() extern int asm_stub_end_

/* C library calls of the game that must not go to the host's versions (see port/src/plat_mem.c). */
#define malloc Port_Malloc

#endif
