
// Dear ImGui: FUCK YOU  -/standalone example application for DirectX 9/-
// With Tomb Raider II Cheat Integration
// ====================================
// This file combines:
// - DirectX 9 and ImGui setup
// - Process memory manipulation for game cheats
// - Multithreaded cheat management
// - GUI interface for cheat control

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include <d3d9.h>
#include <tchar.h> 
#include <windows.h> // We're balls deep in WinAPI
#include <tlhelp32.h>
#include <iostream>
#include <cstdint>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <functional>
#include <memory>
#define IDI_ICON1 101  // Sexy lara icon
// ====================================
// Cheat Configuration Constants
// ====================================

// Process to attach to
constexpr const wchar_t* processName = L"Tomb2Cheat.exe"; // Hardcoded because what fucked up fuck would fuck up the fucking name

// Animation Softlock Fix Configuration
constexpr short replacementAnimValue = 11; // Animation ID that resets Lara
const std::vector<short> brokenAnimIDs = { // Problematic animations
    155, // Swan dive death - "... *crack*"
    139, // Rolling ball death
    24, // Fall stumble
    25 // Fall death - "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA *crack*"
    }; 
constexpr uintptr_t addressAnimationCheat = 0x001207BC; // Base address
constexpr uintptr_t pointerOffsetAnimationCheat = 0x14; // Pointer offset

// Health Cheat Configuration
constexpr uint16_t desiredHealthValue = 1000; // Health value to set
constexpr uintptr_t baseAddressHealth = 0x001207BC; // Base address
constexpr uintptr_t pointerOffsetHealth = 0x22; // Pointer offset

// Noclip Cheat Configuration
constexpr uintptr_t noclipAddress = 0x00028ED9; // Address to patch
uint8_t opcodeNoclipOriginal[5] = { 0xE8, 0x82, 0x13, 0x00, 0x00 }; // Original bytes
uint8_t opcodeNoclipPatched[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions

// Underwater Cheat Configuration
constexpr uintptr_t underwaterPatchAddress1 = 0x00030466; // First patch location
constexpr uintptr_t underwaterPatchAddress2 = 0x00030568; // Second patch location
uint8_t opcodeUnderwaterOriginal1[6] = { 0x0F, 0x84, 0x29, 0x04, 0x00, 0x00 }; // Original
uint8_t opcodeUnderwaterPatched1[6] = { 0x0F, 0x85, 0x29, 0x04, 0x00, 0x00 }; // JE->JNE
uint8_t opcodeUnderwaterOriginal2[6] = { 0x0F, 0x8E, 0x27, 0x03, 0x00, 0x00 }; // Original
uint8_t opcodeUnderwaterPatched2[6] = { 0x0F, 0x8D, 0x27, 0x03, 0x00, 0x00 }; // JNG->JNL

// Slope Cheat Configuration
constexpr uintptr_t slopePatchAddress1 = 0x0002B386; // Address to patch for slope walking
constexpr uintptr_t slopePatchAddress2 = 0x0002B397; // Address to patch for slope walking
uint8_t opcodeSlopeOriginal1[3] = { 0x83, 0xF8, 0x02 }; // Original bytes
uint8_t opcodeSlopePatched1[3] = { 0x83, 0xF8, 0x09 }; // JE->JNE
uint8_t opcodeSlopeOriginal2[3] = { 0x83, 0xF8, 0x02 }; // Original bytes
uint8_t opcodeSlopePatched2[3] = { 0x83, 0xF8, 0x09 }; // JNG->JNL

// Slow Fall Cheat Configuration / AKA "Fall Softly Like a Feather" Cheat / AKA Coyote Time
constexpr uintptr_t slowFallPatchAddress = 0x00028BF4; // Address to patch for slow fall
uint8_t opcodeSlowFallOriginal[2] = { 0x74, 0x38 }; // Original bytes
uint8_t opcodeSlowFallPatched[2] = { 0x75, 0x38 }; // JE->JNE

// Unlimited Air Cheat Configuration
constexpr uintptr_t unlimitedAirPatchAddress = 0x0003092A; // Address to patch for unlimited air
uint8_t opcodeUnlimitedAirOriginal[7] = { 0x66, 0xFF, 0x0D, 0xF6, 0x06, 0x52, 0x00 }; // Original bytes
uint8_t opcodeUnlimitedAirPatched[7] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }; // nop the shit out of

