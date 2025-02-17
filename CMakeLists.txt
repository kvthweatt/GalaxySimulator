cmake_minimum_required(VERSION 3.10)
project(GalaxySimulation)

# Set OpenGL preference to GLVND
set(OpenGL_GL_PREFERENCE GLVND)

# Enable C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenMP REQUIRED)

# GLM is header-only, we just need to include its directory
# First try pkg-config
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(GLM QUIET glm)
endif()

# If pkg-config didn't find GLM, set include path manually
if(NOT GLM_FOUND)
    # Try to find GLM in standard system locations
    find_path(GLM_INCLUDE_DIRS
        NAMES glm/glm.hpp
        PATHS
            /usr/include
            /usr/local/include
            ${CMAKE_SOURCE_DIR}/external
        DOC "GLM header directory"
    )
    if(NOT GLM_INCLUDE_DIRS)
        message(FATAL_ERROR "GLM headers not found! Please install GLM or specify GLM_INCLUDE_DIRS manually.")
    endif()
endif()

# Add executable
add_executable(galaxy_sim main.cpp)

# Link libraries
target_link_libraries(galaxy_sim
    PRIVATE
    OpenGL::GL
    GLEW::GLEW
    glfw
    OpenMP::OpenMP_CXX
)

# Add compiler flags
target_compile_options(galaxy_sim PRIVATE
    -Wall
    -Wextra
    -O3
    ${OpenMP_CXX_FLAGS}
)

# Include directories
target_include_directories(galaxy_sim
    PRIVATE
    ${OPENGL_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
)
