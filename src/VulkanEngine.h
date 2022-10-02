//
// Created by Stanislav Svědiroh on 20.09.2022.
//
#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>

namespace VulkanCompute {
    struct AllocatedBuffer {
        vk::Buffer _buffer;
        vk::DeviceMemory _bufferMemory;
    };

    struct AllocatedImage {
        VkImage _image;
        VkDeviceMemory _imageMemory;
        int32_t width, height, channels;
        VkImageLayout imagelayout;
        VkImageView view;
        VkDescriptorImageInfo descriptor;
        VkSampler sampler;
    };

    class VulkanEngine {
    public:

        VulkanEngine();
        ~VulkanEngine();

        void createShaderModuleFromFile(std::string fileName);

        void createVulkanDevice();
        void createDescriptorSetLayout();
        void createPipeline();
        void createCommandPool();
        void createDescriptorPool();
        void createDescriptorSet();
        void createCommandBuffer();
        void submitCommandBuffer();

        AllocatedBuffer createBuffer(uint32_t allocSize, vk::BufferUsageFlagBits bufferUsage);
        void createInputImage(const char *file);
        void createOutputImage();

        vk::Device getDevice() { return _vkDevice; };

        AllocatedImage inputImage;
        AllocatedImage outputImage;
    private:

        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        bool loadImageFromFile(const char *file, void *pixels, size_t &size, int &width, int &height, int &channels);

        AllocatedImage createImage(uint32_t width, uint32_t height, bool transfer);
        void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
        void copyBufferToImage(VkBuffer buffer, AllocatedImage &image);

        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);

        vk::Instance _vkInstance;
        vk::PhysicalDevice _vkPhysicalDevice;
        vk::PhysicalDeviceProperties _vkPhysicalDeviceProperties;
        vk::PhysicalDeviceLimits _vkPhysicalDeviceLimits;
        uint32_t _computeQueueFamilyIndex;
        vk::Device _vkDevice;
        vk::Queue _computeQueue;

        vk::ShaderModule _shaderModule;

        vk::Pipeline _computePipeline;
        vk::PipelineLayout _pipelineLayout;
        vk::PipelineCache _pipelineCache;
        vk::DescriptorSetLayout _descriptorSetLayout;
        vk::DescriptorPool _descriptorPool;
        std::vector<vk::DescriptorSet> _descriptorSets;
        vk::DescriptorSet _descriptorSet;
        vk::CommandPool _commandPool;
        std::vector<vk::CommandBuffer> _commandBuffers;
        vk::CommandBuffer _commandBuffer;

        vk::Fence _fence;
    };
}