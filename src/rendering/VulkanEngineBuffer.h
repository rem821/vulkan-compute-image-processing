//
// Created by Stanislav Svědiroh on 15.06.2022.
//
#pragma once

#include "VulkanEngineDevice.h"

#include <cassert>
#include <cstring>

class VulkanEngineBuffer {
public:
    VulkanEngineBuffer(
            VulkanEngineDevice &device,
            VkDeviceSize instanceSize,
            uint32_t instanceCount,
            VkBufferUsageFlags usageFlags,
            VkMemoryPropertyFlags memoryPropertyFlags,
            VkDeviceSize minOffsetAlignment = 1);
    ~VulkanEngineBuffer();

    VulkanEngineBuffer(const VulkanEngineBuffer &) = delete;
    VulkanEngineBuffer &operator=(const VulkanEngineBuffer &) = delete;

    VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void unmap();

    void writeToBuffer(void *data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    void writeToIndex(void *data, int index);
    VkResult flushIndex(int index);
    VkDescriptorBufferInfo descriptorInfoForIndex(int index);
    VkResult invalidateIndex(int index);

    VkBuffer* getBuffer() { return &buffer; }

    void *getMappedMemory() const { return mapped; }

    uint32_t getInstanceCount() const { return instanceCount; }

    VkDeviceSize getInstanceSize() const { return instanceSize; }

    VkDeviceSize getAlignmentSize() const { return instanceSize; }

    VkBufferUsageFlags getUsageFlags() const { return usageFlags; }

    VkMemoryPropertyFlags getMemoryPropertyFlags() const { return memoryPropertyFlags; }

    VkDeviceSize getBufferSize() const { return bufferSize; }

    VkDescriptorBufferInfo& getBufferInfo() { return bufferInfo; }

private:
    static VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

    VulkanEngineDevice &engineDevice;
    void *mapped = nullptr;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    VkDeviceSize instanceSize;
    VkDeviceSize bufferSize;
    uint32_t instanceCount;
    VkDeviceSize alignmentSize;
    VkBufferUsageFlags usageFlags;
    VkMemoryPropertyFlags memoryPropertyFlags;
    VkDescriptorBufferInfo bufferInfo;
};

