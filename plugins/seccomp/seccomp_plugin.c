/*
 * Copyright (c) 2009-2013 Todd C. Miller <Todd.Miller@sudo.ws>
 * Copyright (c) 2023-2024 The Sudo Project
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This is an example policy plugin. It demonstrates the C API.
 * The plugin simply allows any command a user is allowed to run
 * as per the sudoers file.
 */

#include <config.h>

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h> // For syslog constants if used directly, though sudo_plugin_printf is preferred

#include <sudo_plugin.h>
#include <seccomp.h> // For libseccomp

/*
 * Statically-linked plugins are preferred for security reasons.
 * Dynamic plugins are loaded from arbitrary paths and may not be safe.
 */
#ifndef STATIC_PLUGIN

static sudo_conv_t sudo_conv;
static sudo_printf_t sudo_log; // Changed from sudo_plugin_printf to avoid conflict with fn param

/*
 * Sudo conversation function.
 */
static int
sudo_conversation(int num_msgs, const struct sudo_conv_message msgs[],
    struct sudo_conv_reply replies[], struct sudo_conv_callback *callback)
{
    if (sudo_conv == NULL)
	return SUDO_CONV_FAILURE;
    return sudo_conv(num_msgs, msgs, replies, callback);
}

/*
 * Sudo printf function.
 */
sudo_printf_attribute__((__format__(__printf__, 2, 3)))
static int
_sudo_printf(int msg_type, const char *fmt, ...)
{
    va_list ap;
    int len;

    if (sudo_log == NULL)
	return -1;

    va_start(ap, fmt);
    len = sudo_log(msg_type, fmt, ap);
    va_end(ap);

    return len;
}

#else /* STATIC_PLUGIN */

/* For static plugins, we use the logging and conversation functions directly. */
# define sudo_conversation	sudo_api.conversation
# define _sudo_printf		sudo_api.printf

#endif /* STATIC_PLUGIN */


static int seccomp_policy_open(unsigned int version, sudo_conv_t conversation,
    sudo_printf_t plugin_printf, char * const settings[],
    char * const user_info[], char * const command_info[],
    char * const runas_info[], char * const argv[],
    char * const user_env[], char * const plugin_options[],
    const char **errstr);
static void seccomp_policy_close(int exit_status, int error);
static int seccomp_policy_show_version(int verbose);
static int seccomp_policy_check_policy(int argc, char * const argv[],
    char *env_add[], char **command_info_out[],
    char **argv_out[], char **user_env_out[], const char **errstr);
static int seccomp_policy_list_user_privs(int verbose, char * const user,
    const char **errstr);
static int seccomp_policy_init_session(struct passwd *pwd, char **user_env[],
    const char **errstr);

/*
 * Exported policy plugin interface.
 */
sudo_dso_public struct policy_plugin seccomp_policy = {
    SUDO_POLICY_PLUGIN,
    SUDO_API_VERSION,
    seccomp_policy_open,
    seccomp_policy_close,
    seccomp_policy_show_version,
    seccomp_policy_check_policy,
    seccomp_policy_list_user_privs,
    NULL, /* check_user_privs */
    NULL, /* invalidate */
    seccomp_policy_init_session,
    NULL, /* init_command */
    NULL, /* register_hooks */
    NULL, /* deregister_hooks */
    NULL  /* event_alloc */
};

static int
seccomp_policy_open(unsigned int version, sudo_conv_t conversation,
    sudo_printf_t plugin_printf, char * const settings[],
    char * const user_info[], char * const command_info[],
    char * const runas_info[], char * const argv[],
    char * const user_env[], char * const plugin_options[],
    const char **errstr)
{
#ifndef STATIC_PLUGIN
    sudo_conv = conversation;
    sudo_log = plugin_printf;
#endif

    /*
     * Sudo versions prior to 1.8.24 don't pass all info to the plugin.
     * For example, settings, user_info and command_info are NULL.
     */
    if (SUDO_API_MKMAJOR(version) == 1 && SUDO_API_MKMINOR(version) < 24) {
	_sudo_printf(SUDO_CONV_ERROR_MSG,
	    "sudo version 1.8.24 or higher required by seccomp_policy plugin");
	return -1;
    }

    _sudo_printf(SUDO_CONV_INFO_MSG, "Seccomp policy plugin initialized.");
    return 1;
}

