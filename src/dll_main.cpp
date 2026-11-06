// FlatOut 2 Trainer - DLL Entry Point
// Injected into FlatOut2.exe process

#include "trainer_common.h"
#include "menu.h"
#include "game_structs.h"
#include "cheats.h"
#include <d3d9.h>
#include <detours.h>  // We'll implement our own minimal detour if needed

// Global state
static bool g_dllInitialized = false;
static bool g_shuttingDown = false;
static HANDLE g_mainThread = nullptr;
static DWORD g_mainThreadId = 0;
static IDirect3DDevice9* g_d3dDevice = nullptr;
static HWND g_gameWindow = nullptr;

// Original DirectX functions
using Present_t = HRESULT(WINAPI*)(IDirect3DDevice9*, CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*);
using Reset_t = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

Present_t g_originalPresent = nullptr;
Reset_t g_originalReset = nullptr;

// Hooked Present
HRESULT WINAPI HookedPresent(IDirect3DDevice9* device, CONST RECT* srcRect, 
                             CONST RECT* destRect, HWND destWindowOverride, 
                             CONST RGNDATA* dirtyRegion) {
    
    if (!g_dllInitialized && device) {
        g_d3dDevice = device;
        g_gameWindow = destWindowOverride ? destWindowOverride : Game::g_hWnd;
        
        // Initialize menu system
        Menu::Initialize(device, g_gameWindow);
        
        // Initialize cheats
        Game::Initialize();
        Cheat::Manager::Instance().LoadConfig();
        
        // Register default hotkeys
        Hotkey::Manager::Instance().RegisterToggle(VK_F1, CheatState::g_hotkeys.godMode, "God Mode");
        Hotkey::Manager::Instance().RegisterToggle(VK_F2, CheatState::g_hotkeys.infiniteNitro, "Infinite Nitro");
        Hotkey::Manager::Instance().RegisterToggle(VK_F3, CheatState::g_hotkeys.oneHitKill, "One Hit Kill");
        Hotkey::Manager::Instance().RegisterToggle(VK_F4, CheatState::g_hotkeys.maxMoney, "Max Money");
        Hotkey::Manager::Instance().RegisterToggle(VK_F5, CheatState::g_hotkeys.alwaysFirst, "Always 1st");
        Hotkey::Manager::Instance().RegisterToggle(VK_F6, CheatState::g_hotkeys.freezeAI, "Freeze AI");
        Hotkey::Manager::Instance().RegisterToggle(VK_F7, CheatState::g_hotkeys.speedHack, "Speed Hack");
        Hotkey::Manager::Instance().RegisterToggle(VK_F8, CheatState::g_hotkeys.unlockAll, "Unlock All");
        Hotkey::Manager::Instance().RegisterToggle(VK_INSERT, g_dllInitialized, "Toggle Menu");
        
        // Load hotkeys from config
        auto& config = Config::Manager::Instance();
        auto loadHotkey = [&](const char* name, int defVk) {
            int vk = config.GetHotkey(name);
            if (vk == 0) vk = defVk;
            return vk;
        };
        
        // Note: Hotkeys are registered above with defaults, config values would override
        
        g_dllInitialized = true;
        LOG_INFO("Trainer fully initialized");
    }
    
    if (g_dllInitialized && !g_shuttingDown) {
        // New frame
        Menu::NewFrame();
        
        // Update game state
        Game::Update();
        
        // Update cheats
        Cheat::Manager::Instance().OnFrame();
        
        // Update hotkeys
        Hotkey::Manager::Instance().Update();
        
        // Draw menu
        if (Menu::IsMenuOpen()) {
            Menu::DrawMainMenu();
        }
        
        // Render ImGui
        Menu::Render();
    }
    
    return g_originalPresent(device, srcRect, destRect, destWindowOverride, dirtyRegion);
}

// Hooked Reset
HRESULT WINAPI HookedReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) {
    if (g_dllInitialized) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }
    
    HRESULT hr = g_originalReset(device, params);
    
    if (g_dllInitialized) {
        ImGui_ImplDX9_CreateDeviceObjects();
    }
    
    return hr;
}

