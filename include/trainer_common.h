#pragma once

// FlatOut 2 Trainer - Common Definitions
// Compatible with Steam version (build 18764761, June 2025)

#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <memory>
#include <cassert>

// Version info
#define TRAINER_VERSION_MAJOR 1
#define TRAINER_VERSION_MINOR 0
#define TRAINER_VERSION_PATCH 0
#define TRAINER_VERSION_STRING "1.0.0"
#define TRAINER_NAME "FlatOut 2 Trainer"
#define TARGET_PROCESS_NAME "FlatOut2.exe"
#define TARGET_WINDOW_CLASS "FlatOut2"

// Game version detection
namespace GameVersion {
    enum Type : uint32_t {
        Unknown = 0,
        Steam_1_0_Original = 0x01000000,
        Steam_1_2_Patch = 0x01020000,
        Steam_2024_Update = 0x20240720,  // July 2024 update (build 15008640)
        Steam_2025_Update = 0x20250606,  // June 2025 update (build 18764761)
    };
    
    extern Type g_detectedVersion;
    extern std::string g_versionString;
    extern bool g_isValidVersion;
}

// Architecture
static_assert(sizeof(void*) == 4, "FlatOut 2 is 32-bit only. Compile as x86.");

// Logging
#define LOG_DEBUG(fmt, ...) Logger::Instance().Log(LogLevel::Debug, fmt, __VA_ARGS__)
#define LOG_INFO(fmt, ...)  Logger::Instance().Log(LogLevel::Info, fmt, __VA_ARGS__)
#define LOG_WARN(fmt, ...)  Logger::Instance().Log(LogLevel::Warn, fmt, __VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::Instance().Log(LogLevel::Error, fmt, __VA_ARGS__)

enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }
    
    void Log(LogLevel level, const char* fmt, ...);
    void SetLogFile(const std::filesystem::path& path);
    void SetConsoleOutput(bool enable) { m_consoleOutput = enable; }
    
private:
    Logger() = default;
    std::ofstream m_logFile;
    bool m_consoleOutput = true;
    std::mutex m_mutex;
};

// Safe memory access
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
    
    // Read pointer chain
    template<typename T>
    std::optional<T> ReadPointerChain(uintptr_t base, const std::vector<uintptr_t>& offsets) {
        uintptr_t addr = base;
        for (size_t i = 0; i < offsets.size(); ++i) {
            uintptr_t next = 0;
            if (!SafeRead(addr + offsets[i], next)) return std::nullopt;
            if (i == offsets.size() - 1) {
                T val;
                if (SafeRead(next, val)) return val;
                return std::nullopt;
            }
            addr = next;
            if (!addr) return std::nullopt;
        }
        return std::nullopt;
    }
    
    // Write pointer chain
    template<typename T>
    bool WritePointerChain(uintptr_t base, const std::vector<uintptr_t>& offsets, const T& value) {
        uintptr_t addr = base;
        for (size_t i = 0; i < offsets.size() - 1; ++i) {
            uintptr_t next = 0;
            if (!SafeRead(addr + offsets[i], next)) return false;
            addr = next;
            if (!addr) return false;
        }
        return SafeWrite(addr + offsets.back(), value);
    }
}

// Pattern scanning
namespace Pattern {
    struct Result {
        uintptr_t address = 0;
        bool found = false;
        explicit operator bool() const { return found; }
    };
    
    // Scan for byte pattern with mask (IDA-style: "48 8B 05 ? ? ? ? 48 85 C0")
    Result Scan(const char* pattern, const char* mask = nullptr, uintptr_t start = 0, size_t length = 0);
    Result ScanModule(const char* moduleName, const char* pattern, const char* mask = nullptr);
    
    // Find all occurrences
    std::vector<uintptr_t> ScanAll(const char* pattern, const char* mask = nullptr, uintptr_t start = 0, size_t length = 0);
    
    // Resolve relative call/jmp (E8/E9) to absolute address
    uintptr_t ResolveRelativeCall(uintptr_t instructionAddress);
    uintptr_t ResolveRelativeJump(uintptr_t instructionAddress);
    uintptr_t ResolveRIPRelative(uintptr_t instructionAddress, int offsetOfDisplacement = 1);
}

