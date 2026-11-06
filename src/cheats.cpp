#include "trainer_common.h"
#include "hooking.h"
#include <algorithm>

// Cheat state storage
namespace CheatState {
    // Original values for restoration
    struct OriginalValues {
        float nitroCurrent = 0;
        float nitroMax = 0;
        float nitroDrainRate = 0;
        float damage = 0;
        int money = 0;
        float regenRate = 0;
        float ragdollGain = 0;
        float nitroAccel = 0;
        float turboAccel = 0;
        bool hasOriginals = false;
    } g_originals;
    
    // Current cheat states
    struct {
        bool godMode = false;
        bool infiniteNitro = false;
        bool infiniteBoost = false;
        bool oneHitKill = false;
        bool maxMoney = false;
        bool alwaysFirst = false;
        bool freezeAI = false;
        bool noDamageVisual = false;
        bool unlockAll = false;
        bool speedHack = false;
        float speedMultiplier = 1.0f;
        bool stuntScoreMultiplier = false;
        float stuntMultiplier = 10.0f;
        bool instantWin = false;
        bool noTraffic = false;
        bool infiniteJumps = false;  // For stunt mode
        bool superGrip = false;
        bool driftMode = false;
    } g_enabled;
    
    // Hotkey states (for toggle keys)
    struct {
        bool godMode = false;
        bool infiniteNitro = false;
        bool oneHitKill = false;
        bool maxMoney = false;
        bool alwaysFirst = false;
        bool freezeAI = false;
        bool speedHack = false;
        bool unlockAll = false;
    } g_hotkeys;
}

// Forward declarations
uintptr_t GetLocalPlayerCar();
uintptr_t GetLocalPlayer();
uintptr_t GetRaceHost();
uintptr_t GetPlayerHost();

// ============================================================================
// CHEAT IMPLEMENTATIONS
// ============================================================================

// ----- God Mode (Infinite Health / No Damage) -----
class GodModeCheat : public Cheat::Base {
public:
    const char* GetName() const override { return "GodMode"; }
    const char* GetDescription() const override { return "Infinite health - car never gets damaged"; }
    
    void OnEnable() override {
        CheatState::g_enabled.godMode = true;
        CheatState::g_hotkeys.godMode = true;
        
        // Save original damage value
        uintptr_t player = GetLocalPlayer();
        if (player) {
            Memory::SafeRead(player + Offsets::Player_ReadOnlyDamage, CheatState::g_originals.damage);
        }
        LOG_INFO("God Mode enabled");
    }
    
    void OnDisable() override {
        CheatState::g_enabled.godMode = false;
        CheatState::g_hotkeys.godMode = false;
        
        // Restore original damage
        uintptr_t player = GetLocalPlayer();
        if (player && CheatState::g_originals.hasOriginals) {
            Memory::SafeWrite(player + Offsets::Player_ReadOnlyDamage, CheatState::g_originals.damage);
        }
        LOG_INFO("God Mode disabled");
    }
    
    void OnFrame() override {
        if (!CheatState::g_enabled.godMode) return;
        
        uintptr_t player = GetLocalPlayer();
        if (player) {
            // Set damage to 0 every frame
            Memory::SafeWrite(player + Offsets::Player_ReadOnlyDamage, 0.0f);
            
            // Also set car damage to 0
            uintptr_t carPtr = 0;
            if (Memory::SafeRead(player + Offsets::Player_CarPtr, carPtr) && carPtr) {
                // Car damage might be at different offset, try common ones
                Memory::SafeWrite(carPtr + 0x49C, 0.0f);  // Same as player damage offset
            }
        }
    }
    
    void OnMenu() override {
        ImGui::Checkbox("God Mode (Infinite Health)", &CheatState::g_hotkeys.godMode);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Car takes no damage. Hotkey: %s", 
            GetKeyName(Config::Manager::Instance().GetHotkey("GodMode")).c_str());
        
        ImGui::SameLine();
        if (ImGui::Button("Reset Damage")) {
            uintptr_t player = GetLocalPlayer();
            if (player) Memory::SafeWrite(player + Offsets::Player_ReadOnlyDamage, 0.0f);
        }
    }
    
    bool IsEnabled() const override { return CheatState::g_enabled.godMode; }
    void SetEnabled(bool enabled) override { enabled ? OnEnable() : OnDisable(); }
    int GetHotkey() const override { return Config::Manager::Instance().GetHotkey("GodMode"); }
    void SetHotkey(int vk) override { Config::Manager::Instance().SetHotkey("GodMode", vk); }
    
