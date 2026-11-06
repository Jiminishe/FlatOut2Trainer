# FlatOut 2 Trainer

A memory-based trainer for **FlatOut 2 (Steam version)** with version-resilient pattern scanning, clean uninject, and extensible architecture.

## Features

### Core Cheats
- **God Mode** (F1) - Infinite health, car never gets damaged
- **Infinite Nitro** (F2) - Nitro never depletes
- **Infinite Boost** - Maximum nitro acceleration
- **One Hit Kill** (F3) - Destroy opponents on contact
- **Max Money** (F4) - 99,999,999 credits
- **Always 1st Place** (F5) - Force first position
- **Freeze AI** (F6) - AI opponents stop moving
- **Speed Hack** (F7) - Game speed multiplier (0.1x - 5x)
- **Unlock All** (F8) - Unlock all cars and tracks

### Advanced Cheats
- **No Visual Damage** - Car looks pristine but physics still work
- **Stunt Score Multiplier** - Multiply stunt mode scores (1x - 100x)
- **Super Grip** - Maximum tire grip, no sliding
- **Drift Mode** - Easy drift handling for stunts
- **Instant Win** - Win race immediately
- **No Traffic** - Remove traffic cars
- **Infinite Jumps** - Unlimited air control in stunt mode

### Technical Features
- **Pattern Scanning** - Works across game updates (Steam 2024/2025 updates)
- **Clean Uninject** - Restores all original values on exit
- **ImGui Interface** - Modern docking UI with categories
- **Hotkey Support** - Toggle cheats with customizable keys
- **Config Persistence** - Saves settings to INI file
- **Logger** - Built-in debug console and in-game log viewer
- **No Original File Modification** - Pure memory injection

## Requirements

- **Windows 10/11** (x64)
- **FlatOut 2 Steam** (AppID: 2990)
- **Visual C++ Redistributable 2015-2022** (x86)
- Game must be run in **32-bit mode** (default)

## Installation

### Option 1: Pre-built Release (Recommended)
1. Download latest release from Releases page
2. Extract to any folder
3. Run `FlatOut2Trainer.exe` as Administrator
4. Click "Launch Game" - trainer injects automatically

### Option 2: Build from Source
```powershell
# Requires: Visual Studio 2022 + CMake 3.20+
git clone https://github.com/your-repo/FlatOut2Trainer
cd FlatOut2Trainer
.\build.ps1 -Configuration Release
```

Output:
- `build/bin/Release/FlatOut2Trainer.exe` - Launcher
- `build/bin/Release/FlatOut2TrainerDLL.dll` - Trainer DLL

## Usage

### Launcher Method (Easiest)
1. Run `FlatOut2Trainer.exe` as Administrator
2. Trainer auto-detects Steam installation
3. Click "Launch Game" - game starts suspended
4. DLL injects, game resumes
5. Press **INSERT** to open menu in-game

### Manual Injection
If you prefer your own injector:
1. Start FlatOut 2 normally
2. Inject `FlatOut2TrainerDLL.dll` using any injector (Extreme Injector, GH Injector, etc.)
3. Press **INSERT** to open menu

## Default Hotkeys

| Key | Cheat |
|-----|-------|
| INSERT | Toggle Menu |
| F1 | God Mode |
| F2 | Infinite Nitro |
| F3 | One Hit Kill |
| F4 | Max Money |
| F5 | Always 1st Place |
| F6 | Freeze AI |
| F7 | Speed Hack |
| F8 | Unlock All |

*Hotkeys can be customized in the Settings tab*

## Menu Interface

The trainer features a modern **ImGui docking interface**:

- **Cheats Tab** - Categorized cheat list with toggles and sliders
- **Logger Tab** - Real-time debug log with filtering
- **Settings Tab** - Hotkey configuration, injection options
- **About Tab** - Version info and credits

### In-Game Menu
- Press **INSERT** to toggle
- Menu pauses game input when open
- Mouse cursor appears for navigation
- All changes apply immediately

## Compatibility

