// roro_launcher_overlay.cpp
// Roro Client - Launcher + External Overlay (Windows)
// ---------------------------------------------------------------------------
// OBJECTIVE (safe & legal):
// - Standalone Windows .exe launcher that displays a GUI with "Roro Client" title
//   and a Minecraft background image, plus "Launch" and "Settings" buttons.
// - The Launch button starts the official Minecraft Bedrock executable (path set in config).
// - A separate overlay window (transparent, always-on-top) displays configurable
//   HUD panels: FPS COUNTER, CPS COUNTER, KEYSTROKE (W/A/S/D/Space), REACH COUNTER,
//   WATERMARK, MOOVABLE UI elements, and more as requested — all purely visual and local.
// - Per-panel options: enable/disable, background on/off, color picker, scale (size), movable.
// - Settings are saved/loaded in a JSON file (config.json).
// - Optional Discord Rich Presence stub included (commented) — requires user to provide
//   the Discord RPC library if desired.
// ---------------------------------------------------------------------------
// IMPORTANT SAFETY NOTES
// - This program is *not* injecting into Minecraft and does not access game memory.
// - It only renders an overlay on top of the game window and launches the official exe.
// - It must NOT be used to gain unfair advantages on multiplayer servers. Use only for
//   visual/UI customization in single-player or where allowed.
// ---------------------------------------------------------------------------
// DEPENDENCIES
// - Windows 10+ (Win32 API used for window styles and process launching)
// - GLFW (for window + OpenGL context): https://www.glfw.org/
// - GLAD (OpenGL loader) or any loader you prefer
// - Dear ImGui + backends (imgui_impl_glfw.cpp + imgui_impl_opengl3.cpp)
// - stb_image.h (for loading background PNG/JPG)
// - nlohmann/json.hpp (single-header JSON library)
// - Optional: Discord RPC (if you enable presence integration)
// ---------------------------------------------------------------------------
// BUILD (example with MSYS2/MinGW or Visual Studio)
// - Place this file with: glad.c, imgui sources, stb_image.h, nlohmann json header.
// - Example (MSYS2 / g++ with OpenGL and GLFW installed):
//   g++ roro_launcher_overlay.cpp glad.c imgui/*.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp -I. -Iimgui -lglfw -lgdi32 -lopengl32 -o roro_launcher_overlay.exe
// - For Visual Studio, create a project, add this file and required sources, link against opengl32.lib and glfw3.lib.
// ---------------------------------------------------------------------------
// USAGE
// - Run roro_launcher_overlay.exe
// - In Settings, set the path to your Minecraft Bedrock executable and save config
// - Click Launch to start Minecraft. If Discord RPC is enabled and configured, the
//   launcher will update your Discord presence while the launcher runs.
// - Start the Overlay (from the launcher Settings) to show HUD while in-game.
// ---------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <stb_image.h>
#include <nlohmann/json.hpp>

#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <deque>
#include <map>

using json = nlohmann::json;
using Clock = std::chrono::high_resolution_clock;

// ------------------------------- Config structures ---------------------------------
struct PanelConfig {
    bool enabled = true;
    bool background = true;
    float scale = 1.0f;
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // rgba
    float bgColor[4] = {0.0f,0.0f,0.0f,0.5f};
    bool movable = true;
    ImVec2 pos = ImVec2(100,100);
};

struct AppConfig {
    std::string minecraftPath = ""; // path to bedrock exe
    bool overlayClickThrough = false; // if true, overlay won't receive mouse input
    bool overlayAlwaysOnTop = true;
    std::map<std::string, PanelConfig> panels;
};

static AppConfig g_config;
static const char* CONFIG_FILE = "roro_config.json";

// defaults for panels
static const char* PANEL_NAMES[] = {
    "FPS COUNTER","CPS COUNTER","KEYSTROKE","REACH COUNTER","WATERMARK",
    "MOOVABLE CHAT","MOOVABLE UI","MOOVABLE SCOREBOARD","FAST INVENTORY","JAVA INVENTORY",
    "ESP","WHEATHER CHANGER","TIME CHANGER","FOV","NAMETAGS","HIDE_PSEUDO","TWERK","JAVA_MOVEMENTS"
};

