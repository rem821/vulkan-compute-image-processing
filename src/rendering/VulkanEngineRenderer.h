//
// Created by Stanislav Svědiroh on 14.06.2022.
//
#pragma once

#include "VulkanEngineDevice.h"
#include "VulkanEngineWindow.h"
#include "VulkanEngineSwapChain.h"
#include <fmt/core.h>
#include <memory>
#include <vector>
#include <stdexcept>
#include <cassert>

class VulkanEngineRenderer {

public:
    VulkanEngineRenderer(VulkanEngineWindow &window, VulkanEngineDevice &device);
    ~VulkanEngineRenderer();

    VulkanEngineRenderer(const VulkanEngineRenderer &) = delete;
    VulkanEngineRenderer &operator=(const VulkanEngineRenderer &) = delete;

    VkRenderPass getSwapChainRenderPass() const { return engineSwapChain->getRenderPass(); };

    float getAspectRatio() const { return engineSwapChain->extentAspectRatio(); };

    bool isFrameInProgress() const { return isFrameStarted; };

    VkCommandBuffer getCurrentGraphicsCommandBuffer() const {
        assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
        return graphicsCommandBuffers[currentFrameIndex];
    }

    VkCommandBuffer getComputeCommandBuffer() const { return computeCommandBuffer; }

    VulkanEngineSwapChain *getEngineSwapChain() const { return engineSwapChain.get(); }

    int getFrameIndex() const {
        assert(isFrameStarted && "Cannot get frame index when frame not in progress");
        return currentFrameIndex;
    }

    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer, VkImage &outputImage);
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

private:
    void createCommandBuffers();
    void freeCommandBuffers();
    void recreateSwapChain();

    VulkanEngineWindow &window;
    VulkanEngineDevice &engineDevice;
    std::unique_ptr<VulkanEngineSwapChain> engineSwapChain;
    std::vector<VkCommandBuffer> graphicsCommandBuffers;
    VkCommandBuffer computeCommandBuffer;

    uint32_t currentImageIndex;
    int currentFrameIndex = 0;

    bool isFrameStarted = false;
};