cmake_minimum_required(VERSION 3.20)
include(FetchContent)

project(cg-lab3 LANGUAGES C CXX)

add_executable(${PROJECT_NAME} source/main.cpp source/graphics_internal.cpp source/application.cpp source/scene.cpp source/geometry.cpp source/controls.cpp source/renderer.cpp source/image.cpp source/gpu_upload.cpp)

if(LINUX)
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
elseif(APPLE)
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
elseif(WIN32)
	if(MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE /W4 /wd4201 /utf-8)
		# Keep the console visible for Vulkan validation and startup errors.
		set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
	else()
		target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
		target_link_options(${PROJECT_NAME} PRIVATE -mwindows)
	endif()
	target_compile_definitions(${PROJECT_NAME} PRIVATE -D_USE_MATH_DEFINES)
else()
	message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not supported")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED TRUE CXX_STANDARD 20)

FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG 3.5.1
)

FetchContent_Declare(
	vk-bootstrap
	GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
	GIT_TAG v1.4.321
)

FetchContent_Declare(
	VulkanMemoryAllocator
	GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
	GIT_TAG v3.4.0
)

FetchContent_Declare(
	imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	GIT_TAG v1.92.9
)

find_package(Vulkan REQUIRED)
# Vendored stb_image at commit 003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5.
target_include_directories(${PROJECT_NAME} PRIVATE third_party/stb)
add_custom_target(copy-assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "${CMAKE_CURRENT_BINARY_DIR}/assets")
add_dependencies(${PROJECT_NAME} copy-assets)

set(GLFW_LIBRARY_TYPE STATIC)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)

FetchContent_MakeAvailable(
	glfw
	vk-bootstrap
	VulkanMemoryAllocator
	imgui
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	glfw
	Vulkan::Vulkan
	vk-bootstrap::vk-bootstrap
	GPUOpen::VulkanMemoryAllocator
)

target_include_directories(${PROJECT_NAME} PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_sources(${PROJECT_NAME} PRIVATE
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
)

# Compile shaders
find_program(GLSLC_FOUND NAMES glslc HINTS "$ENV{VULKAN_SDK}/Bin" REQUIRED)
if(GLSLC_FOUND)
	set(_SHADER_BINARIES)

	macro(compile_shader SHADER_FILE)
		set(SHADER_SOURCE ${CMAKE_SOURCE_DIR}/shaders/${SHADER_FILE})
		set(SHADER_BINARY ${SHADER_FILE}.spv)
		set(SHADER_BINARY_PATH ${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_BINARY})

		add_custom_command(
			OUTPUT ${SHADER_BINARY_PATH}
			COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
			COMMAND "${GLSLC_FOUND}" --target-env=vulkan1.1 "${SHADER_SOURCE}" -o "${SHADER_BINARY_PATH}"
			DEPENDS ${SHADER_SOURCE} ${CMAKE_SOURCE_DIR}/shaders/scene.glsl ${CMAKE_SOURCE_DIR}/shaders/model.glsl ${CMAKE_SOURCE_DIR}/shaders/display.glsl ${CMAKE_SOURCE_DIR}/shaders/lighting.glsl
			COMMENT "Compiling ${SHADER_FILE} shader"
			VERBATIM
		)

		list(APPEND _SHADER_BINARIES ${SHADER_BINARY_PATH})
	endmacro()

	# To compile shader file, use compile_shader function with a file name
	# of a shader inside shaders directory. See example below:

	compile_shader(lab3.vert)
	compile_shader(lab3.frag)
	compile_shader(lab3-base.frag)
	compile_shader(unlit.vert)
	compile_shader(unlit.frag)

	add_custom_target(shaders DEPENDS ${_SHADER_BINARIES})
	add_dependencies(${PROJECT_NAME} shaders)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(lab3-tests tests/scene_tests.cpp source/scene.cpp source/geometry.cpp source/image.cpp)
    target_include_directories(lab3-tests PRIVATE source third_party/stb)
    target_compile_features(lab3-tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(lab3-tests PRIVATE /W4 /utf-8)
    endif()
    add_test(NAME lab3-cpu COMMAND lab3-tests)
    set_tests_properties(lab3-cpu PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
