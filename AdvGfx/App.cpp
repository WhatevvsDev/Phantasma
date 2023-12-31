#include "App.h"
#include "Raytracer.h"

#include <GLFW/glfw3.h>
#include <ImPlot.h>

void CustomImGuiStyle()
{

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding                     = ImVec2(8.00f, 8.00f);
	style.FramePadding                      = ImVec2(8.00f, 4.00f);
	style.CellPadding                       = ImVec2(6.00f, 6.00f);
	style.ItemSpacing                       = ImVec2(6.00f, 6.00f);
	style.ItemInnerSpacing                  = ImVec2(6.00f, 6.00f);
	style.TouchExtraPadding                 = ImVec2(0.00f, 0.00f);
	style.IndentSpacing                     = 25;
	style.ScrollbarSize                     = 15;
	style.GrabMinSize                       = 10;
	style.LogSliderDeadzone                 = 4;
}

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
			return -1;

		LOGDEFAULT("Initialized GLFW.");

		glfwWindowHint(GLFW_RESIZABLE, false);
		window = glfwCreateWindow(desc.width, desc.height, desc.title.c_str(), NULL, NULL);
		if (!window)
		{
			glfwTerminate();
			return -1;
		}

		LOGDEFAULT("Created Window.");
		
		glfwMakeContextCurrent(window);
		
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImPlot::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		// Merge icons into  font
		float text_font_size = 16.0f;
		float icon_font_size = 40.0f;
		io.Fonts->AddFontFromFileTTF("Roboto.ttf", text_font_size);

		// Add in Icon font
		ImFontConfig config;
		config.MergeMode = true;
		config.GlyphMinAdvanceX = icon_font_size;
		config.GlyphOffset.y += (icon_font_size - text_font_size) * 0.5f- 2.0f;
		static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
		io.Fonts->AddFontFromFileTTF("FontAwesome.otf", icon_font_size, &config, icon_ranges);

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		CustomImGuiStyle();

		// Setup Platform/Renderer backends
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 130");

		fps_timer.start();
		
		RaytracerInitDesc raytracer_desc;
		raytracer_desc.width_px = desc.width;
		raytracer_desc.height_px = desc.height;
		raytracer_desc.screen_buffer_ptr = render_buffer;
		
		Raytracer::init(raytracer_desc);

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

		std::string title = std::format("{} Window {}x{} | {} ms ({} FPS) | {}ms update", 
			app_desc.title, 
			app_desc.width, 
			app_desc.height, 
			(int)last_render_time, 
			(int)ceil(1000.0f / last_render_time),  
			(int)last_update_time);

		glfwSetWindowTitle(window, title.c_str());

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
		ImGuizmo::SetRect(0, 0, (float)app_desc.width, (float)app_desc.height);

		// We need total delta time
		Raytracer::update(last_update_time + last_render_time);
		glfwSetInputMode(window, GLFW_CURSOR, Raytracer::ui_is_visible() ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);

		// Only get the render time
		last_update_time =  fps_timer.lap_delta();
		Raytracer::raytrace();
		last_render_time = fps_timer.lap_delta();

		Raytracer::ui();

		// Flipping the buffer so its proper
		glRasterPos2f(-1,1);
		glPixelZoom( 1, -1 );
		glDrawPixels(app_desc.width, app_desc.height, GL_RGBA, GL_UNSIGNED_BYTE, render_buffer);

		ImGuiNotify::RenderNotifications();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		
		glfwSwapBuffers(window);

		glfwPollEvents();
	}
}