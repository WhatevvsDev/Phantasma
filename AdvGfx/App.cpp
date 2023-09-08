#include "App.h"
#include <GLFW/glfw3.h>
#include <cmath>

namespace App
{
    GLFWwindow* window { nullptr };
    AppDesc app_desc;

    uint32_t* render_buffer { nullptr };

    #define FROM_RGBA(r, g, b, a) ((0x01000000 * a) | (0x00010000 * b) | (0x00000100 * g) | (0x00000001 * r))

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

        render();

        glDrawPixels(app_desc.width, app_desc.height, GL_RGBA, GL_UNSIGNED_BYTE, render_buffer);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    void render()
    {
        for(int y = 0; y < app_desc.height; y++)
        {
            for(int x = 0; x < app_desc.width; x++)
            {
                int x_t = ((float)x / (float)app_desc.width) * 255.0f;
                int y_t = ((float)y / (float)app_desc.height) * 255.0f;

                render_buffer[x + y * app_desc.width] = (uint32_t)((0x00000100 * x_t) | (0x00000001 * y_t));
            }
        }
    }
}