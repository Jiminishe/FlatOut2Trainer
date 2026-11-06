#include "trainer_common.h"
#include <vector>
#include <algorithm>

namespace Hook {

// Trampoline implementation
Trampoline::Trampoline(uintptr_t target, void* detour, size_t minLen) 
    : m_target(target), m_detour(detour) {
    
    if (minLen < 5) minLen = 5;
    
    // Calculate required hook size (must be at least 5 for JMP, and cover whole instructions)
    m_hookSize = minLen;
    
    // Read original bytes
    m_originalBytes.resize(m_hookSize);
    Memory::SafeRead(m_target, m_originalBytes.data(), m_hookSize);
    
    // Allocate trampoline memory (original bytes + JMP back)
    SIZE_T trampSize = m_hookSize + 5 + 16;  // extra space for alignment
    void* trampMem = VirtualAlloc(nullptr, trampSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    m_trampoline = reinterpret_cast<uintptr_t>(trampMem);
    
    // Copy original bytes to trampoline
    memcpy(trampMem, m_originalBytes.data(), m_hookSize);
    
    // Add JMP back to original code after hook
    uintptr_t jmpBackAddr = m_target + m_hookSize;
    uint8_t* jmpBack = reinterpret_cast<uint8_t*>(m_trampoline + m_hookSize);
    jmpBack[0] = 0xE9;  // JMP rel32
    *reinterpret_cast<int32_t*>(jmpBack + 1) = static_cast<int32_t>(jmpBackAddr - (m_trampoline + m_hookSize + 5));
}

Trampoline::~Trampoline() {
    Disable();
    if (m_trampoline) {
        VirtualFree(reinterpret_cast<void*>(m_trampoline), 0, MEM_RELEASE);
    }
}

bool Trampoline::Enable() {
    if (m_enabled) return true;
    
    DWORD oldProtect;
    if (!VirtualProtect(reinterpret_cast<void*>(m_target), m_hookSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        LOG_ERROR("Failed to change protection at 0x%p", reinterpret_cast<void*>(m_target));
        return false;
    }
    
    // Write JMP to detour
    uint8_t* targetPtr = reinterpret_cast<uint8_t*>(m_target);
    targetPtr[0] = 0xE9;  // JMP rel32
    int32_t displacement = static_cast<int32_t>(reinterpret_cast<uintptr_t>(m_detour) - (m_target + 5));
    *reinterpret_cast<int32_t*>(targetPtr + 1) = displacement;
    
    // NOP remaining bytes
    for (size_t i = 5; i < m_hookSize; ++i) {
        targetPtr[i] = 0x90;
    }
    
    VirtualProtect(reinterpret_cast<void*>(m_target), m_hookSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(m_target), m_hookSize);
    
    m_enabled = true;
    LOG_INFO("Hook enabled at 0x%p -> 0x%p", reinterpret_cast<void*>(m_target), m_detour);
    return true;
}

bool Trampoline::Disable() {
    if (!m_enabled) return true;
    
    DWORD oldProtect;
    if (!VirtualProtect(reinterpret_cast<void*>(m_target), m_hookSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    
    // Restore original bytes
    memcpy(reinterpret_cast<void*>(m_target), m_originalBytes.data(), m_originalBytes.size());
    
    VirtualProtect(reinterpret_cast<void*>(m_target), m_hookSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(m_target), m_hookSize);
    
    m_enabled = false;
    LOG_INFO("Hook disabled at 0x%p", reinterpret_cast<void*>(m_target));
    return true;
}

// VTableHook implementation
VTableHook::VTableHook(void** vtable, size_t index, void* detour) 
    : m_vtable(vtable), m_index(index), m_detour(detour) {
    m_vtableAddress = reinterpret_cast<uintptr_t>(vtable);
    m_original = vtable[index];
}

VTableHook::~VTableHook() {
    Disable();
}

bool VTableHook::Enable() {
    if (m_enabled) return true;
    
    DWORD oldProtect;
    if (!VirtualProtect(&m_vtable[m_index], sizeof(void*), PAGE_READWRITE, &oldProtect)) {
        return false;
    }
    
    m_vtable[m_index] = m_detour;
    
    VirtualProtect(&m_vtable[m_index], sizeof(void*), oldProtect, &oldProtect);
    m_enabled = true;
    LOG_INFO("VTable hook enabled: vtable=0x%p index=%zu -> 0x%p", m_vtable, m_index, m_detour);
    return true;
}

bool VTableHook::Disable() {
    if (!m_enabled) return true;
    
    DWORD oldProtect;
    if (!VirtualProtect(&m_vtable[m_index], sizeof(void*), PAGE_READWRITE, &oldProtect)) {
        return false;
    }
    
    m_vtable[m_index] = m_original;
    
    VirtualProtect(&m_vtable[m_index], sizeof(void*), oldProtect, &oldProtect);
    m_enabled = false;
    LOG_INFO("VTable hook disabled: vtable=0x%p index=%zu", m_vtable, m_index);
    return true;
}

// Manager implementation
Manager& Manager::Instance() {
    static Manager instance;
    return instance;
}

void Manager::Register(std::unique_ptr<Base> hook) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hooks.push_back(std::move(hook));
}

void Manager::EnableAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& hook : m_hooks) {
        hook->Enable();
    }
}

void Manager::DisableAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& hook : m_hooks) {
        hook->Disable();
    }
}

void Manager::RemoveAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& hook : m_hooks) {
        hook->Disable();
    }
    m_hooks.clear();
}

} // namespace Hook