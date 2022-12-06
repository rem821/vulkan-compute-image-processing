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
#include "rendering/VulkanTools.h"
#include "GlobalConfiguration.h"
#include "rendering/gui/DebugGui.h"
#include "algorithms/DatasetFileReader.h"

#include "glm/glm.hpp"

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
    } uboVertexShader{};

    struct {
        glm::vec3 vanishingPoint;
    } uboFragmentShader{};

    struct {
        glm::int32_t groupCount;
        glm::int32_t imageWidth;
        glm::int32_t imageHeight;
        glm::float32_t omega;
        alignas(32) glm::float32_t epsilon;
    } computePushConstant{};

    struct {
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSetPreCompute;
        VkDescriptorSet descriptorSetPostComputeStageOne;
        VkDescriptorSet descriptorSetPostComputeStageTwo;
        VkDescriptorSet descriptorSetPostComputeStageThree;
        VkDescriptorSet descriptorSetPostComputeStageFour;
        VkDescriptorSet descriptorSetPostComputeFinal;

        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
    } graphics{};

    struct Compute {
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSet;
        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
    };

    VulkanEngineEntryPoint(Dataset *dataset);

    ~VulkanEngineEntryPoint() {
        inputTexture.destroy(engineDevice);
        darkChannelPriorTexture.destroy(engineDevice);
        transmissionTexture.destroy(engineDevice);
        filteredTransmissionTexture.destroy(engineDevice);
        radianceTexture.destroy(engineDevice);


        for (VkShaderModule module: shaderModules) {
            vkDestroyShaderModule(engineDevice.getDevice(), module, nullptr);
        }

        for (Compute comp: compute) {
            vkDestroyDescriptorSetLayout(engineDevice.getDevice(), comp.descriptorSetLayout, nullptr);
            vkDestroyPipelineLayout(engineDevice.getDevice(), comp.pipelineLayout, nullptr);
            vkDestroyPipeline(engineDevice.getDevice(), comp.pipeline, nullptr);
        }

        vkDestroyPipelineLayout(engineDevice.getDevice(), graphics.pipelineLayout, nullptr);
        vkDestroyPipeline(engineDevice.getDevice(), graphics.pipeline, nullptr);

        vkDestroyDescriptorSetLayout(engineDevice.getDevice(), graphics.descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(engineDevice.getDevice(), descriptorPool, nullptr);
    };

    void prepareInputImage();

    void generateQuad();

    void setupVertexDescriptions();

    void prepareGraphicsUniformBuffers();

    void updateGraphicsUniformBuffers();

    void setupDescriptorSetLayout();

    void prepareGraphicsPipeline();

    void setupDescriptorPool();

    void setupDescriptorSet();

    void prepareCompute();

    void updateComputeDescriptorSets();

    void updateGraphicsDescriptorSets();

    void render();

    void prepareNextFrame();

    void handleEvents();

    void saveScreenshot(const char *filename);

    bool isRunning = false; // If set to false, program will end
    bool isFinished = false; // If set to true, no new data is available and program will stop on last frame
    bool isPaused = false; // If set to false program will stop on the current frame and can be then resumed
    bool isStepping = false; // If set to true, program will step one frame and pause
private:

    VkPipelineShaderStageCreateInfo loadShader(const std::string &fileName, VkShaderStageFlagBits stage);

    static VkShaderModule loadShaderModule(const char *fileName, VkDevice device);

    void
    prepareComputePipeline(std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings, const std::string &shaderName);

    VulkanEngineWindow window{WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT,
                              SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE};
    VulkanEngineDevice engineDevice{window, WINDOW_TITLE};
    VulkanEngineRenderer renderer{window, engineDevice};
    Camera camera{};

#if DEBUG_GUI_ENABLED
    DebugGui debugGui{engineDevice, renderer, window.sdlWindow()};
#endif

    Texture2D inputTexture{};
    Texture2D darkChannelPriorTexture{};
    Texture2D transmissionTexture{};
    Texture2D filteredTransmissionTexture{};
    Texture2D radianceTexture{};

    std::unique_ptr<VulkanEngineBuffer> vertexBuffer;
    std::unique_ptr<VulkanEngineBuffer> indexBuffer;

    std::unique_ptr<VulkanEngineBuffer> uniformBufferVertexShader;
    std::unique_ptr<VulkanEngineBuffer> uniformBufferFragmentShader;
    std::unique_ptr<VulkanEngineBuffer> airLightGroupsBuffer;
    std::unique_ptr<VulkanEngineBuffer> airLightMaxBuffer;

    std::vector<VkShaderModule> shaderModules;

    std::vector<Compute> compute;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    uint32_t indexCount{};

    glm::vec2 mouseDragOrigin{};
    float zoom = 0.0f;
    glm::vec2 panPosition = glm::vec2(0.0f, 0.0f);

    // DatasetFileReader
    Dataset *dataset;
};