private:
    std::string GetKeyName(int vk) {
        if (vk == 0) return "None";
        char name[32];
        GetKeyNameTextA(MapVirtualKeyA(vk, MAPVK_VK_TO_VSC) << 16, name, sizeof(name));
        return name;
    }
};

// ----- Infinite Nitro -----
class InfiniteNitroCheat : public Cheat::Base {
public:
    const char* GetName() const override { return "InfiniteNitro"; }
    const char* GetDescription() const override { return "Infinite nitro boost - never runs out"; }
    
    void OnEnable() override {
        CheatState::g_enabled.infiniteNitro = true;
        CheatState::g_hotkeys.infiniteNitro = true;
        
        uintptr_t car = GetLocalPlayerCar();
        if (car) {
            Memory::SafeRead(car + Offsets::Car_NitroCurrent, CheatState::g_originals.nitroCurrent);
            Memory::SafeRead(car + Offsets::Car_NitroMax, CheatState::g_originals.nitroMax);
            Memory::SafeRead(car + Offsets::Car_NitroDrainRate, CheatState::g_originals.nitroDrainRate);
        }
        LOG_INFO("Infinite Nitro enabled");
    }
    
    void OnDisable() override {
        CheatState::g_enabled.infiniteNitro = false;
        CheatState::g_hotkeys.infiniteNitro = false;
        
        uintptr_t car = GetLocalPlayerCar();
        if (car && CheatState::g_originals.hasOriginals) {
            Memory::SafeWrite(car + Offsets::Car_NitroCurrent, CheatState::g_originals.nitroCurrent);
            Memory::SafeWrite(car + Offsets::Car_NitroMax, CheatState::g_originals.nitroMax);
            Memory::SafeWrite(car + Offsets::Car_NitroDrainRate, CheatState::g_originals.nitroDrainRate);
        }
        LOG_INFO("Infinite Nitro disabled");
    }
    
    void OnFrame() override {
        if (!CheatState::g_enabled.infiniteNitro) return;
        
        uintptr_t car = GetLocalPlayerCar();
        if (car) {
            float maxNitro = 0;
            Memory::SafeRead(car + Offsets::Car_NitroMax, maxNitro);
            if (maxNitro > 0) {
                Memory::SafeWrite(car + Offsets::Car_NitroCurrent, maxNitro);
                Memory::SafeWrite(car + Offsets::Car_NitroStorageMirror, maxNitro);
                // Also set display ratio to 1.0
                Memory::SafeWrite(car + Offsets::Car_NitroDisplayRatio, 1.0f);
                Memory::SafeWrite(car + Offsets::Car_NitroDisplayRatioSmoothed, 1.0f);
            }
            
            // Set drain rate to 0
            Memory::SafeWrite(car + Offsets::Car_NitroDrainRate, 0.0f);
        }
    }
    
    void OnMenu() override {
        ImGui::Checkbox("Infinite Nitro", &CheatState::g_hotkeys.infiniteNitro);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Nitro never depletes. Hotkey: %s",
            GetKeyName(Config::Manager::Instance().GetHotkey("InfiniteNitro")).c_str());
        
        ImGui::SameLine();
        if (ImGui::Button("Fill Nitro")) {
            uintptr_t car = GetLocalPlayerCar();
            if (car) {
                float maxNitro = 0;
                Memory::SafeRead(car + Offsets::Car_NitroMax, maxNitro);
                Memory::SafeWrite(car + Offsets::Car_NitroCurrent, maxNitro);
            }
        }
    }
    
    bool IsEnabled() const override { return CheatState::g_enabled.infiniteNitro; }
    void SetEnabled(bool enabled) override { enabled ? OnEnable() : OnDisable(); }
    int GetHotkey() const override { return Config::Manager::Instance().GetHotkey("InfiniteNitro"); }
    void SetHotkey(int vk) override { Config::Manager::Instance().SetHotkey("InfiniteNitro", vk); }
    
private:
    std::string GetKeyName(int vk) {
        if (vk == 0) return "None";
        char name[32];
        GetKeyNameTextA(MapVirtualKeyA(vk, MAPVK_VK_TO_VSC) << 16, name, sizeof(name));
        return name;
    }
};

// ----- Infinite Boost (Nitro Acceleration Multiplier) -----
class InfiniteBoostCheat : public Cheat::Base {
public:
    const char* GetName() const override { return "InfiniteBoost"; }
    const char* GetDescription() const override { return "Massive nitro acceleration - insane speed boost"; }
    
