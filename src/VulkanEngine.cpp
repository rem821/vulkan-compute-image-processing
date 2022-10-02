//
// Created by Stanislav Svědiroh on 20.09.2022.
//
#include "VulkanEngine.h"

#include <iostream>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION

#include "../external/stb/stb_image.h"

namespace VulkanCompute {

    VulkanEngine::VulkanEngine() {
        createVulkanDevice();
        createCommandPool();
        createInputImage("./assets/input.png");
        createOutputImage();
        createDescriptorSetLayout();
        createShaderModuleFromFile("shaders/EdgeDetect.comp.spv");
        createPipeline();
        createDescriptorPool();
        createDescriptorSet();
        createCommandBuffer();
    }

    VulkanEngine::~VulkanEngine() {
        vkDeviceWaitIdle(_vkDevice);

        _vkDevice.destroyImageView(inputImage.view);
        _vkDevice.destroyImage(inputImage._image);
        _vkDevice.destroySampler(inputImage.sampler);
        _vkDevice.freeMemory(inputImage._imageMemory);

        _vkDevice.destroyImageView(outputImage.view);
        _vkDevice.destroyImage(outputImage._image);
        _vkDevice.destroySampler(outputImage.sampler);
        _vkDevice.freeMemory(outputImage._imageMemory);

        _vkDevice.destroyShaderModule(_shaderModule);
        _vkDevice.resetCommandPool(_commandPool, vk::CommandPoolResetFlags());
        _vkDevice.destroyFence(_fence);
        _vkDevice.destroyDescriptorSetLayout(_descriptorSetLayout);
        _vkDevice.destroyPipelineLayout(_pipelineLayout);
        _vkDevice.destroyPipelineCache(_pipelineCache);
        _vkDevice.destroyPipeline(_computePipeline);
        _vkDevice.destroyDescriptorPool(_descriptorPool);
        _vkDevice.destroyCommandPool(_commandPool);
        _vkDevice.destroy();
        _vkInstance.destroy();
    }

