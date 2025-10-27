#include "imgui.h"
#include "imgui_internal.h"
#include <stdio.h>

// Minimal stub for compilation
ImGuiContext* GImGui = nullptr;

IMGUI_API ImGuiContext* ImGui::CreateContext(ImFontAtlas* atlas) {
    GImGui = new ImGuiContext();
    return GImGui;
}

IMGUI_API void ImGui::DestroyContext(ImGuiContext* ctx) {
    if(ctx) delete ctx;
}

IMGUI_API ImGuiIO& ImGui::GetIO() {
    static ImGuiIO io;
    return io;
}

IMGUI_API void ImGui::StyleColorsDark(ImGuiStyle* dst) {}
IMGUI_API void ImGui::Text(const char* fmt, ...) {}
IMGUI_API void ImGui::TextColored(const ImVec4& col, const char* fmt, ...) {}