    void OnEnable() override {
        CheatState::g_enabled.infiniteBoost = true;
        
        // Save originals
        Memory::SafeRead(Addresses::NitroAccelerationAddr, CheatState::g_originals.nitroAccel);
        Memory::SafeRead(Addresses::TurboAccelerationAddr, CheatState::g_originals.turboAccel);
        
        // Set to massive values
        Memory::SafeWrite(Addresses::NitroAccelerationAddr, 1000.0f);
        Memory::SafeWrite(Addresses::TurboAccelerationAddr, 1000.0f);
        LOG_INFO("Infinite Boost enabled");
    }
    
    void OnDisable() override {
        CheatState::g_enabled.infiniteBoost = false;
        
        if (CheatState::g_originals.hasOriginals) {
            Memory::SafeWrite(Addresses::NitroAccelerationAddr, CheatState::g_originals.nitroAccel);
            Memory::SafeWrite(Addresses::TurboAccelerationAddr, CheatState::g_originals.turboAccel);
        }
        LOG_INFO("Infinite Boost disabled");
    }
    
    void OnFrame() override {
        if (!CheatState::g_enabled.infiniteBoost) return;
        // Keep values high
        Memory::SafeWrite(Addresses::NitroAccelerationAddr, 1000.0f);
        Memory::SafeWrite(Addresses::TurboAccelerationAddr, 1000.0f);
    }
    
    void OnMenu() override {
        ImGui::Checkbox("Infinite Boost (Max Acceleration)", &CheatState::g_enabled.infiniteBoost);
        ImGui::SameLine();
        ImGui::SliderFloat("Multiplier", &CheatState::g_originals.nitroAccel, 1.0f, 5000.0f, "%.0f");
    }
    
    bool IsEnabled() const override { return CheatState::g_enabled.infiniteBoost; }
    void SetEnabled(bool enabled) override { enabled ? OnEnable() : OnDisable(); }
    int GetHotkey() const override { return Config::Manager::Instance().GetHotkey("InfiniteBoost"); }
    void SetHotkey(int vk) override { Config::Manager::Instance().SetHotkey("InfiniteBoost", vk); }
};

// ----- One Hit Kill -----
class OneHitKillCheat : public Cheat::Base {
public:
    const char* GetName() const override { return "OneHitKill"; }
    const char* GetDescription() const override { return "Opponents destroyed in one hit"; }
    
    void OnEnable() override {
        CheatState::g_enabled.oneHitKill = true;
        CheatState::g_hotkeys.oneHitKill = true;
        LOG_INFO("One Hit Kill enabled");
    }
    
    void OnDisable() override {
        CheatState::g_enabled.oneHitKill = false;
        CheatState::g_hotkeys.oneHitKill = false;
        LOG_INFO("One Hit Kill disabled");
    }
    
    void OnFrame() override {
        if (!CheatState::g_enabled.oneHitKill) return;
        
        uintptr_t host = GetPlayerHost();
        if (!host) return;
        
        uintptr_t* playersArray = nullptr;
        if (!Memory::SafeRead(host + Offsets::Host_PlayersArray, playersArray) || !playersArray) return;
        
        uintptr_t localPlayer = 0;
        Memory::SafeRead(host + Offsets::Host_LocalPlayerPtr, localPlayer);
        
        // Iterate through all players (max 8)
        for (int i = 0; i < 8; ++i) {
            uintptr_t player = playersArray[i];
            if (!player || player == localPlayer) continue;
            
            // Check if player is valid (has VTable)
            void* vtable = nullptr;
            if (!Memory::SafeRead(player, vtable) || !vtable) continue;
            
            // Set their damage to max (or health to 0)
            // In FlatOut, damage goes from 0 to ~1.0 where 1.0 = destroyed
            Memory::SafeWrite(player + Offsets::Player_ReadOnlyDamage, 100.0f);
            
            // Also try car damage
            uintptr_t carPtr = 0;
            if (Memory::SafeRead(player + Offsets::Player_CarPtr, carPtr) && carPtr) {
                Memory::SafeWrite(carPtr + 0x49C, 100.0f);
            }
        }
    }
    
    void OnMenu() override {
        ImGui::Checkbox("One Hit Kill (Destroy Opponents)", &CheatState::g_hotkeys.oneHitKill);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Opponents wrecked instantly on contact. Hotkey: %s",
            GetKeyName(Config::Manager::Instance().GetHotkey("OneHitKill")).c_str());
    }
    
    bool IsEnabled() const override { return CheatState::g_enabled.oneHitKill; }
    void SetEnabled(bool enabled) override { enabled ? OnEnable() : OnDisable(); }
    int GetHotkey() const override { return Config::Manager::Instance().GetHotkey("OneHitKill"); }
    void SetHotkey(int vk) override { Config::Manager::Instance().SetHotkey("OneHitKill", vk); }
    
