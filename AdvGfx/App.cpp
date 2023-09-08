#include "App.h"
#include <GLFW/glfw3.h>
#include <cmath>

namespace App
{
    GLFWwindow* window { nullptr };
    AppDesc app_desc;

    uint32_t* render_buffer { nullptr };

    int init(AppDesc& desc)
    {
        app_desc = desc;
        delete[] render_buffer;
        render_buffer = new uint32_t[app_desc.width * app_desc.height];
        memset(render_buffer, 0, app_desc.width * app_desc.height * sizeof(uint32_t));

        if (!glfwInit())
        {
            return -1;
        }
        else
        {
            LOGMSG(Log::MessageType::Default, "Initialized GLFW.");
        }

        window = glfwCreateWindow(desc.width, desc.height, desc.title.c_str(), NULL, NULL);
        if (!window)
        {
            glfwTerminate();
            return -1;
        }
        else
        {
            LOGMSG(Log::MessageType::Default, "Created Window.");
        }

        glfwMakeContextCurrent(window);

        while (!glfwWindowShouldClose(window))
        {
            App::update();
        }

        glfwTerminate();
        return 0;
    }

    void update()
    {
        glClear(GL_COLOR_BUFFER_BIT);

        render_buffer[0] = 0xffffffff;

        glDrawPixels(app_desc.width, app_desc.height, GL_RGBA, GL_UNSIGNED_BYTE, render_buffer);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}