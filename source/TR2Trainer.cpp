
// Dear ImGui: FUCK YOU  -/standalone example application for DirectX 9/-
// With Tomb Raider II Cheat Integration
// ====================================
// This file combines:
// - DirectX 9 and ImGui setup
// - Process memory manipulation for game cheats
// - Multithreaded cheat management
// - GUI interface for cheat control

// Why is it in one big file you may ask? Well I didnt expect it to be so big and im a psycho too idk

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
void WriteOneShot(uintptr_t baseAddress, uintptr_t pointerOffset, uint16_t valueToWrite);
// Process to attach to
constexpr const wchar_t* processName = L"Tomb2Cheat.exe"; // Hardcoded because what fucked up fuck would fuck up the fucking name

// Animation Softlock Fix Configuration
constexpr short replacementAnimValue = 11; // Animation ID that resets Lara
const std::vector<short> brokenAnimIDs = { // Problematic animations
    155, // Swan dive death - "... *crack*"
    139, // Rolling ball death
    25 // Fall death - "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA *crack*"
    }; 
const std::vector<short> stumbleAnimIDs = { // Problematic animations
    24, // Fall stumble
    31, // Jump stumble
    82 // Land

}; 
constexpr uintptr_t addressAnimationCheat = 0x001207BC; // Base address
constexpr uintptr_t pointerOffsetAnimationCheat = 0x14; // Pointer offset

// Health Cheat Configuration
constexpr uint16_t desiredHealthValue = 1000; // Health value to set
constexpr uintptr_t baseAddressHealth = 0x001207BC; // Base address
constexpr uintptr_t pointerOffsetHealth = 0x22; // Pointer offset

