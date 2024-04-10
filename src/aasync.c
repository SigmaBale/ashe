#include <signal.h>
#include <stdio.h>

#include "aasync.h"
#include "aprompt.h"
#include "acommon.h"
#include "ainput.h"
#include "ajobcntl.h"
#include "ashell.h"
#ifdef ASHE_DBG
#include "adbg.h"
#endif

/* Signals which we handle and block/unblock. */
static const a_int32 signals[] = {
	SIGINT,
	SIGCHLD,
	SIGWINCH,
};

ASHE_PRIVATE void SIGINT_handler(int signum);
ASHE_PRIVATE void SIGCHLD_handler(int signum);
ASHE_PRIVATE void SIGWINCH_handler(int signum);

static a_sighandler handlers[] = {
	SIGINT_handler,
	SIGCHLD_handler,
	SIGWINCH_handler,
};

/* Mask signal 'signum' by specifying 'how'.
 * 'how' should be SIG_BLOCK or SIG_UNBLOCK. */
ASHE_PUBLIC void ashe_mask_signal(int signum, int how)
{
	sigset_t signal;

	if (ASHE_UNLIKELY(sigemptyset(&signal) < 0 ||
			  sigaddset(&signal, signum) < 0 ||
			  sigprocmask(how, &signal, NULL) < 0)) {
		ashe_perrno("failed masking (how=%d) signal %d", how, signum);
		ashe_panic(NULL);
	}
}

/* SIGWINCH signal handler */
ASHE_PRIVATE void SIGWINCH_handler(int signum)
{
	ASHE_UNUSED(signum);
	ashe_mask_signals(SIG_BLOCK);
	ashe.sh_int = 1;
	ashe_get_winsize_or_panic(&ashe.sh_term.tm_rows, &A_TCOLMAX);
	if (ASHE_UNLIKELY(ashe_get_curpos(NULL, &A_TCOL)))
		ashe_panic("couldn't get cursor position.");
#ifdef ASHE_DBG_CURSOR
	debug_cursor();
#endif
#ifdef ASHE_DBG_LINES
	debug_lines();
#endif
	ashe_mask_signals(SIG_UNBLOCK);
}

/* SIGINT signal handler */
ASHE_PRIVATE void SIGINT_handler(int signum)
{
	ASHE_UNUSED(signum);
	ashe_mask_signals(SIG_BLOCK);
	ashe.sh_int = 1;
	ashe_cursor_end();
	ashe_print("\r\n", stderr);
	ashe_pprompt();
	a_terminput_clear();
	if (ASHE_UNLIKELY(ashe_get_curpos(NULL, &A_TCOL)) < 0)
		ashe_panic("couldn't get cursor position.");
#ifdef ASHE_DBG_CURSOR
	debug_cursor();
#endif
#ifdef ASHE_DBG_LINES
	debug_lines();
#endif
	ashe_mask_signals(SIG_UNBLOCK);
}

/* SIGCHLD signal handler */
ASHE_PRIVATE void SIGCHLD_handler(int signum)
{
	ASHE_UNUSED(signum);
	ashe_mask_signals(SIG_BLOCK);
	ashe.sh_int = 1;
	a_jobcntl_update_and_notify(&ashe.sh_jobcntl);
#ifdef ASHE_DBG_CURSOR
	debug_cursor();
#endif
#ifdef ASHE_DBG_LINES
	debug_lines();
#endif
	ashe_mask_signals(SIG_UNBLOCK);
}

/* Masks signals in 'signals' array. */
ASHE_PUBLIC void ashe_mask_signals(a_int32 how)
{
	for (a_uint32 i = 0; i < ASHE_ELEMENTS(signals); i++)
		ashe_mask_signal(signals[i], how);
}

/* Enables asynchronous 'JobControl' updates. */
ASHE_PUBLIC void ashe_enable_jobcntl_updates(void)
{
	struct sigaction old_action;
	sigaction(SIGCHLD, NULL, &old_action);
	old_action.sa_handler = SIGCHLD_handler;
	sigaction(SIGCHLD, &old_action, NULL);
}

/* disables asynchronous 'JobControl' updates. */
ASHE_PUBLIC void ashe_disable_jobcntl_updates(void)
{
	struct sigaction old_action;
	sigaction(SIGCHLD, NULL, &old_action);
	old_action.sa_handler = SIG_DFL;
	sigaction(SIGCHLD, &old_action, NULL);
}

// clang-format off
/* Initializes signal handlers. */
ASHE_PUBLIC void ashe_init_sighandlers(void)
{
	a_sighandler handler;
	struct sigaction default_action;
	a_uint32 i;

	sigemptyset(&default_action.sa_mask);
	default_action.sa_flags = 0;

	for (i = 0; i < ASHE_ELEMENTS(signals); i++) {
		handler = handlers[i];
		if (signals[i] == SIGCHLD)
			handler = SIG_DFL;
		default_action.sa_handler = handler;
		if (ASHE_UNLIKELY(sigaction(signals[i], &default_action, NULL) < 0))
			ASHE_DEFER_NO_STATUS();
	}

	default_action.sa_handler = SIG_IGN;
	if (ASHE_UNLIKELY(sigaction(SIGTTIN, &default_action, NULL) < 0 ||
			  sigaction(SIGTTOU, &default_action, NULL) < 0 ||
			  sigaction(SIGTSTP, &default_action, NULL) < 0 ||
			  sigaction(SIGQUIT, &default_action, NULL) < 0))
		ASHE_DEFER_NO_STATUS();

	return;
defer:
	ashe_perrno("sigaction");
	ashe_panic(NULL);
}
