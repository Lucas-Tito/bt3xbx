#ifndef X360_SHIM_STDINT_H
#define X360_SHIM_STDINT_H
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef int intptr_t;
typedef unsigned int uintptr_t;
#define INT32_MAX 0x7FFFFFFF
#define UINT32_MAX 0xFFFFFFFFu
#define INT32_MIN (-INT32_MAX - 1)
#define INT16_MAX 0x7FFF
#define INT16_MIN (-INT16_MAX - 1)
#define UINT16_MAX 0xFFFF
#endif
