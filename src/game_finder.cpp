// Game finder - locates FlatOut 2 installation

#include "trainer_common.h"
#include <fstream>
#include <regex>

namespace GameFinder {

struct GameInstall {
    std::filesystem::path executable;
    std::filesystem::path root;
    std::string version;
    bool isSteam = false;
    std::string appId = "2990";
};

std::vector<GameInstall> FindAllInstallations() {
    std::vector<GameInstall> installs;
    
    // 1. Check Steam default locations
    std::vector<std::filesystem::path> steamPaths = GetSteamLibraryPaths();
    
    for (const auto& steamPath : steamPaths) {
        std::filesystem::path gamePath = steamPath / "steamapps" / "common" / "FlatOut 2";
        std::filesystem::path exePath = gamePath / "FlatOut2.exe";
        
        if (std::filesystem::exists(exePath)) {
            GameInstall install;
            install.executable = exePath;
            install.root = gamePath;
            install.isSteam = true;
            install.version = DetectVersion(exePath);
            installs.push_back(install);
        }
    }
    
    // 2. Check registry for non-Steam installs
    CheckRegistryInstalls(installs);
    
    // 3. Check common directories
    CheckCommonDirectories(installs);
    
    return installs;
}

std::vector<std::filesystem::path> GetSteamLibraryPaths() {
    std::vector<std::filesystem::path> paths;
    
    // Get Steam install path from registry
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char path[MAX_PATH];
        DWORD size = sizeof(path);
        if (RegQueryValueExA(hKey, "SteamPath", nullptr, nullptr, (LPBYTE)path, &size) == ERROR_SUCCESS) {
            paths.push_back(path);
        }
        RegCloseKey(hKey);
    }
    
    // Also check HKLM
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wow6432Node\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char path[MAX_PATH];
        DWORD size = sizeof(path);
        if (RegQueryValueExA(hKey, "SteamPath", nullptr, nullptr, (LPBYTE)path, &size) == ERROR_SUCCESS) {
            paths.push_back(path);
        }
        RegCloseKey(hKey);
    }
    
    // Parse libraryfolders.vdf for additional libraries
    for (auto it = paths.begin(); it != paths.end(); ++it) {
        std::filesystem::path vdfPath = *it / "steamapps" / "libraryfolders.vdf";
        if (std::filesystem::exists(vdfPath)) {
            ParseLibraryFolders(vdfPath, paths);
        }
    }
    
    return paths;
}

void ParseLibraryFolders(const std::filesystem::path& vdfPath, std::vector<std::filesystem::path>& paths) {
    std::ifstream file(vdfPath);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // VDF format is key-value pairs
    // Look for "path" "X:\\..."
    std::regex pathRegex("\"path\"\\s+\"([^\"]+)\"");
    std::smatch match;
    std::string::const_iterator searchStart(content.cbegin());
    
    while (std::regex_search(searchStart, content.cend(), match, pathRegex)) {
        std::string pathStr = match[1].str();
        // Unescape backslashes
        std::replace(pathStr.begin(), pathStr.end(), '/', '\\');
        std::string unescaped;
        for (size_t i = 0; i < pathStr.size(); ++i) {
            if (pathStr[i] == '\\' && i + 1 < pathStr.size() && pathStr[i+1] == '\\') {
                unescaped += '\\';
                ++i;
            } else {
                unescaped += pathStr[i];
            }
        }
        paths.push_back(unescaped);
        searchStart = match.suffix().first;
    }
}