private:
    std::string GetKeyName(int vk) {
        if (vk == 0) return "None";
        char name[32];
        GetKeyNameTextA(MapVirtualKeyA(vk, MAPVK_VK_TO_VSC) << 16, name, sizeof(name));
        return name;
    }
};

// ----- Max Money -----
class MaxMoneyCheat : public Cheat::Base {
public:
    const char* GetName() const override { return "MaxMoney"; }
    const char* GetDescription() const override { return "Set money to 99,999,999"; }
    
    void OnEnable() override {
        CheatState::g_enabled.maxMoney = true;
        CheatState::g_hotkeys.maxMoney = true;
        
        uintptr_t raceInfoPtr = 0;
        if (Memory::SafeRead(Addresses::RaceInfo_Instance, raceInfoPtr) && raceInfoPtr) {
            Memory::SafeRead(raceInfoPtr + Offsets::RaceInfo_PlayerProfile + Offsets::Profile_Money, 
                           CheatState::g_originals.money);
            Memory::SafeWrite(raceInfoPtr + Offsets::RaceInfo_PlayerProfile + Offsets::Profile_Money, 99999999);
        }
        LOG_INFO("Max Money enabled");
    }
    
    void OnDisable() override {
        CheatState::g_enabled.maxMoney = false;
        CheatState::g_hotkeys.maxMoney = false;
        
        if (CheatState::g_originals.hasOriginals) {
            uintptr_t raceInfoPtr = 0;
            if (Memory::SafeRead(Addresses::RaceInfo_Instance, raceInfoPtr) && raceInfoPtr) {
                Memory::SafeWrite(raceInfoPtr + Offsets::RaceInfo_PlayerProfile + Offsets::Profile_Money, 
                                CheatState::g_originals.money);
            }
        }
        LOG_INFO("Max Money disabled");
    }
    
    void OnFrame() override {
        if (!CheatState::g_enabled.maxMoney) return;
        
        uintptr_t raceInfoPtr = 0;
        if (Memory::SafeRead(Addresses::RaceInfo_Instance, raceInfoPtr) && raceInfoPtr) {
            Memory::SafeWrite(raceInfoPtr + Offsets::RaceInfo_PlayerProfile + Offsets::Profile_Money, 99999999);
        }
    }
    
    void OnMenu() override {
        ImGui::Checkbox("Max Money (99,999,999)", &CheatState::g_hotkeys.maxMoney);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sets career money to maximum. Hotkey: %s",
            GetKeyName(Config::Manager::Instance().GetHotkey("MaxMoney")).c_str());
        
        ImGui::SameLine();
        if (ImGui::Button("Set Custom")) {
            // Could open input dialog
        }
        
        ImGui::SameLine();
        static int customMoney = 1000000;
        ImGui::InputInt("##customMoney", &customMoney, 1000, 10000);
        if (ImGui::Button("Apply Custom")) {
            uintptr_t raceInfoPtr = 0;
            if (Memory::SafeRead(Addresses::RaceInfo_Instance, raceInfoPtr) && raceInfoPtr) {
                Memory::SafeWrite(raceInfoPtr + Offsets::RaceInfo_PlayerProfile + Offsets::Profile_Money, customMoney);
            }
        }
    }
    
    bool IsEnabled() const override { return CheatState::g_enabled.maxMoney; }
    void SetEnabled(bool enabled) override { enabled ? OnEnable() : OnDisable(); }
    int GetHotkey() const override { return Config::Manager::Instance().GetHotkey("MaxMoney"); }
    void SetHotkey(int vk) override { Config::Manager::Instance().SetHotkey("MaxMoney", vk); }
    
private:
    std::string GetKeyName(int vk) {
        if (vk == 0) return "None";
        char name[32];
        GetKeyNameTextA(MapVirtualKeyA(vk, MAPVK_VK_TO_VSC) << 16, name, sizeof(name));
        return name;
    }
};

// ----- Always 1st Place -----
class AlwaysFirstCheat : public Cheat::Base {
public:
    const char* GetName() const override { return "AlwaysFirst"; }
    const char* GetDescription() const override { return "Always finish in 1st position"; }
    
    void OnEnable() override {
        CheatState::g_enabled.alwaysFirst = true;
        CheatState::g_hotkeys.alwaysFirst = true;
        LOG_INFO("Always 1st Place enabled");
    }
    