// Flare Cheat Configuration
constexpr uintptr_t flareCheatAddress = 0x000317C4; // Address to patch for unlimited flare life
constexpr uintptr_t pointerOffsetFlareCheat = 0x04; // Pointer offset for flare cheat patch
constexpr uint16_t desiredFlareTime = 100; // Desired flare life lock

// ====================================
// Global State Variables
// ====================================

// Process Handling
static DWORD processId = 0;
static HANDLE processHandle = nullptr;
static uintptr_t moduleBase = 0;
static bool processAttached = false;

// Cheat States
static bool noclipCheatPatched = false;
static bool underwaterPatched = false; // Combined state
static bool slopePatched = false;
static bool slowFallCheatPatched = false;
static bool airCheatPatched = false;
// Cheat Management
struct Cheat {
    std::string name;
    bool* active;
    std::thread thread;
    std::function<void()> enableFunction;
    std::function<void()> disableFunction;
    std::function<void()> threadFunction;
};
std::vector<Cheat> cheats;
bool showCheatWindow = true;


// Ignore this shitty shit
void DetachFromProcess(); // Forward declaration
DWORD GetProcessIdByName(const wchar_t* name); // Forward declaration
uintptr_t GetModuleBaseAddress(DWORD pid, const wchar_t* moduleName); // Forward declaration
// ====================================
// Process Handling Functions
// ====================================

/**
 * Attempts to attach to the target process
 * @return true if attachment succeeded, false otherwise
 */
bool AttachToProcess() {
    processId = GetProcessIdByName(processName);
    if (!processId) return false;

    processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!processHandle) return false;

    moduleBase = GetModuleBaseAddress(processId, processName);
    if (!moduleBase) {
        CloseHandle(processHandle);
        return false;
    }

    processAttached = true;
    return true;
}

/**
 * Cleans up process handles and resets state
 */
void DetachFromProcess() {
    // Disable all active cheats first
    for (auto& cheat : cheats) {
        if (*cheat.active) {
            if (cheat.disableFunction) cheat.disableFunction();
            *cheat.active = false;
            if (cheat.thread.joinable()) cheat.thread.join();
        }
    }

    if (processHandle) {
        CloseHandle(processHandle);
        processHandle = nullptr;
    }

    processAttached = false;
    processId = 0;
    moduleBase = 0;
}

/**
 * Finds process ID by executable name
 * @param name Process name to search for
 * @return Process ID or 0 if not found
 */
DWORD GetProcessIdByName(const wchar_t* name) {
    PROCESSENTRY32W entry = { sizeof(PROCESSENTRY32W) };
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    DWORD pid = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, name) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return pid;
}

/**
 * Gets base address of a module in a process
 * @param pid Process ID to inspect
 * @param moduleName Module name to find
 * @return Base address or 0 if not found
 */
uintptr_t GetModuleBaseAddress(DWORD pid, const wchar_t* moduleName) {
    MODULEENTRY32W entry = { sizeof(MODULEENTRY32W) };
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    uintptr_t baseAddress = 0;
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, moduleName) == 0) {
                baseAddress = (uintptr_t)entry.modBaseAddr;
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return baseAddress;
}

/**
 * Reads a chain of pointers to resolve final address
 * @param hProc Process handle
 * @param base Starting address
 * @param offsets List of offsets to follow
 * @param outAddress Output resolved address
 * @return true if successful, false otherwise
 */
bool ReadPointerChain(HANDLE hProc, uintptr_t base, const std::vector<uintptr_t>& offsets, uintptr_t& outAddress) {
    uintptr_t current = base;
    for (auto offset : offsets) {
        uintptr_t next = 0;
        if (!ReadProcessMemory(hProc, (LPCVOID)current, &next, sizeof(next), nullptr))
            return false;
        current = next + offset;
    }
    outAddress = current;
    return true;
}

// ====================================
// Cheat Implementation Functions
// ====================================

/**
 * Toggles noclip by patching game code
 * @param enable Whether to enable or disable
 */
