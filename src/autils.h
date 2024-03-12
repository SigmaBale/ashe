#ifndef AUTILS_H
#define AUTILS_H

#include "atoken.h"
#include "acommon.h"

#include <stdarg.h>

void die(void);
void die_err(const char* err);

/* print error */
void printf_error(const char* errfmt, ...);
void vprintf_error_wprefix(const char* errfmt, va_list argp);
void print_errno(void);

/* print info */
void printf_info(const char* ifmt, ...);
void vprintf_info_wprefix(const char* ifmt, va_list argp);

ubyte in_dq(char* str, memmax len);
ubyte is_escaped(char* bt, memmax curpos);

/* duplicate string using aalloc allocator (wrapper) */
char* dupstr(const char* str);

memmax len_without_seq(const char* prompt);

void unescape(Buffer* str);
void expand_vars(Buffer* str);

#endif
