#pragma once
#include <stdint.h>
#include <x86intrin.h>
#include <sys/stat.h>
#include <time.h>
#define __int64 long long
#define __forceinline inline __attribute__((always_inline))
static inline unsigned char _BitScanForward(unsigned long *i, uint32_t m){ if(!m) return 0; *i=__builtin_ctz(m); return 1; }
static inline unsigned char _BitScanReverse(unsigned long *i, uint32_t m){ if(!m) return 0; *i=31-__builtin_clz(m); return 1; }
#define _byteswap_ushort __builtin_bswap16
#define _byteswap_ulong __builtin_bswap32
#define _byteswap_uint64 __builtin_bswap64
#define _rotl(x,n) ((((uint32_t)(x))<<(n))|(((uint32_t)(x))>>(32-(n))))
#define _rotl64(x,n) ((((uint64_t)(x))<<(n))|(((uint64_t)(x))>>(64-(n))))
#define _stricmp strcasecmp
#include <strings.h>
typedef int64_t LARGE_INTEGER_Q;
typedef union { long long QuadPart; } LARGE_INTEGER;
static inline int QueryPerformanceCounter(LARGE_INTEGER*l){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);l->QuadPart=t.tv_sec*1000000000LL+t.tv_nsec;return 1;}
static inline int QueryPerformanceFrequency(LARGE_INTEGER*l){l->QuadPart=1000000000LL;return 1;}
