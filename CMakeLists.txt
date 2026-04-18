cmake_minimum_required(VERSION 3.20)

# Defining your project
project(Metropolis VERSION 1.0 LANGUAGES CXX)

# Require C++17 (or C++20 if you prefer)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------
# DEPENDENCIES (The Clean Way)
# ---------------------------------------------------------
include(FetchContent)

# Automatically download and configure GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4 # Use the latest stable release
)
# We don't need to build GLFW's examples or tests, just the library
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

# ---------------------------------------------------------
# BUILD TARGET
# ---------------------------------------------------------
# Group your source files
set(SOURCES
    src/main.cpp
    src/core/VulkanContext.cpp
    src/core/Swapchain.cpp
    src/renderer/ComputePass.cpp
    # Add your VulkanContext, Swapchain, etc. here as you build them
)

# Create the executable
add_executable(Metropolis ${SOURCES})
add_custom_command(TARGET Metropolis POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    "${CMAKE_CURRENT_BINARY_DIR}/shaders"
)

# Link GLFW to your executable
target_link_libraries(Metropolis PRIVATE glfw)

# Optional: Add the Vulkan package (you will need this soon!)
find_package(Vulkan REQUIRED)
# Make sure CMake is looking in the Vulkan include directory for GLM
include_directories(${Vulkan_INCLUDE_DIRS})
target_link_libraries(Metropolis PRIVATE Vulkan::Vulkan)
target_include_directories(Metropolis PRIVATE "C:/VulkanSDK/1.4.341.1/Include")
# --- AUTO COMPILE SHADERS ---
# 1. Make sure the output folder exists in the build directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)

# 2. Force CMake to run glslc right before building the C++ executable
add_custom_command(TARGET Metropolis PRE_BUILD
    COMMAND glslc ${CMAKE_SOURCE_DIR}/shaders/shader.comp -o ${CMAKE_BINARY_DIR}/shaders/shader.spv
    COMMENT "Auto-compiling compute shader..."
)