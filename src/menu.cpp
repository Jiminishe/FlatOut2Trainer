#include "trainer_common.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"
#include <d3d9.h>
#include <vector>
#include <string>

namespace Menu {

static bool g_showMenu = true;
static bool g_initialized = false;
static HWND g_gameWindow = nullptr;
static WNDPROC g_originalWndProc = nullptr;
static IDirect3DDevice9* g_device = nullptr;
static std::vector<std::string> g_logBuffer;
static int g_logFilter = 0;  // 0=All, 1=Info, 2=Warn, 3=Error
static bool g_autoScroll = true;
static ImGuiWindowFlags g_windowFlags = 0;

LRESULT CALLBACK WndProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_showMenu && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }
    
    // Pass hotkeys to manager
    Hotkey::Manager::Instance().ProcessMessage(msg, wParam, lParam);
    
    return CallWindowProc(g_originalWndProc, hWnd, msg, wParam, lParam);
}

void Initialize(IDirect3DDevice9* device, HWND window) {
    if (g_initialized) return;
    
    g_device = device;
    g_gameWindow = window;
    
    // Hook window procedure
    g_originalWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProcHook);
    
    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "FlatOut2Trainer_imgui.ini";
    
    // Custom style
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.13f, 0.95f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.3f, 0.7f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.3f, 0.7f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.4f, 0.7f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.5f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.35f, 0.65f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.4f, 0.7f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.5f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.15f, 0.35f, 0.65f, 1.0f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.3f, 0.6f, 1.0f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.45f, 0.75f, 1.0f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.2f, 0.4f, 0.7f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.2f, 0.2f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.3f, 1.0f);
    
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX9_Init(device);
    
    g_initialized = true;
    LOG_INFO("ImGui initialized");
}

void Shutdown() {
    if (!g_initialized) return;
    
    // Restore window procedure
    if (g_gameWindow && g_originalWndProc) {
        SetWindowLongPtr(g_gameWindow, GWLP_WNDPROC, (LONG_PTR)g_originalWndProc);
    }
    
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    
    g_initialized = false;
    LOG_INFO("ImGui shutdown");
}

void NewFrame() {
    if (!g_initialized) return;
    
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    
    // Docking space
    ImGuiWindowFlags dockspaceFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                      ImGuiWindowFlags_NoNavFocus;
    
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("DockSpace", nullptr, dockspaceFlags);
    ImGui::PopStyleVar(3);
    
    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
}

