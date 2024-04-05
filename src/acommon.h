#ifndef ACOMMON_H
#define ACOMMON_H

#include "aconf.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

/* Environment variable valid name characters (subset of PCS) */
#define ENV_VAR_CHARS \
	"0123456789_qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM"

/* Miscellaneous macros */
#define ELEMENTS(arr) (sizeof(arr) / sizeof(arr[0]))
#define unused(x)     (void)(x)
#define ispow2(x)     (((x) & ((x)-1)) == 0)
#define UINT_DIGITS   20
#define INT_DIGITS    10
/* -------------------------------- */

/* Compiler intrinsics */
#if defined(__GNUC__)
#define likely(expr)   __glibc_likely(expr)
#define unlikely(expr) __glibc_unlikely(expr)
#define MAX(a, b)                       \
	__extension__({                 \
		__typeof__(a) _a = (a); \
		__typeof__(b) _b = (b); \
		_a > _b ? _a : _b;      \
	})
#define MIN(a, b)                       \
	__extension__({                 \
		__typeof__(a) _a = (a); \
		__typeof__(b) _b = (b); \
		_a > _b ? _b : _a;      \
	})
#else
#define likely(expr)   (expr)
#define unlikely(expr) (expr)
#define MAX(a, b)      ((a) > (b) ? (a) : (b))
#define MIN(a, b)      ((a) > (b) ? (b) : (a))
#endif // __GNUC__
/* -------------------------------- */

#define ASHE_PRIVATE static
#define ASHE_PUBLIC  extern

#define ASHE_VAR_STATUS	  "?"
#define ASHE_VAR_PID	  "$"
#define ASHE_VAR_STATUS_C '?'
#define ASHE_VAR_PID_C	  '$'

/* ------ integer typedefs ------ */
typedef int8_t byte;
typedef uint8_t ubyte;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;

typedef size_t memmax;
typedef ssize_t ssize;

typedef pid_t pid;
typedef void (*sighandler)(int);
/* -------------------------------- */

#endif
