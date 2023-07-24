#ifndef __ASH_UTILS_H__
#define __ASH_UTILS_H__

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __cplusplus /* For gtest */
#include <atomic>
extern volatile std::atomic_bool sigchld_recv;
#else
#include <stdatomic.h>
extern volatile atomic_bool sigchld_recv;
#endif

typedef char byte;

extern struct termios shell_tmodes;
extern byte **environ;

#define PROMPT "\n[-ASHE-]: "

#define is_null(ptr) ((ptr) == NULL)
#define is_some(ptr) ((ptr) != NULL)
#define char_before_ptr(ptr) *((ptr)-1)
#define char_after_ptr(ptr) *((ptr) + 1)
#define NULL_TERM '\0'
#define EOL NULL_TERM

#define TERMINAL_FD STDIN_FILENO

#define ASHE_PREFIX "ashe"
#define ASHE_WARN_PREFIX ASHE_PREFIX "<warning>: "
#define ASHE_ERR_PREFIX ASHE_PREFIX "<error>"

#define ATOMIC_PRINT(print_block)                                              \
  do {                                                                         \
    if (__glibc_unlikely(flock(STDIN_FILENO, LOCK_EX) < 0 ||                   \
                         flock(STDOUT_FILENO, LOCK_EX) < 0)) {                 \
      perr();                                                                  \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
    print_block;                                                               \
    if (__glibc_unlikely(flock(STDIN_FILENO, LOCK_UN) < 0 ||                   \
                         flock(STDOUT_FILENO, LOCK_UN) < 0)) {                 \
      perr();                                                                  \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

/// Exit codes
#define FAILURE -1
#define SUCCESS 0

void pprompt(void);
void pwarn(const byte *fmt, ...);
void pusage(const byte *fmt, ...);
void perr(void);

#define PCS_EXTRA "/.-"
#define PORTABLE_CHARACTER_SET                                                 \
  "0123456789_qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM"

// clang-format off
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
#endif
