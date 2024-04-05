#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "abuiltin.h"
#include "autils.h"
#include "acommon.h"
#include "ajobcntl.h"
#include "ainput.h"
#include "ashell.h"

/* options for envcmd function */
#define ENV_ADD	      0 /* add variable name/value */
#define ENV_SET	      1 /* add/overwrite variable name/value */
#define ENV_REMOVE    2 /* remove variable */
#define ENV_PRINT     3 /* print variable */
#define ENV_PRINT_ALL 4 /* print all variables */

#define is_help_opt(arg) \
	(strcmp((arg), "-h") == 0 || strcmp((arg), "--help") == 0)
#define HELPSTR \
	"\r\nThe -h or --help options display help information for this command\r\n"
#define print_help_opts(builtin) builtin_printf(builtin, HELPSTR)

#define namef(name)	  bold(green(#name))
#define keywordf(keyword) bold(cyan(#keyword))
#define validf(valid)	  bold(blue(valid))
#define invalidf(invalid) bold(bred(invalid))

#define FLIP_SIGN_BIT(n) ((n) ^ (1 << (sizeof(n) * 8 - 1)))

typedef int32 (*builtinfn)(a_arr_ccharp *argv);

ASHE_PRIVATE inline void print_rows(const char **rows, uint32 len)
{
	uint32 i;

	for (i = 0; i < len; i++)
		ashe_printf(stderr, "%s\r\n", rows[i]);
}

ASHE_PRIVATE void builtin_printf(const char *bi, const char *fmt, ...)
{
	va_list argp;

	va_start(argp, fmt);
	ashe_printf(stderr, "[%s]: ", bi);
	ashe_vprintf(stderr, fmt, argp);
	va_end(argp);
	ashe_print("\r\n", stderr);
}

ASHE_PRIVATE inline void ashe_jobs_print_header(void)
{
	const char *fmt = "\r\n%-*s%-*s%-*s\r\n";

	uint32 pad = 10;
	ashe_printf(stderr, fmt, pad, "Job", pad, "Group", pad, "State");
}

ASHE_PRIVATE void ashe_jobs_print_job(struct a_job *job)
{
	ubyte completed, stopped;
	uint32 pad = 10;
	const char *state = NULL;

	completed = a_job_is_completed(job);
	if (!completed) {
		stopped = a_job_is_stopped(job);
		state = (stopped ? "stopped" : "running");
	} else
		state = "completed";
	ashe_printf(stderr, "%-*ld %-*d %-*s\r\n", pad, job->id, pad, job->pgid,
		    pad, state);
}

ASHE_PRIVATE int32 filter_jobs_by_pid_or_id(a_arr_ccharp *argv, int32 *nums)
{
	const char *arg, *bin;
	char *errstr;
	ubyte id = 0;
	int32 num;
	memmax oklen, i;

	bin = argv->data[0];
	for (i = 1; i < argv->len; i++) {
		arg = *a_arr_ccharp_index(argv, i);
		if (*arg == '%')
			id = 1;
		num = strtol(arg, &errstr, 10);
		if (*errstr != '\0') {
			oklen = errstr - arg;
			ashe_eprintf(bin, "invalid %s provided, %*s'%s'",
				     (id ? "id" : "pid"), oklen, arg, errstr);
		} else if (errno == ERANGE || num > INT_MAX) {
			builtin_printf(bin,
				       "%s '%s' is too large, limit is %d.",
				       (id ? "id" : "pid"), arg, INT_MAX);
		} else {
			*nums++ = (id) ? FLIP_SIGN_BIT(num) : num;
			continue;
		}
		print_help_opts(bin);
		return -1;
	}
	return 0;
}

ASHE_PRIVATE void ashe_jobs_print_filtered_jobs(int32 *nums, memmax len)
{
	struct a_job *job;
	memmax i;

	for (i = 0; i < len; i++) {
		if (nums[i] < 0)
			job = a_jobcntl_get_job_with_id(&ashe.sh_jobcntl,
							FLIP_SIGN_BIT(nums[i]));
		else
			job = a_jobcntl_get_job_with_pgid(&ashe.sh_jobcntl,
							  nums[i]);
		if (job)
			ashe_jobs_print_job(job);
	}
}