// Hooking
namespace Hook {
    enum class Type { Jmp, Call, VTable, Detour };
    
    class Base {
    public:
        virtual ~Base() = default;
        virtual bool Enable() = 0;
        virtual bool Disable() = 0;
        virtual bool IsEnabled() const = 0;
        virtual uintptr_t GetOriginal() const = 0;
        virtual uintptr_t GetTarget() const = 0;
    protected:
        bool m_enabled = false;
    };
    
    // Trampoline hook (JMP detour with original bytes preservation)
    class Trampoline : public Base {
    public:
        Trampoline(uintptr_t target, void* detour, size_t minLen = 5);
        ~Trampoline();
        
        bool Enable() override;
        bool Disable() override;
        bool IsEnabled() const override { return m_enabled; }
        uintptr_t GetOriginal() const override { return m_trampoline; }
        uintptr_t GetTarget() const override { return m_target; }
        
        template<typename Fn>
        Fn GetOriginalFunction() const {
            return reinterpret_cast<Fn>(m_trampoline);
        }
        
    private:
        uintptr_t m_target = 0;
        void* m_detour = nullptr;
        uintptr_t m_trampoline = 0;
        std::vector<uint8_t> m_originalBytes;
        size_t m_hookSize = 0;
    };
    
    // VTable hook
    class VTableHook : public Base {
    public:
        VTableHook(void** vtable, size_t index, void* detour);
        ~VTableHook();
        
        bool Enable() override;
        bool Disable() override;
        bool IsEnabled() const override { return m_enabled; }
        uintptr_t GetOriginal() const override { return m_original; }
        uintptr_t GetTarget() const override { return m_vtableAddress; }
        
        template<typename Fn>
        Fn GetOriginalFunction() const {
            return reinterpret_cast<Fn>(m_original);
        }
        
    private:
        void** m_vtable = nullptr;
        size_t m_index = 0;
        void* m_detour = nullptr;
        void* m_original = nullptr;
        uintptr_t m_vtableAddress = 0;
    };
    
    // Hook manager for cleanup
    class Manager {
    public:
        static Manager& Instance();
        void Register(std::unique_ptr<Base> hook);
        void EnableAll();
        void DisableAll();
        void RemoveAll();
    private:
        std::vector<std::unique_ptr<Base>> m_hooks;
        std::mutex m_mutex;
    };
}

// Configuration
namespace Config {
    struct CheatConfig {
        bool enabled = false;
        int hotkey = 0;  // VK_ code
        std::unordered_map<std::string, std::string> values;
    };
    
    class Manager {
    public:
        static Manager& Instance();
        bool Load(const std::filesystem::path& path);
        bool Save(const std::filesystem::path& path);
        
        CheatConfig& GetCheat(const std::string& name);
        const CheatConfig& GetCheat(const std::string& name) const;
        bool IsEnabled(const std::string& name) const;
        void SetEnabled(const std::string& name, bool enabled);
        int GetHotkey(const std::string& name) const;
        void SetHotkey(const std::string& name, int vk);
        
        template<typename T>
        T GetValue(const std::string& cheat, const std::string& key, T defaultValue) const;
        
        template<typename T>
        void SetValue(const std::string& cheat, const std::string& key, const T& value);
        
    private:
        std::unordered_map<std::string, CheatConfig> m_cheats;
        std::filesystem::path m_configPath;
        std::mutex m_mutex;
    };
}

// Hotkey management
namespace Hotkey {
    struct Entry {
        int vk = 0;
        std::function<void()> callback;
        bool toggle = true;
        bool* statePtr = nullptr;
        std::string description;
    };
    
    class Manager {
    public:
        static Manager& Instance();
        void Register(int vk, std::function<void()> callback, bool toggle = true, bool* statePtr = nullptr, const char* desc = "");
        void RegisterToggle(int vk, bool& state, const char* desc = "");
        void Unregister(int vk);
        void ProcessMessage(UINT msg, WPARAM wp, LPARAM lp);
        void Update();
    private:
        std::unordered_map<int, Entry> m_hotkeys;
        std::unordered_map<int, bool> m_keyStates;
        std::mutex m_mutex;
    };
}

