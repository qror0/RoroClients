#include <glad/glad.h>
#include <KHR/khrplatform.h>

#include <stdio.h>
#include <stdlib.h>

static void* get_proc(const char* name) {
    return (void*)glfwGetProcAddress(name);
}

int gladLoadGL(void) {
    if(!gladLoadGLLoader((GLADloadproc)get_proc)) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return 0;
    }
    return 1;
}
