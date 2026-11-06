// Advanced injection techniques

#include "trainer_common.h"
#include <tlhelp32.h>

namespace Injector {

enum class Method {
    CreateRemoteThread,
    ThreadHijacking,
    QueueUserAPC,
    SetWindowsHookEx,
    ManualMap
};

struct InjectionResult {
    bool success = false;
    DWORD errorCode = 0;
    uintptr_t moduleBase = 0;
    std::string errorMessage;
};

InjectionResult InjectCreateRemoteThread(DWORD pid, const std::filesystem::path& dllPath) {
    InjectionResult result;
    
    HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | 
                                   PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                   FALSE, pid);
    if (!hProcess) {
        result.errorCode = GetLastError();
        result.errorMessage = "OpenProcess failed";
        return result;
    }
    
    std::string dllPathStr = dllPath.string();
    size_t pathSize = dllPathStr.size() + 1;
    
    LPVOID remotePath = VirtualAllocEx(hProcess, nullptr, pathSize, 
                                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        result.errorCode = GetLastError();
        result.errorMessage = "VirtualAllocEx failed";
        CloseHandle(hProcess);
        return result;
    }
    
    if (!WriteProcessMemory(hProcess, remotePath, dllPathStr.c_str(), pathSize, nullptr)) {
        result.errorCode = GetLastError();
        result.errorMessage = "WriteProcessMemory failed";
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return result;
    }
    
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    LPVOID loadLibraryAddr = GetProcAddress(hKernel32, "LoadLibraryA");
    if (!loadLibraryAddr) {
        result.errorCode = GetLastError();
        result.errorMessage = "GetProcAddress(LoadLibraryA) failed";
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return result;
    }
    
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
                                        (LPTHREAD_START_ROUTINE)loadLibraryAddr,
                                        remotePath, 0, nullptr);
    if (!hThread) {
        result.errorCode = GetLastError();
        result.errorMessage = "CreateRemoteThread failed";
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return result;
    }
    
    WaitForSingleObject(hThread, 10000);
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    
    if (exitCode == 0) {
        result.errorMessage = "LoadLibrary returned 0 (DLL entry point failed)";
        return result;
    }
    
    result.success = true;
    result.moduleBase = exitCode;
    return result;
}

InjectionResult InjectThreadHijacking(DWORD pid, const std::filesystem::path& dllPath) {
    InjectionResult result;
    result.errorMessage = "Thread hijacking not implemented";
    return result;
}

InjectionResult InjectQueueUserAPC(DWORD pid, const std::filesystem::path& dllPath) {
    InjectionResult result;
    result.errorMessage = "QueueUserAPC not implemented";
    return result;
}

InjectionResult Inject(DWORD pid, const std::filesystem::path& dllPath, Method method = Method::CreateRemoteThread) {
    switch (method) {
        case Method::CreateRemoteThread:
            return InjectCreateRemoteThread(pid, dllPath);
        case Method::ThreadHijacking:
            return InjectThreadHijacking(pid, dllPath);
        case Method::QueueUserAPC:
            return InjectQueueUserAPC(pid, dllPath);
        default:
            result.errorMessage = "Unknown injection method";
            return InjectionResult{};
    }
}

bool Eject(DWORD pid, const char* dllName) {
    // Get module handle in target process
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;
    
    MODULEENTRY32 me = { sizeof(me) };
    HMODULE hMod = nullptr;
    
    if (Module32First(hSnapshot, &me)) {
        do {
            if (_stricmp(me.szModule, dllName) == 0) {
                hMod = me.hModule;
                break;
            }
        } while (Module32Next(hSnapshot, &me));
    }
    
    CloseHandle(hSnapshot);
    
    if (!hMod) return false;
    
    // Use CreateRemoteThread with FreeLibrary
    HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION, FALSE, pid);
    if (!hProcess) return false;
    
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    LPVOID freeLibraryAddr = GetProcAddress(hKernel32, "FreeLibrary");
    
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
                                        (LPTHREAD_START_ROUTINE)freeLibraryAddr,
                                        hMod, 0, nullptr);
    if (!hThread) {
        CloseHandle(hProcess);
        return false;
    }
    
    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    return true;
}

std::vector<DWORD> FindProcesses(const char* processName) {
    std::vector<DWORD> pids;
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return pids;
    
    PROCESSENTRY32 pe = { sizeof(pe) };
    if (Process32First(hSnapshot, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, processName) == 0) {
                pids.push_back(pe.th32ProcessID);
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    
    CloseHandle(hSnapshot);
    return pids;
}

bool IsProcess64Bit(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return false;
    
    BOOL isWow64 = FALSE;
    IsWow64Process(hProcess, &isWow64);
    CloseHandle(hProcess);
    
    return !isWow64;  // True if 64-bit
}

} // namespace Injector