    void OnDisable() override {
        CheatState::g_enabled.alwaysFirst = false;
        CheatState::g_hotkeys.alwaysFirst = false;
        LOG_INFO("Always 1st Place disabled");
    }
    
    void OnFrame() override {
        if (!CheatState::g_enabled.alwaysFirst) return;
        if (!Game::IsInRace()) return;
        
        // This is complex - position is managed by race host
        // We can try to manipulate the race host's position tracking
        uintptr_t host = GetRaceHost();
        if (!host) return;
        
        // Try to find position offset in race host
        // For now, we'll use a different approach: make player super fast
        // or manipulate the lap counter
    }
    
    void OnMenu() override {
        ImGui::Checkbox("Always 1st Place", &CheatState::g_hotkeys.alwaysFirst);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Force 1st position. Hotkey: %s",
            GetKeyName(Config::Manager::Instance().GetHotkey("AlwaysFirst")).c_str());
        
        ImGui::SameLine();
        if (ImGui::Button("Win Race Now")) {
            // Try to trigger win condition
            uintptr_t host = GetRaceHost();
            if (host) {
                // This would need specific offset research
            }
        }
    }
    
    bool IsEnabled() const override { return CheatState::g_enabled.alwaysFirst; }
    void SetEnabled(bool enabled) override { enabled ? OnEnable() : OnDisable(); }
    int GetHotkey() const override { return Config::Manager::Instance().GetHotkey("AlwaysFirst"); }
    void SetHotkey(int vk) override { Config::Manager::Instance().SetHotkey("AlwaysFirst", vk); }
    
private:
    std::string GetKeyName(int vk) {
        if (vk == 0) return "None";
        char name[32];
        GetKeyNameTextA(MapVirtualKeyA(vk, MAPVK_VK_TO_VSC) << 16, name, sizeof(name));
        return name;
    }
};

// ----- Freeze AI -----
class FreezeAICheat : public Cheat::Base {
public:
    const char* GetName() const override { return "FreezeAI"; }
    const char* GetDescription() const override { return "Freeze all AI opponents in place"; }
    
    void OnEnable() override {
        CheatState::g_enabled.freezeAI = true;
        CheatState::g_hotkeys.freezeAI = true;
        LOG_INFO("Freeze AI enabled");
    }
    
    void OnDisable() override {
        CheatState::g_enabled.freezeAI = false;
        CheatState::g_hotkeys.freezeAI = false;
        LOG_INFO("Freeze AI disabled");
    }
    
    void OnFrame() override {
        if (!CheatState::g_enabled.freezeAI) return;
        
        uintptr_t host = GetPlayerHost();
        if (!host) return;
        
        uintptr_t* playersArray = nullptr;
        if (!Memory::SafeRead(host + Offsets::Host_PlayersArray, playersArray) || !playersArray) return;
        
        uintptr_t localPlayer = 0;
        Memory::SafeRead(host + Offsets::Host_LocalPlayerPtr, localPlayer);
        
        for (int i = 0; i < 8; ++i) {
            uintptr_t player = playersArray[i];
            if (!player || player == localPlayer) continue;
            
            void* vtable = nullptr;
            if (!Memory::SafeRead(player, vtable) || !vtable) continue;
            
            // Zero out their inputs
            Memory::SafeWrite(player + Offsets::Player_GasPedal, 0.0f);
            Memory::SafeWrite(player + Offsets::Player_BrakePedal, 1.0f);  // Full brake
            Memory::SafeWrite(player + Offsets::Player_SteerAngle, 0.0f);
        }
    }
    
    void OnMenu() override {
        ImGui::Checkbox("Freeze AI Opponents", &CheatState::g_hotkeys.freezeAI);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("AI cars stop moving. Hotkey: %s",
            GetKeyName(Config::Manager::Instance().GetHotkey("FreezeAI")).c_str());
    }
    
    bool IsEnabled() const override { return CheatState::g_enabled.freezeAI; }
    void SetEnabled(bool enabled) override { enabled ? OnEnable() : OnDisable(); }
    int GetHotkey() const override { return Config::Manager::Instance().GetHotkey("FreezeAI"); }
    void SetHotkey(int vk) override { Config::Manager::Instance().SetHotkey("FreezeAI", vk); }
    
private:
    std::string GetKeyName(int vk) {
        if (vk == 0) return "None";
        char name[32];
        GetKeyNameTextA(MapVirtualKeyA(vk, MAPVK_VK_TO_VSC) << 16, name, sizeof(name));
        return name;
    }
};

