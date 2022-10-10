//
// Created by Stanislav Svědiroh on 11.06.2022.
//
#pragma once

#include "SDL.h"
#include "SDL_vulkan.h"
#include "VulkanEngineWindow.h"
#include <vulkan/vulkan.h>
#include <set>
#include <vector>
#include <fmt/core.h>
#include <string>
#include <vector>
#include <cstring>
#include <iostream>
#include <unordered_set>

#define VALIDATION_LAYER_NAME "VK_LAYER_LUNARG_standard_validation"
//#define VALIDATION_LAYER_NAME "VK_LAYER_KHRONOS_validation"

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
    uint32_t graphicsFamily;
    uint32_t presentFamily;
    uint32_t computeFamily;

    bool graphicsFamilyHasValue = false;
    bool presentFamilyHasValue = false;
    bool computeFamilyHasValue = false;

    bool isComplete() { return graphicsFamilyHasValue && presentFamilyHasValue && computeFamilyHasValue; }
};

class VulkanEngineDevice {
public:
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    VulkanEngineDevice(VulkanEngineWindow &window, const char *title);
    ~VulkanEngineDevice();

    VulkanEngineDevice(const VulkanEngineDevice &) = delete;
    VulkanEngineDevice operator=(const VulkanEngineDevice &) = delete;
    VulkanEngineDevice(VulkanEngineDevice &&) = delete;
    VulkanEngineDevice &operator=(VulkanEngineDevice &&) = delete;

    VkCommandPool getGraphicsCommandPool() { return graphicsCommandPool; }
    VkCommandPool getComputeCommandPool() { return computeCommandPool; }

    VkInstance getInstance() { return instance; }

    VkPhysicalDevice getPhysicalDevice() { return physicalDevice; }

    const VkDevice getDevice() { return device_; }

    VkSurfaceKHR surface() { return surface_; }

    VkQueue graphicsQueue() { return graphicsQueue_; }

    VkQueue presentQueue() { return presentQueue_; }

    VkQueue computeQueue() { return computeQueue_; }

    SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupport(physicalDevice); }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies(physicalDevice); }

    VkFormat findSupportedFormat(
            const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    // Buffer Helper Functions
    void createBuffer(
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties,
            VkBuffer &buffer,
            VkDeviceMemory &bufferMemory);

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkQueue queue);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void copyBufferToImage(
            VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);
    void createImageWithInfo(
            const VkImageCreateInfo &imageInfo,
            VkMemoryPropertyFlags properties,
            VkImage &image,
            VkDeviceMemory &imageMemory);

    VkPhysicalDeviceProperties properties;

private:
    void createInstance(const char *title);

    void setupDebugMessenger();

    void createSurface();

    void pickPhysicalDevice();

    void createLogicalDevice();

    void createCommandPool();

    // helper functions
    bool isDeviceSuitable(VkPhysicalDevice device);

    std::vector<const char *> getRequiredExtensions();

    bool checkValidationLayerSupport();

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);

    void hasSDLRequiredInstanceExtensions();

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VulkanEngineWindow &window;
    VkCommandPool graphicsCommandPool;
    VkCommandPool computeCommandPool;

    VkDevice device_;
    VkSurfaceKHR surface_;
    VkQueue graphicsQueue_;
    VkQueue presentQueue_;
    VkQueue computeQueue_;

    const std::vector<const char *> validationLayers = {VALIDATION_LAYER_NAME};
    const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
};