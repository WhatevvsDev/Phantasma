#include "App.h"
#include "Raytracer.h"

#include "Timer.h"

#include <GLFW/glfw3.h>
#include <cmath>
#include <format>

namespace App
{
    GLFWwindow* window { nullptr };
    AppDesc app_desc;

    uint32_t* render_buffer { nullptr };

    Timer fps_timer;
    float render_time { 1.0f };
    float update_time { 1.0f };

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

        glfwWindowHint(GLFW_RESIZABLE, false);
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

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 130");

        fps_timer.start();

        Raytracer::init();

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

        // Limit to min 1.0f
        render_time = max(render_time, 1.0f);
        update_time = max(update_time, 1.0f);

        std::string title = std::format("{} Window | {} ms ({} FPS) | {}ms update", app_desc.title, (int)render_time, (int)(1000.0f / render_time),  (int)update_time);

        glfwSetWindowTitle(window, title.c_str());
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();

        // Only get the render time
        update_time =  fps_timer.lap_delta();
        Raytracer::raytrace(app_desc.width, app_desc.height, render_buffer);
        render_time = fps_timer.lap_delta();

        glDrawPixels(app_desc.width, app_desc.height, GL_RGBA, GL_UNSIGNED_BYTE, render_buffer);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}