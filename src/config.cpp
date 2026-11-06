#include "trainer_common.h"
#include <fstream>
#include <sstream>
#include <shlobj.h>

namespace Config {

CheatConfig& Manager::GetCheat(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cheats[name];  // Creates if not exists
}

const CheatConfig& Manager::GetCheat(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cheats.find(name);
    static CheatConfig empty;
    return it != m_cheats.end() ? it->second : empty;
}

bool Manager::IsEnabled(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cheats.find(name);
    return it != m_cheats.end() && it->second.enabled;
}

void Manager::SetEnabled(const std::string& name, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cheats[name].enabled = enabled;
}

int Manager::GetHotkey(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cheats.find(name);
    return it != m_cheats.end() ? it->second.hotkey : 0;
}

void Manager::SetHotkey(const std::string& name, int vk) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cheats[name].hotkey = vk;
}

template<typename T>
T Manager::GetValue(const std::string& cheat, const std::string& key, T defaultValue) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cheats.find(cheat);
    if (it == m_cheats.end()) return defaultValue;
    auto vit = it->second.values.find(key);
    if (vit == it->second.values.end()) return defaultValue;
    
    if constexpr (std::is_same_v<T, bool>) {
        return vit->second == "1" || vit->second == "true";
    } else if constexpr (std::is_same_v<T, int>) {
        return std::stoi(vit->second);
    } else if constexpr (std::is_same_v<T, float>) {
        return std::stof(vit->second);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return vit->second;
    }
    return defaultValue;
}

template<typename T>
void Manager::SetValue(const std::string& cheat, const std::string& key, const T& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if constexpr (std::is_same_v<T, bool>) {
        m_cheats[cheat].values[key] = value ? "1" : "0";
    } else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float>) {
        m_cheats[cheat].values[key] = std::to_string(value);
    } else if constexpr (std::is_same_v<T, std::string>) {
        m_cheats[cheat].values[key] = value;
    }
}

// Explicit instantiations
template bool Manager::GetValue<bool>(const std::string&, const std::string&, bool) const;
template int Manager::GetValue<int>(const std::string&, const std::string&, int) const;
template float Manager::GetValue<float>(const std::string&, const std::string&, float) const;
template std::string Manager::GetValue<std::string>(const std::string&, const std::string&, std::string) const;
template void Manager::SetValue<bool>(const std::string&, const std::string&, const bool&);
template void Manager::SetValue<int>(const std::string&, const std::string&, const int&);
template void Manager::SetValue<float>(const std::string&, const std::string&, const float&);
template void Manager::SetValue<std::string>(const std::string&, const std::string&, const std::string&);

bool Manager::Load(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_configPath = path;
    
    if (!std::filesystem::exists(path)) {
        LOG_INFO("Config file not found: %s", path.string().c_str());
        return false;
    }
    
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open config: %s", path.string().c_str());
        return false;
    }
    
    std::string line;
    std::string currentCheat;
    while (std::getline(file, line)) {
        // Trim
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        
        if (line[0] == '[' && line.back() == ']') {
            currentCheat = line.substr(1, line.size() - 2);
            continue;
        }
        
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;
        
        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);
        
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        if (key == "enabled") {
            m_cheats[currentCheat].enabled = (value == "1" || value == "true");
        } else if (key == "hotkey") {
            m_cheats[currentCheat].hotkey = std::stoi(value);
        } else {
            m_cheats[currentCheat].values[key] = value;
        }
    }
    
    LOG_INFO("Config loaded from %s", path.string().c_str());
    return true;
}

bool Manager::Save(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to save config: %s", path.string().c_str());
        return false;
    }
    
    file << "# " << TRAINER_NAME << " Configuration\n";
    file << "# Version: " << TRAINER_VERSION_STRING << "\n\n";
    
    for (const auto& [name, config] : m_cheats) {
        file << "[" << name << "]\n";
        file << "enabled=" << (config.enabled ? "1" : "0") << "\n";
        file << "hotkey=" << config.hotkey << "\n";
        for (const auto& [key, value] : config.values) {
            file << key << "=" << value << "\n";
        }
        file << "\n";
    }
    
    LOG_INFO("Config saved to %s", path.string().c_str());
    return true;
}

Manager& Manager::Instance() {
    static Manager instance;
    return instance;
}

} // namespace Config