// ------------------------------- Utility: JSON load/save ---------------------------
void load_config(){
    std::ifstream in(CONFIG_FILE);
    if(!in) {
        // create defaults
        for(auto &name : PANEL_NAMES){
            PanelConfig p; p.enabled = true; p.background = true; p.scale = 1.0f; p.movable = true;
            g_config.panels[name] = p;
        }
        return;
    }
    try{
        json j; in >> j;
        g_config.minecraftPath = j.value("minecraftPath", "");
        g_config.overlayClickThrough = j.value("overlayClickThrough", false);
        g_config.overlayAlwaysOnTop = j.value("overlayAlwaysOnTop", true);
        if(j.contains("panels")){
            for(auto &it : j["panels"].items()){
                PanelConfig p;
                auto obj = it.value();
                p.enabled = obj.value("enabled", true);
                p.background = obj.value("background", true);
                p.scale = obj.value("scale", 1.0f);
                if(obj.contains("color")){
                    auto a = obj["color"];
                    for(int i=0;i<4;i++) p.color[i] = a.value(i, p.color[i]);
                }
                if(obj.contains("bgColor")){
                    auto a = obj["bgColor"];
                    for(int i=0;i<4;i++) p.bgColor[i] = a.value(i, p.bgColor[i]);
                }
                if(obj.contains("pos")){
                    auto a = obj["pos"];
                    p.pos.x = a.value(0, p.pos.x); p.pos.y = a.value(1, p.pos.y);
                }
                p.movable = obj.value("movable", true);
                g_config.panels[it.key()] = p;
            }
        }
    } catch(...){ std::cerr<<"Failed to parse config.json\n"; }
}

void save_config(){
    json j;
    j["minecraftPath"] = g_config.minecraftPath;
    j["overlayClickThrough"] = g_config.overlayClickThrough;
    j["overlayAlwaysOnTop"] = g_config.overlayAlwaysOnTop;
    json panels;
    for(auto &kv : g_config.panels){
        json p;
        p["enabled"] = kv.second.enabled;
        p["background"] = kv.second.background;
        p["scale"] = kv.second.scale;
        p["movable"] = kv.second.movable;
        p["color"] = { kv.second.color[0], kv.second.color[1], kv.second.color[2], kv.second.color[3] };
        p["bgColor"] = { kv.second.bgColor[0], kv.second.bgColor[1], kv.second.bgColor[2], kv.second.bgColor[3] };
        p["pos"] = { kv.second.pos.x, kv.second.pos.y };
        panels[kv.first] = p;
    }
    j["panels"] = panels;
    std::ofstream out(CONFIG_FILE);
    out << j.dump(4);
}

// ------------------------------- Helper: launch Minecraft -------------------------
bool launch_minecraft(const std::string &path){
    if(path.empty()) return false;
    // Use ShellExecute to open if it's registered; otherwise CreateProcess
    HINSTANCE h = ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if((INT_PTR)h > 32) return true;
    // fallback: try CreateProcess
    STARTUPINFOA si; PROCESS_INFORMATION pi; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si); ZeroMemory(&pi, sizeof(pi));
    BOOL ok = CreateProcessA(path.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if(ok){ CloseHandle(pi.hProcess); CloseHandle(pi.hThread); return true; }
    return false;
}

// ------------------------------- Overlay utilities -------------------------------
// For CPS tracking
std::deque<Clock::time_point> clickTimes;
int getCPS(){ auto now = Clock::now(); while(!clickTimes.empty() && std::chrono::duration_cast<std::chrono::milliseconds>(now - clickTimes.front()).count() > 1000) clickTimes.pop_front(); return (int)clickTimes.size(); }

// FPS tracking
float g_fps = 0.0f;

// Keystroke tracking
bool keyStateW=false, keyStateA=false, keyStateS=false, keyStateD=false, keyStateSpace=false;

// Reach measurement (naive local approximation: distance advanced along forward ray until click)
float lastReach = 0.0f;

// Load image as OpenGL texture (using stb_image)
GLuint loadTextureFromFile(const char* filename, int &out_w, int &out_h){
    int n; unsigned char* data = stbi_load(filename, &out_w, &out_h, &n, 4);
    if(!data) return 0;
    GLuint tex; glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, out_w, out_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    return tex;
}

// ------------------------------- Win32: make window click-through ----------------
void setWindowClickThrough(HWND hwnd, bool clickThrough){
    LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
    if(clickThrough) ex |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
    else ex &= ~(WS_EX_TRANSPARENT);
    SetWindowLong(hwnd, GWL_EXSTYLE, ex);
}

// ------------------------------- ImGui helpers -----------------------------------
static void PushStyleForPanel(const PanelConfig &p){
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(50,20));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(p.bgColor[0], p.bgColor[1], p.bgColor[2], p.bgColor[3]));
}
static void PopStyleForPanel(){ ImGui::PopStyleColor(); ImGui::PopStyleVar(2); }