void Render() {
    if (!g_initialized || !g_showMenu) return;
    
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void ToggleMenu() {
    g_showMenu = !g_showMenu;
    
    // Restore cursor when menu is open
    if (g_showMenu) {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDrawCursor = true;
        io.WantCaptureMouse = true;
        io.WantCaptureKeyboard = true;
    } else {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDrawCursor = false;
    }
}

bool IsMenuOpen() { return g_showMenu; }
void SetMenuOpen(bool open) { g_showMenu = open; }

void AddLog(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    g_logBuffer.push_back(buffer);
    if (g_logBuffer.size() > 1000) g_logBuffer.erase(g_logBuffer.begin());
}

void DrawMainMenu() {
    if (!g_showMenu) return;
    
    // Main window
    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin(TRAINER_NAME " v" TRAINER_VERSION_STRING, &g_showMenu, 
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse);
    
    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Config")) {
                Cheat::Manager::Instance().SaveConfig();
            }
            if (ImGui::MenuItem("Load Config")) {
                Cheat::Manager::Instance().LoadConfig();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Uninject / Exit")) {
                // Signal unload
                PostMessage(g_gameWindow, WM_CLOSE, 0, 0);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Cheats", nullptr, nullptr, true);
            ImGui::MenuItem("Logger", nullptr, nullptr, true);
            ImGui::MenuItem("Settings", nullptr, nullptr, true);
            ImGui::MenuItem("About", nullptr, nullptr, true);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Hotkeys")) {}
            if (ImGui::MenuItem("GitHub")) {}
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Tab bar
    if (ImGui::BeginTabBar("MainTabs")) {
        // CHEATS TAB
        if (ImGui::BeginTabItem("Cheats")) {
            DrawCheatsTab();
            ImGui::EndTabItem();
        }
        
        // LOGGER TAB
        if (ImGui::BeginTabItem("Logger")) {
            DrawLoggerTab();
            ImGui::EndTabItem();
        }
        
        // SETTINGS TAB
        if (ImGui::BeginTabItem("Settings")) {
            DrawSettingsTab();
            ImGui::EndTabItem();
        }
        
        // ABOUT TAB
        if (ImGui::BeginTabItem("About")) {
            DrawAboutTab();
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
    
    ImGui::End();
}

void DrawCheatsTab() {
    // Game status
    ImGui::BeginChild("GameStatus", ImVec2(0, 80), true);
    ImGui::Text("Game Status:");
    ImGui::SameLine();
    ImGui::TextColored(Game::IsInRace() ? ImVec4(0,1,0,1) : ImVec4(1,0.5f,0,1), 
                       Game::IsInRace() ? "IN RACE" : "IN MENU");
    ImGui::SameLine(200);
    ImGui::Text("Track: %s", Game::GetCurrentTrackName());
    ImGui::SameLine(400);
    ImGui::Text("Car ID: %d", Game::GetCurrentCarId());
    ImGui::Separator();
    ImGui::Text("Version: %s | Multiplayer: %s", 
                GameVersion::g_versionString.c_str(),
                Game::IsMultiplayer() ? "Yes" : "No");
    ImGui::EndChild();
    
    ImGui::Separator();
    
    // Cheat categories
    static const char* categories[] = {
        "Player", "Vehicle", "Race", "AI", "Career", "Stunt", "Experimental"
    };
    static int currentCategory = 0;
    
    ImGui::BeginChild("Categories", ImVec2(150, 0), true);
    for (int i = 0; i < IM_ARRAYSIZE(categories); ++i) {
        if (ImGui::Selectable(categories[i], currentCategory == i)) {
            currentCategory = i;
        }
    }
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    ImGui::BeginChild("CheatsList", ImVec2(0, 0), true);
    
    // Filter cheats by category (simplified - all shown for now)
    Cheat::Manager::Instance().RenderMenu();
    
    ImGui::EndChild();
}

void DrawLoggerTab() {
    ImGui::BeginChild("LogFilter", ImVec2(0, 30), false);
    ImGui::RadioButton("All", &g_logFilter, 0); ImGui::SameLine();
    ImGui::RadioButton("Info", &g_logFilter, 1); ImGui::SameLine();
    ImGui::RadioButton("Warn", &g_logFilter, 2); ImGui::SameLine();
    ImGui::RadioButton("Error", &g_logFilter, 3); ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &g_autoScroll); ImGui::SameLine();
    if (ImGui::Button("Clear")) g_logBuffer.clear();
    ImGui::EndChild();
    
    ImGui::BeginChild("LogOutput", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& line : g_logBuffer) {
        // Simple filtering (would need log level storage for real filtering)
        ImGui::TextUnformatted(line.c_str());
    }
    if (g_autoScroll) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
}

void DrawSettingsTab() {
    ImGui::Text("Trainer Settings");
    ImGui::Separator();
    
    static bool autoSave = true;
    ImGui::Checkbox("Auto-save config on change", &autoSave);
    
    static bool showHidden = false;
    ImGui::Checkbox("Show experimental cheats", &showHidden);
    
    ImGui::Separator();
    ImGui::Text("Hotkey Configuration");
    ImGui::Separator();
    
    // Hotkey editor would go here
    ImGui::Text("Click a cheat in the Cheats tab to configure its hotkey.");
    
    ImGui::Separator();
    ImGui::Text("Injection Settings");
    ImGui::Separator();
    
    static bool uninjectOnClose = true;
    ImGui::Checkbox("Clean uninject on game close", &uninjectOnClose);
    
    static bool verifySignatures = true;
    ImGui::Checkbox("Verify patterns on startup", &verifySignatures);
}

void DrawAboutTab() {
    ImGui::Text(TRAINER_NAME " v" TRAINER_VERSION_STRING);
    ImGui::Separator();
    ImGui::Text("A memory-based trainer for FlatOut 2 (Steam)");
    ImGui::Text("Built with ImGui and custom pattern scanning");
    ImGui::Separator();
    ImGui::Text("Features:");
    ImGui::BulletText("Version-resilient pattern scanning");
    ImGui::BulletText("Clean uninject - restores all values");
    ImGui::BulletText("Extensible cheat framework");
    ImGui::BulletText("ImGui docking interface");
    ImGui::Separator();
    ImGui::Text("Credits:");
    ImGui::BulletText("FlatOut2.SDK by Sewer56 (structure definitions)");
    ImGui::BulletText("Reverse engineering by Zolika1351, Dread_Pony_Roberts");
    ImGui::BulletText("ImGui by Omar Cornut");
    ImGui::Separator();
    if (ImGui::Button("Check for Updates")) {
        // Would open browser
    }
}

// Override Logger to also send to UI
void Logger::Log(LogLevel level, const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Prefix with level
    const char* prefix = "";
    switch(level) {
        case LogLevel::Debug: prefix = "[DEBUG] "; break;
        case LogLevel::Info:  prefix = "[INFO]  "; break;
        case LogLevel::Warn:  prefix = "[WARN]  "; break;
        case LogLevel::Error: prefix = "[ERROR] "; break;
    }
    
    std::string fullMsg = prefix + std::string(buffer);
    
    // Console output
    if (m_consoleOutput) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        WORD color = 7;
        switch(level) {
            case LogLevel::Debug: color = 8; break;
            case LogLevel::Info:  color = 7; break;
            case LogLevel::Warn:  color = 14; break;
            case LogLevel::Error: color = 12; break;
        }
        SetConsoleTextAttribute(hConsole, color);
        printf("%s\n", fullMsg.c_str());
        SetConsoleTextAttribute(hConsole, 7);
    }
    
    // File output
    if (m_logFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", std::localtime(&time));
        m_logFile << "[" << timeStr << "] " << fullMsg << std::endl;
        m_logFile.flush();
    }
    
    // UI log buffer
    Menu::AddLog("%s", fullMsg.c_str());
}

void Logger::SetLogFile(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logFile.is_open()) m_logFile.close();
    m_logFile.open(path, std::ios::app);
    if (m_logFile.is_open()) {
        m_logFile << "\n=== " << TRAINER_NAME << " Session Started ===\n";
    }
}

} // namespace Menu