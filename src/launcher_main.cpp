// FlatOut 2 Trainer Launcher
// Finds Steam install, launches game, injects DLL

#include "trainer_common.h"
#include <shellapi.h>
#include <shlobj.h>
#include <regex>

namespace Launcher {

std::filesystem::path g_gamePath;
std::filesystem::path g_dllPath;
std::filesystem::path g_steamPath;
DWORD g_processId = 0;
HANDLE g_processHandle = nullptr;

bool FindSteamInstall() {
    // Check registry for Steam path
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char path[MAX_PATH];
        DWORD size = sizeof(path);
        if (RegQueryValueExA(hKey, "SteamPath", nullptr, nullptr, (LPBYTE)path, &size) == ERROR_SUCCESS) {
            g_steamPath = path;
            RegCloseKey(hKey);
            return true;
        }
        RegCloseKey(hKey);
    }
    
    // Check 64-bit registry
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wow6432Node\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char path[MAX_PATH];
        DWORD size = sizeof(path);
        if (RegQueryValueExA(hKey, "SteamPath", nullptr, nullptr, (LPBYTE)path, &size) == ERROR_SUCCESS) {
            g_steamPath = path;
            RegCloseKey(hKey);
            return true;
        }
        RegCloseKey(hKey);
    }
    
    // Common default locations
    std::vector<std::filesystem::path> defaultPaths = {
        "C:\\Program Files (x86)\\Steam",
        "C:\\Program Files\\Steam",
        "D:\\Steam",
        "E:\\Steam"
    };
    
    for (const auto& p : defaultPaths) {
        if (std::filesystem::exists(p / "steam.exe")) {
            g_steamPath = p;
            return true;
        }
    }
    
    return false;
}

bool FindGameInstall() {
    if (g_steamPath.empty() && !FindSteamInstall()) {
        LOG_ERROR("Steam not found");
        return false;
    }
    
    // Check libraryfolders.vdf for all library locations
    std::filesystem::path libraryFolders = g_steamPath / "steamapps" / "libraryfolders.vdf";
    std::vector<std::filesystem::path> libraryPaths = { g_steamPath };
    
    if (std::filesystem::exists(libraryFolders)) {
        std::ifstream file(libraryFolders);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        
        // Simple parsing for "path" entries
        std::regex pathRegex("\"path\"\\s+\"([^\"]+)\"");
        std::smatch match;
        std::string::const_iterator searchStart(content.cbegin());
        while (std::regex_search(searchStart, content.cend(), match, pathRegex)) {
            std::filesystem::path libPath = match[1].str();
            // Fix escaped backslashes
            std::string pathStr = libPath.string();
            std::replace(pathStr.begin(), pathStr.end(), '/', '\\');
            libraryPaths.push_back(pathStr);
            searchStart = match.suffix().first;
        }
    }
    
    // FlatOut 2 AppID: 2990
    for (const auto& lib : libraryPaths) {
        std::filesystem::path gamePath = lib / "steamapps" / "common" / "FlatOut 2";
        std::filesystem::path exePath = gamePath / TARGET_PROCESS_NAME;
        
        if (std::filesystem::exists(exePath)) {
            g_gamePath = exePath;
            LOG_INFO("Found game at: %s", g_gamePath.string().c_str());
            return true;
        }
    }
    
    LOG_ERROR("FlatOut 2 not found in Steam libraries");
    return false;
}

bool FindDLL() {
    // Look for DLL next to launcher
    std::filesystem::path exePath = std::filesystem::path(GetModuleFileNameA(nullptr, nullptr, 0));
    g_dllPath = exePath.parent_path() / "FlatOut2TrainerDLL.dll";
    
    if (!std::filesystem::exists(g_dllPath)) {
        // Try build directory
        g_dllPath = std::filesystem::current_path() / "build" / "bin" / "FlatOut2TrainerDLL.dll";
    }
    
    if (!std::filesystem::exists(g_dllPath)) {
        // Try relative to game
        g_dllPath = g_gamePath.parent_path() / "FlatOut2TrainerDLL.dll";
    }
    
    if (std::filesystem::exists(g_dllPath)) {
        LOG_INFO("Found DLL at: %s", g_dllPath.string().c_str());
        return true;
    }
    
    LOG_ERROR("Trainer DLL not found");
    return false;
}

bool LaunchGame(bool suspended = true) {
    if (g_gamePath.empty()) return false;
    
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    DWORD flags = suspended ? CREATE_SUSPENDED : 0;
    std::string cmdLine = "\"" + g_gamePath.string() + "\"";
    
    if (!CreateProcessA(
            g_gamePath.string().c_str(),
            cmdLine.data(),
            nullptr, nullptr, FALSE,
            flags,
            nullptr,
            g_gamePath.parent_path().string().c_str(),
            &si, &pi)) {
        LOG_ERROR("CreateProcess failed: %lu", GetLastError());
        return false;
    }
    
    g_processId = pi.dwProcessId;
    g_processHandle = pi.hProcess;
    CloseHandle(pi.hThread);
    
    LOG_INFO("Game launched: PID=%lu", g_processId);
    return true;
}