ASHE_PRIVATE int32 ashe_jobs(a_arr_ccharp *argv)
{
	static const char *usage[] = {
		"jobs - print currently running/stopped jobs.\r\n",
		"jobs [PID | %ID, [PID | %ID]]\r\n",
		"Prints all currently running jobs and their status.",
		"This command can accept arguments in the form of PID or ID (job id).",
		"If the PID is supplied then the output is restricted to jobs",
		"that contain the selected process ID.",
		"In case job ID is used, then the output will contain only the",
		"job with the matching job ID.",
	};

	struct a_jobcntl *jobcntl = &ashe.sh_jobcntl;
	struct a_job *job = NULL;
	memmax jobcnt;
	memmax argc = argv->len;
	int32 *nums;
	memmax i = 0;
	int32 status = 0;

	if (argc > 1)
		nums = acalloc(sizeof(int32), argc - 1);

	switch (argc) {
	case 1:
		jobcnt = a_jobcntl_jobs(jobcntl);
		if (jobcnt == 0) {
			builtin_printf("jobs", "there are no jobs running.");
			goto l_error;
		}
		ashe_jobs_print_header();
		for (i = 0; i < jobcnt; i++) {
			job = a_jobcntl_get_job_at(jobcntl, i);
			ashe_jobs_print_job(job);
		}
		break;
	case 2:
		if (is_help_opt(argv->data[1])) {
			print_rows(usage, ELEMENTS(usage));
			break;
		}
		if (filter_jobs_by_pid_or_id(argv, nums) < 0) {
l_error:
			status = -1;
			break;
		}
		ashe_jobs_print_header();
		ashe_jobs_print_filtered_jobs(nums, argc - 1);
		break;
	default:
		status = -1;
		print_help_opts("jobs");
		break;
	}
	return status;
}

/* Auxiliary to ashe_bg() */
ASHE_PRIVATE int32 ashe_bg_last(void)
{
	struct a_job *job = a_jobcntl_get_job_from(&ashe.sh_jobcntl, 1);

	if (job) {
		a_job_continue(job, 0);
		return 0;
	}
	builtin_printf("bg", "there is no suitable job.");
	return -1;
}

/* Auxiliary to ashe_bg() */
ASHE_PRIVATE void ashe_bg_id(int32 id)
{
	struct a_job *job = a_jobcntl_get_job_with_id(&ashe.sh_jobcntl, id);

	if (job) {
		a_job_continue(job, 0);
	} else
		builtin_printf("bg", "there is no suitable job with id %d.",
			       id);
}

/* Auxiliary to ashe_bg() */
ASHE_PRIVATE void ashe_bg_pid(pid_t pid)
{
	struct a_job *job = a_jobcntl_get_job_with_pid(&ashe.sh_jobcntl, pid);

	if (job)
		a_job_mark_as_background(job, 1);
	else
		builtin_printf("bg", "there is no suitable job with pid %d.",
			       pid);
}

ASHE_PRIVATE void ashe_bg_background_filtered_jobs(int32 *nums, memmax len)
{
	memmax i;

	for (i = 0; i < len; i++) {
		if (nums[i] < 0)
			ashe_bg_id(FLIP_SIGN_BIT(nums[i]));
		else
			ashe_bg_pid(nums[i]);
	}
}

