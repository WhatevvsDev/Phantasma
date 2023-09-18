#include "App.h"
#include "Raytracer.h"
#include "Compute.h"

#include "Timer.h"
#include "LogUtility.h"

#include <GLFW/glfw3.h>

#include <ImGuizmo.h>
#include "Math.h"

#include <cmath>
#include <format>

namespace App
{
    GLFWwindow* window { nullptr };
    AppDesc app_desc;

    uint32_t* render_buffer { nullptr };

    Timer fps_timer;
    float last_render_time { 1.0f };
    float last_update_time { 1.0f };

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
        
        glfwSetKeyCallback(window, Raytracer::Input::key_callback);

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

        Compute::init();
        Raytracer::init();

        while (!glfwWindowShouldClose(window))
        {
            App::update();
        }

        Raytracer::terminate();

        glfwTerminate();
        return 0;
    }

    void update()
    {
        glClear(GL_COLOR_BUFFER_BIT);

        // Limit to min 1.0f
        last_render_time = fmax(last_render_time, 1.0f);
        last_update_time = fmax(last_update_time, 1.0f);

        std::string title = std::format("{} Window {}x{} | {} ms ({} {}) | {}ms update", 
            app_desc.title, 
            app_desc.width, 
            app_desc.height, 
            (int)last_render_time, 
            (int)(1000.0f / last_render_time),  
            (Raytracer::get_target_fps() == 1000) ? "FPS" : "Limited FPS",
            (int)last_update_time);

        glfwSetWindowTitle(window, title.c_str());
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        ImGuizmo::SetRect(0, 0, app_desc.width, app_desc.height);
		ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

        // We need total delta time
        Raytracer::update(last_update_time + last_render_time);
        
        // Only get the render time
        last_update_time =  fps_timer.lap_delta();
        Raytracer::raytrace(app_desc.width, app_desc.height, render_buffer);
        float sleep_time = (1000.0f / Raytracer::get_target_fps() - fps_timer.peek_delta() - last_update_time);
        if(sleep_time < 0) sleep_time = 0;
        Sleep((DWORD)sleep_time);
        last_render_time = fps_timer.lap_delta();

        Raytracer::ui();

        glDrawPixels(app_desc.width, app_desc.height, GL_RGBA, GL_UNSIGNED_BYTE, render_buffer);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}