// Hook DirectX
bool HookDirectX() {
    // Get device from game - try multiple methods
    
    // Method 1: Find via pattern scan for IDirect3DDevice9 creation
    // Method 2: Hook Direct3DCreate9
    // Method 3: Use known vtable offset
    
    // For now, we'll hook the vtable directly
    // Find the device pointer by scanning
    Pattern::Result result = Pattern::ScanModule(nullptr, "A1 ? ? ? ? 8B 10 8B 42 70");
    if (result) {
        uintptr_t devicePtr = 0;
        if (Memory::SafeRead(result.address + 1, devicePtr)) {
            g_d3dDevice = *reinterpret_cast<IDirect3DDevice9**>(devicePtr);
            LOG_INFO("Found D3D device at 0x%p", g_d3dDevice);
        }
    }
    
    if (!g_d3dDevice) {
        LOG_ERROR("Failed to find D3D device");
        return false;
    }
    
    // Hook vtable
    void** vtable = *reinterpret_cast<void***>(g_d3dDevice);
    g_originalPresent = reinterpret_cast<Present_t>(vtable[17]);  // Present is at index 17
    g_originalReset = reinterpret_cast<Reset_t>(vtable[16]);      // Reset is at index 16
    
    DWORD oldProtect;
    VirtualProtect(&vtable[17], sizeof(void*), PAGE_READWRITE, &oldProtect);
    vtable[17] = &HookedPresent;
    VirtualProtect(&vtable[17], sizeof(void*), oldProtect, &oldProtect);
    
    VirtualProtect(&vtable[16], sizeof(void*), PAGE_READWRITE, &oldProtect);
    vtable[16] = &HookedReset;
    VirtualProtect(&vtable[16], sizeof(void*), oldProtect, &oldProtect);
    
    LOG_INFO("DirectX hooked: Present=0x%p, Reset=0x%p", g_originalPresent, g_originalReset);
    return true;
}

void UnhookDirectX() {
    if (!g_d3dDevice) return;
    
    void** vtable = *reinterpret_cast<void***>(g_d3dDevice);
    if (g_originalPresent) vtable[17] = g_originalPresent;
    if (g_originalReset) vtable[16] = g_originalReset;
    
    g_originalPresent = nullptr;
    g_originalReset = nullptr;
    LOG_INFO("DirectX unhooked");
}

// Cleanup all cheats and restore values
void Cleanup() {
    g_shuttingDown = true;
    LOG_INFO("Starting cleanup...");
    
    // Disable all cheats (restores original values)
    Cheat::Manager::Instance().DisableAll();
    
    // Save config
    Cheat::Manager::Instance().SaveConfig();
    
    // Unhook DirectX
    UnhookDirectX();
    
    // Shutdown menu
    Menu::Shutdown();
    
    // Remove all hooks
    Hook::Manager::Instance().RemoveAll();
    
    g_dllInitialized = false;
    LOG_INFO("Cleanup complete");
}

// Main thread function
DWORD WINAPI MainThread(LPVOID lpParam) {
    // Wait for game to initialize
    Sleep(1000);
    
    // Hook DirectX
    int retries = 0;
    while (!HookDirectX() && retries < 30) {
        Sleep(100);
        retries++;
    }
    
    if (!g_dllInitialized) {
        LOG_ERROR("Failed to initialize trainer after %d retries", retries);
        return 1;
    }
    
    // Keep thread alive
    while (!g_shuttingDown) {
        Sleep(100);
        
        // Check if game window still exists
        if (g_gameWindow && !IsWindow(g_gameWindow)) {
            LOG_INFO("Game window closed, shutting down");
            Cleanup();
            break;
        }
    }
    
    return 0;
}

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);
            
            // Alloc console for debugging
            AllocConsole();
            FILE* fp;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
            SetConsoleTitleA(TRAINER_NAME " Console");
            
            LOG_INFO("=== %s v%s Injected ===", TRAINER_NAME, TRAINER_VERSION_STRING);
            LOG_INFO("Module base: 0x%p", hModule);
            
            // Start main thread
            g_mainThread = CreateThread(nullptr, 0, MainThread, nullptr, 0, &g_mainThreadId);
            if (!g_mainThread) {
                LOG_ERROR("Failed to create main thread");
            }
            break;
        }
        
        case DLL_PROCESS_DETACH: {
            if (lpReserved == nullptr) {  // Not process termination
                Cleanup();
            }
            if (g_mainThread) {
                WaitForSingleObject(g_mainThread, 2000);
                CloseHandle(g_mainThread);
            }
            FreeConsole();
            break;
        }
    }
    return TRUE;
}

// Export for injector verification
extern "C" __declspec(dllexport) const char* GetTrainerName() {
    return TRAINER_NAME;
}

extern "C" __declspec(dllexport) const char* GetTrainerVersion() {
    return TRAINER_VERSION_STRING;
}

extern "C" __declspec(dllexport) bool IsTrainerLoaded() {
    return g_dllInitialized;
}