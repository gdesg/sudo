#!/bin/sh
# Test payloads for seccomp plugin

# These commands would be run via sudo with the seccomp plugin enabled, e.g.:
# sudo -P /path/to/build/plugins/seccomp/seccomp_plugin.so <command>
# The actual test harness would handle sudo invocation and result checking.

echo "--- Test Payloads for Seccomp Plugin ---"

echo ""
echo "1. Allowed command (should succeed)"
echo "Description: A simple command that uses common, typically allowed syscalls."
echo "Payload: /bin/echo 'Seccomp test: allowed command'"
# Expected outcome: Success (exit code 0)

echo ""
echo "2. Command attempting a disallowed syscall (e.g., socket(), if not whitelisted)"
echo "Description: This command attempts to create a socket. If socket() is not in the"
echo "             seccomp plugin's whitelist, the command should be terminated by SIGSYS."
echo "Payload to compile and run a program that calls socket():"
echo "cat << EOF > /tmp/socket_test_program.c
#include <sys/socket.h>
#include <unistd.h> // For alarm
int main() {
  alarm(2); // Prevent hanging if not killed by seccomp for some reason
  socket(AF_INET, SOCK_STREAM, 0);
  return 0; // Should not be reached if seccomp active and socket disallowed
}
EOF
cc -o /tmp/socket_test_program /tmp/socket_test_program.c && /tmp/socket_test_program ; rm -f /tmp/socket_test_program /tmp/socket_test_program.c"
# Expected outcome: Failure (killed by SIGSYS, or non-zero exit if plugin handles differently)

echo ""
echo "3. File creation command (should succeed if creat/open(O_CREAT) is allowed)"
echo "Description: Tests file system write operations (create, then delete)."
echo "Payload: /bin/touch /tmp/seccomp_test_file.txt && /bin/ls /tmp/seccomp_test_file.txt && /bin/rm /tmp/seccomp_test_file.txt"
# Expected outcome: Success (exit code 0)

echo ""
echo "4. Command attempting to use 'ptrace' (typically disallowed)"
echo "Description: ptrace is a powerful syscall often restricted."
echo "Payload: /usr/bin/strace /bin/true"
# Expected outcome: Failure if ptrace is not in the whitelist. strace itself might be blocked, or the ptrace call it makes.

echo ""
echo "5. Command attempting to change user ID (setuid - typically disallowed post-exec)"
echo "Description: setuid is a privileged syscall. The seccomp filter should prevent it."
echo "Payload to compile and run a program that calls setuid():"
echo "cat << EOF > /tmp/setuid_test_program.c
#include <unistd.h>
#include <stdio.h>
int main() {
  alarm(2);
  if (setuid(0) == -1) { // Attempt to become root
    // perror(\"setuid failed as expected\"); // This might use syscalls not allowed after setuid attempt
    return 1; // Expected failure from setuid call itself if not killed
  }
  return 0; // Should not be reached if seccomp active
}
EOF
cc -o /tmp/setuid_test_program /tmp/setuid_test_program.c && /tmp/setuid_test_program ; rm -f /tmp/setuid_test_program /tmp/setuid_test_program.c"
# Expected outcome: Failure (killed by SIGSYS or setuid call fails and exits 1)

echo ""
echo "--- End of Test Payloads ---"
# Note: Actual execution requires a test harness that invokes these payloads with:
# sudo -P /path/to/seccomp_plugin.so <payload_command>
# and then checks the exit status or signal.
# The sudo.conf must also be configured to load seccomp_plugin.so.
# For example, by adding:
# Plugin seccomp_plugin /path/to/build/plugins/seccomp/seccomp_plugin.so
# to a temporary sudo.conf used by the test harness.
