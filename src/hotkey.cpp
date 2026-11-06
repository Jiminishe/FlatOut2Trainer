#include "trainer_common.h"

namespace Hotkey {

Manager& Manager::Instance() {
    static Manager instance;
    return instance;
}

void Manager::Register(int vk, std::function<void()> callback, bool toggle, bool* statePtr, const char* desc) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hotkeys[vk] = Entry{vk, std::move(callback), toggle, statePtr, desc ? desc : ""};
    m_keyStates[vk] = false;
    LOG_DEBUG("Registered hotkey: VK=0x%X (%s) toggle=%d", vk, m_hotkeys[vk].description.c_str(), toggle);
}

void Manager::RegisterToggle(int vk, bool& state, const char* desc) {
    Register(vk, [&state]() { state = !state; }, true, &state, desc);
}

void Manager::Unregister(int vk) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hotkeys.erase(vk);
    m_keyStates.erase(vk);
}

void Manager::ProcessMessage(UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        int vk = static_cast<int>(wp);
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_hotkeys.find(vk);
        if (it != m_hotkeys.end() && !m_keyStates[vk]) {
            m_keyStates[vk] = true;
            if (it->second.toggle) {
                it->second.callback();
                if (it->second.statePtr) {
                    *it->second.statePtr = !*it->second.statePtr;
                }
            }
        }
    } else if (msg == WM_KEYUP || msg == WM_SYSKEYUP) {
        int vk = static_cast<int>(wp);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_keyStates[vk] = false;
    }
}

void Manager::Update() {
    // Poll for keys held down (for non-toggle actions)
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [vk, entry] : m_hotkeys) {
        if (!entry.toggle && (GetAsyncKeyState(vk) & 0x8000)) {
            entry.callback();
        }
    }
}

} // namespace Hotkey