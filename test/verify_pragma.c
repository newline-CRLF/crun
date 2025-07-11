#include <Windows.h>
#include <dwmapi.h>
#include <stdio.h>

// Statically link with dwmapi.lib
//#pragma comment(lib, "dwmapi.lib")

int main() {
    BOOL isCompositionEnabled = FALSE;
    HRESULT hr = DwmIsCompositionEnabled(&isCompositionEnabled);
    if (SUCCEEDED(hr)) {
        if (isCompositionEnabled) {
            printf("DWM composition is enabled.\n");
        } else {
            printf("DWM composition is disabled.\n");
        }
    } else {
        printf("Failed to check DWM composition. Error code: %ld\n", hr);
    }
    return 0;
}