ASHE_PRIVATE int32 ashe_bg(a_arr_ccharp *argv)
{
	static const char *usage[] = {
		"bg - sends jobs to background resuming them if they are paused.\n\r",
		"bg [PID | '%'ID [PID | '%'ID]]\n\r",
		"There are multiple ways of specifying which job to move, either by PID,",
		"%ID or by not giving any arguments to the command.",
		"Multiple PIDs can be provided at once, same holds for %ID.",
		"By not specifying any arguments then the last jobs that was",
		"used/created will be moved to background.",
	};

	int32 status = 0;
	memmax argc = argv->len;

	switch (argc) {
	case 1:
		status = ashe_bg_last();
		break;
	case 2:
		if (is_help_opt(argv->data[1])) {
			print_rows(usage, ELEMENTS(usage));
		} else {
			int32 nums[argc - 1];
			if (filter_jobs_by_pid_or_id(argv, nums) < 0)
				return -1;
			ashe_bg_background_filtered_jobs(nums, argc - 1);
		}
		break;
	default:
		status = -1;
		print_help_opts("bg");
		break;
	}
	return status;
}

/* Auxiliary to ashe_fg() */
ASHE_PRIVATE int32 ashe_fg_last(void)
{
	struct a_job *job = a_jobcntl_get_job_from(&ashe.sh_jobcntl, 0);

	if (job) {
		a_job_continue(job, 1);
	} else {
		builtin_printf("fg", "there is no suitable job.");
		return -1;
	}
	return 0;
}

/* Auxiliary to ashe_fg() */
ASHE_PRIVATE void ashe_fg_id(int32 id)
{
	struct a_job *job =
		a_jobcntl_get_job_with_id_from(&ashe.sh_jobcntl, id, 0);

	if (job)
		a_job_continue(job, 1);
	else
		builtin_printf("fg", "there is no suitable job with id %d.",
			       id);
}

/* Auxiliary to ashe_fg() */
ASHE_PRIVATE void ashe_fg_pid(pid_t pid)
{
	struct a_job *job =
		a_jobcntl_get_job_with_pid_from(&ashe.sh_jobcntl, pid, 0);

	if (job)
		a_job_continue(job, 1);
	else
		builtin_printf("fg", "there is no suitable job with pid %d.",
			       pid);
}

/* Foreground a job */
ASHE_PRIVATE int32 ashe_fg(a_arr_ccharp *argv)
{
	static const char *usage[] = {
		"fg - move job to foreground\n\r",
		"fg [PID | %ID]\r\n",
		"Moves the job into the foreground additionally resuming it",
		"if it was stopped.",
		"If no arguments were supplied then the last job that was",
		"created or moved to background is used.",
		"User can provide PID or %ID to specify which job to foreground.",
	};

	memmax argc = argv->len;
	int32 status = 0;
	int32 n = 0;

	switch (argc) {
	case 1:
		status = ashe_fg_last();
		break;
	case 2:
		if (is_help_opt(argv->data[1])) {
			print_rows(usage, ELEMENTS(usage));
		} else {
			if (filter_jobs_by_pid_or_id(argv, &n) < 0)
				return -1;
			if (n < 0)
				ashe_fg_id(FLIP_SIGN_BIT(n));
			else
				ashe_fg_pid(n);
		}
		break;
	default:
		print_help_opts("fg");
		status = -1;
		break;
	}
	return status;
}

/* Exit shell */
ASHE_PRIVATE int32 ashe_exit(a_arr_ccharp *argv)
{
	static const char *usage[] = {
		"exit - exits the shell\r\n",
		"exit [CODE]\r\n",
		"Exits the shell with the supplied CODE, if no code was",
		"provided, then the default 0 is used.\r\n",
	};
	static const char *exit_warn[] = {
		"There are background jobs that are still running or are stopped!",
		"Running exit again will result in termination of these jobs.",
	};

	memmax jobcnt;
	memmax argc = argv->len;
	char *endptr;
	int32 status;

	status = EXIT_SUCCESS;
	switch (argc) {
	case 1:
		jobcnt = a_jobcntl_jobs(&ashe.sh_jobcntl);
		ashe.sh_flags.exit = (ashe.sh_flags.isfork || jobcnt == 0);
		if (!ashe.sh_flags.exit) {
			ashe.sh_flags.exit = 1;
			print_rows(exit_warn, ELEMENTS(exit_warn));
		} else {
			goto l_exit;
		}
		break;
	case 2:
		if (is_help_opt(argv->data[1])) {
			print_rows(usage, ELEMENTS(usage));
			break;
		}
		status = strtol(argv->data[1], &endptr, 10);
		if (errno == EINVAL || errno == ERANGE || *endptr != '\0' ||
		    status < 0) {
			builtin_printf("exit", "invalid exit status CODE '%s'.",
				       argv->data[1]);
			status = -1;
		} else {
l_exit:
			if (ashe.sh_flags.isfork) {
				cleanup_fork();
				_exit(status);
			} else {
				cleanup_all();
				exit(status);
			}
		}
		break;
	default:
		print_help_opts("exit");
		ashe.sh_flags.exit = 0;
		status = -1;
	}
	return status;
}

