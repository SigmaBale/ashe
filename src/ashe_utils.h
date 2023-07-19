// clang-format off
#ifndef __ASH_UTILS_H__
#define __ASH_UTILS_H__

typedef char byte;

#include <stdio.h>

#define is_null(ptr) ((ptr) == NULL)
#define is_some(ptr) ((ptr) != NULL)
#define char_before_ptr(ptr) *((ptr) -1)
#define char_after_ptr(ptr) *((ptr) +1)
#define EOL '\0'
#define NULL_TERM '\0'

#define PCS_EXTRA "/.-"
#define PORTABLE_CHARACTER_SET \
    "0123456789_qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM"

#if defined(__unix__) || defined(__linux__)
    #define PATH_DELIM ':'
#elif defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__)
    #define PATH_DELIM ';'
#endif

#if defined(_WIN32) || defined(_WIN64)
    #define ARG_MAX 32767 // Windows
    #define HOME "HOMEPATH"
#elif defined(__CYGWIN__) && !defined(_WIN32)
    #include <limits.h>// Windows (Cygwin POSIX under Microsoft Window)
#elif defined(__linux__)
    #include <linux/limits.h> // Debian, Ubuntu, Gentoo, Fedora, openSUSE, RedHat, Centos and other
    #define HOME "HOME"
#elif defined(__unix__) || !defined(__APPLE__) && defined(__MACH__)
    #include <limits.h>
    #if defined(BSD)
        #include <sys/syslimits.h> // FreeBSD, NetBSD, OpenBSD, DragonFly BSD
    #endif
#elif defined(__hpux)
    #include <limits.h> // HP-UX
#elif defined(_AIX)
    #include <sys/limits.h> // IBM AIX
#elif defined(__sun) && defined(__SVR4)
    #include <limits.h> // Oracle Solaris, Open Indiana
#else
    #undef ARG_MAX
    #undef PATH_MAX
    #define HOME "HOME"
#endif

#if !defined(ARG_MAX) || !defined(PATH_MAX)
    #define _POSIX_SOURCE
    #include <limits.h>
    #ifndef ARG_MAX
        #define ARG_MAX _POSIX_ARG_MAX
    #endif
    #ifndef PATH_MAX
        #define PATH_MAX _POSIX_PATH_MAX
    #endif
#endif

// clang-format on

/// ERRORS

void pwarn(const byte *fmt, ...);
void pusage(const byte *fmt, ...);
void perr(void);

#define ASHE_PREFIX "ashe"
#define ASHE_WARN_PREFIX ASHE_PREFIX "<warning>: "
#define ASHE_ERR_PREFIX ASHE_PREFIX "<error>"

/// Exit codes
#define FAILURE -1
#define SUCCESS 0

#endif