    void VulkanEngine::createVulkanDevice() {
        vk::ApplicationInfo AppInfo{
                "VulkanCompute",    // Application Name
                1,                    // Application Version
                nullptr,            // Engine Name or nullptr
                0,                    // Engine Version
                VK_API_VERSION_1_1  // Vulkan API version
        };

        const std::vector<const char *> Layers = {"VK_LAYER_KHRONOS_validation"};
        vk::InstanceCreateInfo InstanceCreateInfo(vk::InstanceCreateFlags(),    // Flags
                                                  &AppInfo,                     // Application Info
                                                  Layers.size(),                // Layers count
                                                  Layers.data());               // Layers
        _vkInstance = vk::createInstance(InstanceCreateInfo);

        _vkPhysicalDevice = _vkInstance.enumeratePhysicalDevices().front();
        _vkPhysicalDeviceProperties = _vkPhysicalDevice.getProperties();
        std::cout << "Device Name    : " << _vkPhysicalDeviceProperties.deviceName << std::endl;
        const uint32_t ApiVersion = _vkPhysicalDeviceProperties.apiVersion;
        std::cout << "Vulkan Version : " << VK_VERSION_MAJOR(ApiVersion) << "." << VK_VERSION_MINOR(ApiVersion) << "." << VK_VERSION_PATCH(ApiVersion)
                  << std::endl;
        _vkPhysicalDeviceLimits = _vkPhysicalDeviceProperties.limits;
        std::cout << "Max Compute Shared Memory Size: " << _vkPhysicalDeviceLimits.maxComputeSharedMemorySize / 1024 << " KB" << std::endl;

        std::vector<vk::QueueFamilyProperties> QueueFamilyProps = _vkPhysicalDevice.getQueueFamilyProperties();
        auto PropIt = std::find_if(QueueFamilyProps.begin(), QueueFamilyProps.end(), [](const vk::QueueFamilyProperties &Prop) {
            return Prop.queueFlags & vk::QueueFlagBits::eCompute;
        });
        _computeQueueFamilyIndex = std::distance(QueueFamilyProps.begin(), PropIt);
        std::cout << "Compute Queue Family Index: " << _computeQueueFamilyIndex << std::endl;

        // Just to avoid a warning from the Vulkan Validation Layer
        const float QueuePriority = 1.0f;
        vk::DeviceQueueCreateInfo DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), _computeQueueFamilyIndex, 1, &QueuePriority);
        vk::DeviceCreateInfo DeviceCreateInfo(vk::DeviceCreateFlags(), DeviceQueueCreateInfo);
        _vkDevice = _vkPhysicalDevice.createDevice(DeviceCreateInfo);

        _computeQueue = _vkDevice.getQueue(_computeQueueFamilyIndex, 0);
    }

    bool VulkanEngine::loadImageFromFile(const char *file, void *pixels, size_t &size, int &width, int &height, int &channels) {
        stbi_uc *px = stbi_load(file, &width, &height, nullptr, STBI_rgb_alpha);

        channels = 4;
        size = width * height * channels;
        if (!px || !pixels) {
            std::cout << "Failed to load texture file " << file << std::endl;
            return false;
        }
        pixels = px;

        return true;
    }


    uint32_t VulkanEngine::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(_vkPhysicalDevice, &memProperties);

        uint32_t memTypeIndex = uint32_t(~0);
        vk::DeviceSize memHeapSize = uint32_t(~0);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
            VkMemoryType memType = memProperties.memoryTypes[i];
            if ((typeFilter & (1 << i)) && (memType.propertyFlags & properties) == properties) {
                memHeapSize = memProperties.memoryHeaps[memType.heapIndex].size;
                memTypeIndex = i;
                break;
            }
        }

        std::cout << "Memory Type Index: " << memTypeIndex << std::endl;
        std::cout << "Memory Heap Size : " << memHeapSize / 1024 / 1024 / 1024 << " GB" << std::endl;

        return memTypeIndex;
    }

    AllocatedBuffer VulkanEngine::createBuffer(uint32_t allocSize, vk::BufferUsageFlagBits bufferUsage) {
        vk::BufferCreateInfo bufferInfo{
                vk::BufferCreateFlags(),
                allocSize,
                bufferUsage,
                vk::SharingMode::eExclusive,
                1,
                &_computeQueueFamilyIndex
        };

        AllocatedBuffer newBuffer;
        newBuffer._buffer = _vkDevice.createBuffer(bufferInfo);

        vk::MemoryRequirements bufferMemoryRequirements = _vkDevice.getBufferMemoryRequirements(newBuffer._buffer);
        vk::PhysicalDeviceMemoryProperties MemoryProperties = _vkPhysicalDevice.getMemoryProperties();

        uint32_t memTypeIndex = findMemoryType(bufferMemoryRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vk::MemoryAllocateInfo bufferMemoryAllocateInfo(bufferMemoryRequirements.size, memTypeIndex);
        newBuffer._bufferMemory = _vkDevice.allocateMemory(bufferMemoryAllocateInfo);
        _vkDevice.bindBufferMemory(newBuffer._buffer, newBuffer._bufferMemory, 0);

        return newBuffer;
    }

    void VulkanEngine::createInputImage(const char *file) {
        size_t size;
        int32_t width, height, channels;
        loadImageFromFile(file, nullptr, size, width, height, channels);
        auto *pixels = new int8_t[size];
        loadImageFromFile(file, pixels, size, width, height, channels);

        VkDeviceSize imageSize = size;

        VulkanCompute::AllocatedBuffer stagingBuffer = createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc);

        void *data = _vkDevice.mapMemory(stagingBuffer._bufferMemory, 0, size);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        _vkDevice.unmapMemory(stagingBuffer._bufferMemory);

        stbi_image_free(pixels);

        inputImage = createImage(width, height, true);

        transitionImageLayout(inputImage._image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer._buffer, inputImage);
        inputImage.imagelayout = VK_IMAGE_LAYOUT_GENERAL;
        transitionImageLayout(inputImage._image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, inputImage.imagelayout);

        vkDestroyBuffer(_vkDevice, stagingBuffer._buffer, nullptr);
        vkFreeMemory(_vkDevice, stagingBuffer._bufferMemory, nullptr);

        VkSamplerCreateInfo samplerCreateInfo = {};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = 0.0f;
        // Only enable anisotropic filtering if enabled on the device
        samplerCreateInfo.maxAnisotropy = 1.0f;
        samplerCreateInfo.anisotropyEnable = false;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        vkCreateSampler(_vkDevice, &samplerCreateInfo, nullptr, &inputImage.sampler);

        VkImageViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCreateInfo.pNext = nullptr;
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewCreateInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
        viewCreateInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.image = inputImage._image;
        vkCreateImageView(_vkDevice, &viewCreateInfo, nullptr, &inputImage.view);

        inputImage.descriptor.sampler = inputImage.sampler;
        inputImage.descriptor.imageView = inputImage.view;
        inputImage.descriptor.imageLayout = inputImage.imagelayout;
    }

    void VulkanEngine::createOutputImage() {
        VkFormatProperties formatProperties;
        // Get device properties for the requested texture format
        vkGetPhysicalDeviceFormatProperties(_vkPhysicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &formatProperties);
        // Check if requested image format supports image storage operations
        assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

        outputImage = createImage(inputImage.width, inputImage.height, false);

        outputImage.imagelayout = VK_IMAGE_LAYOUT_GENERAL;
        transitionImageLayout(outputImage._image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, outputImage.imagelayout);


        VkSamplerCreateInfo samplerCreateInfo = {};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = 0.0f;
        // Only enable anisotropic filtering if enabled on the device
        samplerCreateInfo.maxAnisotropy = 1.0f;
        samplerCreateInfo.anisotropyEnable = false;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        vkCreateSampler(_vkDevice, &samplerCreateInfo, nullptr, &outputImage.sampler);


        VkImageViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCreateInfo.pNext = NULL;
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewCreateInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
        viewCreateInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.image = outputImage._image;
        vkCreateImageView(_vkDevice, &viewCreateInfo, nullptr, &outputImage.view);

        // Initialize a descriptor for later use
        outputImage.descriptor.imageLayout = outputImage.imagelayout;
        outputImage.descriptor.imageView = outputImage.view;
        outputImage.descriptor.sampler = outputImage.sampler;
    }

    AllocatedImage VulkanEngine::createImage(uint32_t width, uint32_t height, bool transfer) {
        VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;

        VkImageCreateInfo imgCreateInfo = {};
        imgCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgCreateInfo.pNext = nullptr;
        imgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imgCreateInfo.format = image_format;
        imgCreateInfo.extent = {uint32_t(width), uint32_t(height), 1};
        imgCreateInfo.mipLevels = 1;
        imgCreateInfo.arrayLayers = 1;
        imgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        if (transfer) {
            imgCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        VulkanCompute::AllocatedImage newImage;

        vkCreateImage(_vkDevice, &imgCreateInfo, nullptr, &newImage._image);
        newImage.width = width;
        newImage.height = height;
        newImage.channels = 4;

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(_vkDevice, newImage._image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = VulkanCompute::VulkanEngine::findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT); //TODO: DEVICE LOCAL

        vkAllocateMemory(_vkDevice, &allocInfo, nullptr, &newImage._imageMemory);
        vkBindImageMemory(_vkDevice, newImage._image, newImage._imageMemory, 0);

        return newImage;
    }

    void VulkanEngine::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;


        VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
            barrier.srcAccessMask = 0;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else {
            throw std::invalid_argument("unsupported source layout transition!");
        }

        if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; //TODO: ??
        } else {
            std::cout << "Unsupported destination layout transition!" << std::endl;
        }

        vkCmdPipelineBarrier(
                commandBuffer,
                sourceStage, destinationStage,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
        );

        endSingleTimeCommands(commandBuffer);
    }

    void VulkanEngine::copyBufferToImage(VkBuffer buffer, AllocatedImage &image) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.bufferOffset = 0;
        region.imageExtent = {uint32_t(image.width), uint32_t(image.height), 1};

        vkCmdCopyBufferToImage(commandBuffer, buffer, image._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(commandBuffer);
    }


    void VulkanEngine::createShaderModuleFromFile(std::string fileName) {
        std::vector<char> ShaderContents;
        if (std::ifstream ShaderFile{fileName, std::ios::binary | std::ios::ate}) {
            const size_t FileSize = ShaderFile.tellg();
            ShaderFile.seekg(0);
            ShaderContents.resize(FileSize, '\0');
            ShaderFile.read(ShaderContents.data(), FileSize);
        }

        vk::ShaderModuleCreateInfo ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(),
                                                          ShaderContents.size(),
                                                          reinterpret_cast<const uint32_t *>(ShaderContents.data()));
        _shaderModule = _vkDevice.createShaderModule(ShaderModuleCreateInfo);
    }

    void VulkanEngine::createDescriptorSetLayout() {
        const std::vector<vk::DescriptorSetLayoutBinding> DescriptorSetLayoutBinding = {
                {0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute},
                {1, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute}
        };
        vk::DescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), DescriptorSetLayoutBinding);
        _descriptorSetLayout = _vkDevice.createDescriptorSetLayout(DescriptorSetLayoutCreateInfo);
    }

    void VulkanEngine::createPipeline() {
        vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), _descriptorSetLayout);
        _pipelineLayout = _vkDevice.createPipelineLayout(PipelineLayoutCreateInfo);
        _pipelineCache = _vkDevice.createPipelineCache(vk::PipelineCacheCreateInfo());

        vk::PipelineShaderStageCreateInfo PipelineShaderCreateInfo(vk::PipelineShaderStageCreateFlags(),
                                                                   vk::ShaderStageFlagBits::eCompute,
                                                                   _shaderModule,
                                                                   "main");
        vk::ComputePipelineCreateInfo ComputePipelineCreateInfo(vk::PipelineCreateFlags(),
                                                                PipelineShaderCreateInfo,
                                                                _pipelineLayout);
        vk::Pipeline ComputePipeline = _vkDevice.createComputePipeline(_pipelineCache, ComputePipelineCreateInfo);
        _computePipeline = ComputePipeline;
    }

    void VulkanEngine::createCommandPool() {
        vk::CommandPoolCreateInfo CommandPoolCreateInfo(vk::CommandPoolCreateFlags(), _computeQueueFamilyIndex);
        _commandPool = _vkDevice.createCommandPool(CommandPoolCreateInfo);
    }

    void VulkanEngine::createDescriptorPool() {
        vk::DescriptorPoolSize DescriptorPoolSize(vk::DescriptorType::eStorageImage, 2);
        vk::DescriptorPoolCreateInfo DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlags(), 1, DescriptorPoolSize);
        _descriptorPool = _vkDevice.createDescriptorPool(DescriptorPoolCreateInfo);
    }

    void VulkanEngine::createDescriptorSet() {
        vk::DescriptorSetAllocateInfo DescriptorSetAllocInfo(_descriptorPool, 1, &_descriptorSetLayout);
        _descriptorSets = _vkDevice.allocateDescriptorSets(DescriptorSetAllocInfo);
        _descriptorSet = _descriptorSets.front();

        VkDescriptorImageInfo inImageInfo = {};
        inImageInfo.sampler = inputImage.sampler;
        inImageInfo.imageView = inputImage.view;
        inImageInfo.imageLayout = inputImage.imagelayout;

        VkDescriptorImageInfo outImageInfo = {};
        outImageInfo.sampler = outputImage.sampler;
        outImageInfo.imageView = outputImage.view;
        outImageInfo.imageLayout = outputImage.imagelayout;

        std::vector<VkWriteDescriptorSet> writeDescriptorSets(2);
        writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[0].dstSet = _descriptorSet;
        writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writeDescriptorSets[0].dstBinding = 0;
        writeDescriptorSets[0].dstArrayElement = 0;
        writeDescriptorSets[0].descriptorCount = 1;
        writeDescriptorSets[0].pImageInfo = &inputImage.descriptor;

        writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[1].dstSet = _descriptorSet;
        writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writeDescriptorSets[1].dstBinding = 1;
        writeDescriptorSets[1].dstArrayElement = 0;
        writeDescriptorSets[1].descriptorCount = 1;
        writeDescriptorSets[1].pImageInfo = &outputImage.descriptor;

        vkUpdateDescriptorSets(_vkDevice, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
    }

    void VulkanEngine::createCommandBuffer() {
        vk::CommandBufferAllocateInfo CommandBufferAllocInfo(_commandPool, vk::CommandBufferLevel::ePrimary, 1);
        _commandBuffers = _vkDevice.allocateCommandBuffers(CommandBufferAllocInfo);
        _commandBuffer = _commandBuffers.front();

        vk::CommandBufferBeginInfo CmdBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        _commandBuffer.begin(CmdBufferBeginInfo);
        _commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, _computePipeline);
        _commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, _pipelineLayout, 0, {_descriptorSet}, {});
        _commandBuffer.dispatch(inputImage.width / 16, inputImage.height / 16, 1);
        _commandBuffer.end();
    }

    VkCommandBuffer VulkanEngine::beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = _commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(_vkDevice, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void VulkanEngine::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(_computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(_computeQueue);

        vkFreeCommandBuffers(_vkDevice, _commandPool, 1, &commandBuffer);
    }

    void VulkanEngine::submitCommandBuffer() {
        vk::Queue Queue = _vkDevice.getQueue(_computeQueueFamilyIndex, 0);
        _fence = _vkDevice.createFence(vk::FenceCreateInfo());

        vk::SubmitInfo SubmitInfo(0,            // Num Wait Semaphores
                                  nullptr,        // Wait Semaphores
                                  nullptr,        // Pipeline Stage Flags
                                  1,            // Num Command Buffers
                                  &_commandBuffer);  // List of command buffers
        Queue.submit({SubmitInfo}, _fence);
        _vkDevice.waitForFences({_fence},            // List of fences
                                true,                // Wait All
                                uint64_t(-1));        // Timeout
    }
}