// ----- Speed Hack -----
class SpeedHackCheat : public Cheat::Base {
public:
    const char* GetName() const override { return "SpeedHack"; }
    const char* GetDescription() const override { return "Global game speed multiplier"; }
    
    void OnEnable() override {
        CheatState::g_enabled.speedHack = true;
        CheatState::g_hotkeys.speedHack = true;
        LOG_INFO("Speed Hack enabled: %.2fx", CheatState::g_enabled.speedMultiplier);
    }
    
    void OnDisable() override {
        CheatState::g_enabled.speedHack = false;
        CheatState::g_hotkeys.speedHack = false;
        LOG_INFO("Speed Hack disabled");
    }
    
    void OnFrame() override {
        if (!CheatState::g_enabled.speedHack) return;
        
        // Hook timeGetTime or QueryPerformanceCounter for speed hack
        // This is a simplified version - real implementation needs hooking
    }
    
    void OnMenu() override {
        ImGui::Checkbox("Speed Hack", &CheatState::g_hotkeys.speedHack);
        ImGui::SameLine();
        ImGui::SliderFloat("Multiplier", &CheatState::g_enabled.speedMultiplier, 0.1f, 5.0f, "%.2fx");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Game speed multiplier. Hotkey: %s",
            GetKeyName(Config::Manager::Instance().GetHotkey("SpeedHack")).c_str());
    }
    
    bool IsEnabled() const override { return CheatState::g_enabled.speedHack; }
    void SetEnabled(bool enabled) override { enabled ? OnEnable() : OnDisable(); }
    int GetHotkey() const override { return Config::Manager::Instance().GetHotkey("SpeedHack"); }
    void SetHotkey(int vk) override { Config::Manager::Instance().SetHotkey("SpeedHack", vk); }
    
private:
    std::string GetKeyName(int vk) {
        if (vk == 0) return "None";
        char name[32];
        GetKeyNameTextA(MapVirtualKeyA(vk, MAPVK_VK_TO_VSC) << 16, name, sizeof(name));
        return name;
    }
};

// ----- Unlock All Cars/Tracks -----
class UnlockAllCheat : public Cheat::Base {
public:
    const char* GetName() const override { return "UnlockAll"; }
    const char* GetDescription() const override { return "Unlock all cars, tracks, and modes"; }
    
    void OnEnable() override {
        CheatState::g_enabled.unlockAll = true;
        CheatState::g_hotkeys.unlockAll = true;
        
        // This modifies savegame/profile data
        uintptr_t raceInfoPtr = 0;
        if (Memory::SafeRead(Addresses::RaceInfo_Instance, raceInfoPtr) && raceInfoPtr) {
            uintptr_t profilePtr = raceInfoPtr + Offsets::RaceInfo_PlayerProfile;
            
            // Set all cars unlocked (NumCars = max)
            Memory::SafeWrite(profilePtr + Offsets::Profile_NumCars, 45);  // Max cars
            
            // Unlock all tracks - this is more complex, stored in profile
        }
        LOG_INFO("Unlock All enabled");
    }
    
    void OnDisable() override {
        CheatState::g_enabled.unlockAll = false;
        CheatState::g_hotkeys.unlockAll = false;
        LOG_INFO("Unlock All disabled");
    }
    
    void OnFrame() override {
        if (!CheatState::g_enabled.unlockAll) return;
    }
    
    void OnMenu() override {
        ImGui::Checkbox("Unlock All Cars/Tracks", &CheatState::g_hotkeys.unlockAll);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Unlocks everything in career. Hotkey: %s",
            GetKeyName(Config::Manager::Instance().GetHotkey("UnlockAll")).c_str());
        
        if (ImGui::Button("Unlock All Cars Now")) {
            uintptr_t raceInfoPtr = 0;
            if (Memory::SafeRead(Addresses::RaceInfo_Instance, raceInfoPtr) && raceInfoPtr) {
                Memory::SafeWrite(raceInfoPtr + Offsets::RaceInfo_PlayerProfile + Offsets::Profile_NumCars, 45);
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Unlock All Tracks")) {
            // Would need profile modification
        }
    }
    
    bool IsEnabled() const override { return CheatState::g_enabled.unlockAll; }
    void SetEnabled(bool enabled) override { enabled ? OnEnable() : OnDisable(); }
    int GetHotkey() const override { return Config::Manager::Instance().GetHotkey("UnlockAll"); }
    void SetHotkey(int vk) override { Config::Manager::Instance().SetHotkey("UnlockAll", vk); }
    
private:
    std::string GetKeyName(int vk) {
        if (vk == 0) return "None";
        char name[32];
        GetKeyNameTextA(MapVirtualKeyA(vk, MAPVK_VK_TO_VSC) << 16, name, sizeof(name));
        return name;
    }
};