// Game state
namespace Game {
    extern HWND g_hWnd;
    extern HINSTANCE g_hInstance;
    extern uintptr_t g_moduleBase;
    extern uintptr_t g_moduleSize;
    extern bool g_inRace;
    extern bool g_inMenu;
    extern bool g_isMultiplayer;
    
    void Initialize();
    void Update();
    bool IsInRace();
    bool IsInMenu();
    bool IsMultiplayer();
    const char* GetCurrentTrackName();
    int GetCurrentTrackId();
    int GetCurrentCarId();
}

// Cheat base class
namespace Cheat {
    class Base {
    public:
        virtual ~Base() = default;
        virtual const char* GetName() const = 0;
        virtual const char* GetDescription() const = 0;
        virtual void OnEnable() = 0;
        virtual void OnDisable() = 0;
        virtual void OnFrame() = 0;  // Called every frame when enabled
        virtual void OnMenu() = 0;   // Called for ImGui menu
        virtual bool IsEnabled() const = 0;
        virtual void SetEnabled(bool enabled) = 0;
        virtual int GetHotkey() const = 0;
        virtual void SetHotkey(int vk) = 0;
    };
    
    class Manager {
    public:
        static Manager& Instance();
        void Register(std::unique_ptr<Base> cheat);
        Base* Get(const char* name);
        const std::vector<Base*>& GetAll() const;
        void EnableAll();
        void DisableAll();
        void Update();
        void RenderMenu();
        void OnFrame();
        void SaveConfig();
        void LoadConfig();
    private:
        std::vector<std::unique_ptr<Base>> m_cheats;
        mutable std::vector<Base*> m_cheatPointers;
        std::mutex m_mutex;
    };
}

// Macros for cheat registration
#define REGISTER_CHEAT(name) \
    namespace { \
        struct name##_Registrar { \
            name##_Registrar() { \
                Cheat::Manager::Instance().Register(std::make_unique<name>()); \
            } \
        } name##_registrar; \
    }

// Utility macros
#define OFFSET_OF(type, member) (reinterpret_cast<uintptr_t>(&(((type*)0)->member)))
#define MAKE_PTR(base, offset) (reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(base) + (offset)))
#define READ_PTR(type, base, offset) (*reinterpret_cast<type*>(reinterpret_cast<uintptr_t>(base) + (offset)))
#define WRITE_PTR(type, base, offset, value) (*reinterpret_cast<type*>(reinterpret_cast<uintptr_t>(base) + (offset)) = (value))

// Color helpers
struct Color {
    float r, g, b, a;
    Color(float r_=1, float g_=1, float b_=1, float a_=1) : r(r_), g(g_), b(b_), a(a_) {}
    Color(uint32_t rgba) : r((rgba>>24)/255.f), g((rgba>>16&0xFF)/255.f), b((rgba>>8&0xFF)/255.f), a((rgba&0xFF)/255.f) {}
    uint32_t ToU32() const { return (uint32_t(r*255)<<24) | (uint32_t(g*255)<<16) | (uint32_t(b*255)<<8) | uint32_t(a*255); }
    ImVec4 ToImVec4() const { return ImVec4(r, g, b, a); }
};

inline Color operator*(const Color& c, float m) { return Color(c.r*m, c.g*m, c.b*m, c.a); }
inline Color operator+(const Color& a, const Color& b) { return Color(a.r+b.r, a.g+b.g, a.b+b.b, a.a+b.a); }

// Constants for known addresses (will be resolved via pattern scanning at runtime)
namespace Addresses {
    // Global pointers (from FlatOut2.SDK and reverse engineering)
    extern uintptr_t RaceInfo_Instance;        // 0x8E8410
    extern uintptr_t LinkSession_Instance;     // 0x73B018
    extern uintptr_t PlayerHost_VTable_SP;     // 0x66D890
    extern uintptr_t PlayerHost_VTable_MP;     // 0x674C10
    extern uintptr_t LiteDb_Instance;          // 0x8C8D90 (estimated)
    extern uintptr_t LoadScreenInfo_Instance;  // 0x8E840C (estimated)
    
