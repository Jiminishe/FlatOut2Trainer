#include "trainer_common.h"
#include <vector>

namespace Memory {

template<typename T>
bool SafeRead(uintptr_t address, T& outValue) {
    if (!address) return false;
    __try {
        outValue = *reinterpret_cast<T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Overload for reading arrays/buffers
bool SafeRead(uintptr_t address, void* buffer, size_t size) {
    if (!address || !buffer || size == 0) return false;
    __try {
        memcpy(buffer, reinterpret_cast<void*>(address), size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template<typename T>
bool SafeWrite(uintptr_t address, const T& value) {
    if (!address) return false;
    DWORD oldProtect;
    if (!VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    __try {
        *reinterpret_cast<T*>(address) = value;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), oldProtect, &oldProtect);
        return false;
    }
    VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), oldProtect, &oldProtect);
    return true;
}

// Explicit instantiations for common types
template bool SafeRead<uint8_t>(uintptr_t, uint8_t&);
template bool SafeRead<uint16_t>(uintptr_t, uint16_t&);
template bool SafeRead<uint32_t>(uintptr_t, uint32_t&);
template bool SafeRead<uint64_t>(uintptr_t, uint64_t&);
template bool SafeRead<int8_t>(uintptr_t, int8_t&);
template bool SafeRead<int16_t>(uintptr_t, int16_t&);
template bool SafeRead<int32_t>(uintptr_t, int32_t&);
template bool SafeRead<int64_t>(uintptr_t, int64_t&);
template bool SafeRead<float>(uintptr_t, float&);
template bool SafeRead<double>(uintptr_t, double&);
template bool SafeRead<void*>(uintptr_t, void*&);

template bool SafeWrite<uint8_t>(uintptr_t, const uint8_t&);
template bool SafeWrite<uint16_t>(uintptr_t, const uint16_t&);
template bool SafeWrite<uint32_t>(uintptr_t, const uint32_t&);
template bool SafeWrite<uint64_t>(uintptr_t, const uint64_t&);
template bool SafeWrite<int8_t>(uintptr_t, const int8_t&);
template bool SafeWrite<int16_t>(uintptr_t, const int16_t&);
template bool SafeWrite<int32_t>(uintptr_t, const int32_t&);
template bool SafeWrite<int64_t>(uintptr_t, const int64_t&);
template bool SafeWrite<float>(uintptr_t, const float&);
template bool SafeWrite<double>(uintptr_t, const double&);
template bool SafeWrite<void*>(uintptr_t, void* const&);

} // namespace Memory