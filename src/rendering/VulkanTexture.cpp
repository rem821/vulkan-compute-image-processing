/*
* Vulkan texture loader
*
* Copyright(C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license(MIT) (http://opensource.org/licenses/MIT)
*/

#include "VulkanTexture.h"
#include "VulkanTools.h"
#include <cassert>

void Texture::updateDescriptor() {
    descriptor.sampler = sampler;
    descriptor.imageView = view;
    descriptor.imageLayout = imageLayout;
}

void Texture::destroy(VulkanEngineDevice &device) {
    vkDestroyImageView(device.getDevice(), view, nullptr);
    view = nullptr;
    vkDestroyImage(device.getDevice(), image, nullptr);
    image = nullptr;
    if (sampler) {
        vkDestroySampler(device.getDevice(), sampler, nullptr);
        sampler = nullptr;
    }
    vkFreeMemory(device.getDevice(), deviceMemory, nullptr);
    imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    deviceMemory = nullptr;
}

/**
* Creates a 2D texture from a buffer
*
* @param buffer Buffer containing texture data to upload
* @param bufferSize Size of the buffer in machine units
* @param width Width of the texture to create
* @param height Height of the texture to create
* @param format Vulkan format of the image data stored in the file
* @param device Vulkan device to create the texture on
* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
* @param (Optional) filter Texture filtering for the sampler (defaults to VK_FILTER_LINEAR)
* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
*/
void
Texture2D::fromImageFile(void *buffer, VkDeviceSize bufferSize, VkFormat format, uint32_t texWidth, uint32_t texHeight,
                         VulkanEngineDevice &device,
                         VkQueue copyQueue, VkFilter filter, VkImageUsageFlags imageUsageFlags,
                         VkImageLayout imageLayout) {
    assert(buffer);
    VkMemoryAllocateInfo memAllocInfo{};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkMemoryRequirements memReqs;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = texWidth;
    bufferCopyRegion.imageExtent.height = texHeight;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegion.bufferOffset = 0;

    // Create a host-visible staging buffer that contains the raw image data
    {
        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = bufferSize;
        // This buffer is used as a transfer source for the buffer copy
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(device.getDevice(), &bufferCreateInfo, nullptr, &stagingBuffer));
        // Get memory requirements for the staging buffer (alignment, memory type bits)
        vkGetBufferMemoryRequirements(device.getDevice(), stagingBuffer, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;
        // Get memory type index for a host visible buffer
        memAllocInfo.memoryTypeIndex = device.findMemoryType(memReqs.memoryTypeBits,
                                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VK_CHECK(vkAllocateMemory(device.getDevice(), &memAllocInfo, nullptr, &stagingMemory));
        VK_CHECK(vkBindBufferMemory(device.getDevice(), stagingBuffer, stagingMemory, 0));

        // Copy texture data into staging buffer
        uint8_t *data;
        VK_CHECK(vkMapMemory(device.getDevice(), stagingMemory, 0, memReqs.size, 0, (void **) &data));
        memcpy(data, buffer, bufferSize);
        vkUnmapMemory(device.getDevice(), stagingMemory);
    }

    // In case the ImageView doesn't yet exist
    if (width != texWidth && height != texHeight) {
        width = texWidth;
        height = texHeight;
        mipLevels = 1;

        // Create optimal tiled target image
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = mipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = {width, height, 1};
        imageCreateInfo.usage = imageUsageFlags;
        // Ensure that the TRANSFER_DST bit is set for staging
        if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        VK_CHECK(vkCreateImage(device.getDevice(), &imageCreateInfo, nullptr, &image));
        vkGetImageMemoryRequirements(device.getDevice(), image, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;

        memAllocInfo.memoryTypeIndex = device.findMemoryType(memReqs.memoryTypeBits,
                                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(device.getDevice(), &memAllocInfo, nullptr, &deviceMemory));
        VK_CHECK(vkBindImageMemory(device.getDevice(), image, deviceMemory, 0));

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mipLevels;
        subresourceRange.layerCount = 1;

        // Create sampler
        VkSamplerCreateInfo samplerCreateInfo = {};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = filter;
        samplerCreateInfo.minFilter = filter;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = 0.0f;
        samplerCreateInfo.maxAnisotropy = 1.0f;
        VK_CHECK(vkCreateSampler(device.getDevice(), &samplerCreateInfo, nullptr, &sampler));

        // Create image view
        VkImageViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCreateInfo.pNext = nullptr;
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = format;
        viewCreateInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                                     VK_COMPONENT_SWIZZLE_A};
        viewCreateInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.image = image;
        VK_CHECK(vkCreateImageView(device.getDevice(), &viewCreateInfo, nullptr, &view));

    }

    // Use a separate command buffer for texture loading
    VkCommandBuffer copyCmd = device.beginSingleTimeCommands();

    // Image barrier for optimal image (target)
    // Optimal image will be used as destination for the copy
    setImageLayout(
            copyCmd,
            image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy mip levels from staging buffer
    vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &bufferCopyRegion
    );

    // Change texture image layout to shader read after all mip levels have been copied
    this->imageLayout = imageLayout;
    setImageLayout(
            copyCmd,
            image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            imageLayout);

    device.endSingleTimeCommands(copyCmd, copyQueue);

    // Clean up staging resources
    vkFreeMemory(device.getDevice(), stagingMemory, nullptr);
    vkDestroyBuffer(device.getDevice(), stagingBuffer, nullptr);

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();
}

void Texture2D::createTextureTarget(VulkanEngineDevice &engineDevice, Texture2D inputTexture) {
    VkFormatProperties formatProperties;

    // Get device properties for the requested texture format
    vkGetPhysicalDeviceFormatProperties(engineDevice.getPhysicalDevice(), VK_FORMAT_R8G8B8A8_UNORM, &formatProperties);
    // Check if requested image format supports image storage operations
    assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

    // Prepare blit target texture
    width = inputTexture.width;
    height = inputTexture.height;
    mipLevels = 1;

    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreateInfo.extent.width = inputTexture.width;
    imageCreateInfo.extent.height = inputTexture.height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    // Image will be sampled in the fragment shader and used as storage target in the compute shader
    imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.flags = 0;
    // If compute and graphics queue family indices differ, we create an image that can be shared between them
    // This can result in worse performance than exclusive sharing mode, but save some synchronization to keep the sample simple
    QueueFamilyIndices _queueFamilyIndices = engineDevice.findPhysicalQueueFamilies();
    std::vector<uint32_t> queueFamilyIndices;
    if (_queueFamilyIndices.graphicsFamily != _queueFamilyIndices.computeFamily) {
        queueFamilyIndices = {
                _queueFamilyIndices.graphicsFamily,
                _queueFamilyIndices.computeFamily
        };
        imageCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
        imageCreateInfo.queueFamilyIndexCount = 2;
        imageCreateInfo.pQueueFamilyIndices = queueFamilyIndices.data();
    }

    VkMemoryAllocateInfo memAllocInfo{};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkMemoryRequirements memReqs;

    VK_CHECK(vkCreateImage(engineDevice.getDevice(), &imageCreateInfo, nullptr, &image));
    vkGetImageMemoryRequirements(engineDevice.getDevice(), image, &memReqs);
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = engineDevice.findMemoryType(memReqs.memoryTypeBits,
                                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(engineDevice.getDevice(), &memAllocInfo, nullptr, &deviceMemory));
    VK_CHECK(vkBindImageMemory(engineDevice.getDevice(), image, deviceMemory, 0));

    VkCommandBuffer layoutCmd = engineDevice.beginSingleTimeCommands();

    imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    setImageLayout(layoutCmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, imageLayout);

    engineDevice.endSingleTimeCommands(layoutCmd, engineDevice.graphicsQueue());

    // Create sampler
    VkSamplerCreateInfo sampler{};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler.addressModeV = sampler.addressModeU;
    sampler.addressModeW = sampler.addressModeU;
    sampler.mipLodBias = 0.0f;
    sampler.maxAnisotropy = 1.0f;
    sampler.compareOp = VK_COMPARE_OP_NEVER;
    sampler.minLod = 0.0f;
    sampler.maxLod = (float) mipLevels;
    sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK(vkCreateSampler(engineDevice.getDevice(), &sampler, nullptr, &this->sampler));

    // Create image view
    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = VK_NULL_HANDLE;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = VK_FORMAT_R8G8B8A8_UNORM;
    view.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                       VK_COMPONENT_SWIZZLE_A}; // Here it doesn't matter rn as the output is usually BW
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    view.image = image;
    VK_CHECK(vkCreateImageView(engineDevice.getDevice(), &view, nullptr, &this->view));

    // Initialize a descriptor for later use
    descriptor.imageLayout = imageLayout;
    descriptor.imageView = this->view;
    descriptor.sampler = this->sampler;
}