void CheckRegistryInstalls(std::vector<GameInstall>& installs) {
    // Check common uninstall registry keys
    const char* regPaths[] = {
        "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"
    };
    
    for (const char* regPath : regPaths) {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD index = 0;
            char subKeyName[256];
            while (RegEnumKeyExA(hKey, index++, subKeyName, sizeof(subKeyName), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                HKEY hSubKey;
                if (RegOpenKeyExA(hKey, subKeyName, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                    char displayName[256];
                    DWORD size = sizeof(displayName);
                    if (RegQueryValueExA(hSubKey, "DisplayName", nullptr, nullptr, (LPBYTE)displayName, &size) == ERROR_SUCCESS) {
                        if (strstr(displayName, "FlatOut 2") || strstr(displayName, "FlatOut2")) {
                            char installPath[MAX_PATH];
                            size = sizeof(installPath);
                            if (RegQueryValueExA(hSubKey, "InstallLocation", nullptr, nullptr, (LPBYTE)installPath, &size) == ERROR_SUCCESS) {
                                std::filesystem::path exePath = std::filesystem::path(installPath) / "FlatOut2.exe";
                                if (std::filesystem::exists(exePath)) {
                                    GameInstall install;
                                    install.executable = exePath;
                                    install.root = installPath;
                                    install.isSteam = false;
                                    install.version = DetectVersion(exePath);
                                    installs.push_back(install);
                                }
                            }
                        }
                    }
                    RegCloseKey(hSubKey);
                }
            }
            RegCloseKey(hKey);
        }
    }
}

void CheckCommonDirectories(std::vector<GameInstall>& installs) {
    std::vector<std::filesystem::path> commonDirs = {
        "C:\\Games\\FlatOut 2",
        "D:\\Games\\FlatOut 2",
        "E:\\Games\\FlatOut 2",
        "C:\\FlatOut 2",
        "D:\\FlatOut 2"
    };
    
    for (const auto& dir : commonDirs) {
        std::filesystem::path exePath = dir / "FlatOut2.exe";
        if (std::filesystem::exists(exePath)) {
            GameInstall install;
            install.executable = exePath;
            install.root = dir;
            install.isSteam = false;
            install.version = DetectVersion(exePath);
            installs.push_back(install);
        }
    }
}

std::string DetectVersion(const std::filesystem::path& exePath) {
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeA(exePath.string().c_str(), &handle);
    if (size == 0) return "Unknown";
    
    std::vector<char> buffer(size);
    if (!GetFileVersionInfoA(exePath.string().c_str(), 0, size, buffer.data())) return "Unknown";
    
    VS_FIXEDFILEINFO* info = nullptr;
    UINT len = 0;
    if (VerQueryValueA(buffer.data(), "\\", (LPVOID*)&info, &len) && info) {
        return std::to_string(HIWORD(info->dwFileVersionMS)) + "." +
               std::to_string(LOWORD(info->dwFileVersionMS)) + "." +
               std::to_string(HIWORD(info->dwFileVersionLS)) + "." +
               std::to_string(LOWORD(info->dwFileVersionLS));
    }
    
    // Check file size for known versions
    std::filesystem::file_size(exePath);
    auto fileSize = std::filesystem::file_size(exePath);
    
    if (fileSize == 2990080) return "1.2 (v1.2 Patch)";
    if (fileSize == 3014656) return "2024 Update (build 15008640)";
    if (fileSize == 3080192) return "2025 Update (build 18764761)";
    
    return "Unknown (" + std::to_string(fileSize) + " bytes)";
}

bool ValidateExecutable(const std::filesystem::path& exePath) {
    // Check if it's the right game
    if (!std::filesystem::exists(exePath)) return false;
    
    // Check file size (FlatOut2.exe should be around 3MB)
    auto size = std::filesystem::file_size(exePath);
    if (size < 2000000 || size > 5000000) return false;
    
    // Could add more validation (PE header, imports, etc.)
    return true;
}

std::optional<GameInstall> FindBestInstallation() {
    auto installs = FindAllInstallations();
    if (installs.empty()) return std::nullopt;
    
    // Prefer Steam version, then latest version
    std::sort(installs.begin(), installs.end(), [](const GameInstall& a, const GameInstall& b) {
        if (a.isSteam != b.isSteam) return a.isSteam;  // Steam first
        return a.version > b.version;  // Newer version first
    });
    
    return installs[0];
}

} // namespace GameFinder