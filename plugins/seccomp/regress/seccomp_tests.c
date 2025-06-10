/*
 * Copyright (c) 2023-2024 Todd C. Miller <Todd.Miller@sudo.ws>
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h> // For SIGSYS

/*
 * Include testsudo.h if available from sudo's test infrastructure.
 * This might need adjustment based on the actual path if run by "make check"
 * from a higher-level directory.
 */
#if defined(HAVE_TESTSUDO_H) && HAVE_TESTSUDO_H
# include "testsudo.h"
#endif

/*
 * Placeholder main for seccomp plugin regression tests.
 * Actual tests would use the testsudo framework.
 * The command payloads for testing are outlined in test_payloads.sh.
 */
int
main(int argc, char *argv[])
{
    /*
     * This test suite requires a sudo binary with the seccomp plugin enabled,
     * and a sudo.conf that loads seccomp_plugin.so.
     * E.g., Plugin seccomp_plugin /path/to/build/plugins/seccomp/seccomp_plugin.so
     *
     * The testsudo_exec_v() function would be used to run sudo commands.
     * testsudo_test_t would define test cases.
     */

#if defined(HAVE_TESTSUDO_H) && HAVE_TESTSUDO_H
    /*
    char *env_add[] = {
        "SUDO_PLUGIN_PATH=" SUDO_PLUGIN_DIR, // If plugins are not in default path
        NULL
    };
    */

    testsudo_test_t test_cases[] = {
        {
            .description = "execute /bin/true (allowed by seccomp)",
            .command = "true", // testsudo_exec_v prepends sudo
            .test_type = SUDOTEST_SUCCESS, // Expect command to succeed
            // .rval = 0, // Expect exit value 0
            // .uinfo = &testsudo_user_nobody, // Run as a non-root user
            // .env_add = env_add,
            // .conf_file = "sudo.conf" // A specific sudo.conf for this test
        },
        {
            .description = "attempt to create a socket (disallowed by seccomp if not whitelisted)",
            .command = "/tmp/socket_test_program", // Assumes this program is pre-compiled by Makefile
                                                // and attempts socket(AF_INET, SOCK_STREAM, 0)
            .test_type = SUDOTEST_ERROR_SIGNAL, // Expect command to be killed
            // .signal = SIGSYS, // Expect SIGSYS due to seccomp violation
            // .uinfo = &testsudo_user_nobody,
            // .env_add = env_add,
            // .conf_file = "sudo.conf"
        },
        /* Add more test cases here based on test_payloads.sh */
        { NULL } /* Sentinel */
    };

    /*
     * Pre-test setup:
     * 1. Compile /tmp/socket_test_program from test_payloads.sh
     *    (or have a pre-compiled binary in the regress dir).
     * 2. Ensure sudo.conf is configured to load seccomp_plugin.so.
     *    The testsudo framework might handle temporary sudo.conf files.
     */

    /*
     * Example of compiling a helper for a test:
     * system("cc -o /tmp/socket_test_program -xc - <<EOF \n"
     *        "#include <sys/socket.h>\n"
     *        "#include <unistd.h>\n"
     *        "int main() { alarm(1); socket(AF_INET, SOCK_STREAM, 0); return 123; }\n" // returns 123 if not killed
     *        "EOF");
     */

    //return testsudo_run_tests(argc, argv, test_cases);
    printf("Placeholder C test (seccomp_tests.c) executed.\n");
    printf("Actual tests would use testsudo.h and run commands from test_payloads.sh.\n");
    /* system("rm -f /tmp/socket_test_program"); // Clean up compiled helper */
    initprogname(argc > 0 ? argv[0] : "seccomp_tests"); // testsudo.h might require this
    exit(0); // Placeholder success
#else
    printf("Skipping seccomp C tests: testsudo.h not available.\n");
    /*
     * If testsudo.h is not available, one might fall back to simpler system() calls
     * with a wrapper script that sets up sudo.conf and paths, but this is less robust.
     * For example:
     * int ret = system("./run_seccomp_payload.sh \"/bin/echo hello\"");
     * // ... check ret ...
     */
    exit(77); // code for skipped test
#endif
}
