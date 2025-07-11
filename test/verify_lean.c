//#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// The winsock.h header, which defines the SOCKET type, is excluded when
// WIN32_LEAN_AND_MEAN is defined. This will cause a compile error.
int main() {
    SOCKET s;
    (void)s; // Avoid unused variable warning if compilation proceeds
    return 0;
}