#include "async.h"
#include "jobctl.h"

#include <signal.h>
#include <stdbool.h>

/// TODO: Make use of these
volatile atomic_bool sigint_recv = false;
volatile atomic_bool sigchld_recv = false;

void sigint_handler(__attribute__((unused)) int signum)
{
    block_sigchld();
    sigint_recv = true;
    // TODO: recover terminal input
    // if(reading) {
    //      todo()
    // }

    flock(STDERR_FILENO, LOCK_UN);
    flock(STDOUT_FILENO, LOCK_UN);
    ATOMIC_PRINT({ pprompt(); });
    unblock_sigchld();
}

void sigchld_handler(int signum)
{
    block_sigint();
    sigchld_recv = true;
    // TODO: recover terminal input
    // if(reading) {
    //      todo()
    // }

    flock(STDERR_FILENO, LOCK_UN);
    flock(STDOUT_FILENO, LOCK_UN);
    joblist_update_and_notify(signum);
    unblock_sigint();
}

void block_sigint(void)
{
    sigset_t block_sigint;
    sigemptyset(&block_sigint);
    sigaddset(&block_sigint, SIGINT);
    sigprocmask(SIG_BLOCK, &block_sigint, NULL);
}

void unblock_sigint(void)
{
    sigset_t ublock_sigint;
    sigemptyset(&ublock_sigint);
    sigaddset(&ublock_sigint, SIGINT);
    sigprocmask(SIG_UNBLOCK, &ublock_sigint, NULL);
}

void unblock_sigchld(void)
{
    sigset_t unblock_sigchld;
    sigemptyset(&unblock_sigchld);
    sigaddset(&unblock_sigchld, SIGCHLD);
    sigprocmask(SIG_UNBLOCK, &unblock_sigchld, NULL);
}

void block_sigchld(void)
{
    sigset_t block_sigchld;
    sigemptyset(&block_sigchld);
    sigaddset(&block_sigchld, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_sigchld, NULL);
}

void enable_async_joblist_update(void)
{
    struct sigaction old_action;
    sigaction(SIGCHLD, NULL, &old_action);
    old_action.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &old_action, NULL);
}

void disable_async_joblist_update(void)
{
    struct sigaction old_action;
    sigaction(SIGCHLD, NULL, &old_action);
    old_action.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &old_action, NULL);
}

void setup_default_signal_handling(void)
{
    struct sigaction default_action;
    sigemptyset(&default_action.sa_mask);
    default_action.sa_handler = sigint_handler;
    default_action.sa_flags = 0;

    if(__glibc_unlikely(sigaction(SIGINT, &default_action, NULL) < 0)) {
        ATOMIC_PRINT({
            pwarn("failed setting up shell SIGINT signal handler");
            perr();
        });
        exit(EXIT_FAILURE);
    }

    default_action.sa_handler = SIG_DFL;

    if(__glibc_unlikely(sigaction(SIGCHLD, &default_action, NULL) < 0)) {
        ATOMIC_PRINT({
            pwarn("failed setting up shell SIGCHLD signal handler");
            perr();
        });
        exit(EXIT_FAILURE);
    }
    default_action.sa_handler = SIG_IGN;

    if(__glibc_unlikely(
           sigaction(SIGTTIN, &default_action, NULL) < 0
           || sigaction(SIGTTOU, &default_action, NULL) < 0
           || sigaction(SIGTSTP, &default_action, NULL) < 0
           || sigaction(SIGQUIT, &default_action, NULL) < 0))
    {
        ATOMIC_PRINT({
            pwarn("failed setting up shell signal handlers");
            perr();
        });
        exit(EXIT_FAILURE);
    }
}

void try_wait_missed_sigchld_signals(void)
{
    joblist_update_and_notify(0);
}