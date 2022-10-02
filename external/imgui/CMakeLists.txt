cmake_minimum_required(VERSION 3.21)

set(IMGUI_VERSION 1.88)
project(ImGui VERSION ${IMGUI_VERSION})
message(STATUS "ImGui: Version " ${IMGUI_VERSION})

set(IMGUI_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui.h
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui_draw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui_tables.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui_widgets.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui_demo.cpp
        )

add_library(ImGui ${IMGUI_SOURCE})