// Noclip Cheat Configuration
constexpr uintptr_t noclipAddress = 0x00028EFD; // Address to patch
uint8_t opcodeNoclipOriginal[5] = { 0xE8, 0xCE, 0x13, 0x00, 0x00 }; // Original bytes
uint8_t opcodeNoclipPatched[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions

//! Worse noclip with no fall, maybe implement as another cheat?
constexpr uintptr_t noclipAlt2Address = 0x00028ED9; // Address to patch
uint8_t opcodeNoclipAlt2Original[5] = { 0xE8, 0x82, 0x13, 0x00, 0x00 }; // Original bytes
uint8_t opcodeNoclipAlt2Patched[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions

// Underwater Cheat Configuration
constexpr uintptr_t underwaterPatchAddress1 = 0x00030466; // First patch location
constexpr uintptr_t underwaterPatchAddress2 = 0x00030568; // Second patch location
uint8_t opcodeUnderwaterOriginal1[6] = { 0x0F, 0x84, 0x29, 0x04, 0x00, 0x00 }; // Original
uint8_t opcodeUnderwaterPatched1[6] = { 0x0F, 0x85, 0x29, 0x04, 0x00, 0x00 }; // JE->JNE
uint8_t opcodeUnderwaterOriginal2[6] = { 0x0F, 0x8E, 0x27, 0x03, 0x00, 0x00 }; // Original
uint8_t opcodeUnderwaterPatched2[6] = { 0x0F, 0x8D, 0x27, 0x03, 0x00, 0x00 }; // JNG->JNL
// Air Time Cheat Configuration
constexpr uintptr_t airTimePatchAddress1 = 0x00027C28; // First patch location
constexpr uintptr_t airTimePatchAddress2 = 0x00027EDD; // Second patch location
constexpr uintptr_t airTimePatchAddress3 = 0x00028304; // Third patch location

uint8_t opcodeairTimeOriginal1[6] = { 0x66, 0x81, 0x78, 0x20, 0x83, 0x00 }; // Original
uint8_t opcodeairTimePatched1[6] = { 0x66, 0x81, 0x78, 0x20, 0xFF, 0x00 }; // JE->JNE
uint8_t opcodeairTimeOriginal2[6] = { 0x66, 0x81, 0x78, 0x20, 0x83, 0x00 }; // Original
uint8_t opcodeairTimePatched2[6] = { 0x66, 0x81, 0x78, 0x20, 0xFF, 0x00 }; // JNG->JNL
uint8_t opcodeairTimeOriginal3[6] = { 0x66, 0x81, 0x78, 0x20, 0x83, 0x00 }; // Original
uint8_t opcodeairTimePatched3[6] = { 0x66, 0x81, 0x78, 0x20, 0xFF, 0x00 }; // JNG->JNL
// Slope Cheat Configuration
constexpr uintptr_t slopePatchAddress1 = 0x0002B386; // Address to patch for slope walking
constexpr uintptr_t slopePatchAddress2 = 0x0002B397; // Address to patch for slope walking
uint8_t opcodeSlopeOriginal1[3] = { 0x83, 0xF8, 0x02 }; // Original bytes
uint8_t opcodeSlopePatched1[3] = { 0x83, 0xF8, 0x19 }; // JE->JNE
uint8_t opcodeSlopeOriginal2[3] = { 0x83, 0xF8, 0x02 }; // Original bytes
uint8_t opcodeSlopePatched2[3] = { 0x83, 0xF8, 0x19 }; // JNG->JNL

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

// No static collision
//Tomb2Cheat.exe+12CD6 - E8 E5020000           - call Tomb2Cheat.exe+12FC0
constexpr uintptr_t noStaticCheatAddress = 0x00012CD6; // Address to the no collision to statics
uint8_t opcodeNoStaticCollisionOriginal[5] = { 0xE8, 0xE5, 0x02, 0x00, 0x00 };
uint8_t opcodeNoStaticCollisionPatched[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 };

// No baddie collision
//Tomb2Cheat.exe+13789 - E8 32000000           - call Tomb2Cheat.exe+137C0
constexpr uintptr_t noBaddieCollisionAddress = 0x00013789; // Addy
uint8_t opcodeNoBaddieCollisionOriginal[5] = { 0xE8, 0x32, 0x00, 0x00, 0x00 }; // Originny
uint8_t opcodeNoBaddieCollisionPatched[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // Noppies

// Call flipmap
constexpr uintptr_t flipMapAddress = 0x00016610;


// No creature collision
//Tomb2Cheat.exe+138AB - E8 60010000           - call Tomb2Cheat.exe+13A10
constexpr uintptr_t noCreatureCollisionAddress = 0x000138AB; // Address to patch for no creature collision
uint8_t opcodeNoCreatureCollisionOriginal[5] = { 0xE8, 0x60, 0x01, 0x00, 0x00 }; // Original bytes
uint8_t opcodeNoCreatureCollisionPatched[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions

// No door collision
// This patch finna be so fucking big im gonna cry
// Actually, lemme rephrase that
// 
// 
// // 🚪💀 Door collision is OFFICIALLY CANCELLED.
// This patch finna be so fucking fat it’s gonna need gzip compression just to compile.
// 
// 
// Helpers
// Door collision
// Tomb2Cheat.exe+13942 - E8 D9030000           - call Tomb2Cheat.exe+13D20
// Tomb2Cheat.exe+13950 - E8 0BC10200           - call Tomb2Cheat.exe+3FA60
// Tomb2Cheat.exe+1397A - E8 91000000           - call Tomb2Cheat.exe+13A10
// Tomb2Cheat.exe+1398B - E8 80000000           - call Tomb2Cheat.exe+13A10
// Door openers at start or whatever
// Tomb2Cheat.exe+353A0 - E8 5BFDFFFF           - call Tomb2Cheat.exe+35100
// Tomb2Cheat.exe+353A9 - E8 52FDFFFF           - call Tomb2Cheat.exe+35100
// Tomb2Cheat.exe+35539 - E8 C2FBFFFF           - call Tomb2Cheat.exe+35100
// Tomb2Cheat.exe+35542 - E8 B9FBFFFF           - call Tomb2Cheat.exe+35100
// Tomb2Cheat.exe+35600 - E8 FBFAFFFF           - call Tomb2Cheat.exe+35100
// Tomb2Cheat.exe+3560C - E8 EFFAFFFF           - call Tomb2Cheat.exe+35100
// Tomb2Cheat.exe+35618 - E8 E3FAFFFF           - call Tomb2Cheat.exe+35100
// Tomb2Cheat.exe+35624 - E8 D7FAFFFF           - call Tomb2Cheat.exe+35100

// Always replace the call +35100 with +35150 (close door to open door btw)

// Door collision patch addresses
constexpr uintptr_t doorCollisionPatchAddress1 = 0x00013942; // First patch location
constexpr uintptr_t doorCollisionPatchAddress2 = 0x00013950; // Second patch location
constexpr uintptr_t doorCollisionPatchAddress3 = 0x0001397A; // Third patch location
constexpr uintptr_t doorCollisionPatchAddress4 = 0x0001398B; // Fourth patch location
// Door opener patch addresses
constexpr uintptr_t doorOpenerPatchAddress1 = 0x000353A0; // First door opener patch
constexpr uintptr_t doorOpenerPatchAddress2 = 0x000353A9; // Second door opener patch
constexpr uintptr_t doorOpenerPatchAddress3 = 0x00035539; // Third door opener patch
constexpr uintptr_t doorOpenerPatchAddress4 = 0x00035542; // Fourth door opener patch
constexpr uintptr_t doorOpenerPatchAddress5 = 0x00035600; // Fifth door opener patch
constexpr uintptr_t doorOpenerPatchAddress6 = 0x0003560C; // Sixth door opener patch
constexpr uintptr_t doorOpenerPatchAddress7 = 0x00035618; // Seventh door opener patch
constexpr uintptr_t doorOpenerPatchAddress8 = 0x00035624; // Eighth door opener patch


// Door collision patch original bytes
uint8_t opcodeDoorCollisionOriginal1[5] = { 0xE8, 0xD9, 0x03, 0x00, 0x00 }; // Original bytes for first patch
uint8_t opcodeDoorCollisionOriginal2[5] = { 0xE8, 0x0B, 0xC1, 0x02, 0x00 }; // Original bytes for second patch
uint8_t opcodeDoorCollisionOriginal3[5] = { 0xE8, 0x91, 0x00, 0x00, 0x00 }; // Original bytes for third patch
uint8_t opcodeDoorCollisionOriginal4[5] = { 0xE8, 0x80, 0x00, 0x00, 0x00 }; // Original bytes for fourth patch
// Door opener patch original bytes
uint8_t opcodeDoorOpenerOriginal1[5] = { 0xE8, 0x5B, 0xFD, 0xFF, 0xFF }; // Original bytes for first door opener patch
uint8_t opcodeDoorOpenerOriginal2[5] = { 0xE8, 0x52, 0xFD, 0xFF, 0xFF }; // Original bytes for second door opener patch
uint8_t opcodeDoorOpenerOriginal3[5] = { 0xE8, 0xC2, 0xFB, 0xFF, 0xFF }; // Original bytes for third door opener patch
uint8_t opcodeDoorOpenerOriginal4[5] = { 0xE8, 0xB9, 0xFB, 0xFF, 0xFF }; // Original bytes for fourth door opener patch
uint8_t opcodeDoorOpenerOriginal5[5] = { 0xE8, 0xFB, 0xFA, 0xFF, 0xFF }; // Original bytes for fifth door opener patch
uint8_t opcodeDoorOpenerOriginal6[5] = { 0xE8, 0xEF, 0xFA, 0xFF, 0xFF }; // Original bytes for sixth door opener patch
uint8_t opcodeDoorOpenerOriginal7[5] = { 0xE8, 0xE3, 0xFA, 0xFF, 0xFF }; // Original bytes for seventh door opener patch
uint8_t opcodeDoorOpenerOriginal8[5] = { 0xE8, 0xD7, 0xFA, 0xFF, 0xFF }; // Original bytes for eighth door opener patch
// Door collision patch patched bytes
uint8_t opcodeDoorCollisionPatched1[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions for first patch
uint8_t opcodeDoorCollisionPatched2[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions for second patch
uint8_t opcodeDoorCollisionPatched3[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions for third patch
uint8_t opcodeDoorCollisionPatched4[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions for fourth patch
uint8_t opcodeDoorOpenerPatched1[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions for first door opener patch
uint8_t opcodeDoorOpenerPatched2[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions for second door opener patch
uint8_t opcodeDoorOpenerPatched3[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions for third door opener patch
uint8_t opcodeDoorOpenerPatched4[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions for fourth door opener patch
uint8_t opcodeDoorOpenerPatched5[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions for fifth door opener patch
uint8_t opcodeDoorOpenerPatched6[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions for sixth door opener patch
uint8_t opcodeDoorOpenerPatched7[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions for seventh door opener patch
uint8_t opcodeDoorOpenerPatched8[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // NOP instructions for eighth door opener patch


// Finish Level cheat
constexpr uint16_t finishLevelNum = 1; // Health value to set
constexpr uintptr_t baseAddressFinish = 0x00014634; // Base address
constexpr uintptr_t pointerOffsetFinish = 0x04; // Pointer offset

// Climb anything cheat
uintptr_t climbAnythingAddress = 0x000151E3; // Address to patch for climb anything
uintptr_t climbAnythingStateAddress = 0x000243E0; // Address to check climb state
uintptr_t climbAnythingStateOffset = 0x06; // Offset for climb state pointer
//Tomb2Cheat.exe+151E3 - 75 07                 - jne Tomb2Cheat.exe+151EC
uint8_t opcodeClimbAnythingOriginal[2] = { 0x75, 0x07 }; // Original bytes
uint8_t opcodeClimbAnythingPatched[2] = { 0x74, 0x07 }; // Jump over the check






// MAKE THIS CHGEAT !!!!!! RAOFLLMAO
//Tomb2Cheat.exe+30C41 - C1 F8 0F              - sar eax,0F { 15 } - Run
//Tomb2Cheat.exe+30C70 - C1 F8 10              - sar eax,10 { 16 } - Jump
//Tomb2Cheat.exe+30C70 - B8 80000000           - mov eax,00000080 { 128 } - jump patched

// Tomb2Cheat.exe+423A4 - 7E/D 02                 - jle Tomb2Cheat.exe+423A8 trapdoor floor
// Tomb2Cheat.exe+423E4 - 7D/E 07                 - jnl Tomb2Cheat.exe+423ED trapdoor ceiling

//Patched/Unpactched


//DOZY FINALLY
// Tomb2Cheat.exe+32604 - 74 55                 - je Tomb2Cheat.exe+3265B - Water check to let you glide to other rooms
// Tomb2Cheat.exe+3076A - 0F85 25010000         - jne Tomb2Cheat.exe+30895 - Do not reset back to above water
// Tomb2Cheat.exe+3076A - 0F84 25010000         - je Tomb2Cheat.exe+30895 - above but patched

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
static bool airTimeCheatPatched = false;
static bool underwaterPatched = false; // Combined state
static bool slopePatched = false;
static bool slowFallCheatPatched = false;
static bool airCheatPatched = false;
static bool noStaticCollisionPatched = false;
static bool noBaddieCollisionPatched = false;
static bool noCreatureCollisionPatched = false;
static bool altNoclipCheatPatched = false; // Alternative noclip state
static bool noDoorCollisionPatched = false; // Combined state for door collision
static bool climbAnythingPatched = false;
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
                break; // If youre reading this, hit me up on Discord (Boleklolo), idk why, just to tell me that somebody reads those comments/code
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
 * No door collision and opener patches
 */

 // I swear to God if someone asks me why there are 12 hardcoded patches here,
 // I will personally walk into their office and replace their chair with a TR2 spike trap.
 // This is how we're doing it now. No loops, no macros, no nothing.
 // I regret nothing.
Patch doorCollisionPatches[] = {
    { doorCollisionPatchAddress1, opcodeDoorCollisionPatched1, sizeof(opcodeDoorCollisionPatched1), opcodeDoorCollisionOriginal1 },
    { doorCollisionPatchAddress2, opcodeDoorCollisionPatched2, sizeof(opcodeDoorCollisionPatched2), opcodeDoorCollisionOriginal2 },
    { doorCollisionPatchAddress3, opcodeDoorCollisionPatched3, sizeof(opcodeDoorCollisionPatched3), opcodeDoorCollisionOriginal3 },
    { doorCollisionPatchAddress4, opcodeDoorCollisionPatched4, sizeof(opcodeDoorCollisionPatched4), opcodeDoorCollisionOriginal4 },
    { doorOpenerPatchAddress1, opcodeDoorOpenerPatched1, sizeof(opcodeDoorOpenerPatched1), opcodeDoorOpenerOriginal1 },
    { doorOpenerPatchAddress2, opcodeDoorOpenerPatched2, sizeof(opcodeDoorOpenerPatched2), opcodeDoorOpenerOriginal2 },
    { doorOpenerPatchAddress3, opcodeDoorOpenerPatched3, sizeof(opcodeDoorOpenerPatched3), opcodeDoorOpenerOriginal3 },
    { doorOpenerPatchAddress4, opcodeDoorOpenerPatched4, sizeof(opcodeDoorOpenerPatched4), opcodeDoorOpenerOriginal4 },
    { doorOpenerPatchAddress5, opcodeDoorOpenerPatched5, sizeof(opcodeDoorOpenerPatched5), opcodeDoorOpenerOriginal5 },
    { doorOpenerPatchAddress6, opcodeDoorOpenerPatched6, sizeof(opcodeDoorOpenerPatched6), opcodeDoorOpenerOriginal6 },
    { doorOpenerPatchAddress7, opcodeDoorOpenerPatched7, sizeof(opcodeDoorOpenerPatched7), opcodeDoorOpenerOriginal7 },
    { doorOpenerPatchAddress8, opcodeDoorOpenerPatched8, sizeof(opcodeDoorOpenerPatched8), opcodeDoorOpenerOriginal8 }
};


//airtime thingies

Patch airTimePatches[] = {
    { airTimePatchAddress1, opcodeairTimePatched1, sizeof(opcodeairTimePatched1), opcodeairTimeOriginal1 },
    { airTimePatchAddress2, opcodeairTimePatched2, sizeof(opcodeairTimePatched2), opcodeairTimeOriginal2 },
    { airTimePatchAddress3, opcodeairTimePatched3, sizeof(opcodeairTimePatched3), opcodeairTimeOriginal3 }
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
    void ToggleAirTime(bool enable) {
        if (!processAttached) return;

        if (enable && !airTimeCheatPatched) {
            ToggleMultiPatch(true, airTimePatches, sizeof(airTimePatches) / sizeof(Patch));
            airTimeCheatPatched = true;
        }
        else if (!enable && airTimeCheatPatched) {
            ToggleMultiPatch(false, airTimePatches, sizeof(airTimePatches) / sizeof(Patch));
            airTimeCheatPatched = false;
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
 * Toggles no static collision cheat
 * @param enable Whether to enable
 */
void ToggleStatic(bool enable) {
    if (!processAttached) return;
    if (enable && !noStaticCollisionPatched) {
        WriteProcessMemory(processHandle, (LPVOID)(moduleBase + noStaticCheatAddress),
            opcodeNoStaticCollisionPatched, sizeof(opcodeNoStaticCollisionPatched), nullptr);
        noStaticCollisionPatched = true;
    }
    else if (!enable && noStaticCollisionPatched) {
        WriteProcessMemory(processHandle, (LPVOID)(moduleBase + noStaticCheatAddress),
            opcodeNoStaticCollisionOriginal, sizeof(opcodeNoStaticCollisionOriginal), nullptr);
        noStaticCollisionPatched = false;
    }
}
void ToggleClimbAnything(bool enable) {
    if (!processAttached) return;
    if (enable && !climbAnythingPatched) {
        WriteProcessMemory(processHandle, (LPVOID)(moduleBase + climbAnythingAddress),
            opcodeClimbAnythingPatched, sizeof(opcodeClimbAnythingPatched), nullptr);
        climbAnythingPatched = true;
        WriteOneShot(climbAnythingStateAddress, climbAnythingStateOffset, 1); // Set climb state to enabled
    }
    else if (!enable && climbAnythingPatched) {
        WriteProcessMemory(processHandle, (LPVOID)(moduleBase + climbAnythingAddress),
            opcodeClimbAnythingOriginal, sizeof(opcodeClimbAnythingOriginal), nullptr);
        climbAnythingPatched = false;
        WriteOneShot(climbAnythingStateAddress, climbAnythingStateOffset, 0); // Set climb state to disabled
    }
}
/**
 * Toggles no baddie collision cheat
 * @param enable Whether to enable
 */
void ToggleNoBaddieCollision(bool enable) {
    if (!processAttached) return;
    if (enable && !noBaddieCollisionPatched) {
        WriteProcessMemory(processHandle, (LPVOID)(moduleBase + noBaddieCollisionAddress),
            opcodeNoBaddieCollisionPatched, sizeof(opcodeNoBaddieCollisionPatched), nullptr);
        noBaddieCollisionPatched = true;
    }
    else if (!enable && noBaddieCollisionPatched) {
        WriteProcessMemory(processHandle, (LPVOID)(moduleBase + noBaddieCollisionAddress),
            opcodeNoBaddieCollisionOriginal, sizeof(opcodeNoBaddieCollisionOriginal), nullptr);
        noBaddieCollisionPatched = false;
    }
}
/**
 * Toggles no creature collision cheat
 * @param enable Whether to enable
 */
void ToggleNoCreatureCollision(bool enable) {
    if (!processAttached) return;
    if (enable && !noCreatureCollisionPatched) {
        WriteProcessMemory(processHandle, (LPVOID)(moduleBase + noCreatureCollisionAddress),
            opcodeNoCreatureCollisionPatched, sizeof(opcodeNoCreatureCollisionPatched), nullptr);
        noCreatureCollisionPatched = true;
    }
    else if (!enable && noCreatureCollisionPatched) {
        WriteProcessMemory(processHandle, (LPVOID)(moduleBase + noCreatureCollisionAddress),
            opcodeNoCreatureCollisionOriginal, sizeof(opcodeNoCreatureCollisionOriginal), nullptr);
        noCreatureCollisionPatched = false;
    }
}
/**
 * Toggles alternate noclip cheat
 * @param enable Whether to enable
 */
void ToggleAltNoclip(bool enable) {
    if (!processAttached) return;
    if (enable && !altNoclipCheatPatched) {
        WriteProcessMemory(processHandle, (LPVOID)(moduleBase + noclipAlt2Address),
            opcodeNoclipAlt2Patched, sizeof(opcodeNoclipAlt2Patched), nullptr);
        altNoclipCheatPatched = true;
    }
    else if (!enable && altNoclipCheatPatched) {
        WriteProcessMemory(processHandle, (LPVOID)(moduleBase + noclipAlt2Address),
            opcodeNoclipAlt2Original, sizeof(opcodeNoclipAlt2Original), nullptr);
        altNoclipCheatPatched = false;
    }
}
/**
 * Toggles no door collision cheat
 * @param enable Whether to enable
 **/ 
void ToggleNoDoorCollision(bool enable) {
    if (!processAttached) return;
    if (enable && !noDoorCollisionPatched) {
        ToggleMultiPatch(true, doorCollisionPatches, sizeof(doorCollisionPatches) / sizeof(Patch));
        noDoorCollisionPatched = true;
    }
    else if (!enable && noDoorCollisionPatched) {
        ToggleMultiPatch(false, doorCollisionPatches, sizeof(doorCollisionPatches) / sizeof(Patch));
        noDoorCollisionPatched = false;
    }
}
void ToggleFlipmapFunction(bool enable) {
    if (!processAttached || !enable) return;

    HANDLE hThread = CreateRemoteThread(
        processHandle,
        nullptr,
        0,
        (LPTHREAD_START_ROUTINE)(moduleBase + flipMapAddress),
        nullptr,
        0,
        nullptr);

    if (hThread) {
        WaitForSingleObject(hThread, 500);
        CloseHandle(hThread);
    }
}
void GiveItem(int argument) {
    if (!processAttached) return;  // Only check process attachment

    HANDLE hThread = CreateRemoteThread(
        processHandle,
        nullptr,
        0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(moduleBase + 0x000242D0),
        reinterpret_cast<LPVOID>(static_cast<INT_PTR>(argument)),  // Cast int to pointer type
        0,
        nullptr
    );

    if (hThread) {
        WaitForSingleObject(hThread, 500);
        CloseHandle(hThread);
    }
}

void GiveAllItems() {
    int itemIDs[] = { 193, 194, 195, 196, 174, 175, 176, 177, 205, 206};
    for (int id : itemIDs) {
        GiveItem(id);
    }
}
void GiveAllWeapons() {
    int itemIDs[] = { 135, 136, 137, 138, 139, 140, 141 };
    for (int id : itemIDs) {
        GiveItem(id);
    }
}

void GiveAmmo() {
    int itemIDs[] = { 143, 144, 145, 146, 147, 148, 151, 149, 150 };
    for (int id : itemIDs) {
        for (int i = 0; i < 15; i++)
        {
            GiveItem(id);
        }
        
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

void AnimationStubmleFixThread(std::atomic<bool>& active) { // Typo im too lazy to fix so Im writing a comment that takes longer to write than to fix it :)
    uintptr_t animAddress = 0;
    if (!ReadPointerChain(processHandle, moduleBase + addressAnimationCheat,
        { pointerOffsetAnimationCheat }, animAddress)) {
        return;
    }

    while (active) {
        short currentAnim = 0;
        if (ReadProcessMemory(processHandle, (LPCVOID)animAddress, &currentAnim, sizeof(currentAnim), nullptr)) {
            if (std::find(stumbleAnimIDs.begin(), stumbleAnimIDs.end(), currentAnim) != stumbleAnimIDs.end()) {
                WriteProcessMemory(processHandle, (LPVOID)animAddress,
                    &replacementAnimValue, sizeof(replacementAnimValue), nullptr);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}



void WriteOneShot(uintptr_t baseAddress, uintptr_t pointerOffset, uint16_t valueToWrite) {
    uintptr_t targetAddress = 0;

    // Resolve pointer chain
    if (!ReadPointerChain(processHandle, moduleBase + baseAddress,
        { pointerOffset }, targetAddress)) {
        std::cerr << "Failed to resolve pointer chain!" << std::endl;
        return;
    }
    // Perform write
    if (!WriteProcessMemory(processHandle, (LPVOID)targetAddress,
        &valueToWrite, sizeof(valueToWrite), nullptr)) {
        std::cerr << "Failed to write to memory!" << std::endl;
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
    static bool animationStumbleFixActive = false;

    static bool noclipActive = false;
    static bool airTimeActive = false;
    static bool underwaterActive = false;
    static bool healthCheatActive = false;
    static bool flareCheatActive = false;
    static bool slopeActive = false;
    static bool slowFallActive = false;
    static bool unlimitedAirActive = false; // Unlimited air cheat
    static bool staticActive = false; // No static collision cheat
    static bool climbActive = false;
    static bool noBaddieCollisionActive = false; // No baddie collision cheat
    static bool noCreatureCollisionActive = false; // No creature collision cheat
    static bool noDoorCollisionActive = false; // No door collision cheat
    static bool altNoclipActive = false; // Alternate noclip cheat
    static bool callFunctionActive = false;
    static bool addItemsFunctionActive = false;
    static bool addWeaponsFunctionActive = false;
    static bool addAmmoFunctionActive = false;
    static bool finishLevelActive = false;

    static std::atomic<bool> flareCheatActiveAtomic{ false };
    static std::atomic<bool> animationFixActiveAtomic{ false };
    static std::atomic<bool> animationStumbleFixActiveAtomic{ false };
    static std::atomic<bool> healthCheatActiveAtomic{ false };

    cheats.clear();

    // Animation Fix Cheat
    cheats.push_back({
        "No Animation Softlock (i.e. Swan Dive death)",
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
    // Finish Level
    cheats.push_back({
        "Finish Level",
        &finishLevelActive,
        std::thread(),
        [&]() { WriteOneShot(baseAddressFinish, pointerOffsetFinish, 1); },
        []() {}, // No disable action needed
        nullptr
        });
    cheats.push_back({
    "No Animation Stumbles",
    &animationStumbleFixActive,
    std::thread(),
    [&]() {
        animationStumbleFixActive = true;
        animationStumbleFixActiveAtomic = true;
        cheats[2].thread = std::thread(AnimationStubmleFixThread, std::ref(animationStumbleFixActiveAtomic));
    },
    [&]() {
        animationStumbleFixActive = false;
        animationStumbleFixActiveAtomic = false;
        if (cheats[2].thread.joinable()) cheats[2].thread.join();
    },
    nullptr
        });
    cheats.push_back({
        "Toggle FlipMap",
        &callFunctionActive,
        std::thread(),
        [&]() { ToggleFlipmapFunction(true); },
        []() {}, // No disable action needed
        nullptr
        });
    cheats.push_back({
    "Give All Keys/Puzzle Items",
    &addItemsFunctionActive,
    std::thread(),
    [&]() { GiveAllItems(); },
    []() {}, // No disable action needed
    nullptr
        });
    cheats.push_back({
    "Give All Weapons",
    &addWeaponsFunctionActive,
    std::thread(),
    [&]() { GiveAllWeapons(); },
    []() {}, // No disable action needed
    nullptr
            });
    cheats.push_back({
"Give Ammo, Meds & Flares",
&addAmmoFunctionActive,
std::thread(),
[&]() { GiveAmmo(); },
[]() {}, // No disable action needed
nullptr
        });
    // Unlimited Flare Cheat
    cheats.push_back({
        "Unlimited Flare Life",
        &flareCheatActive,
        std::thread(),
        [&]() {
            flareCheatActive = true;
            flareCheatActiveAtomic = true;
            cheats[1].thread = std::thread(FlareCheatThread, std::ref(flareCheatActiveAtomic));
        },
        [&]() {
            flareCheatActive = false;
            flareCheatActiveAtomic = false;
            if (cheats[1].thread.joinable()) cheats[1].thread.join();
        },
        nullptr
        });
    // Noclip Cheat
    cheats.push_back({
        "Noclip (No collision) (Doom style) (Works well with slope fix)",
        &noclipActive,
        std::thread(),
        [&]() { ToggleNoclip(true); },
        [&]() { ToggleNoclip(false); },
        nullptr
        });
    // No fall thingy
    cheats.push_back({
        "Unlimited Air Time",
        &airTimeActive,
        std::thread(),
        [&]() { ToggleAirTime(true); },
        [&]() { ToggleAirTime(false); },
        nullptr
        });
    // No static collision Cheat
    cheats.push_back({
        "No Static Object Collision",
        &staticActive,
        std::thread(),
        [&]() { ToggleStatic(true); },
        [&]() { ToggleStatic(false); },
        nullptr
        });
    // Climb
    cheats.push_back({
        "Climb Anywhere",
        &climbActive,
        std::thread(),
        [&]() { ToggleClimbAnything(true); },
        [&]() { ToggleClimbAnything(false); },
        nullptr
        });
    // No baddie collision Cheat
    cheats.push_back({
        "No Baddie Collision",
        &noBaddieCollisionActive,
        std::thread(),
        [&]() { ToggleNoBaddieCollision(true); },
        [&]() { ToggleNoBaddieCollision(false); },
        nullptr
        });
    // No creature collision Cheat
    cheats.push_back({
        "No Creature Collision",
        &noCreatureCollisionActive,
        std::thread(),
        [&]() { ToggleNoCreatureCollision(true); },
        [&]() { ToggleNoCreatureCollision(false); },
        nullptr
        });
    // No door collision Cheat
    cheats.push_back({
        "No Door Collision (Note: You need to reload the level before using)",
        &noDoorCollisionActive,
        std::thread(),
        [&]() { ToggleNoDoorCollision(true); },
        [&]() { ToggleNoDoorCollision(false); },
        nullptr
        });
    cheats.push_back({
        "Alternate Noclip (Includes walking in air)",
        &altNoclipActive,
        std::thread(),
        [&]() { ToggleAltNoclip(true); },
        [&]() { ToggleAltNoclip(false); },
        nullptr
        });
    // Underwater Cheat
    cheats.push_back({
        "Ignore Water (Turn lara to a scuba diver on foot)",
        &underwaterActive,
        std::thread(),
        [&]() { ToggleUnderwater(true); },
        [&]() { ToggleUnderwater(false); },
        nullptr
        });
    // Walk on slopes Cheat
    cheats.push_back({
        "Walkable slopes (Note: Works better with noclip)",
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
        "Unlimited Health",// Well technically it just sets health to 1000 but im just too lazy to do that shit
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
Cheat* FindCheatByName(const std::string& name) {
    for (auto& cheat : cheats) {
        if (cheat.name == name) return &cheat;
    }
    return nullptr;
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
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Disclaimer: This trainer is unfinished");
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "and might get updates if it gets enough popularity");
        ImGui::Separator();
        if (processAttached) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Attached to process");
            ImGui::Text("Process ID: %lu", processId);
            ImGui::Text("Module Base: 0x%p", (void*)moduleBase);

            ImGui::Separator();
            ImGui::Text("Available cheats:");

            // Movement Cheats Section
            if (ImGui::CollapsingHeader("Movement Enhancements", ImGuiTreeNodeFlags_DefaultOpen)) {
                // Noclip
                if (auto* noclip = FindCheatByName("Noclip (No collision) (Doom style) (Works well with slope fix)")) {
                    bool active = *noclip->active;
                    if (ImGui::Checkbox(noclip->name.c_str(), &active)) {
                        if (active && noclip->enableFunction) noclip->enableFunction();
                        else if (!active && noclip->disableFunction) noclip->disableFunction();
                        *noclip->active = active;
                    }
                }

                // Underwater Movement
                if (auto* underwater = FindCheatByName("Ignore Water (Turn lara to a scuba diver on foot)")) {
                    bool active = *underwater->active;
                    if (ImGui::Checkbox(underwater->name.c_str(), &active)) {
                        if (active && underwater->enableFunction) underwater->enableFunction();
                        else if (!active && underwater->disableFunction) underwater->disableFunction();
                        *underwater->active = active;
                    }
                }

                // Climb
                if (auto* climb = FindCheatByName("Climb Anywhere")) {
                    bool active = *climb->active;
                    if (ImGui::Checkbox(climb->name.c_str(), &active)) {
                        if (active && climb->enableFunction) climb->enableFunction();
                        else if (!active && climb->disableFunction) climb->disableFunction();
                        *climb->active = active;
                    }
                }

                // Slope Walking
                if (auto* slope = FindCheatByName("Walkable slopes (Note: Works better with noclip)")) {
                    bool active = *slope->active;
                    if (ImGui::Checkbox(slope->name.c_str(), &active)) {
                        if (active && slope->enableFunction) slope->enableFunction();
                        else if (!active && slope->disableFunction) slope->disableFunction();
                        *slope->active = active;
                    }
                }

                // Slow Fall
                if (auto* slowFall = FindCheatByName("Slow Fall From Edges")) {
                    bool active = *slowFall->active;
                    if (ImGui::Checkbox(slowFall->name.c_str(), &active)) {
                        if (active && slowFall->enableFunction) slowFall->enableFunction();
                        else if (!active && slowFall->disableFunction) slowFall->disableFunction();
                        *slowFall->active = active;
                    }
                }
                // no y pose AAAAAAAA fall (based)
                if (auto* air = FindCheatByName("Unlimited Air Time")) {
                    bool active = *air->active;
                    if (ImGui::Checkbox(air->name.c_str(), &active)) {
                        if (active && air->enableFunction) air->enableFunction();
                        else if (!active && air->disableFunction) air->disableFunction();
                        *air->active = air;
                    }
                }
                    // Inifnite oxygen
                    if (auto* ox = FindCheatByName("Unlimited Air")) {
                        bool active = *ox->active;
                        if (ImGui::Checkbox(ox->name.c_str(), &active)) {
                            if (active && ox->enableFunction) ox->enableFunction();
                            else if (!active && ox->disableFunction) ox->disableFunction();
                            *ox->active = ox;
                        }
                    }
                // Softlock fix
                if (auto* animSoft = FindCheatByName("No Animation Softlock (i.e. Swan Dive death)")) {
                    bool active = *animSoft->active;
                    if (ImGui::Checkbox(animSoft->name.c_str(), &active)) {
                        if (active && animSoft->enableFunction) animSoft->enableFunction();
                        else if (!active && animSoft->disableFunction) animSoft->disableFunction();
                        *animSoft->active = active;
                    }
                }
                // No stumbles
                if (auto* animStumble = FindCheatByName("No Animation Stumbles")) {
                    bool active = *animStumble->active;
                    if (ImGui::Checkbox(animStumble->name.c_str(), &active)) {
                        if (active && animStumble->enableFunction) animStumble->enableFunction();
                        else if (!active && animStumble->disableFunction) animStumble->disableFunction();
                        *animStumble->active = active;
                    }
                }
            }

            // Collision Modifiers Section
            if (ImGui::CollapsingHeader("Collision Modifications", ImGuiTreeNodeFlags_DefaultOpen)) {
                // No Static Collision
                if (auto* noStatic = FindCheatByName("No Static Object Collision")) {
                    bool active = *noStatic->active;
                    if (ImGui::Checkbox(noStatic->name.c_str(), &active)) {
                        if (active && noStatic->enableFunction) noStatic->enableFunction();
                        else if (!active && noStatic->disableFunction) noStatic->disableFunction();
                        *noStatic->active = active;
                    }
                }

                // No Baddie Collision
                if (auto* baddie = FindCheatByName("No Baddie Collision")) {
                    bool active = *baddie->active;
                    if (ImGui::Checkbox(baddie->name.c_str(), &active)) {
                        if (active && baddie->enableFunction) baddie->enableFunction();
                        else if (!active && baddie->disableFunction) baddie->disableFunction();
                        *baddie->active = active;
                    }
                }

                // No Creature Collision
                if (auto* creature = FindCheatByName("No Creature Collision")) {
                    bool active = *creature->active;
                    if (ImGui::Checkbox(creature->name.c_str(), &active)) {
                        if (active && creature->enableFunction) creature->enableFunction();
                        else if (!active && creature->disableFunction) creature->disableFunction();
                        *creature->active = active;
                    }
                }

                // No Door Collision
                if (auto* door = FindCheatByName("No Door Collision (Note: You need to reload the level before using)")) {
                    bool active = *door->active;
                    if (ImGui::Checkbox(door->name.c_str(), &active)) {
                        if (active && door->enableFunction) door->enableFunction();
                        else if (!active && door->disableFunction) door->disableFunction();
                        *door->active = active;
                    }
                }
            }

            // Special Actions Section
            if (ImGui::CollapsingHeader("Special Actions")) {
                // FlipMap Toggle
                if (auto* flipmap = FindCheatByName("Toggle FlipMap")) {
                    if (ImGui::Button(flipmap->name.c_str())) {
                        if (flipmap->enableFunction) flipmap->enableFunction();
                    }
                }
                // Give keys
                if (auto* puzzle = FindCheatByName("Give All Keys/Puzzle Items")) {
                    if (ImGui::Button(puzzle->name.c_str())) {
                        if (puzzle->enableFunction) puzzle->enableFunction();
                    }
                }
                // Give wpns
                if (auto* weapon = FindCheatByName("Give All Weapons")) {
                    if (ImGui::Button(weapon->name.c_str())) {
                        if (weapon->enableFunction) weapon->enableFunction();
                    }
                }
                // Give amo (sex)
                if (auto* ammo = FindCheatByName("Give Ammo, Meds & Flares")) {
                    if (ImGui::Button(ammo->name.c_str())) {
                        if (ammo->enableFunction) ammo->enableFunction();
                    }
                }
                // Finish Level
                if (auto* finish = FindCheatByName("Finish Level")) {
                    if (ImGui::Button(finish->name.c_str())) {
                        if (finish->enableFunction) finish->enableFunction();
                    }
                }
                // Alternate Noclip
                if (auto* altNoclip = FindCheatByName("Alternate Noclip (Includes walking in air)")) {
                    bool active = *altNoclip->active;
                    if (ImGui::Checkbox(altNoclip->name.c_str(), &active)) {
                        if (active && altNoclip->enableFunction) altNoclip->enableFunction();
                        else if (!active && altNoclip->disableFunction) altNoclip->disableFunction();
                        *altNoclip->active = active;
                    }
                }
            }

            // Health & Resources Section
            if (ImGui::CollapsingHeader("Health & Resources")) {
                // Unlimited Health
                if (auto* health = FindCheatByName("Unlimited Health")) {
                    bool active = *health->active;
                    if (ImGui::Checkbox(health->name.c_str(), &active)) {
                        if (active && health->enableFunction) health->enableFunction();
                        else if (!active && health->disableFunction) health->disableFunction();
                        *health->active = active;
                    }
                }

                // Unlimited Flares
                if (auto* flares = FindCheatByName("Unlimited Flare Life")) {
                    bool active = *flares->active;
                    if (ImGui::Checkbox(flares->name.c_str(), &active)) {
                        if (active && flares->enableFunction) flares->enableFunction();
                        else if (!active && flares->disableFunction) flares->disableFunction();
                        *flares->active = active;
                    }
                }
            }
        }
        // Draw an input field for integers
        static int myValue = 0; // This stores the number the user inputs
        ImGui::InputInt("Enter a number", &myValue);
        ImGui::Text("Value box for later noob");
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
