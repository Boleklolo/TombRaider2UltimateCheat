#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <cstdint>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <conio.h>
#include <functional>
#include <memory>

// Configuration :)
constexpr const wchar_t* processName = L"Tomb2Cheat.exe"; // Name of the process

// Cheat configuration below

// Animation Softlock Fix
constexpr short replacementAnimValue = 11; // Animation that resets laura (Stand still and sybau Lara)
const std::vector<short> brokenAnimIDs = { 155, 139, 24, 25 }; // Animations that should trigger animation change
// In order; Swan dive death | Rolling ball crush | Hard and slow stumble cause why not | Freefall death
//

constexpr uintptr_t addressAnimationCheat = 0x001207BC; // Address of the current animation (technically a pointer)
constexpr uintptr_t pointerOffsetAnimationCheat = 0x14; // Offset to the pointer




// Health Cheat (Because Im too lazy to turn off damage for every fucking kind of ammunition brand they shootin)
constexpr uint16_t desiredHealthValue = 1000; // The value that sets health of laura craft (I am steve)
constexpr uintptr_t baseAddressHealth = 0x001207BC; // Base address of the health pointer
constexpr uintptr_t pointerOffsetHealth = 0x22; // Offset to the health pointer


// Noclip cheat doomguy style (Walk into a wall and end up on top of it rofl)
constexpr uintptr_t noclipAddress = 0x00028ED9; // Address of the noclip
uint8_t opcodeNoclipOriginal[5] = { 0xE8, 0x82, 0x13, 0x00, 0x00 }; // Original bytes for the noclip
//Tomb2Cheat.exe+28ED9 - E8 82130000           - call Tomb2Cheat.exe+2A260
uint8_t opcodeNoclipPatched[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 }; // Patched bytes for the noclip (om om om nop nop nop silly shit)
static bool noclipCheatPatched = false;

// Walk underwater (Holy shit aquaman)
constexpr uintptr_t underwaterPatchAddress1 = 0x00030466; // Fixed address for the water entrance by kaboom
//Tomb2Cheat.exe+30466 - 0F84 29040000         - je Tomb2Cheat.exe+30895
//Tomb2Cheat.exe+30466 - 0F85 29040000         - jne Tomb2Cheat.exe+30895
constexpr uintptr_t underwaterPatchAddress2 = 0x00030568; // Fixed address for the water entrance by wade
//Tomb2Cheat.exe+30568 - 0F8E 27030000         - jng Tomb2Cheat.exe+30895
//Tomb2Cheat.exe+30568 - 0F8D 27030000         - jnl Tomb2Cheat.exe+30895

// Holy fuck it took me 3 hours to realize that I had 8F and 85 instead of 8E and 8D values kill me
uint8_t opcodeUnderwaterOriginal1[6] = { 0x0F, 0x84, 0x29, 0x04, 0x00, 0x00 }; // original bytes for kaboom entrance
uint8_t opcodeUnderwaterPatched1[6] = { 0x0F, 0x85, 0x29, 0x04, 0x00, 0x00 }; // patched bytes for kaboom entrance (je to jne)
uint8_t opcodeUnderwaterOriginal2[6] = { 0x0F, 0x8E, 0x27, 0x03, 0x00, 0x00 }; // original bytes for wade entrance
uint8_t opcodeUnderwaterPatched2[6] = { 0x0F, 0x8D, 0x27, 0x03, 0x00, 0x00 }; // patched bytes for wade entrance (jng to jnl)    
static bool underwaterCheat1Patched = false; // Conditional to check if its on brav
static bool underwaterCheat2Patched = false; // Conditional to check if its on brav







enum ConsoleColor { WHITE = 7, GREEN = 10 }; // Silly

struct Command {
    std::wstring name;
    uintptr_t baseAddress;
    std::vector<uintptr_t> offsets;
    std::function<void(HANDLE, uintptr_t, std::shared_ptr<std::atomic<bool>>&)> loopFunction;
    std::shared_ptr<std::atomic<bool>> active;
    std::thread thread;
    uintptr_t resolvedAddress{ 0 };
    bool addressValid{ false };