    // Global values
    extern uintptr_t MaxPlayersAddr;           // 0x893DBC
    extern uintptr_t RagdollingGainAddr;       // 0x6970A8
    extern uintptr_t RegenerationRateAddr;     // 0x6970AC
    extern uintptr_t NitroAccelerationAddr;    // 0x6D6AB4
    extern uintptr_t TurboAccelerationAddr;    // 0x6D6AB8
    extern uintptr_t GainSFXThresholdAddr;     // 0x67DBA8
    
    // Function addresses (to be resolved)
    extern uintptr_t Vehicle_ParseAerialPhysics;  // 0x46A190
    extern uintptr_t sub_46E6D0;                  // Nitro gain logic
    extern uintptr_t EngineConfig_Calc;           // 0x4562D0
    extern uintptr_t RaceHost_Update;             // Main race loop
}

// Player structure offsets (from FlatOut2.SDK)
namespace Offsets {
    // Player (size 0x6E0)
    constexpr uintptr_t Player_VTable = 0x0;
    constexpr uintptr_t Player_BlipColour = 0x2D0;
    constexpr uintptr_t Player_CarPtr = 0x33C;
    constexpr uintptr_t Player_CarId = 0x340;
    constexpr uintptr_t Player_CarSkinId = 0x344;
    constexpr uintptr_t Player_IsDriverFemale = 0x34C;
    constexpr uintptr_t Player_DriverSkin = 0x350;
    constexpr uintptr_t Player_PlayerID = 0x368;
    constexpr uintptr_t Player_Flags = 0x36C;
    constexpr uintptr_t Player_DisableControlAndReset = 0x380;
    constexpr uintptr_t Player_ReadOnlyDamage = 0x49C;
    constexpr uintptr_t Player_Nudges = 0x4F8;
    constexpr uintptr_t Player_SteerAngle = 0x684;
    constexpr uintptr_t Player_GasPedal = 0x688;
    constexpr uintptr_t Player_BrakePedal = 0x68C;
    
    // Car (FO1_Car) - from reverse engineering
    constexpr uintptr_t Car_EngineConfigPtr = 0x98;
    constexpr uintptr_t Car_OpponentStatePtr = 0x33C;
    constexpr uintptr_t Car_NitroDisplayRatio = 0x354;
    constexpr uintptr_t Car_NitroDisplayRatioSmoothed = 0x358;
    constexpr uintptr_t Car_LocalPlayerSlotId = 0x368;
    constexpr uintptr_t Car_IsLocalPlayer = 0x36C;
    constexpr uintptr_t Car_TimeSinceSpawn = 0x3B0;
    constexpr uintptr_t Car_NitroStorageMirror = 0x3B8;
    constexpr uintptr_t Car_AirtimeAccumulator = 0x548;
    constexpr uintptr_t Car_NitroCurrent = 0x5CC;
    constexpr uintptr_t Car_NitroMax = 0x5D0;
    constexpr uintptr_t Car_CrashVelocity = 0x638;  // Per opponent, stride 28
    constexpr uintptr_t Car_SmoothedNitroInput = 0x694;
    constexpr uintptr_t Car_Throttle = 0x1DF8;
    constexpr uintptr_t Car_NitroDrainRate = 0x1DFC;
    constexpr uintptr_t Car_BrakeInput = 0x1E00;
    
    // Engine Config (pointed by Car + 0x98)
    constexpr uintptr_t EngineConfig_NitroStorage = 0x20;
    constexpr uintptr_t EngineConfig_NitroAcceleration = 0x24;
    constexpr uintptr_t EngineConfig_TurboAcceleration = 0x28;
    
    // PlayerProfile
    constexpr uintptr_t Profile_NumCars = 0x3C8;
    constexpr uintptr_t Profile_CarClass = 0x3D0;
    constexpr uintptr_t Profile_ProfileName = 0xE34;
    constexpr uintptr_t Profile_Money = 0xE58;
    constexpr uintptr_t Profile_CareerCar = 0xE5E;
    
