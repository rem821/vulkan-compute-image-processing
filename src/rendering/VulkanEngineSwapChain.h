//
// Created by Stanislav Svědiroh on 13.06.2022.
//
#pragma once

#include "VulkanEngineDevice.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <array>
#include <memory>

class VulkanEngineSwapChain {
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 1;

    VulkanEngineSwapChain(VulkanEngineDevice &engineDevice, VkExtent2D extent);
    VulkanEngineSwapChain(VulkanEngineDevice &engineDevice, VkExtent2D extent,
                          std::shared_ptr<VulkanEngineSwapChain> previous);

    ~VulkanEngineSwapChain();

    VulkanEngineSwapChain(const VulkanEngineSwapChain &) = delete;
    VulkanEngineSwapChain operator=(const VulkanEngineSwapChain &) = delete;

    VkFramebuffer getFrameBuffer(int index) { return swapChainFramebuffers[index]; };

    VkRenderPass getRenderPass() { return renderPass; };

    VkImageView getImageView(int index) { return swapChainImageViews[index]; };

    size_t imageCount() { return swapChainImages.size(); };

    VkFormat getSwapChainImageFormat() { return swapChainImageFormat; };

    VkExtent2D getSwapChainExtent() { return swapChainExtent; };

    uint32_t width() { return swapChainExtent.width; };

    uint32_t height() { return swapChainExtent.height; };

    float extentAspectRatio() {
        return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
    };

    VkFormat findDepthFormat();
    VkResult acquireNextImage(uint32_t *imageIndex);
    VkResult submitCommandBuffers(const VkCommandBuffer *buffers, const VkCommandBuffer *computeCommandBuffer, const uint32_t *imageIndex);

    bool compareSwapFormats(const VulkanEngineSwapChain &swapChain) const {
        return swapChain.swapChainDepthFormat == swapChainDepthFormat &&
               swapChain.swapChainImageFormat == swapChainImageFormat;
    }

private:
    void init();
    void createSwapChain();
    void createImageViews();
    void createDepthResources();
    void createRenderPass();
    void createFrameBuffers();
    void createSyncObjects();

    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
    static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

    VkFormat swapChainImageFormat;
    VkFormat swapChainDepthFormat;
    VkExtent2D swapChainExtent;

    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkRenderPass renderPass;

    std::vector<VkImage> depthImages;
    std::vector<VkDeviceMemory> depthImageMemory;
    std::vector<VkImageView> depthImageViews;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;

    VulkanEngineDevice &engineDevice;
    VkExtent2D windowExtent;

    VkSwapchainKHR swapChain;
    std::shared_ptr<VulkanEngineSwapChain> oldSwapChain;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    VkSemaphore computeSemaphore;

    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    size_t currentFrame = 0;

};