    Command(const std::wstring& name, uintptr_t baseAddr, const std::vector<uintptr_t>& offsets,
        const std::function<void(HANDLE, uintptr_t, std::shared_ptr<std::atomic<bool>>&)>& loopFunc)
        : name(name), baseAddress(baseAddr), offsets(offsets), loopFunction(loopFunc), active(std::make_shared<std::atomic<bool>>(false)) {
    }
};

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

bool ReadPointerChain(HANDLE hProc, uintptr_t base, const std::vector<uintptr_t>& offsets, uintptr_t& outAddress) {
    uintptr_t current = base;
    for (auto offset : offsets) {
        uintptr_t next = 0;
        if (!ReadProcessMemory(hProc, (LPCVOID)current, &next, sizeof(next), nullptr)) return false;
        current = next + offset;
    }
    outAddress = current;
    return true;
}

void SetConsoleTextColor(ConsoleColor color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

int wmain() {
    std::wcout << L"=== Tomb Raider II Cheat Console ===\n";

    DWORD pid = GetProcessIdByName(processName);
    if (!pid) {
        std::wcerr << L"[Error] Process not found.\n";
        return 1;
    }

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        std::wcerr << L"[Error] Cannot open process.\n";
        return 1;
    }

    uintptr_t modBase = GetModuleBaseAddress(pid, processName);
    if (!modBase) {
        std::wcerr << L"[Error] Module base not found.\n";
        CloseHandle(hProc);
        return 1;
    }

    std::vector<Command> commands;

    commands.emplace_back(
        L"Fix Animation",
        addressAnimationCheat,
        std::vector<uintptr_t>{pointerOffsetAnimationCheat},
        [](HANDLE hProc, uintptr_t address, std::shared_ptr<std::atomic<bool>>& active) {
            while (*active) {
                short currentAnim = 0;
                if (ReadProcessMemory(hProc, (LPCVOID)address, &currentAnim, sizeof(currentAnim), nullptr)) {
                    if (std::find(brokenAnimIDs.begin(), brokenAnimIDs.end(), currentAnim) != brokenAnimIDs.end()) {
                        WriteProcessMemory(hProc, (LPVOID)address, &replacementAnimValue, sizeof(replacementAnimValue), nullptr);
                    }
                }
                Sleep(50);
            }
        }
    );
    commands.emplace_back(
        L"Noclip (Doom style) !! ADD OTHER CALLS RELATED FOR JUMPING OR OVERRIDE THE WHOLE FUNCTION",
        noclipAddress,
        std::vector<uintptr_t>{},
        [](HANDLE hProc, uintptr_t noclipAddress, std::shared_ptr<std::atomic<bool>>& active) {
            while (*active) {
                // Keep the patch applied while active
                if (!noclipCheatPatched) {
                    if (WriteProcessMemory(hProc, (LPVOID)noclipAddress, opcodeNoclipPatched, sizeof(opcodeNoclipPatched), nullptr)) {
                        noclipCheatPatched = true;
                    }
                }
                Sleep(50);
            }
            // Revert when deactivated
            if (noclipCheatPatched) {
                WriteProcessMemory(hProc, (LPVOID)noclipAddress, opcodeNoclipOriginal, sizeof(opcodeNoclipOriginal), nullptr);
                noclipCheatPatched = false;
            }
        }
    );
    commands.emplace_back(
        L"Walk Underwater (Patch 1)",
        underwaterPatchAddress1,
        std::vector<uintptr_t>{},
        [](HANDLE hProc, uintptr_t underwaterPatchAddress1, std::shared_ptr<std::atomic<bool>>& active) {
            while (*active) {
                // Keep the patch applied while active
                if (!underwaterCheat1Patched) {
                    if (WriteProcessMemory(hProc, (LPVOID)underwaterPatchAddress1, opcodeUnderwaterPatched1, sizeof(opcodeUnderwaterPatched1), nullptr)) {
                        underwaterCheat1Patched = true;
                    }
                }
                Sleep(50);
            }
            // Revert when deactivated
            if (underwaterCheat1Patched) {
                WriteProcessMemory(hProc, (LPVOID)underwaterPatchAddress1, opcodeUnderwaterOriginal1, sizeof(opcodeUnderwaterOriginal1), nullptr);
                underwaterCheat1Patched = false;
            }
        }
    );
    commands.emplace_back(
        L"Walk Underwater (Patch 2 for Wade)",
        underwaterPatchAddress2,
        std::vector<uintptr_t>{},
        [](HANDLE hProc, uintptr_t underwaterPatchAddress2, std::shared_ptr<std::atomic<bool>>& active) {
            while (*active) {
                // Keep the patch applied while active
                if (!underwaterCheat2Patched) {
                    if (WriteProcessMemory(hProc, (LPVOID)underwaterPatchAddress2, opcodeUnderwaterPatched2, sizeof(opcodeUnderwaterPatched1), nullptr)) {
                        underwaterCheat2Patched = true;
                    }
                }
                Sleep(50);
            }
            // Revert when deactivated
            if (underwaterCheat2Patched) {
                WriteProcessMemory(hProc, (LPVOID)underwaterPatchAddress2, opcodeUnderwaterOriginal2, sizeof(opcodeUnderwaterOriginal1), nullptr);
                underwaterCheat2Patched = false;
            }
        }
    );



    commands.emplace_back(
        L"Restore Health",
        baseAddressHealth,
        std::vector<uintptr_t>{pointerOffsetHealth},
        [](HANDLE hProc, uintptr_t address, std::shared_ptr<std::atomic<bool>>& active) {
            while (*active) {
                WriteProcessMemory(hProc, (LPVOID)address, &desiredHealthValue, sizeof(desiredHealthValue), nullptr);
                Sleep(50);
            }
        }
    );

    for (auto& cmd : commands) {
        if (!cmd.offsets.empty()) {
            if (!ReadPointerChain(hProc, modBase + cmd.baseAddress, cmd.offsets, cmd.resolvedAddress)) {
                std::wcerr << L"[Error] Failed resolving address for " << cmd.name << L"\n";
                cmd.addressValid = false;
                continue;
            }
        }
        else {
            cmd.resolvedAddress = modBase + cmd.baseAddress; // just use base+offset directly
        }

        cmd.addressValid = true;
    }


    while (true) {
        system("cls");
        std::wcout << L"=== Tomb Raider II Cheat Console ===\n";

        int index = 1;
        for (const auto& cmd : commands) {
            SetConsoleTextColor(*cmd.active ? GREEN : WHITE);
            std::wcout << index << L". " << cmd.name << L" [" << (*cmd.active ? L"ON" : L"OFF") << L"]\n";
            index++;
        }

        SetConsoleTextColor(WHITE);
        std::wcout << index << L". Exit\n> ";
        std::wcout.flush();

        wint_t input = _getwch();
        int option = (input >= L'1' && input <= L'0' + index) ? input - L'0' : 0;
        std::wcout << static_cast<wchar_t>(input) << L"\n";

        if (option >= 1 && option <= static_cast<int>(commands.size())) {
            Command& cmd = commands[option - 1];
            if (!cmd.addressValid) {
                std::wcout << cmd.name << L" address invalid.\n";
                Sleep(1000);
                continue;
            }

            *cmd.active = !*cmd.active;
            if (*cmd.active) {
                cmd.thread = std::thread(cmd.loopFunction, hProc, cmd.resolvedAddress, std::ref(cmd.active));
                std::wcout << cmd.name << L" ENABLED.\n";
            }
            else {
                if (cmd.thread.joinable()) cmd.thread.join();
                std::wcout << cmd.name << L" DISABLED.\n";
            }
            Sleep(1000);
        }
        else if (option == static_cast<int>(commands.size()) + 1) {
            for (auto& cmd : commands) {
                *cmd.active = false;
                if (cmd.thread.joinable()) cmd.thread.join();
            }
            CloseHandle(hProc);
            return 0;
        }
        else {
            std::wcout << L"Invalid option.\n";
            Sleep(1000);
        }
    }
}
