// Launcher utilities

#include "trainer_common.h"

namespace Launcher {

std::string GetKeyName(int vk) {
    if (vk == 0) return "None";
    char name[32];
    GetKeyNameTextA(MapVirtualKeyA(vk, MAPVK_VK_TO_VSC) << 16, name, sizeof(name));
    return name;
}

bool IsGameRunning() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;
    
    PROCESSENTRY32 pe = { sizeof(pe) };
    bool found = false;
    
    if (Process32First(hSnapshot, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, TARGET_PROCESS_NAME) == 0) {
                found = true;
                break;
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    
    CloseHandle(hSnapshot);
    return found;
}

DWORD FindGameProcessId() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;
    
    PROCESSENTRY32 pe = { sizeof(pe) };
    DWORD pid = 0;
    
    if (Process32First(hSnapshot, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, TARGET_PROCESS_NAME) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    
    CloseHandle(hSnapshot);
    return pid;
}

bool InjectIntoRunning(DWORD pid, const std::filesystem::path& dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) return false;
    
    std::string dllPathStr = dllPath.string();
    size_t pathSize = dllPathStr.size() + 1;
    
    LPVOID remotePath = VirtualAllocEx(hProcess, nullptr, pathSize, 
                                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        CloseHandle(hProcess);
        return false;
    }
    
    if (!WriteProcessMemory(hProcess, remotePath, dllPathStr.c_str(), pathSize, nullptr)) {
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    LPVOID loadLibraryAddr = GetProcAddress(hKernel32, "LoadLibraryA");
    
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
                                        (LPTHREAD_START_ROUTINE)loadLibraryAddr,
                                        remotePath, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    
    WaitForSingleObject(hThread, 5000);
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    
    return exitCode != 0;
}

void ShowError(const char* title, const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    MessageBoxA(nullptr, buffer, title, MB_ICONERROR);
}

void ShowInfo(const char* title, const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    MessageBoxA(nullptr, buffer, title, MB_ICONINFORMATION);
}

} // namespace Launcher