// ----- No Damage Visual -----
class NoDamageVisualCheat : public Cheat::Base {
public:
    const char* GetName() const override { return "NoDamageVisual"; }
    const char* GetDescription() const override { return "Car looks pristine but still takes damage"; }
    
    void OnEnable() override {
        CheatState::g_enabled.noDamageVisual = true;
        LOG_INFO("No Damage Visual enabled");
    }
    
    void OnDisable() override {
        CheatState::g_enabled.noDamageVisual = false;
        LOG_INFO("No Damage Visual disabled");
    }
    
    void OnFrame() override {
        if (!CheatState::g_enabled.noDamageVisual) return;
        // This would require hooking the rendering code
        // For now, just keep visual damage at 0
        uintptr_t car = GetLocalPlayerCar();
        if (car) {
            // Visual damage might be separate from physics damage
        }
    }
    
    void OnMenu() override {
        ImGui::Checkbox("No Visual Damage", &CheatState::g_enabled.noDamageVisual);
    }
    
    bool IsEnabled() const override { return CheatState::g_enabled.noDamageVisual; }
    void SetEnabled(bool enabled) override { enabled ? OnEnable() : OnDisable(); }
    int GetHotkey() const override { return 0; }
    void SetHotkey(int vk) override {}
};

// ----- Stunt Score Multiplier -----
class StuntScoreMultiplierCheat : public Cheat::Base {
public:
    const char* GetName() const override { return "StuntScoreMultiplier"; }
    const char* GetDescription() const override { return "Multiply stunt mode scores"; }
    
    void OnEnable() override {
        CheatState::g_enabled.stuntScoreMultiplier = true;
        LOG_INFO("Stunt Score Multiplier enabled: %.1fx", CheatState::g_enabled.stuntMultiplier);
    }
    
    void OnDisable() override {
        CheatState::g_enabled.stuntScoreMultiplier = false;
        LOG_INFO("Stunt Score Multiplier disabled");
    }
    
    void OnFrame() override {
        if (!CheatState::g_enabled.stuntScoreMultiplier) return;
        // Hook score calculation
    }
    
    void OnMenu() override {
        ImGui::Checkbox("Stunt Score Multiplier", &CheatState::g_enabled.stuntScoreMultiplier);
        ImGui::SameLine();
        ImGui::SliderFloat("Multiplier", &CheatState::g_enabled.stuntMultiplier, 1.0f, 100.0f, "%.1fx");
    }
    
    bool IsEnabled() const override { return CheatState::g_enabled.stuntScoreMultiplier; }
    void SetEnabled(bool enabled) override { enabled ? OnEnable() : OnDisable(); }
    int GetHotkey() const override { return 0; }
    void SetHotkey(int vk) override {}
};

// ----- Super Grip -----
class SuperGripCheat : public Cheat::Base {
public:
    const char* GetName() const override { return "SuperGrip"; }
    const char* GetDescription() const override { return "Maximum tire grip - no sliding"; }
    
    void OnEnable() override {
        CheatState::g_enabled.superGrip = true;
        LOG_INFO("Super Grip enabled");
    }
    
    void OnDisable() override {
        CheatState::g_enabled.superGrip = false;
        LOG_INFO("Super Grip disabled");
    }
    
    void OnFrame() override {
        if (!CheatState::g_enabled.superGrip) return;
        // Modify car physics - would need engine config hooking
    }
    
    void OnMenu() override {
        ImGui::Checkbox("Super Grip (No Slide)", &CheatState::g_enabled.superGrip);
    }
    
    bool IsEnabled() const override { return CheatState::g_enabled.superGrip; }
    void SetEnabled(bool enabled) override { enabled ? OnEnable() : OnDisable(); }
    int GetHotkey() const override { return 0; }
    void SetHotkey(int vk) override {}
};

// ----- Drift Mode -----
class DriftModeCheat : public Cheat::Base {
public:
    const char* GetName() const override { return "DriftMode"; }
    const char* GetDescription() const override { return "Easy drift handling for stunts" }
    
    void OnEnable() override {
        CheatState::g_enabled.driftMode = true;
        LOG_INFO("Drift Mode enabled");
    }
    
    void OnDisable() override {
        CheatState::g_enabled.driftMode = false;
        LOG_INFO("Drift Mode disabled");
    }
    
    void OnFrame() override {
        if (!CheatState::g_enabled.driftMode) return;
        // Modify handling for easy drifting
    }
    
