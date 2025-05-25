// GodModeDLL.cpp
#include <windows.h>

extern "C" __declspec(dllexport) void EnableGodMode()
{
    uintptr_t baseAddress = (uintptr_t)GetModuleHandle(NULL); // Base of EXE
    uintptr_t healthAddress = baseAddress + 0x00021B70;
    DWORD oldProtect;

    // Change memory protection to writable
    if (VirtualProtect((LPVOID)healthAddress, sizeof(int), PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        int* pHealth = (int*)healthAddress;
        *pHealth = 1000; // Set health to 1000 (god mode)
        // Optionally, you can keep writing in a loop or create a thread for continuous patching
        VirtualProtect((LPVOID)healthAddress, sizeof(int), oldProtect, &oldProtect);
    }
}

// DLL entry point (optional)
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        EnableGodMode();
    }
    return TRUE;
}