/* Change working directory */
ASHE_PRIVATE int32 ashe_cd(a_arr_ccharp *argv)
{
	static const char *usage[] = {
		"cd - change directory\r\n",
		"cd [DIRNAME]\r\n",
		"Changes current working directory.",
		"If no DIRNAME is given, then the $HOME environment variable\r\n"
		"is used as DIRNAME.",
	};

	int32 status = 0;
	memmax argc = argv->len;

	switch (argc) {
	case 1:
		if (unlikely(chdir(getenv(HOME)) < 0))
			goto l_error;
		break;
	case 2:
		if (is_help_opt(argv->data[1])) {
			print_rows(usage, ELEMENTS(usage));
		} else if (chdir(argv->data[1]) < 0) {
l_error:
			ashe_perrno();
			status = -1;
		}
		break;
	default:
		print_help_opts("cd");
		status = -1;
		break;
	}
	return status;
}

/* Auxiliary to envcmd() */
ASHE_PRIVATE inline void print_environ(void)
{
	int32 i = 0;

	while (__environ[i] != NULL)
		ashe_printf(stdout, "%s\r\n", __environ[i++]);
}

/* Auxiliary function for handling environment variables */
ASHE_PRIVATE int32 envcmd(a_arr_ccharp *argv, int32 option)
{
	const char *temp = NULL;

	switch (option) {
	case ENV_ADD:
	case ENV_SET:
		if (setenv(argv->data[1], argv->data[2], option != ENV_ADD) <
		    0) {
			ashe_perrno();
			return -1;
		}
		break;
	case ENV_REMOVE:
		unsetenv(argv->data[1]);
		break;
	case ENV_PRINT:
		if ((temp = getenv(argv->data[1])) != NULL) {
			ashe_printf(stdout, "%s\r\n", temp);
			break;
		} else {
			builtin_printf("penv", "variable '%s' doesn't exist.",
				       argv->data[1]);
			return -1;
		}
	case ENV_PRINT_ALL:
		print_environ();
		break;
	default: /* UNREACHED */
		ashe_assertf(0, "unreachable");
	}
	return 0;
}

ASHE_PRIVATE int32 ashe_penv(a_arr_ccharp *argv)
{
	static const char *usage[] = {
		"penv - print environment variable\r\n",
		"penv [NAME]\r\n",
		"Prints environment variable NAME; in case user didn't\r\n"
		"specify NAME, then all of the variables are printed.",
	};

	int32 status = 0;
	memmax argc = argv->len;

	switch (argc) {
	case 1:
		status = envcmd(NULL, ENV_PRINT_ALL);
		break;
	case 2:
		if (is_help_opt(argv->data[1]))
			print_rows(usage, ELEMENTS(usage));
		else
			status = envcmd(argv, ENV_PRINT);
		break;
	default:
		print_help_opts("penv");
		status = -1;
		break;
	}
	return status;
}

