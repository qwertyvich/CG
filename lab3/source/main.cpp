#include <cstdint>
#include <climits>
#include <string_view>
#include <charconv>

#include <iostream>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "graphics_internal.hpp"
#include "application.hpp"

namespace {

constexpr int32_t default_window_width = 1280;
constexpr int32_t default_window_height = 720;

constexpr char default_window_title[] = "Lab 3 - Texture Mapping";

GLFWwindow* glfw_window;

} // namespace

int main(int argc,char** argv) {
    bool smoke=false;
    unsigned maxFrames=0;
    std::string_view demo="base";
    for(int i=1;i<argc;++i) {
        std::string_view arg=argv[i];
        if(arg=="--smoke-test") smoke=true;
        else if(arg.starts_with("--demo=")) demo=arg.substr(7);
        else if(arg.starts_with("--frames=")) {
            auto value=arg.substr(9);
            auto parsed=std::from_chars(value.data(),value.data()+value.size(),maxFrames);
            if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || !maxFrames) return EXIT_FAILURE;
        } else {std::cerr<<"Unknown argument: "<<arg<<"\n";return EXIT_FAILURE;}
    }
    unsigned frame=0;
	int status = EXIT_SUCCESS;

	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW\n";
		return EXIT_FAILURE;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	glfw_window = glfwCreateWindow(default_window_width, default_window_height,
	                               default_window_title, nullptr, nullptr);
	if (glfw_window == nullptr) {
		status = EXIT_FAILURE;
		goto err_null_window;
	}

	glfwSetFramebufferSizeCallback(glfw_window, [](GLFWwindow*, int width, int height){
		if (width == 0 || height == 0) {
			return;
		}

		graphics::internal::resize(width, height);
	});

	if (ImGui::CreateContext() == nullptr) {
		std::cerr << "Failed to create ImGUI context\n";
		status = EXIT_FAILURE;
		goto err_imgui_init;
	}

	if (!ImGui_ImplGlfw_InitForVulkan(glfw_window, true)) {
		std::cerr << "Failed to initialize ImGUI GLFW backend for Vulkan renderer\n";
		status = EXIT_FAILURE;
		goto err_imgui_glfw_init;
	}

	if (!graphics::internal::initialize(glfw_window)) {
		std::cerr << "Failed to initialize graphics\n";
		status = EXIT_FAILURE;
		goto err_graphics_init;
	}

	if (!application::initialize()) {
		std::cerr << "Failed to initialize application\n";
		status = EXIT_FAILURE;
		goto err_application_init;
	}
    if(!application::selectDemo(demo)) {
        std::cerr<<"Unknown demo: "<<demo<<"\n";
        application::shutdown();status=EXIT_FAILURE;goto err_application_init;
    }

	while (!glfwWindowShouldClose(glfw_window)) {
        if(maxFrames && frame>=maxFrames) break;
        if(smoke) {
            application::smokeStep(frame);
            if(frame==100) glfwSetWindowSize(glfw_window,1024,768);
            if(frame==120) glfwSetWindowSize(glfw_window,1400,800);
            if(frame==200) {
                glfwIconifyWindow(glfw_window);glfwPollEvents();
                glfwWaitEventsTimeout(0.35);glfwRestoreWindow(glfw_window);
            }
            if(frame==240) break;
        }
        ++frame;
        int width=0,height=0;
        glfwGetFramebufferSize(glfw_window,&width,&height);
        if(!width || !height) {glfwWaitEventsTimeout(0.1);continue;}
        const double time = glfwGetTime();

		glfwPollEvents();
        glfwGetFramebufferSize(glfw_window,&width,&height);
        if(!width || !height || glfwGetWindowAttrib(glfw_window,GLFW_ICONIFIED)) {
            glfwWaitEventsTimeout(0.1);continue;
        }
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();

		ImGui::NewFrame();
		application::update(time);
		ImGui::Render();

		graphics::internal::FrameData fd = graphics::internal::prepare();
        if(fd.failed) {status=EXIT_FAILURE;break;}
        if(!fd.command_buffer) continue;
        if(!application::render(fd) || !graphics::internal::submitAndPresent()) {
            status=EXIT_FAILURE;break;
        }
	}

	application::shutdown();
err_application_init:
err_graphics_init:
	graphics::internal::shutdown();
	ImGui_ImplGlfw_Shutdown();
err_imgui_glfw_init:
	ImGui::DestroyContext();
err_imgui_init:
	glfwDestroyWindow(glfw_window);
err_null_window:
	glfwTerminate();

	if(graphics::internal::validationErrors() || graphics::internal::generalErrors()) status=EXIT_FAILURE;
    if(smoke) std::cout<<"SMOKE finished: frames="<<frame<<" validation_errors="<<graphics::internal::validationErrors()<<" general_errors="<<graphics::internal::generalErrors()<<" status="<<status<<"\n";
	return status;
}
