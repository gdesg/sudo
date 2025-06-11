#include <stdio.h>
#include <sys/socket.h> // For socket()
#include <unistd.h>     // For alarm()

int main(void) {
    // Make sure the test doesn't hang indefinitely if seccomp causes weirdness
    alarm(2);
    // Attempt to create a socket, a common syscall to restrict.
    // If the seccomp filter is active and doesn't allow socket(),
    // the process should be killed, likely by SIGSYS.
    socket(AF_INET, SOCK_STREAM, 0);
    fprintf(stderr, "Error: disallowed_payload executed socket() call successfully!\n");
    return 1; // Should not be reached if seccomp kills it
}
