#include <netinet/in.h>
#include <stdint.h>
#include <switch.h>
#include <sys/socket.h>
#include <stdatomic.h>

#ifndef __SWITCH2__
// Observe the actual buffer without changing libnx's socket initialization.
static atomic_int largest_receive_buffer = 0;

int __real_setsockopt(int socket, int level, int option, const void* value, socklen_t length);

int __wrap_setsockopt(int socket, int level, int option, const void* value, socklen_t length) {
    const int result = __real_setsockopt(socket, level, option, value, length);
    if (result == 0 && level == SOL_SOCKET && option == SO_RCVBUF) {
        int actual = 0;
        socklen_t size = sizeof(actual);
        if (getsockopt(socket, SOL_SOCKET, SO_RCVBUF, &actual, &size) == 0) {
            int previous = atomic_load_explicit(&largest_receive_buffer, memory_order_relaxed);
            while (actual > previous && !atomic_compare_exchange_weak_explicit(
                       &largest_receive_buffer, &previous, actual,
                       memory_order_relaxed, memory_order_relaxed)) {}
        }
    }
    return result;
}

int moonlightSocketReceiveBufferBytes(void) {
    return atomic_load_explicit(&largest_receive_buffer, memory_order_relaxed);
}
#endif

struct sigaction;

int sigaction(int a, const struct sigaction* b, struct sigaction* c) {
    return 0;
}