    // PlayerHost
    constexpr uintptr_t Host_VTable = 0x0;
    constexpr uintptr_t Host_PlayersArray = 0x14;
    constexpr uintptr_t Host_LocalPlayerPtr = 0x20;
    constexpr uintptr_t Host_IsPaused = 0x2C;
    constexpr uintptr_t Host_NumCars = 0x988;
    constexpr uintptr_t Host_LevelId = 0x2086C;
    constexpr uintptr_t Host_Timer = 0x2087C;
    
    // RaceInfo
    constexpr uintptr_t RaceInfo_SessionType = 0x458;
    constexpr uintptr_t RaceInfo_GameMode = 0x464;
    constexpr uintptr_t RaceInfo_LevelId = 0x480;
    constexpr uintptr_t RaceInfo_NumLaps = 0x488;
    constexpr uintptr_t RaceInfo_GameRules = 0x4AC;
    constexpr uintptr_t RaceInfo_HostObject = 0x9B8;
    constexpr uintptr_t RaceInfo_MenuInterface = 0x9C8;
    constexpr uintptr_t RaceInfo_PlayerProfile = 0xFE0;
}

// Session types
enum class SessionType : uint32_t {
    None = 0,
    Menu = 1,
    Race = 2,
};

enum class GameMode : uint32_t {
    None = 0,
    Career = 1,
    TimeAttack = 2,
    SingleRace = 3,
    InstantAction = 4,
    CrashTestDummy = 5,
    OnlineMultiplayer = 6,
    PartyMode = 7,
    Developer = 8,
    Tournament = 9,
    Splitscreen = 10,
    Test = 11,
};

enum class GameRules : uint32_t {
    Default = 0,
    Race = 1,
    Stunt = 2,
    Derby = 3,
    HunterPrey = 4,
    ArcadeAdventure = 5,
    PongRace = 6,
    TimeAttack = 7,
    Test = 8,
};

// Helper functions
inline const char* SessionTypeToStr(SessionType t) {
    switch(t) {
        case SessionType::Menu: return "In Menu";
        case SessionType::Race: return "In Race";
        default: return "Unknown";
    }
}

// Cheat state storage (shared between cheats)
namespace CheatState {
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
    };
    extern OriginalValues g_originals;
    
    struct EnabledCheats {
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
        bool infiniteJumps = false;
        bool superGrip = false;
        bool driftMode = false;
    };
    extern EnabledCheats g_enabled;
    
    struct HotkeyStates {
        bool godMode = false;
        bool infiniteNitro = false;
        bool oneHitKill = false;
        bool maxMoney = false;
        bool alwaysFirst = false;
        bool freezeAI = false;
        bool speedHack = false;
        bool unlockAll = false;
    };
    extern HotkeyStates g_hotkeys;
}

#endif

inline const char* GameModeToStr(GameMode m) {
    switch(m) {
        case GameMode::Career: return "Career";
        case GameMode::TimeAttack: return "Time Attack";
        case GameMode::SingleRace: return "Single Race";
        case GameMode::InstantAction: return "Instant Action";
        case GameMode::CrashTestDummy: return "Crash Test";
        case GameMode::OnlineMultiplayer: return "Online MP";
        case GameMode::PartyMode: return "Party";
        case GameMode::Developer: return "Developer";
        case GameMode::Tournament: return "Tournament";
        case GameMode::Splitscreen: return "Splitscreen";
        case GameMode::Test: return "Test";
        default: return "Unknown";
    }
}

inline const char* GameRulesToStr(GameRules r) {
    switch(r) {
        case GameRules::Race: return "Race";
        case GameRules::Stunt: return "Stunt";
        case GameRules::Derby: return "Derby";
        case GameRules::HunterPrey: return "Hunter/Prey";
        case GameRules::ArcadeAdventure: return "Arcade Adventure";
        case GameRules::PongRace: return "Pong Race";
        case GameRules::TimeAttack: return "Time Attack";
        case GameRules::Test: return "Test";
        default: return "Race";
    }
}