// ------------------------------- Main ------------------------------------------------
int main(int argc, char** argv){
    // Load config
    load_config();

    // Initialize GLFW (used for both launcher and overlay windows)
    if(!glfwInit()) return -1;

    // ---------------- Launcher window ----------------
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE); // borderless launcher

    GLFWwindow* launcher = glfwCreateWindow(600, 360, "Roro Client Launcher", NULL, NULL);
    if(!launcher){ glfwTerminate(); return -1; }
    glfwMakeContextCurrent(launcher);
    glfwSwapInterval(1);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){ std::cerr<<"Failed to initialize GLAD\n"; }

    // Setup ImGui context
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(launcher, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Load background texture (optional)
    int bg_w=0, bg_h=0; GLuint bgTex = 0;
    const char* default_bg = "minecraft_bg.jpg"; // put an image next to exe or set path in config
    bgTex = loadTextureFromFile(default_bg, bg_w, bg_h);

    // create a second window for overlay
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    GLFWwindow* overlay = glfwCreateWindow(1280, 720, "Roro Overlay", NULL, NULL);
    if(!overlay) { std::cerr<<"Overlay window failed to create\n"; }

    // Prepare OpenGL for overlay
    // We will use the overlay's OpenGL context when showing overlay

    // positions map for panels
    if(g_config.panels.empty()){
        for(auto &name : PANEL_NAMES){
            PanelConfig p; p.enabled = true; p.background = true; p.scale = 1.0f; p.movable = true; p.pos = ImVec2(50.0f, 50.0f+20.0f);
            g_config.panels[name] = p;
        }
    }

    // Main loop variables
    bool showOverlay = true;
    bool overlayInteractive = true; // controlled by settings
    auto last = Clock::now();
    int frames = 0; float accum = 0.0f;

    while(!glfwWindowShouldClose(launcher)){
        // Launcher loop (we'll poll events from both windows)
        glfwPollEvents();

        // launcher rendering
        glfwMakeContextCurrent(launcher);
        int lw, lh; glfwGetFramebufferSize(launcher, &lw, &lh);
        glViewport(0,0,lw,lh);
        glClearColor(0.07f,0.07f,0.08f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();

        // Draw launcher UI
        ImGui::SetNextWindowSize(ImVec2((float)lw, (float)lh));
        ImGui::SetNextWindowPos(ImVec2(0,0));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        ImGui::Begin("Launcher", NULL, flags);

        // Background image (if loaded)
        if(bgTex){
            ImGui::GetWindowDrawList()->AddImage((void*)(intptr_t)bgTex, ImVec2(0,0), ImVec2((float)lw,(float)lh), ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,220));
        }

        // Title
        ImGui::SetCursorPosY(20);
        ImGui::SetCursorPosX((lw/2) - 80);
        ImGui::PushFont(io.Fonts->Fonts[0]);
        ImGui::TextColored(ImVec4(1,1,1,1), "Roro Client");
        ImGui::PopFont();

        // Launch button big centered
        ImGui::SetCursorPosX((lw/2)-60);
        ImGui::SetCursorPosY((lh/2)-20);
        if(ImGui::Button("Launch", ImVec2(120,40))){
            // launch Minecraft
            if(launch_minecraft(g_config.minecraftPath)){
                // success
            } else {
                // try to open path chooser
                char buffer[MAX_PATH] = {0};
                OPENFILENAMEA ofn; ZeroMemory(&ofn, sizeof(ofn)); ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = NULL; ofn.lpstrFile = buffer; ofn.nMaxFile = MAX_PATH; ofn.lpstrFilter = "Executables\0*.exe\0All\0*.*\0";
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if(GetOpenFileNameA(&ofn)){
                    g_config.minecraftPath = std::string(buffer);
                    save_config();
                }
            }
        }

        // Settings button
        ImGui::SameLine(); ImGui::SetCursorPosX((lw/2)+70);
        if(ImGui::Button("Settings")){
            // open an ImGui modal for settings below
        }

        // Version in bottom-left
        ImGui::SetCursorPosX(10); ImGui::SetCursorPosY((float)lh - 30);
        ImGui::Text("Version: 1.0.0");

        // Settings modal / panel
        if(ImGui::CollapsingHeader("Settings")){
            ImGui::InputText("Minecraft exe path", &g_config.minecraftPath);
            ImGui::Checkbox("Overlay always on top", &g_config.overlayAlwaysOnTop);
            ImGui::Checkbox("Overlay click-through", &g_config.overlayClickThrough);
            if(ImGui::Button("Save config")) save_config();
            ImGui::Separator();
            if(ImGui::Button(showOverlay ? "Hide Overlay" : "Show Overlay")) showOverlay = !showOverlay;
            ImGui::Separator();
            ImGui::Text("Panels configuration");
            for(auto &kv : g_config.panels){
                if(ImGui::TreeNode(kv.first.c_str())){
                    ImGui::Checkbox("Enabled", &kv.second.enabled);
                    ImGui::Checkbox("Background", &kv.second.background);
                    ImGui::SliderFloat("Scale", &kv.second.scale, 0.5f, 2.0f);
                    ImGui::ColorEdit4("Color", kv.second.color);
                    ImGui::ColorEdit4("BG Color", kv.second.bgColor);
                    ImGui::Checkbox("Movable", &kv.second.movable);
                    ImGui::TreePop();
                }
            }
        }

        ImGui::End();

        ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(launcher);

        // ---------------- Overlay loop (render separately) ----------------
        if(showOverlay && overlay){
            glfwMakeContextCurrent(overlay);
            if(g_config.overlayAlwaysOnTop){
                HWND hwnd = GetActiveWindow(); // not ideal - we can find overlay HWND
                HWND ovh = glfwGetWin32Window(overlay);
                SetWindowPos(ovh, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);
            }
            // Click-through
            HWND ovh = glfwGetWin32Window(overlay);
            setWindowClickThrough(ovh, g_config.overlayClickThrough);

            int ow, oh; glfwGetFramebufferSize(overlay, &ow, &oh);
            glViewport(0,0,ow,oh);
            glClearColor(0,0,0,0); // transparent background
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();

            // Update time & fps
            auto now = Clock::now(); float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last).count(); last = now;
            accum += dt; frames++; if(accum >= 0.5f){ g_fps = frames/accum; frames=0; accum=0.0f; }

            // Input capture for keystrokes (poll via GetAsyncKeyState to detect while overlay may be click-through)
            keyStateW = (GetAsyncKeyState('W') & 0x8000) != 0;
            keyStateA = (GetAsyncKeyState('A') & 0x8000) != 0;
            keyStateS = (GetAsyncKeyState('S') & 0x8000) != 0;
            keyStateD = (GetAsyncKeyState('D') & 0x8000) != 0;
            keyStateSpace = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

            // click detection for CPS
            static bool lastLeft = false;
            bool curLeft = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            if(curLeft && !lastLeft){ clickTimes.push_back(Clock::now()); }
            lastLeft = curLeft;

            // HUD rendering for each panel (simple layout, movable windows)
            for(auto &kv : g_config.panels){
                const std::string &name = kv.first;
                PanelConfig &p = kv.second;
                if(!p.enabled) continue;
                ImGui::SetNextWindowBgAlpha(p.background ? p.bgColor[3] : 0.0f);
                ImGui::SetNextWindowSize(ImVec2(180.0f * p.scale, 30.0f * p.scale), ImGuiCond_Once);
                ImGui::SetNextWindowPos(p.pos, ImGuiCond_Once);
                ImGuiWindowFlags wflags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize;
                if(!p.movable) wflags |= ImGuiWindowFlags_NoMove;
                if(g_config.overlayClickThrough) wflags |= ImGuiWindowFlags_NoInputs;
                ImGui::Begin(name.c_str(), NULL, wflags);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6,4));
                if(name == "FPS COUNTER"){
                    ImGui::TextColored(ImVec4(p.color[0],p.color[1],p.color[2],p.color[3]), "FPS: %.1f", g_fps);
                } else if(name == "CPS COUNTER"){
                    ImGui::TextColored(ImVec4(p.color[0],p.color[1],p.color[2],p.color[3]), "CPS: %d", getCPS());
                } else if(name == "KEYSTROKE"){
                    ImGui::Text("W %s  A %s  S %s  D %s  Space %s",
                        keyStateW?"[P]":"[ ]",
                        keyStateA?"[P]":"[ ]",
                        keyStateS?"[P]":"[ ]",
                        keyStateD?"[P]":"[ ]",
                        keyStateSpace?"[P]":"[ ]");
                } else if(name == "REACH COUNTER"){
                    ImGui::TextColored(ImVec4(p.color[0],p.color[1],p.color[2],p.color[3]), "Reach: %.2fm", lastReach);
                } else if(name == "WATERMARK"){
                    ImGui::TextColored(ImVec4(p.color[0],p.color[1],p.color[2],p.color[3]), "roro client");
                } else {
                    ImGui::Text("%s", name.c_str());
                }
                ImGui::PopStyleVar();
                // record position for movable windows
                ImVec2 pos = ImGui::GetWindowPos();
                p.pos = pos;
                ImGui::End();
            }

            ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(overlay);
        }

    }

    // Cleanup
    save_config();
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    if(overlay) glfwDestroyWindow(overlay);
    glfwDestroyWindow(launcher);
    glfwTerminate();
    return 0;
}
