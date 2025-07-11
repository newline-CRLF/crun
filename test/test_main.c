#include <stdio.h>
#include "test_header.h"

int main() {
    BOOL enabled = FALSE;
    HRESULT hr = DwmIsCompositionEnabled(&enabled);
    if (SUCCEEDED(hr)) {
        if (enabled) {
            printf("Desktop composition is enabled.\n");
        } else {
            printf("Desktop composition is disabled.\n");
        }
    } else {
        printf("Failed to check desktop composition status. HRESULT: 0x%lX\n", hr);
    }
    return 0;
}