| Game Version | Status | Notes |
|--------------|--------|-------|
| Steam Original (1.0) | ✅ Tested | Pattern scanning works |
| Steam v1.2 Patch | ✅ Tested | Known addresses verified |
| Steam 2024 Update (Jul) | ✅ Tested | Build 15008640 |
| Steam 2025 Update (Jun) | ✅ Tested | Build 18764761 |
| GOG Version | ⚠️ Untested | May need different patterns |
| Epic Games Store | ⚠️ Untested | May need different patterns |
| Zoom Platform | ⚠️ Untested | Has exclusive fixes |

## How It Works

### Pattern Scanning
Instead of hardcoded addresses, the trainer uses **byte pattern signatures** to find:
- Global pointers (RaceInfo, LinkSession, PlayerHost)
- Function entry points (Vehicle physics, nitro logic)
- Global variables (RagdollingGain, RegenerationRate)

This makes it **resilient to game updates** - patterns survive recompilation.

### Memory Hooking
- **DirectX 9 Present/Reset hook** - For ImGui rendering
- **VTable hooks** - For game function interception
- **Trampoline hooks** - For code detouring with original preservation

### Clean Restoration
On exit/uninject:
1. All cheats disabled → original values restored
2. DirectX hooks removed
3. Window procedure restored
4. Config saved
5. Console freed

## Extending Cheats

Add new cheats by creating a class inheriting from `Cheat::Base`:

```cpp
class MyCheat : public Cheat::Base {
    const char* GetName() const override { return "MyCheat"; }
    const char* GetDescription() const override { return "Description"; }
    
    void OnEnable() override { /* Setup */ }
    void OnDisable() override { /* Restore */ }
    void OnFrame() override { /* Per-frame logic */ }
    void OnMenu() override { /* ImGui UI */ }
    
    bool IsEnabled() const override { return m_enabled; }
    void SetEnabled(bool e) override { e ? OnEnable() : OnDisable(); }
    int GetHotkey() const override { return Config::Manager::Instance().GetHotkey("MyCheat"); }
    void SetHotkey(int vk) override { Config::Manager::Instance().SetHotkey("MyCheat", vk); }
    
private:
    bool m_enabled = false;
};

// Register in cheats.cpp:
mgr.Register(std::make_unique<MyCheat>());
```

## Troubleshooting

### "Game not found"
- Ensure FlatOut 2 is installed via Steam
- Check `libraryfolders.vdf` for custom library locations
- Run launcher as Administrator

### "Injection failed"
- Disable antivirus temporarily
- Run as Administrator
- Ensure game is 32-bit (not running through 64-bit wrapper)

### "Crash on inject"
- Verify game version matches supported versions
- Check console output for pattern scan failures
- Try manual injection with Extreme Injector

### "Menu doesn't open"
- Press INSERT key
- Check if ImGui initialized (see console)
- Try ALT+TAB to refocus

### "Cheats don't work"
- Ensure you're in a race (not menus)
- Check Logger tab for errors
- Some cheats only work in specific modes (Career, Single Race, Stunt)

## Credits

- **FlatOut2.SDK** by [Sewer56](https://github.com/Sewer56/FlatOut2.SDK) - Structure definitions
- **Reverse Engineering** by Zolika1351, Dread_Pony_Roberts, elliotttate
- **ImGui** by Omar Cornut
- **Pattern Scanning** techniques from Game Hacking community

## License

MIT License - See LICENSE file for details.

## Disclaimer

**For single-player/offline use only.** Using trainers in online multiplayer may result in bans. This tool modifies game memory at runtime and does not modify game files. Use at your own risk.

---

### Building for Distribution

```powershell
# Release build with optimizations
.\build.ps1 -Configuration Release

# Creates:
# build/bin/Release/FlatOut2Trainer.exe     (~200 KB)
# build/bin/Release/FlatOut2TrainerDLL.dll  (~500 KB)
```

### Debug Build

```powershell
.\build.ps1 -Configuration Debug
# Includes debug symbols, console output, extra logging
```