bool InjectDLL() {
    if (!g_processHandle || !std::filesystem::exists(g_dllPath)) return false;
    
    // Allocate memory in target process for DLL path
    std::string dllPathStr = g_dllPath.string();
    size_t pathSize = dllPathStr.size() + 1;
    
    LPVOID remotePath = VirtualAllocEx(g_processHandle, nullptr, pathSize, 
                                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        LOG_ERROR("VirtualAllocEx failed: %lu", GetLastError());
        return false;
    }
    
    if (!WriteProcessMemory(g_processHandle, remotePath, dllPathStr.c_str(), pathSize, nullptr)) {
        LOG_ERROR("WriteProcessMemory failed: %lu", GetLastError());
        VirtualFreeEx(g_processHandle, remotePath, 0, MEM_RELEASE);
        return false;
    }
    
    // Get LoadLibraryA address
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    LPVOID loadLibraryAddr = GetProcAddress(hKernel32, "LoadLibraryA");
    if (!loadLibraryAddr) {
        LOG_ERROR("GetProcAddress failed");
        VirtualFreeEx(g_processHandle, remotePath, 0, MEM_RELEASE);
        return false;
    }
    
    // Create remote thread
    HANDLE hThread = CreateRemoteThread(g_processHandle, nullptr, 0,
                                        (LPTHREAD_START_ROUTINE)loadLibraryAddr,
                                        remotePath, 0, nullptr);
    if (!hThread) {
        LOG_ERROR("CreateRemoteThread failed: %lu", GetLastError());
        VirtualFreeEx(g_processHandle, remotePath, 0, MEM_RELEASE);
        return false;
    }
    
    // Wait for injection to complete
    WaitForSingleObject(hThread, 5000);
    
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    
    // Free allocated memory
    VirtualFreeEx(g_processHandle, remotePath, 0, MEM_RELEASE);
    
    if (exitCode == 0) {
        LOG_ERROR("DLL injection failed (LoadLibrary returned 0)");
        return false;
    }
    
    LOG_INFO("DLL injected successfully (base=0x%p)", (void*)exitCode);
    
    // Resume main thread if suspended
    // Note: We suspended the main thread, but CreateRemoteThread runs on a new thread
    // The game's main thread is still suspended. We need to resume it.
    // Actually, we suspended the process, so all threads are suspended.
    // We need to find and resume the main thread.
    
    return true;
}

bool ResumeGame() {
    if (!g_processHandle) return false;
    
    // Enumerate threads and resume the main thread
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;
    
    THREADENTRY32 te = { sizeof(te) };
    bool resumed = false;
    
    if (Thread32First(hSnapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == g_processId) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (hThread) {
                    DWORD suspendCount = ResumeThread(hThread);
                    if (suspendCount != (DWORD)-1) {
                        resumed = true;
                        LOG_INFO("Resumed thread %lu (suspend count: %lu)", te.th32ThreadID, suspendCount);
                    }
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(hSnapshot, &te));
    }
    
    CloseHandle(hSnapshot);
    return resumed;
}

bool WaitForTrainerInit(int timeoutMs = 10000) {
    // Could use named pipe or shared memory to communicate with injected DLL
    // For now, just wait
    Sleep(2000);
    return true;
}

void MonitorGame() {
    if (!g_processHandle) return;
    
    LOG_INFO("Monitoring game process...");
    
    while (true) {
        DWORD result = WaitForSingleObject(g_processHandle, 1000);
        if (result == WAIT_OBJECT_0) {
            LOG_INFO("Game process exited");
            break;
        }
        // Check for trainer messages, hotkeys, etc.
    }
    
    CloseHandle(g_processHandle);
    g_processHandle = nullptr;
}

int Run() {
    LOG_INFO("=== %s Launcher v%s ===", TRAINER_NAME, TRAINER_VERSION_STRING);
    
    // Find game
    if (!FindGameInstall()) {
        MessageBoxA(nullptr, "FlatOut 2 not found!\nMake sure it's installed via Steam.", 
                    "Error", MB_ICONERROR);
        return 1;
    }
    
    // Find DLL
    if (!FindDLL()) {
        MessageBoxA(nullptr, "Trainer DLL not found!\nPlace FlatOut2TrainerDLL.dll next to this launcher.", 
                    "Error", MB_ICONERROR);
        return 1;
    }
    
    // Launch game suspended
    if (!LaunchGame(true)) {
        MessageBoxA(nullptr, "Failed to launch game!", "Error", MB_ICONERROR);
        return 1;
    }
    
    // Inject DLL
    if (!InjectDLL()) {
        MessageBoxA(nullptr, "Failed to inject trainer DLL!", "Error", MB_ICONERROR);
        TerminateProcess(g_processHandle, 1);
        return 1;
    }
    
    // Resume game
    if (!ResumeGame()) {
        LOG_WARN("Failed to resume game threads");
    }
    
    // Wait for trainer to initialize
    WaitForTrainerInit();
    
    // Monitor
    MonitorGame();
    
    LOG_INFO("Launcher exiting");
    return 0;
}

} // namespace Launcher

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Alloc console
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    SetConsoleTitleA("FlatOut 2 Trainer Launcher");
    
    // Set up log file
    std::filesystem::path logPath = std::filesystem::current_path() / "FlatOut2Trainer_Launcher.log";
    Logger::Instance().SetLogFile(logPath);
    
    int result = Launcher::Run();
    
    FreeConsole();
    return result;
}