ASHE_PRIVATE int32 ashe_senv(a_arr_ccharp *argv)
{
	static const char *usage[] = {
		"senv - set environment variable\r\n",
		"senv NAME VALUE\r\n",
		"Sets or adds variable in the environment.",
		"If variable with NAME already exists then its VALUE",
		"is overwritten.",
		"In case variable with NAME is not found, then it is newly",
		"created and added into the environment with the value set",
		"to match VALUE.",
		"If the NAME was provided but not the VALUE then the value",
		"of that variable will be an emtpy string.",
	};

	memmax argc = argv->len;
	int32 status = 0;

	switch (argc) {
	case 2:
		if (is_help_opt(argv->data[1]))
			print_rows(usage, ELEMENTS(usage));
		else
			status = envcmd(argv, ENV_SET);
		break;
	case 3:
		status = envcmd(argv, ENV_SET);
		break;
	default:
		status = -1;
		print_help_opts("senv");
		break;
	}
	return status;
}

ASHE_PRIVATE int32 ashe_renv(a_arr_ccharp *argv)
{
	static const char *usage[] = {
		"renv - remove environment variable\r\n",
		"renv NAME\r\n",
		"Removes variable NAME from the environment.",
	};

	int32 status = 0;
	memmax argc = argv->len;

	switch (argc) {
	case 2:
		if (is_help_opt(argv->data[1]))
			print_rows(usage, ELEMENTS(usage));
		else
			status = envcmd(argv, ENV_REMOVE);
		break;
	default:
		print_help_opts("renv");
		status = -1;
	}
	return status;
}

ASHE_PRIVATE int32 ashe_pwd(a_arr_ccharp *argv)
{
	static const char *usage[] = {
		"pwd - print current working directory\r\n",
		"pwd\r\n",
		"Prints the current working directory.",
	};

	int32 status = 0;
	memmax argc = argv->len;
	char buff[PATH_MAX];

	switch (argc) {
	case 1:
		if (unlikely(!getcwd(buff, PATH_MAX))) {
			ashe_perrno();
			return -1;
		}
		ashe_printf(stdout, "%s\r\n", buff);
		break;
	case 2:
		if (is_help_opt(argv->data[1])) {
			print_rows(usage, ELEMENTS(usage));
			break;
		}
		/* FALLTHRU */
	default:
		status = -1;
		print_help_opts("pwd");
		break;
	}
	return status;
}

ASHE_PRIVATE int32 ashe_clear(a_arr_ccharp *argv)
{
	static const char *usage[] = {
		"clear - clears the screen\r\n",
		"clear\r\n",
		"Clears the terminal screen including scrollback.",
	};

	int32 status = 0;
	memmax argc = argv->len;

	switch (argc) {
	case 1:
		ashe_clear_screen_raw();
		break;
	case 2:
		if (is_help_opt(argv->data[1])) {
			print_rows(usage, ELEMENTS(usage));
			break;
		}
		/* FALLTHRU */
	default:
		status = -1;
		print_help_opts("clear");
		break;
	}
	return status;
}

ASHE_PRIVATE void print_builtins(void)
{
	static const char *builtin[] = {
		"cd",	"pwd",	"clear", "builtin", "fg",   "bg",
		"jobs", "exec", "exit",	 "penv",    "senv", "renv",
	};
	memmax i;

	for (i = 0; i < ELEMENTS(builtin); i++)
		ashe_printf(stdout, "%s\r\n", builtin[i]);
}

ASHE_PRIVATE int32 ashe_builtin(a_arr_ccharp *argv)
{
	static const char *usage[] = {
		"builtin - lists all builtin commands\r\n",
		"builtin\r\n",
		"Prints all of the builtin commands.",
	};

	int32 status = 0;
	memmax argc = argv->len;

	switch (argc) {
	case 1:
		print_builtins();
		break;
	case 2:
		if (is_help_opt(argv->data[1])) {
			print_rows(usage, ELEMENTS(usage));
			break;
		}
		/* FALLTHRU */
	default:
		status = -1;
		print_help_opts("builtin");
		break;
	}
	return status;
}