void ToggleNoclip(bool enable) {
    if (!processAttached) return;

    if (enable) {
        if (!noclipCheatPatched) {
            WriteProcessMemory(processHandle, (LPVOID)(moduleBase + noclipAddress),
                opcodeNoclipPatched, sizeof(opcodeNoclipPatched), nullptr);
            noclipCheatPatched = true;
        }
    }
    else {
        if (noclipCheatPatched) {
            WriteProcessMemory(processHandle, (LPVOID)(moduleBase + noclipAddress),
                opcodeNoclipOriginal, sizeof(opcodeNoclipOriginal), nullptr);
            noclipCheatPatched = false;
        }
    }
}
/**
 * Toggles slow fall by patching game code
 * @param enable Whether to enable or disable
 */
void ToggleSlowFall(bool enable) {
    if (!processAttached) return;

    if (enable) {
        if (!slowFallCheatPatched) {
            WriteProcessMemory(processHandle, (LPVOID)(moduleBase + slowFallPatchAddress),
                opcodeSlowFallPatched, sizeof(opcodeSlowFallPatched), nullptr);
            slowFallCheatPatched = true;
        }
    }
    else {
        if (slowFallCheatPatched) {
            WriteProcessMemory(processHandle, (LPVOID)(moduleBase + slowFallPatchAddress),
                opcodeSlowFallOriginal, sizeof(opcodeSlowFallOriginal), nullptr);
            slowFallCheatPatched = false;
        }
    }
}
/**
 * Toggles aquaman lara by patching game code
 * @param enable Whether to enable or disable
 */
void ToggleUnlimitedAir(bool enable) {
    if (!processAttached) return;

    if (enable) {
        if (!airCheatPatched) {
            WriteProcessMemory(processHandle, (LPVOID)(moduleBase + unlimitedAirPatchAddress),
                opcodeUnlimitedAirPatched, sizeof(opcodeUnlimitedAirPatched), nullptr);
            airCheatPatched = true;
        }
    }
    else {
        if (airCheatPatched) {
            WriteProcessMemory(processHandle, (LPVOID)(moduleBase + unlimitedAirPatchAddress),
                opcodeUnlimitedAirOriginal, sizeof(opcodeUnlimitedAirOriginal), nullptr);
            airCheatPatched = false;
        }
    }
}
/**
 * Memory patch configuration structure
 */
struct Patch {
    uintptr_t address;     // Address to patch
    BYTE* patchBytes;      // New bytes to write
    size_t patchSize;      // Number of bytes
    BYTE* originalBytes;   // Original bytes
};

/**
 * Underwater cheat patches
 */
Patch underwaterPatches[] = {
    { underwaterPatchAddress1, opcodeUnderwaterPatched1, sizeof(opcodeUnderwaterPatched1), opcodeUnderwaterOriginal1 },
    { underwaterPatchAddress2, opcodeUnderwaterPatched2, sizeof(opcodeUnderwaterPatched2), opcodeUnderwaterOriginal2 }
};

/**
 * Slope cheat patches
 */
Patch slopePatches[] = {
    { slopePatchAddress1, opcodeSlopePatched1, sizeof(opcodeSlopePatched1), opcodeSlopeOriginal1 },
    { slopePatchAddress2, opcodeSlopePatched2, sizeof(opcodeSlopePatched2), opcodeSlopeOriginal2 }
};
/**
 * Applies or reverts multiple memory patches
 * @param enable Whether to apply patches
 * @param patches Array of patches
 * @param patchCount Number of patches
 */
void ToggleMultiPatch(bool enable, Patch patches[], size_t patchCount) {
    if (!processAttached) return;

    for (size_t i = 0; i < patchCount; ++i) {
        BYTE* bytesToWrite = enable ? patches[i].patchBytes : patches[i].originalBytes;
        WriteProcessMemory(processHandle, (LPVOID)(moduleBase + patches[i].address),
            bytesToWrite, patches[i].patchSize, nullptr);
    }
}

/**
 * Toggles underwater movement cheat
 * @param enable Whether to enable
 */
