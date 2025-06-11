#include <config.h> // Should be included first if present for sudo builds

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h> // For SIGSYS

// Assume SUDO_PATH is defined during compilation or is just "sudo"
// It will be defined by the Makefile when compiling this test program.
#ifndef SUDO_PATH_FOR_TESTS
#define SUDO_PATH_FOR_TESTS "sudo"
#endif

// The actual plugin name as sudo will load it (without path, typically)
#define SECCOMP_PLUGIN_NAME "seccomp_plugin.so"


/*
 * This function attempts to run a command via sudo.
 * For actual plugin testing, sudo needs to be configured to load the seccomp plugin.
 * This usually means a custom sudo.conf file. The sudo test harness often provides
 * macros or functions (like testsudo_command_t and testsudo_run_tests) to manage this.
 * Since this is a standalone C file for now, direct execution of `sudo` here
 * will use the system's default sudo and sudo.conf, which is NOT what we want for testing
 * the plugin we just built.
 *
 * The strategy here is that this C file will be compiled, and then *executed*
 * by `make check`. The `make check` in `plugins/seccomp/Makefile.in` will be
 * responsible for ensuring that SUDO_PATH_FOR_TESTS points to the newly built sudo
 * and potentially setting up environment variables (like SUDO_CONF_PATH) to point
 * to a test-specific sudo.conf that loads our seccomp_plugin.so.
 */
int run_test_command(const char* test_name, const char* command_payload, int expect_kill_signal, int expected_signal_val) {
    pid_t pid;
    int status;
    char *sudo_argv[64]; // Max arguments for sudo + command
    int argc = 0;
    char temp_sudo_command_line[1024]; // For logging

    printf("Preparing test [%s]: Command to be executed by sudo: %s\n", test_name, command_payload);

    // Construct argv for execvp: $(SUDO_PATH_FOR_TESTS) <command_payload components>
    // This is a simplified approach. A real harness might take a full command line.
    // Here, we assume command_payload is a simple command or a script.
    // If command_payload contains spaces, it should ideally be broken into multiple args for execvp.
    // For this example, we'll pass it as a single argument to "sh -c" for simplicity in `execl` below.

    snprintf(temp_sudo_command_line, sizeof(temp_sudo_command_line), "%s %s", SUDO_PATH_FOR_TESTS, command_payload);
    printf("Executing: /bin/sh -c \"%s\"\n", temp_sudo_command_line);


    pid = fork();
    if (pid == -1) {
        perror("fork");
        return 0; // Test framework failure (indicates failure to run test)
    } else if (pid == 0) { // Child process
        // In a real test harness, SUDO_CONF_PATH would be set in the environment
        // to point to a test-specific sudo.conf that loads the plugin.
        // Example: setenv("SUDO_CONF_PATH", "./regress/sudo.conf", 1);
        // Example: setenv("SUDO_PLUGIN_PATH", "../.libs", 1); // If plugin not installed yet

        // Using execl to run "sh -c 'sudo command'" to handle sudo path and command parsing easily.
        execl("/bin/sh", "sh", "-c", temp_sudo_command_line, (char *)NULL);
        perror("execl /bin/sh failed"); // Should not be reached
        _exit(127); // exec error
    }

    // Parent process
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return 0; // Test framework failure
    }

    if (expect_kill_signal) {
        if (WIFSIGNALED(status)) {
            if (WTERMSIG(status) == expected_signal_val) {
                printf("PASS [%s]: Command terminated by expected signal %d.\n", test_name, expected_signal_val);
                return 1; // Test passed
            } else {
                printf("FAIL [%s]: Command terminated by unexpected signal %d (expected %d).\n", test_name, WTERMSIG(status), expected_signal_val);
                return 0; // Test failed
            }
        } else {
            printf("FAIL [%s]: Command exited (status %d) but expected to be killed by signal %d.\n", test_name, WEXITSTATUS(status), expected_signal_val);
            return 0; // Test failed
        }
    } else { // Expect successful exit
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("PASS [%s]: Command exited successfully (status 0).\n", test_name);
            return 1; // Test passed
        } else if (WIFEXITED(status)) {
            printf("FAIL [%s]: Command exited with status %d (expected 0).\n", test_name, WEXITSTATUS(status));
            return 0; // Test failed
        } else if (WIFSIGNALED(status)) {
            printf("FAIL [%s]: Command terminated by signal %d (expected clean exit 0).\n", test_name, WTERMSIG(status));
            return 0; // Test failed
        }
    }
    return 0; // Should not be reached in ideal cases
}

int main(int argc, char *argv[]) {
    int tests_passed = 0;
    int total_tests = 0;
    char disallowed_payload_executable[512];

    // Construct path to disallowed_payload executable (assuming it's in the same dir as seccomp_tests)
    // In `make check`, this will be in the build directory for plugins/seccomp/regress
    // For simplicity, we assume it's just "disallowed_payload" and Makefile places it correctly or CWD is regress/
    snprintf(disallowed_payload_executable, sizeof(disallowed_payload_executable), "./disallowed_payload");

    printf("--- Starting Seccomp Plugin Regression Tests ---\n");
    printf("NOTE: These tests assume 'sudo' is properly configured to load '%s'.\n", SECCOMP_PLUGIN_NAME);
    printf("SUDO_PATH_FOR_TESTS is defined as: %s\n", SUDO_PATH_FOR_TESTS);


    // Test 1: Allowed command (simple echo script)
    total_tests++;
    // The allowed_payload.sh script needs to be executable and findable.
    // Assuming it's in a path relative to where 'make check' is run, or test harness handles paths.
    // For `make check` from `plugins/seccomp`, `$(REGRESS_DIR)/allowed_payload.sh` would be the path.
    // We pass the path to the script as the command for sudo.
    if (run_test_command("allowed_payload_script", "$(srcdir)/regress/allowed_payload.sh", 0, 0)) tests_passed++;

    // Test 2: Disallowed command (attempts socket(), expects SIGSYS if seccomp blocks it)
    total_tests++;
    // SIGSYS is typically signal 31. This can vary by architecture.
    // On x86_64 Linux, SIGSYS is 31.
    if (run_test_command("disallowed_socket_call", disallowed_payload_executable, 1, SIGSYS)) tests_passed++;

    // Test 3: File creation (touch) - should be allowed by the current basic rules.
    // This command is more complex for the simple execl("/bin/sh"...) model if paths are tricky.
    // For now, let's use a simple /bin/touch command.
    total_tests++;
    if (run_test_command("create_and_remove_file", "/bin/sh -c '/bin/touch /tmp/seccomp_test_file.txt && /bin/rm /tmp/seccomp_test_file.txt'", 0, 0)) tests_passed++;


    printf("\n--- Seccomp Plugin Test Summary ---\n");
    printf("%d/%d tests passed.\n", tests_passed, total_tests);

    // Cleanup: disallowed_payload_executable is created by Makefile, should be cleaned by make clean.
    // remove("/tmp/seccomp_test_file.txt"); // Could be removed here too

    return (tests_passed == total_tests) ? 0 : 1;
}
