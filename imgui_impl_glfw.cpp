#include "imgui.h"
#include "imgui_impl_glfw.h"
#include <GLFW/glfw3.h>

static GLFWwindow* g_Window = nullptr;

bool ImGui_ImplGlfw_InitForOpenGL(GLFWwindow* window, bool install_callbacks) {
    g_Window = window;
    return true;
}

void ImGui_ImplGlfw_NewFrame() {}
