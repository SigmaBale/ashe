#ifndef ASHELL_H
#define ASHELL_H

#include "aparser.h"
#include "acommon.h"
#include "ainput.h"
#include "ajobcntl.h"

#include <signal.h>
#include <setjmp.h>

struct a_jmpbuf {
	jmp_buf buf_jmpbuf;
	volatile int32 buf_code;
};

enum a_setting_type {
	ASETTING_NOCLOBBER = (1 << 1),
};

struct a_settings {
	ubyte sett_noclobber : 1; /* do not overwrite existing file */
}; /* shell settings */

struct a_flags {
	volatile ubyte exit : 1; /* set if last command was 'exit' */
	volatile ubyte isfork : 1; /* set if this is forked shell process */
	volatile ubyte interactive : 1; /* set if shell is interactive */
};

struct a_shell {
	struct a_jobcntl sh_jobcntl;
	struct a_term sh_term;
	struct a_lexer sh_lexer;
	a_arr_ccharp sh_buffers;
	a_arr_char sh_prompt;
	a_arr_char sh_welcome;
	struct a_block sh_block;
	struct a_jmpbuf sh_buf;
	struct a_flags sh_flags;
	struct a_settings sh_settings;
	volatile sig_atomic_t sh_int; /* set if we got interrupted */
};

extern struct a_shell ashe; /* global */

void a_shell_init(struct a_shell *sh);
void wafree_charp(void *ptr);
void a_shell_free(struct a_shell *sh);

#endif
