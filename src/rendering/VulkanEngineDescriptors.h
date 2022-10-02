//
// Created by Stanislav Svědiroh on 15.06.2022.
//
#pragma once

#include "VulkanEngineDevice.h"

#include <memory>
#include <unordered_map>
#include <vector>
#include <cassert>
#include <stdexcept>


class VulkanEngineDescriptorSetLayout {
public:
    class Builder {
    public:
        Builder(VulkanEngineDevice &device) : engineDevice{device} {}

        Builder &addBinding(
                uint32_t binding,
                VkDescriptorType descriptorType,
                VkShaderStageFlags stageFlags,
                uint32_t count = 1);
        std::unique_ptr<VulkanEngineDescriptorSetLayout> build() const;

    private:
        VulkanEngineDevice &engineDevice;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{};
    };

    VulkanEngineDescriptorSetLayout(VulkanEngineDevice &device, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
    ~VulkanEngineDescriptorSetLayout();
    VulkanEngineDescriptorSetLayout(const VulkanEngineDescriptorSetLayout &) = delete;
    VulkanEngineDescriptorSetLayout &operator=(const VulkanEngineDescriptorSetLayout &) = delete;

    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> getBindings() { return bindings; };
    VkDescriptorSetLayoutBinding& getBindingAt(uint32_t index) { return bindings[index]; };

private:
    VulkanEngineDevice &engineDevice;
    VkDescriptorSetLayout descriptorSetLayout;
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

    friend class LveDescriptorWriter;
};

class VulkanEngineDescriptorPool {
public:
    class Builder {
    public:
        Builder(VulkanEngineDevice &device) : engineDevice{device} {}

        Builder &addPoolSize(VkDescriptorType descriptorType, uint32_t count);
        Builder &setPoolFlags(VkDescriptorPoolCreateFlags flags);
        Builder &setMaxSets(uint32_t count);
        std::unique_ptr<VulkanEngineDescriptorPool> build() const;

    private:
        VulkanEngineDevice &engineDevice;
        std::vector<VkDescriptorPoolSize> poolSizes{};
        uint32_t maxSets = 1000;
        VkDescriptorPoolCreateFlags poolFlags = 0;
    };

    VulkanEngineDescriptorPool(
            VulkanEngineDevice &device,
            uint32_t maxSets,
            VkDescriptorPoolCreateFlags poolFlags,
            const std::vector<VkDescriptorPoolSize> &poolSizes);
    ~VulkanEngineDescriptorPool();
    VulkanEngineDescriptorPool(const VulkanEngineDescriptorPool &) = delete;
    VulkanEngineDescriptorPool &operator=(const VulkanEngineDescriptorPool &) = delete;

    bool allocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const;

    void freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const;


    VkDescriptorPool getPool() { return descriptorPool; }

    void resetPool();

    VulkanEngineDevice& getEngineDevice() { return engineDevice; };

private:
    VulkanEngineDevice &engineDevice;
    VkDescriptorPool descriptorPool;

    friend class LveDescriptorWriter;
};

class VulkanEngineDescriptorWriter {
public:
    VulkanEngineDescriptorWriter(VulkanEngineDescriptorSetLayout &setLayout, VulkanEngineDescriptorPool &pool);

    VulkanEngineDescriptorWriter &writeBuffer(uint32_t binding, VkDescriptorBufferInfo *bufferInfo);
    VulkanEngineDescriptorWriter &writeImage(uint32_t binding, VkDescriptorImageInfo *imageInfo);

    bool build(VkDescriptorSet &set);
    void overwrite(VkDescriptorSet &set);

private:
    VulkanEngineDescriptorSetLayout &setLayout;
    VulkanEngineDescriptorPool &pool;
    std::vector<VkWriteDescriptorSet> writes;
};
