#include "trainer_common.h"
#include "pattern_scanner.h"
#include <Psapi.h>

// Global address storage
namespace Addresses {
    uintptr_t RaceInfo_Instance = 0;
    uintptr_t LinkSession_Instance = 0;
    uintptr_t PlayerHost_VTable_SP = 0;
    uintptr_t PlayerHost_VTable_MP = 0;
    uintptr_t LiteDb_Instance = 0;
    uintptr_t LoadScreenInfo_Instance = 0;
    uintptr_t MaxPlayersAddr = 0;
    uintptr_t RagdollingGainAddr = 0;
    uintptr_t RegenerationRateAddr = 0;
    uintptr_t NitroAccelerationAddr = 0;
    uintptr_t TurboAccelerationAddr = 0;
    uintptr_t GainSFXThresholdAddr = 0;
    uintptr_t Vehicle_ParseAerialPhysics = 0;
    uintptr_t sub_46E6D0 = 0;
    uintptr_t EngineConfig_Calc = 0;
    uintptr_t RaceHost_Update = 0;
}

// Known static addresses for Steam 2025 version (as fallback)
namespace {
    const uintptr_t KNOWN_RaceInfo_Instance = 0x8E8410;
    const uintptr_t KNOWN_LinkSession_Instance = 0x73B018;
    const uintptr_t KNOWN_PlayerHost_VTable_SP = 0x66D890;
    const uintptr_t KNOWN_PlayerHost_VTable_MP = 0x674C10;
    const uintptr_t KNOWN_MaxPlayersAddr = 0x893DBC;
    const uintptr_t KNOWN_RagdollingGainAddr = 0x6970A8;
    const uintptr_t KNOWN_RegenerationRateAddr = 0x6970AC;
    const uintptr_t KNOWN_NitroAccelerationAddr = 0x6D6AB4;
    const uintptr_t KNOWN_TurboAccelerationAddr = 0x6D6AB8;
    const uintptr_t KNOWN_GainSFXThresholdAddr = 0x67DBA8;
    const uintptr_t KNOWN_Vehicle_ParseAerialPhysics = 0x46A190;
    const uintptr_t KNOWN_sub_46E6D0 = 0x46E6D0;
    const uintptr_t KNOWN_EngineConfig_Calc = 0x4562D0;
    const uintptr_t KNOWN_RaceHost_Update = 0x4B8D60; // estimated
}

// Pattern signatures for version-resilient scanning
namespace Patterns {
    // RaceInfo::Instance - look for "A1 ?? ?? ?? ?? 85 C0 74 ?? 8B 48 04" (mov eax, [RaceInfo.Instance]; test eax, eax; jz; mov ecx, [eax+4])
    const char* RaceInfo_Instance_Pattern = "A1 ? ? ? ? 85 C0 74 ? 8B 48 04";
    
    // LinkSession::Instance - "A1 ?? ?? ?? ?? 8B 40 04 85 C0" (mov eax, [LinkSession.Instance]; mov eax, [eax+4]; test eax, eax)
    const char* LinkSession_Instance_Pattern = "A1 ? ? ? ? 8B 40 04 85 C0";
    
    // PlayerHost VTable (SP) - look for vtable with specific functions
    const char* PlayerHost_VTable_Pattern = "55 8B EC 83 EC 10 53 56 57 8B F9 8B 47 14";
    
    // RagdollingGain - "C7 05 ?? ?? ?? ?? 00 00 80 3F" (mov [RagdollingGain], 1.0f)
    const char* RagdollingGain_Pattern = "C7 05 ? ? ? ? 00 00 80 3F";
    
    // RegenerationRate - "C7 05 ?? ?? ?? ?? CD CC CC 3D" (mov [RegenerationRate], 0.05f)
    const char* RegenerationRate_Pattern = "C7 05 ? ? ? ? CD CC CC 3D";
    
    // Vehicle_ParseAerialPhysics - distinctive function prologue
    const char* Vehicle_ParseAerialPhysics_Pattern = "55 8B EC 83 EC 38 53 56 57 8B F9 8B 47 04";
    
    // sub_46E6D0 (nitro gain) - look for nitro regeneration logic
    const char* NitroGain_Pattern = "F3 0F 10 05 ? ? ? ? 0F 2F 05 ? ? ? ? 76";
    
    // EngineConfig_Calc - "55 8B EC 83 EC 10 56 8B F1 8B 46 04"
    const char* EngineConfig_Calc_Pattern = "55 8B EC 83 EC 10 56 8B F1 8B 46 04";
    