    void OnMenu() override {
        ImGui::Checkbox("Drift Mode", &CheatState::g_enabled.driftMode);
    }
    
    bool IsEnabled() const override { return CheatState::g_enabled.driftMode; }
    void SetEnabled(bool enabled) override { enabled ? OnEnable() : OnDisable(); }
    int GetHotkey() const override { return 0; }
    void SetHotkey(int vk) override {}
};

// ----- Helper functions -----
uintptr_t GetLocalPlayer() {
    uintptr_t raceInfoPtr = 0;
    if (!Memory::SafeRead(Addresses::RaceInfo_Instance, raceInfoPtr) || !raceInfoPtr) return 0;
    
    uintptr_t hostObj = 0;
    if (!Memory::SafeRead(raceInfoPtr + Offsets::RaceInfo_HostObject, hostObj) || !hostObj) return 0;
    
    uintptr_t localPlayer = 0;
    Memory::SafeRead(hostObj + Offsets::Host_LocalPlayerPtr, localPlayer);
    return localPlayer;
}

uintptr_t GetLocalPlayerCar() {
    uintptr_t player = GetLocalPlayer();
    if (!player) return 0;
    
    uintptr_t carPtr = 0;
    Memory::SafeRead(player + Offsets::Player_CarPtr, carPtr);
    return carPtr;
}

uintptr_t GetRaceHost() {
    uintptr_t raceInfoPtr = 0;
    if (!Memory::SafeRead(Addresses::RaceInfo_Instance, raceInfoPtr) || !raceInfoPtr) return 0;
    
    uintptr_t hostObj = 0;
    Memory::SafeRead(raceInfoPtr + Offsets::RaceInfo_HostObject, hostObj);
    return hostObj;
}

uintptr_t GetPlayerHost() {
    return GetRaceHost();  // Same thing in single player
}

// ============================================================================
// CHEAT REGISTRATION
// ============================================================================

// Register all cheats
namespace {
    struct CheatRegistrar {
        CheatRegistrar() {
            auto& mgr = Cheat::Manager::Instance();
            mgr.Register(std::make_unique<GodModeCheat>());
            mgr.Register(std::make_unique<InfiniteNitroCheat>());
            mgr.Register(std::make_unique<InfiniteBoostCheat>());
            mgr.Register(std::make_unique<OneHitKillCheat>());
            mgr.Register(std::make_unique<MaxMoneyCheat>());
            mgr.Register(std::make_unique<AlwaysFirstCheat>());
            mgr.Register(std::make_unique<FreezeAICheat>());
            mgr.Register(std::make_unique<SpeedHackCheat>());
            mgr.Register(std::make_unique<UnlockAllCheat>());
            mgr.Register(std::make_unique<NoDamageVisualCheat>());
            mgr.Register(std::make_unique<StuntScoreMultiplierCheat>());
            mgr.Register(std::make_unique<SuperGripCheat>());
            mgr.Register(std::make_unique<DriftModeCheat>());
            
            LOG_INFO("Registered %zu cheats", mgr.GetAll().size());
        }
    } g_cheatRegistrar;
}

namespace Cheat {
    const std::vector<Base*>& Manager::GetAll() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cheatPointers.clear();
        m_cheatPointers.reserve(m_cheats.size());
        for (auto& cheat : m_cheats) {
            m_cheatPointers.push_back(cheat.get());
        }
        return m_cheatPointers;
    }
    
    void Manager::Register(std::unique_ptr<Base> cheat) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Base* ptr = cheat.get();
        m_cheats.push_back(std::move(cheat));
    }
    
    Base* Manager::Get(const char* name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto* cheat : m_cheats) {
            if (strcmp(cheat->GetName(), name) == 0) return cheat;
        }
        return nullptr;
    }
    
    void Manager::EnableAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto* cheat : m_cheats) cheat->SetEnabled(true);
    }
    
    void Manager::DisableAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto* cheat : m_cheats) cheat->SetEnabled(false);
    }
    
    void Manager::Update() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto* cheat : m_cheats) {
            if (cheat->IsEnabled()) cheat->OnFrame();
        }
    }
    
    void Manager::RenderMenu() {
        for (auto* cheat : m_cheats) {
            cheat->OnMenu();
        }
    }
    
    void Manager::OnFrame() {
        Update();
    }
    
    void Manager::SaveConfig() {
        Config::Manager::Instance().Save(Config::Manager::Instance().m_configPath);
    }
    
    void Manager::LoadConfig() {
        Config::Manager::Instance().Load(Config::Manager::Instance().m_configPath);
    }
}