ASHE_PRIVATE int32 ashe_exec(a_arr_ccharp *argv)
{
	static const char *usage[] = {
		"exec - execute a command and open, close or copy file descriptors\r\n",
		"exec [command [argument...]]\r\n",
		"Exec will open, close and/or copy file descriptors as specified",
		"by any redirection as part of the command.\r\n",
		"If exec is specified with 'command' then it will replace the shell",
		"with that 'command' without creating a new process.",
		"Additionally if any arguments were specified they will be arguments",
		"to the 'command'.",
		"Redirections will persist in a new process image (if exec was invoked with a 'command')",
	};

	memmax argc = argv->len;
	const char **execargs;
	uint32 i;

	if (argc == 1)
		return 0;
	if (is_help_opt(argv->data[1])) {
		print_rows(usage, ELEMENTS(usage));
		return 0;
	}

	execargs = amalloc(sizeof(*execargs) * argc);
	for (i = 0; i < argc - 1; i++)
		execargs[i] = *a_arr_ccharp_index(argv, i + 1);
	execargs[argc - 1] = NULL;

	if (unlikely(execvp(execargs[0], (char *const *)execargs) < 0)) {
		afree(execargs);
		ashe_perrno();
		return -1;
	}
	/* UNREACHED */
	ashe_assert(0);
	return 0;
}

ASHE_PRIVATE inline int32 builtin_match(const char *str, uint32 start,
					uint32 len, const char *pattern,
					enum a_builtin_type type)
{
	if (strlen(str) == start + len &&
	    memcmp(str + start, pattern, len) == 0)
		return type;
	return -1;
}

/* Check if the 'command' is builtin command and
 * return the 'builtin' enum value of it or -1. */
ASHE_PUBLIC int32 is_builtin(const char *command)
{
	int32 bi = -1;

	ashe_assert(command != NULL);
	switch (command[0]) {
	case 'b':
		switch (command[1]) {
		case 'u':
			return builtin_match(command, 2, 5, "iltin",
					     TBI_BUILTIN);
		case 'g':
			if (command[2] == '\0')
				return TBI_BG;
			else
				return -1;
		default:
			break;
		}
		break;
	case 'c':
		switch (command[1]) {
		case 'd':
			if (command[2] == '\0')
				return TBI_CD;
			else
				return -1;
		case 'l':
			return builtin_match(command, 2, 3, "ear", TBI_CLEAR);
		default:
			break;
		}
		break;
	case 'e':
		switch (command[1]) {
		case 'x':
			switch (command[2]) {
			case 'e':
				return builtin_match(command, 2, 2, "ec",
						     TBI_EXEC);
			case 'i':
				return builtin_match(command, 2, 2, "it",
						     TBI_EXIT);
			default:
				break;
			}
		default:
			break;
		}
		break;
	case 'f':
		return builtin_match(command, 1, 1, "g", TBI_FG);
	case 'j':
		return builtin_match(command, 1, 3, "obs", TBI_JOBS);
	case 'p':
		switch (command[1]) {
		case 'e':
			return builtin_match(command, 2, 2, "nv", TBI_PENV);
		case 'w':
			return builtin_match(command, 2, 1, "d", TBI_PWD);
		default:
			break;
		}
		break;
	case 'r':
		return builtin_match(command, 1, 3, "env", TBI_RENV);
	case 's':
		return builtin_match(command, 1, 3, "env", TBI_SENV);
	default:
		break;
	}

	return bi;
}

/* Runs builting function 'bi'. */
ASHE_PUBLIC int32 run_builtin(struct a_simple_cmd *scmd,
			      enum a_builtin_type tbi)
{
	static const builtinfn table[] = {
		ashe_builtin, ashe_bg,	 ashe_cd,   ashe_clear,
		ashe_fg,      ashe_jobs, ashe_penv, ashe_pwd,
		ashe_renv,    ashe_senv, ashe_exec, NULL /* ashe_exit */,
	};

	ashe_assertf(tbi >= TBI_BUILTIN && tbi <= TBI_EXIT, "invalid tbi");
	if (unlikely(tbi == TBI_EXIT))
		return ashe_exit(&scmd->sc_argv);
	ashe.sh_flags.exit = 0;
	return table[tbi](&scmd->sc_argv);
}
