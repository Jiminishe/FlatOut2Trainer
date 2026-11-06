#include "trainer_common.h"
#include <Psapi.h>

namespace Pattern {

Result Scan(const char* pattern, const char* mask, uintptr_t start, size_t length) {
    Result result;
    if (!start) {
        MODULEINFO mi = {};
        GetModuleInformation(GetCurrentProcess(), GetModuleHandle(nullptr), &mi, sizeof(mi));
        start = reinterpret_cast<uintptr_t>(mi.lpBaseOfDll);
        length = mi.SizeOfImage;
    }
    if (!length) return result;
    
    // Parse pattern
    std::vector<uint8_t> patternBytes;
    std::vector<bool> patternMask;
    
    const char* p = pattern;
    while (*p) {
        if (*p == ' ') { p++; continue; }
        if (*p == '?') {
            patternBytes.push_back(0);
            patternMask.push_back(false);
            p++;
            if (*p == '?') p++;  // Skip second ?
        } else {
            char byteStr[3] = { p[0], p[1], 0 };
            patternBytes.push_back(static_cast<uint8_t>(strtoul(byteStr, nullptr, 16)));
            patternMask.push_back(true);
            p += 2;
        }
    }
    
    if (patternBytes.empty()) return result;
    
    const uint8_t* scanStart = reinterpret_cast<const uint8_t*>(start);
    const uint8_t* scanEnd = scanStart + length - patternBytes.size();
    
    for (const uint8_t* cur = scanStart; cur <= scanEnd; ++cur) {
        bool match = true;
        for (size_t i = 0; i < patternBytes.size(); ++i) {
            if (patternMask[i] && cur[i] != patternBytes[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            result.address = reinterpret_cast<uintptr_t>(cur);
            result.found = true;
            return result;
        }
    }
    
    return result;
}

Result ScanModule(const char* moduleName, const char* pattern, const char* mask) {
    HMODULE hMod = GetModuleHandleA(moduleName);
    if (!hMod) return Result{};
    
    MODULEINFO mi = {};
    GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi));
    return Scan(pattern, mask, reinterpret_cast<uintptr_t>(mi.lpBaseOfDll), mi.SizeOfImage);
}

std::vector<uintptr_t> ScanAll(const char* pattern, const char* mask, uintptr_t start, size_t length) {
    std::vector<uintptr_t> results;
    if (!start) {
        MODULEINFO mi = {};
        GetModuleInformation(GetCurrentProcess(), GetModuleHandle(nullptr), &mi, sizeof(mi));
        start = reinterpret_cast<uintptr_t>(mi.lpBaseOfDll);
        length = mi.SizeOfImage;
    }
    if (!length) return results;
    
    std::vector<uint8_t> patternBytes;
    std::vector<bool> patternMask;
    
    const char* p = pattern;
    while (*p) {
        if (*p == ' ') { p++; continue; }
        if (*p == '?') {
            patternBytes.push_back(0);
            patternMask.push_back(false);
            p++;
            if (*p == '?') p++;
        } else {
            char byteStr[3] = { p[0], p[1], 0 };
            patternBytes.push_back(static_cast<uint8_t>(strtoul(byteStr, nullptr, 16)));
            patternMask.push_back(true);
            p += 2;
        }
    }
    
    if (patternBytes.empty()) return results;
    
    const uint8_t* scanStart = reinterpret_cast<const uint8_t*>(start);
    const uint8_t* scanEnd = scanStart + length - patternBytes.size();
    
    for (const uint8_t* cur = scanStart; cur <= scanEnd; ++cur) {
        bool match = true;
        for (size_t i = 0; i < patternBytes.size(); ++i) {
            if (patternMask[i] && cur[i] != patternBytes[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            results.push_back(reinterpret_cast<uintptr_t>(cur));
        }
    }
    
    return results;
}

uintptr_t ResolveRelativeCall(uintptr_t instructionAddress) {
    // E8 rel32
    int32_t displacement = 0;
    if (!Memory::SafeRead(instructionAddress + 1, displacement)) return 0;
    return instructionAddress + 5 + displacement;
}

uintptr_t ResolveRelativeJump(uintptr_t instructionAddress) {
    // E9 rel32
    int32_t displacement = 0;
    if (!Memory::SafeRead(instructionAddress + 1, displacement)) return 0;
    return instructionAddress + 5 + displacement;
}

uintptr_t ResolveRIPRelative(uintptr_t instructionAddress, int offsetOfDisplacement) {
    // For x86: disp32 at offset, target = instructionAddress + instructionLength + disp32
    // instructionLength varies, assume 5-7 bytes typical
    int32_t displacement = 0;
    if (!Memory::SafeRead(instructionAddress + offsetOfDisplacement, displacement)) return 0;
    // Need to know instruction length - heuristic: assume next instruction starts at address + 6
    return instructionAddress + 6 + displacement;
}

} // namespace Pattern