    // RaceHost::Update - main race loop
    const char* RaceHost_Update_Pattern = "55 8B EC 83 EC 28 53 56 57 8B F9 8B 87 ? ? ? ? 85 C0";
}

bool ResolveAddresses() {
    bool allFound = true;
    uintptr_t moduleBase = Game::g_moduleBase;
    
    LOG_INFO("Resolving addresses for module base 0x%p", reinterpret_cast<void*>(moduleBase));
    
    // Try pattern scanning first
    auto tryScan = [&](const char* name, const char* pattern, uintptr_t& outAddr, uintptr_t knownAddr, int offset = 0) {
        Pattern::Result result = Pattern::ScanModule(nullptr, pattern);
        if (result) {
            outAddr = result.address + offset;
            LOG_INFO("%s: Found via pattern at 0x%p", name, reinterpret_cast<void*>(outAddr));
            return true;
        } else if (knownAddr) {
            // Verify known address is valid
            uint32_t testVal = 0;
            if (Memory::SafeRead(knownAddr, testVal)) {
                outAddr = knownAddr;
                LOG_WARN("%s: Pattern failed, using known address 0x%p", name, reinterpret_cast<void*>(outAddr));
                return true;
            }
        }
        LOG_ERROR("%s: FAILED to resolve!", name);
        return false;
    };
    
    // RaceInfo::Instance - pattern gives address of the instruction, need to resolve the displacement
    {
        Pattern::Result result = Pattern::ScanModule(nullptr, Patterns::RaceInfo_Instance_Pattern);
        if (result) {
            // Instruction: A1 xx xx xx xx (mov eax, [addr])
            int32_t disp = 0;
            if (Memory::SafeRead(result.address + 1, disp)) {
                Addresses::RaceInfo_Instance = result.address + 5 + disp;
                LOG_INFO("RaceInfo_Instance: Resolved to 0x%p", reinterpret_cast<void*>(Addresses::RaceInfo_Instance));
            }
        } else {
            Addresses::RaceInfo_Instance = KNOWN_RaceInfo_Instance;
            LOG_WARN("RaceInfo_Instance: Using known 0x%p", reinterpret_cast<void*>(Addresses::RaceInfo_Instance));
        }
    }
    
    // LinkSession::Instance
    {
        Pattern::Result result = Pattern::ScanModule(nullptr, Patterns::LinkSession_Instance_Pattern);
        if (result) {
            int32_t disp = 0;
            if (Memory::SafeRead(result.address + 1, disp)) {
                Addresses::LinkSession_Instance = result.address + 5 + disp;
                LOG_INFO("LinkSession_Instance: Resolved to 0x%p", reinterpret_cast<void*>(Addresses::LinkSession_Instance));
            }
        } else {
            Addresses::LinkSession_Instance = KNOWN_LinkSession_Instance;
            LOG_WARN("LinkSession_Instance: Using known 0x%p", reinterpret_cast<void*>(Addresses::LinkSession_Instance));
        }
    }
    
    // PlayerHost VTable - find via pattern
    {
        Pattern::Result result = Pattern::ScanModule(nullptr, Patterns::PlayerHost_VTable_Pattern);
        if (result) {
            // The pattern matches the function, vtable points to it
            // Search backwards for vtable
            for (int i = 0; i < 0x1000; i += 4) {
                uintptr_t candidate = result.address - i;
                uintptr_t* vtable = reinterpret_cast<uintptr_t*>(candidate);
                if (vtable[0] == result.address) {
                    Addresses::PlayerHost_VTable_SP = candidate;
                    LOG_INFO("PlayerHost_VTable_SP: Found at 0x%p", reinterpret_cast<void*>(Addresses::PlayerHost_VTable_SP));
                    break;
                }
            }
        }
        if (!Addresses::PlayerHost_VTable_SP) {
            Addresses::PlayerHost_VTable_SP = KNOWN_PlayerHost_VTable_SP;
            LOG_WARN("PlayerHost_VTable_SP: Using known 0x%p", reinterpret_cast<void*>(Addresses::PlayerHost_VTable_SP));
        }
        Addresses::PlayerHost_VTable_MP = KNOWN_PlayerHost_VTable_MP;
    }
    
    // Global values
    tryScan("MaxPlayersAddr", "A1 ? ? ? ? 83 F8 08", Addresses::MaxPlayersAddr, KNOWN_MaxPlayersAddr);
    
    // RagdollingGain - pattern: C7 05 addr 00 00 80 3F
    {
        Pattern::Result result = Pattern::ScanModule(nullptr, Patterns::RagdollingGain_Pattern);
        if (result) {
            int32_t disp = 0;
            if (Memory::SafeRead(result.address + 2, disp)) {
                Addresses::RagdollingGainAddr = result.address + 6 + disp;
                LOG_INFO("RagdollingGainAddr: Resolved to 0x%p", reinterpret_cast<void*>(Addresses::RagdollingGainAddr));
            }
        } else {
            Addresses::RagdollingGainAddr = KNOWN_RagdollingGainAddr;
        }
    }
    
    // RegenerationRate
    {
        Pattern::Result result = Pattern::ScanModule(nullptr, Patterns::RegenerationRate_Pattern);
        if (result) {
            int32_t disp = 0;
            if (Memory::SafeRead(result.address + 2, disp)) {
                Addresses::RegenerationRateAddr = result.address + 6 + disp;
                LOG_INFO("RegenerationRateAddr: Resolved to 0x%p", reinterpret_cast<void*>(Addresses::RegenerationRateAddr));
            }
        } else {
            Addresses::RegenerationRateAddr = KNOWN_RegenerationRateAddr;
        }
    }
    
    // Vehicle_ParseAerialPhysics
    {
        Pattern::Result result = Pattern::ScanModule(nullptr, Patterns::Vehicle_ParseAerialPhysics_Pattern);
        if (result) {
            Addresses::Vehicle_ParseAerialPhysics = result.address;
            LOG_INFO("Vehicle_ParseAerialPhysics: Found at 0x%p", reinterpret_cast<void*>(Addresses::Vehicle_ParseAerialPhysics));
        } else {
            Addresses::Vehicle_ParseAerialPhysics = KNOWN_Vehicle_ParseAerialPhysics;
        }
    }
    
    // Nitro gain function
    {
        Pattern::Result result = Pattern::ScanModule(nullptr, Patterns::NitroGain_Pattern);
        if (result) {
            Addresses::sub_46E6D0 = result.address;
            LOG_INFO("sub_46E6D0 (nitro gain): Found at 0x%p", reinterpret_cast<void*>(Addresses::sub_46E6D0));
        } else {
            Addresses::sub_46E6D0 = KNOWN_sub_46E6D0;
        }
    }
    
    // EngineConfig_Calc
    {
        Pattern::Result result = Pattern::ScanModule(nullptr, Patterns::EngineConfig_Calc_Pattern);
        if (result) {
            Addresses::EngineConfig_Calc = result.address;
            LOG_INFO("EngineConfig_Calc: Found at 0x%p", reinterpret_cast<void*>(Addresses::EngineConfig_Calc));
        } else {
            Addresses::EngineConfig_Calc = KNOWN_EngineConfig_Calc;
        }
    }
    
    // RaceHost_Update
    {
        Pattern::Result result = Pattern::ScanModule(nullptr, Patterns::RaceHost_Update_Pattern);
        if (result) {
            Addresses::RaceHost_Update = result.address;
            LOG_INFO("RaceHost_Update: Found at 0x%p", reinterpret_cast<void*>(Addresses::RaceHost_Update));
        } else {
            Addresses::RaceHost_Update = KNOWN_RaceHost_Update;
        }
    }
    
    // NitroAcceleration and TurboAcceleration - these are in the engine config global block
    // They're at fixed offsets from the engine config base
    Addresses::NitroAccelerationAddr = KNOWN_NitroAccelerationAddr;
    Addresses::TurboAccelerationAddr = KNOWN_TurboAccelerationAddr;
    Addresses::GainSFXThresholdAddr = KNOWN_GainSFXThresholdAddr;
    
    // Verify critical addresses
    if (!Addresses::RaceInfo_Instance || !Addresses::LinkSession_Instance || 
        !Addresses::PlayerHost_VTable_SP) {
        LOG_ERROR("Critical addresses missing!");
        allFound = false;
    }
    
    GameVersion::g_isValidVersion = allFound;
    return allFound;
}

