//
// Created by Stanislav Svědiroh on 13.06.2022.
//
#pragma once

#include "SDL.h"
#include "SDL_vulkan.h"
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <fmt/core.h>

class VulkanEngineWindow {

public:
    VulkanEngineWindow(const char *title, uint32_t width, uint32_t height, int flags);
    ~VulkanEngineWindow();

    VulkanEngineWindow(const VulkanEngineWindow &) = delete;
    VulkanEngineWindow &operator=(const VulkanEngineWindow &) = delete;

    VkExtent2D getExtent() { return {width_, height_}; };

    void onWindowResized(uint32_t width, uint32_t height) {
        frameBufferResized = true;
        width_ = width;
        height_ = height;
    }

    [[nodiscard]] bool wasWindowResized() const { return frameBufferResized; };

    void resetWindowsResizedFlag() { frameBufferResized = false; };

    SDL_Window *sdlWindow() { return window; };
private:
    void initWindow();

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    bool frameBufferResized = false;

    const char *windowName;
    const int flags_;
    SDL_Window *window;
};