static void
seccomp_policy_close(int exit_status, int error)
{
    /* Log exit status if not a normal exit? */
    _sudo_printf(SUDO_CONV_INFO_MSG, "Seccomp policy plugin exiting.");
}

static int
seccomp_policy_show_version(int verbose)
{
    _sudo_printf(SUDO_CONV_INFO_MSG, "Seccomp policy plugin version %s", PACKAGE_VERSION);
    return 1;
}

static int
seccomp_policy_check_policy(int argc, char * const argv[], char *env_add[],
    char **command_info_out[], char **argv_out[],
    char **user_env_out[], const char **errstr)
{
    scmp_filter_ctx ctx;
    int rc;

    _sudo_printf(SUDO_CONV_INFO_MSG, "Seccomp policy plugin: Applying seccomp filter.");

    ctx = seccomp_init(SCMP_ACT_KILL);
    if (ctx == NULL) {
        _sudo_printf(SUDO_CONV_ERROR_MSG, "seccomp_init failed");
        return 0; // Deny
    }

    // Minimal set of allowed syscalls
    int syscalls_to_allow[] = {
        SCMP_SYS(read), SCMP_SYS(write), SCMP_SYS(openat), SCMP_SYS(close),
        SCMP_SYS(stat), SCMP_SYS(fstat), SCMP_SYS(lstat), SCMP_SYS(poll),
        SCMP_SYS(lseek), SCMP_SYS(mmap), SCMP_SYS(mprotect), SCMP_SYS(munmap),
        SCMP_SYS(brk), SCMP_SYS(rt_sigaction), SCMP_SYS(rt_sigprocmask),
        SCMP_SYS(ioctl), SCMP_SYS(access), SCMP_SYS(execve),
        SCMP_SYS(exit_group), SCMP_SYS(exit), SCMP_SYS(arch_prctl),
        SCMP_SYS(getuid), SCMP_SYS(getgid), SCMP_SYS(geteuid), SCMP_SYS(getegid),
        SCMP_SYS(getrlimit), SCMP_SYS(fcntl), SCMP_SYS(getdents64), SCMP_SYS(dup2),
        SCMP_SYS(rt_sigreturn), SCMP_SYS(set_tid_address), SCMP_SYS(set_robust_list),
        SCMP_SYS(futex), SCMP_SYS(readlink), SCMP_SYS(sysinfo)
        // Add more essential syscalls as identified during testing
    };

    for (size_t i = 0; i < sizeof(syscalls_to_allow) / sizeof(syscalls_to_allow[0]); i++) {
        rc = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, syscalls_to_allow[i], 0);
        if (rc < 0) {
            _sudo_printf(SUDO_CONV_ERROR_MSG, "seccomp_rule_add failed for syscall %d: %s",
                syscalls_to_allow[i], strerror(-rc));
            seccomp_release(ctx);
            return 0; // Deny
        }
    }

    rc = seccomp_load(ctx);
    if (rc < 0) {
        _sudo_printf(SUDO_CONV_ERROR_MSG, "seccomp_load failed: %s", strerror(-rc));
        seccomp_release(ctx);
        return 0; // Deny
    }

    seccomp_release(ctx);
    _sudo_printf(SUDO_CONV_INFO_MSG, "Seccomp filter applied successfully.");

    /* This plugin does not change the command or environment. */
    *command_info_out = NULL;
    *argv_out = NULL;
    *user_env_out = NULL;

    return 1; /* Allow command */
}

static int
seccomp_policy_list_user_privs(int verbose, char * const user,
    const char **errstr)
{
    _sudo_printf(SUDO_CONV_INFO_MSG, "Seccomp policy plugin: list_user_privs called for %s (verbose %d), returning success.", user, verbose);
    /* This plugin doesn't modify privileges based on sudoers, so this is a no-op. */
    return 1;
}

static int
seccomp_policy_init_session(struct passwd *pwd, char **user_env[],
    const char **errstr)
{
    if (pwd != NULL) {
        _sudo_printf(SUDO_CONV_INFO_MSG, "Seccomp policy plugin: init_session for user %s.", pwd->pw_name);
    } else {
        _sudo_printf(SUDO_CONV_INFO_MSG, "Seccomp policy plugin: init_session (no pwd).");
    }
    /* This plugin doesn't modify the environment. */
    return 1;
}
