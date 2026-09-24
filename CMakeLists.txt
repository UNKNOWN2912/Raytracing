cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

project(Raytracing)

add_subdirectory(Libraries/glfw)

find_package(Vulkan REQUIRED)

file(GLOB sources
	"Sources/*.cpp"
	"Header/*.cpp"
)

add_executable(Raytracing ${sources} "main.cpp")
target_link_libraries(Raytracing Vulkan::Vulkan)
target_link_libraries(Raytracing glfw)