void ToggleUnderwater(bool enable) {
    if (!processAttached) return;

    if (enable && !underwaterPatched) {
        ToggleMultiPatch(true, underwaterPatches, sizeof(underwaterPatches) / sizeof(Patch));
        underwaterPatched = true;
    }
    else if (!enable && underwaterPatched) {
        ToggleMultiPatch(false, underwaterPatches, sizeof(underwaterPatches) / sizeof(Patch));
        underwaterPatched = false;
    }
}
/**
 * Toggles slope movement cheat
 * @param enable Whether to enable
 */
void ToggleSlopeCheat(bool enable) {
    if (!processAttached) return;

    if (enable && !slopePatched) {
        ToggleMultiPatch(true, slopePatches, sizeof(slopePatches) / sizeof(Patch));
        slopePatched = true;
    }
    else if (!enable && slopePatched) {
        ToggleMultiPatch(false, slopePatches, sizeof(slopePatches) / sizeof(Patch));
        slopePatched = false;
    }
}
/**
 * Worker thread for animation fix cheat
 * @param active Atomic flag to control execution
 */
void AnimationFixThread(std::atomic<bool>& active) {
    uintptr_t animAddress = 0;
    if (!ReadPointerChain(processHandle, moduleBase + addressAnimationCheat,
        { pointerOffsetAnimationCheat }, animAddress)) {
        return;
    }

    while (active) {
        short currentAnim = 0;
        if (ReadProcessMemory(processHandle, (LPCVOID)animAddress, &currentAnim, sizeof(currentAnim), nullptr)) {
            if (std::find(brokenAnimIDs.begin(), brokenAnimIDs.end(), currentAnim) != brokenAnimIDs.end()) {
                WriteProcessMemory(processHandle, (LPVOID)animAddress,
                    &replacementAnimValue, sizeof(replacementAnimValue), nullptr);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void FlareCheatThread(std::atomic<bool>& active) {
    uintptr_t flareAddress = 0;
    if (!ReadPointerChain(processHandle, moduleBase + flareCheatAddress,
        { pointerOffsetFlareCheat }, flareAddress)) {
        return;
    }

    while (active) {
        short currentFlareLife = 0;
        if (ReadProcessMemory(processHandle, (LPCVOID)flareAddress, &currentFlareLife, sizeof(currentFlareLife), nullptr)) {
            if (currentFlareLife > desiredFlareTime) {
                WriteProcessMemory(processHandle, (LPVOID)flareAddress,
                    &desiredFlareTime, sizeof(desiredFlareTime), nullptr);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

/**
 * Worker thread for health cheat
 * @param active Atomic flag to control execution
 */
void HealthCheatThread(std::atomic<bool>& active) {
    uintptr_t healthAddress = 0;
    if (!ReadPointerChain(processHandle, moduleBase + baseAddressHealth,
        { pointerOffsetHealth }, healthAddress)) {
        return;
    }

    while (active) {
        WriteProcessMemory(processHandle, (LPVOID)healthAddress,
            &desiredHealthValue, sizeof(desiredHealthValue), nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// ====================================
// Cheat System Initialization
// ====================================

/**
 * Sets up all cheat modules with their callbacks
 */
void InitializeCheats() {
    static bool animationFixActive = false;
    static bool noclipActive = false;
    static bool underwaterActive = false;
    static bool healthCheatActive = false;
    static bool flareCheatActive = false;
    static bool slopeActive = false;
    static bool slowFallActive = false;
    static bool unlimitedAirActive = false; // Unlimited air cheat
    static std::atomic<bool> flareCheatActiveAtomic{ false };
    static std::atomic<bool> animationFixActiveAtomic{ false };
    static std::atomic<bool> healthCheatActiveAtomic{ false };

    cheats.clear();

    // Animation Fix Cheat
    cheats.push_back({
        "No Animation softlock & stumble anims (i.e. Swan dive death) (Note: Separate stumbles)",
        &animationFixActive,
        std::thread(),
        [&]() {
            animationFixActive = true;
            animationFixActiveAtomic = true;
            cheats[0].thread = std::thread(AnimationFixThread, std::ref(animationFixActiveAtomic));
        },
        [&]() {
            animationFixActive = false;
            animationFixActiveAtomic = false;
            if (cheats[0].thread.joinable()) cheats[0].thread.join();
        },
        nullptr
        });
    // Unlimited Flare Cheat
    cheats.push_back({
        "Unlimited Flare Life (WARNING: DO NOT USE WITH ANIMATION CHEAT ABOVE) (Note: Perhaps a conditional to turn off the checkbox?)",
        &flareCheatActive,
        std::thread(),
        [&]() {
            flareCheatActive = true;
            flareCheatActiveAtomic = true;
            cheats[0].thread = std::thread(FlareCheatThread, std::ref(flareCheatActiveAtomic));
        },
        [&]() {
            flareCheatActive = false;
            flareCheatActiveAtomic = false;
            if (cheats[0].thread.joinable()) cheats[0].thread.join();
        },
        nullptr
        });
    // Noclip Cheat
    cheats.push_back({
        "Noclip (No collision) (Doom style) (Note: Incomplete)",
        &noclipActive,
        std::thread(),
        [&]() { ToggleNoclip(true); },
        [&]() { ToggleNoclip(false); },
        nullptr
        });

    // Underwater Cheat
    cheats.push_back({
        "Ignore Water (Make it walkable)",
        &underwaterActive,
        std::thread(),
        [&]() { ToggleUnderwater(true); },
        [&]() { ToggleUnderwater(false); },
        nullptr
        });
    // Walk on slopes Cheat
    cheats.push_back({
        "Walkable slopes (Note: Half assed. Fix)",
        &slopeActive,
        std::thread(),
        [&]() { ToggleSlopeCheat(true); },
        [&]() { ToggleSlopeCheat(false); },
        nullptr
        });
    // Slow fall Cheat
    cheats.push_back({
        "Slow Fall From Edges",
        &slowFallActive,
        std::thread(),
        [&]() { ToggleSlowFall(true); },
        [&]() { ToggleSlowFall(false); },
        nullptr
        });
    cheats.push_back({
    "Unlimited Air",
    &unlimitedAirActive,
    std::thread(),
    [&]() { ToggleUnlimitedAir(true); },
    [&]() { ToggleUnlimitedAir(false); },
    nullptr
        });
    // Health Cheat
    cheats.push_back({
        "Health Cheat (Note: Set 5000 nops to mute out half of the deaths)",// Well technically it just sets health to 1000 but im just too lazy to do that shit
        &healthCheatActive,
        std::thread(),
        [&]() {
            healthCheatActive = true;
            healthCheatActiveAtomic = true;
            cheats.back().thread = std::thread(HealthCheatThread, std::ref(healthCheatActiveAtomic));
        },
        [&]() {
            healthCheatActive = false;
            healthCheatActiveAtomic = false;
            if (cheats.back().thread.joinable()) cheats.back().thread.join();
        },
        nullptr
        });
}

// ====================================
// DirectX9 and ImGui Setup (Original Code)
// ====================================
// Note: Do not touch it or die

// Fancy theme for daddy
void SetFancyTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Backgrounds
    colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.07f, 0.04f, 1.00f);   // Very dark greenish black
    colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.12f, 0.07f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.15f, 0.09f, 1.00f);

    // Headers & Buttons - dark green with bright hover
    colors[ImGuiCol_Header] = ImVec4(0.12f, 0.25f, 0.12f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.40f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.15f, 0.35f, 0.15f, 1.00f);

    colors[ImGuiCol_Button] = ImVec4(0.10f, 0.22f, 0.10f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.44f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.18f, 0.36f, 0.18f, 1.00f);

    // Frame background (input fields, combo boxes)
    colors[ImGuiCol_FrameBg] = ImVec4(0.07f, 0.14f, 0.07f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.30f, 0.15f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.36f, 0.18f, 1.00f);

    // Text colors
    colors[ImGuiCol_Text] = ImVec4(0.80f, 0.95f, 0.80f, 1.00f);       // Slightly pale green for less strain
    colors[ImGuiCol_TextDisabled] = ImVec4(0.35f, 0.45f, 0.35f, 1.00f);

    // Borders & separators
    colors[ImGuiCol_Border] = ImVec4(0.15f, 0.30f, 0.15f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.12f, 0.25f, 0.12f, 1.00f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.03f, 0.06f, 0.03f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.10f, 0.20f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.36f, 0.18f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.22f, 0.44f, 0.22f, 1.00f);

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.20f, 0.10f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.40f, 0.20f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.15f, 0.30f, 0.15f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.06f, 0.12f, 0.06f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.10f, 0.22f, 0.10f, 1.00f);

    // Title & Menus
    colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.07f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.20f, 0.10f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.07f, 0.12f, 0.07f, 1.00f);

    // Style tweaks
    style.WindowRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 4.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowBorderSize = 1.5f;
    style.ItemSpacing = ImVec2(8, 6);
}

static LPDIRECT3D9              g_pD3D = nullptr;
static LPDIRECT3DDEVICE9        g_pd3dDevice = nullptr;
static bool                     g_DeviceLost = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

bool CreateDeviceD3D(HWND hWnd) {
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D() {
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

void ResetDevice() {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"Tomb Raider II Cheat", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Tomb Raider II Extreme Trainer by Boleklolo", WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, nullptr, nullptr, wc.hInstance, nullptr);
    
    
    // Load small icon 
    HICON hIconSmall = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 64, 64, LR_DEFAULTCOLOR);

    // Load large icon 
    HICON hIconLarge = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 128, 128, LR_DEFAULTCOLOR);

    if (hIconSmall && hIconLarge)
    {
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconLarge);
    }
    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);
    SetFancyTheme(); // Set some fancy theme for daddy
    // Initialize our cheat system
    InitializeCheats();

    // Main loop
    bool done = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    while (!done)
    {
        // Poll and handle messages
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle device lost
        if (g_DeviceLost)
        {
            HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
            if (hr == D3DERR_DEVICELOST)
            {
                ::Sleep(10);
                continue;
            }
            if (hr == D3DERR_DEVICENOTRESET)
                ResetDevice();
            g_DeviceLost = false;
        }

        // Handle window resize
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            g_d3dpp.BackBufferWidth = g_ResizeWidth;
            g_d3dpp.BackBufferHeight = g_ResizeHeight;
            g_ResizeWidth = g_ResizeHeight = 0;
            ResetDevice();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Get current client rect
        RECT rect;
        GetClientRect(hwnd, &rect);
        ImVec2 size = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

        // Set ImGui window to fill the entire client area
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(size);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

        ImGui::Begin("Tomb Raider II Cheat Menu", &showCheatWindow, flags);

        if (ImGui::Button(processAttached ? "Detach from Process" : "Attach to Process")) {
            if (processAttached) {
                DetachFromProcess();
            }
            else {
                if (!AttachToProcess()) {
                    ImGui::OpenPopup("Error");
                }
            }
        }

        if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Failed to attach to process! Are you sure you've opened the Tomb2Cheat executable and not Tomb2?");
            if (ImGui::Button("OK")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Separator();

        if (processAttached) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Attached to process");
            ImGui::Text("Process ID: %lu", processId);
            ImGui::Text("Module Base: 0x%p", (void*)moduleBase);

            ImGui::Separator();
            ImGui::Text("Available cheats:");

            for (size_t i = 0; i < cheats.size(); i++) {
                bool active = *cheats[i].active;
                if (ImGui::Checkbox(cheats[i].name.c_str(), &active)) {
                    if (active) {
                        if (cheats[i].enableFunction) cheats[i].enableFunction();
                    }
                    else {
                        if (cheats[i].disableFunction) cheats[i].disableFunction();
                    }
                    *cheats[i].active = active;
                }
            }
        }
        else {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Not attached to process");
        }
        ImGui::Separator();
        ImGui::Text("Pozdrawiam Pana Baja");
        ImGui::End();

        // Rendering
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA(
            (int)(clear_color.x * clear_color.w * 255.0f),
            (int)(clear_color.y * clear_color.w * 255.0f),
            (int)(clear_color.z * clear_color.w * 255.0f),
            (int)(clear_color.w * 255.0f));
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
        if (result == D3DERR_DEVICELOST)
            g_DeviceLost = true;
    }

    // Cleanup
    DetachFromProcess();
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}
