//
// Created by Stanislav Svědiroh on 11.07.2022.
//
#pragma once

#include "../VulkanEngineDescriptors.h"
#include "../VulkanEngineSwapChain.h"
#include "../VulkanEngineRenderer.h"
#include "../../GlobalConfiguration.h"

#include "SDL.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"

#include <vector>

class DebugGui {
public:
    DebugGui(VulkanEngineDevice &engineDevice, VulkanEngineRenderer &renderer, SDL_Window *window);
    ~DebugGui();

    DebugGui(const DebugGui &) = delete;
    DebugGui &operator=(const DebugGui &) = delete;

    void showWindow(SDL_Window *window);

    void render(VkCommandBuffer &commandBuffer);
    void processEvent(SDL_Event event);
private:

    double movingFPSAverage = 0;
    bool chunkBordersShown = false;
    bool renderWireframes = false;

    std::unique_ptr<VulkanEngineDescriptorPool> imguiPool{};
};