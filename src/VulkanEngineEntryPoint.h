//
// Created by Stanislav Svědiroh on 27.09.2022.
//
#pragma once

#include "rendering/VulkanEngineWindow.h"
#include "rendering/VulkanEngineDevice.h"
#include "rendering/VulkanEngineRenderer.h"
#include "rendering/VulkanEngineDescriptors.h"
#include "rendering/VulkanEngineBuffer.h"
#include "rendering/VulkanTexture.h"
#include "rendering/Camera.h"

#include "glm/glm.hpp"

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define WINDOW_TITLE "Vulkan Compute Render Window"

struct Vertex {
    float pos[3];
    float uv[2];
};

class VulkanEngineEntryPoint {
public:
    struct {
        VkPipelineVertexInputStateCreateInfo inputState;
        std::vector<VkVertexInputBindingDescription> bindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    } vertices;

    struct {
        glm::mat4 projection;
        glm::mat4 modelView;
    } uboVS;

    // Resources for the graphics part of the example
    struct {
        VkDescriptorSetLayout descriptorSetLayout;    // Image display shader binding layout
        VkDescriptorSet descriptorSetPreCompute;    // Image display shader bindings before compute shader image manipulation
        VkDescriptorSet descriptorSetPostCompute;    // Image display shader bindings after compute shader image manipulation
        VkPipeline pipeline;                        // Image display pipeline
        VkPipelineLayout pipelineLayout;            // Layout of the graphics pipeline
    } graphics;

    // Resources for the compute part of the example
    struct Compute {
        VkDescriptorSetLayout descriptorSetLayout;    // Compute shader binding layout
        VkDescriptorSet descriptorSet;                // Compute shader bindings
        VkPipelineLayout pipelineLayout;            // Layout of the compute pipeline
        std::vector<VkPipeline> pipelines;            // Compute pipelines for image filters
        int32_t pipelineIndex = 0;                    // Current image filtering compute pipeline index
    } compute;

    VulkanEngineEntryPoint();
    ~VulkanEngineEntryPoint();

    void prepareInputImage();
    bool loadImageFromFile(const char *file, void *pixels, size_t &size, int &width, int &height, int &channels);
    void generateQuad();
    void setupVertexDescriptions();
    void prepareUniformBuffers();
    void updateUniformBuffers();
    void setupDescriptorSetLayout();
    void preparePipelines();
    void setupDescriptorPool();
    void setupDescriptorSet();
    void prepareCompute();

    void render();

    bool isRunning = false;
private:

    VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);
    VkShaderModule loadShaderModule(const char *fileName, VkDevice device);


    VulkanEngineWindow window{WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE};
    VulkanEngineDevice engineDevice{window, WINDOW_TITLE};
    VulkanEngineRenderer renderer{window, engineDevice};
    Camera camera{};

    Texture2D inputTexture;
    Texture2D outputTexture;

    std::unique_ptr<VulkanEngineBuffer> vertexBuffer;
    std::unique_ptr<VulkanEngineBuffer> indexBuffer;

    std::unique_ptr<VulkanEngineBuffer> uniformBufferVS;

    std::vector<VkShaderModule> shaderModules;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    std::vector<std::string> shaderNames;

    uint32_t indexCount;
};


