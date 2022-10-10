cmake_minimum_required(VERSION 3.21)

set(NAME VulkanComputeEngine)
project(${NAME} VERSION 0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)
message(${SOURCES})
add_executable(${NAME} ${SOURCES})

target_include_directories(${NAME} PUBLIC external/sdl/)

add_subdirectory(external/sdl)
target_link_directories(${NAME} PRIVATE external/sdl/)

add_subdirectory(external/fmt)
target_link_directories(${PROJECT_NAME} PRIVATE external/fmt/)

add_subdirectory(external/glm)
target_link_directories(${PROJECT_NAME} PRIVATE external/glm/)

add_subdirectory(external/imgui)
target_link_directories(${PROJECT_NAME} PRIVATE external/imgui/)

find_package(Vulkan REQUIRED)

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

target_link_libraries(${NAME} Vulkan::Vulkan SDL2 fmt glm ImGui ${OpenCV_LIBS} -ldl)

############## Build SHADERS #######################

# Find all vertex and fragment sources within shaders directory
# taken from VBlancos vulkan tutorial
# https://github.com/vblanco20-1/vulkan-guide/blob/all-chapters/CMakeLists.txt
find_program(GLSL_VALIDATOR glslangValidator HINTS
        ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}
        /usr/bin
        /usr/local/bin
        ${VULKAN_SDK_PATH}/Bin
        ${VULKAN_SDK_PATH}/Bin32
        $ENV{VULKAN_SDK}/Bin/
        $ENV{VULKAN_SDK}/Bin32/
        )

# get all .comp files in shaders directory
file(GLOB_RECURSE GLSL_SOURCE_FILES "${PROJECT_SOURCE_DIR}/shaders/*.comp" "${PROJECT_SOURCE_DIR}/shaders/*.vert" "${PROJECT_SOURCE_DIR}/shaders/*.frag")

foreach (GLSL ${GLSL_SOURCE_FILES})
    message(STATUS "BUILDING SHADER")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach (GLSL)

add_custom_target(ComputeShaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(${NAME} ComputeShaders)