// Game structure accessors
namespace Game {

HWND g_hWnd = nullptr;
HINSTANCE g_hInstance = nullptr;
uintptr_t g_moduleBase = 0;
uintptr_t g_moduleSize = 0;
bool g_inRace = false;
bool g_inMenu = true;
bool g_isMultiplayer = false;

void Initialize() {
    g_hInstance = GetModuleHandle(nullptr);
    g_moduleBase = reinterpret_cast<uintptr_t>(g_hInstance);
    
    MODULEINFO mi = {};
    GetModuleInformation(GetCurrentProcess(), g_hInstance, &mi, sizeof(mi));
    g_moduleSize = mi.SizeOfImage;
    
    // Find game window
    g_hWnd = FindWindowA(TARGET_WINDOW_CLASS, nullptr);
    if (!g_hWnd) {
        // Try alternative
        g_hWnd = FindWindowA(nullptr, "FlatOut 2");
    }
    
    LOG_INFO("Game initialized: base=0x%p size=0x%X hwnd=0x%p", 
             reinterpret_cast<void*>(g_moduleBase), g_moduleSize, g_hWnd);
    
    ResolveAddresses();
}

void Update() {
    // Update game state
    uintptr_t raceInfoPtr = 0;
    if (Memory::SafeRead(Addresses::RaceInfo_Instance, raceInfoPtr) && raceInfoPtr) {
        SessionType sessionType = SessionType::None;
        if (Memory::SafeRead(raceInfoPtr + Offsets::RaceInfo_SessionType, sessionType)) {
            g_inRace = (sessionType == SessionType::Race);
            g_inMenu = (sessionType == SessionType::Menu);
        }
        
        // Check multiplayer
        uintptr_t linkSessionPtr = 0;
        if (Memory::SafeRead(Addresses::LinkSession_Instance, linkSessionPtr) && linkSessionPtr) {
            uintptr_t vtable = 0;
            if (Memory::SafeRead(linkSessionPtr, vtable)) {
                g_isMultiplayer = (vtable == 0x677478);  // MultiplayerVTable from SDK
            }
        }
    }
}

bool IsInRace() { return g_inRace; }
bool IsInMenu() { return g_inMenu; }
bool IsMultiplayer() { return g_isMultiplayer; }

const char* GetCurrentTrackName() {
    static char buffer[256] = "Unknown";
    uintptr_t raceInfoPtr = 0;
    if (Memory::SafeRead(Addresses::RaceInfo_Instance, raceInfoPtr) && raceInfoPtr) {
        int levelId = 0;
        if (Memory::SafeRead(raceInfoPtr + Offsets::RaceInfo_LevelId, levelId)) {
            // Level names from game data
            static const char* levelNames[] = {
                "Unknown", "Timberlands 1", "Timberlands 2", "Timberlands 3",
                "Gravel Pit 1", "Gravel Pit 2", "Gravel Pit 3",
                "Storm Drain 1", "Storm Drain 2", "Storm Drain 3",
                "Canyon 1", "Canyon 2", "Canyon 3",
                "Rooftops 1", "Rooftops 2", "Rooftops 3",
                "Waterworks 1", "Waterworks 2", "Waterworks 3",
                "L.A. Freeway 1", "L.A. Freeway 2", "L.A. Freeway 3",
                "Derby 1", "Derby 2", "Derby 3",
                "Stunt 1", "Stunt 2", "Stunt 3"
            };
            if (levelId >= 0 && levelId < (int)(sizeof(levelNames)/sizeof(levelNames[0]))) {
                return levelNames[levelId];
            }
            snprintf(buffer, sizeof(buffer), "Level %d", levelId);
            return buffer;
        }
    }
    return buffer;
}

int GetCurrentTrackId() {
    uintptr_t raceInfoPtr = 0;
    if (Memory::SafeRead(Addresses::RaceInfo_Instance, raceInfoPtr) && raceInfoPtr) {
        int levelId = 0;
        if (Memory::SafeRead(raceInfoPtr + Offsets::RaceInfo_LevelId, levelId)) {
            return levelId;
        }
    }
    return -1;
}

int GetCurrentCarId() {
    uintptr_t raceInfoPtr = 0;
    if (Memory::SafeRead(Addresses::RaceInfo_Instance, raceInfoPtr) && raceInfoPtr) {
        uintptr_t hostObj = 0;
        if (Memory::SafeRead(raceInfoPtr + Offsets::RaceInfo_HostObject, hostObj) && hostObj) {
            uintptr_t* playersArray = nullptr;
            if (Memory::SafeRead(hostObj + Offsets::Host_PlayersArray, playersArray) && playersArray) {
                uintptr_t localPlayer = 0;
                if (Memory::SafeRead(hostObj + Offsets::Host_LocalPlayerPtr, localPlayer) && localPlayer) {
                    int carId = 0;
                    if (Memory::SafeRead(localPlayer + Offsets::Player_CarId, carId)) {
                        return carId;
                    }
                }
            }
        }
    }
    return -1;
}

} // namespace Game