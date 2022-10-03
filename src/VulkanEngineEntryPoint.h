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

#include "opencv4/opencv2/opencv.hpp"

#include "glm/glm.hpp"

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define WINDOW_TITLE "Vulkan Compute Render Window"
//#define VIDEO_PATH "/home/standa/3_1_1_1/camera_ir/video.mp4"
#define VIDEO_PATH "../assets/video.mp4"
#define VIDEO_DOWNSCALE_FACTOR 1
#define SHADER_NAME "DarkChannelPrior"
#define WORKGROUP_COUNT 16
#define PLAY_VIDEO false

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

    void prepareInputImage(bool playVideo);
    bool loadImageFromFile(const std::string& file, void *pixels, size_t &size, int &width, int &height, int &channels);
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
    void handleEvents();

    void updateComputeDescriptorSets();

    void saveScreenshot(const char *filename);

    bool isRunning = false;
    uint32_t frameIndex = 0;
    uint32_t totalFrames = -1;
private:

    VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);
    VkShaderModule loadShaderModule(const char *fileName, VkDevice device);

    void insertImageMemoryBarrier(
            VkCommandBuffer cmdbuffer,
            VkImage image,
            VkAccessFlags srcAccessMask,
            VkAccessFlags dstAccessMask,
            VkImageLayout oldImageLayout,
            VkImageLayout newImageLayout,
            VkPipelineStageFlags srcStageMask,
            VkPipelineStageFlags dstStageMask,
            VkImageSubresourceRange subresourceRange);


    VulkanEngineWindow window{WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE};
    VulkanEngineDevice engineDevice{window, WINDOW_TITLE};
    VulkanEngineRenderer renderer{window, engineDevice};
    Camera camera{};

    cv::VideoCapture video{VIDEO_PATH };

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


