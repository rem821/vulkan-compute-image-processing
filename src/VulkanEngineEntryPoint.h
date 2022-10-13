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

#include "opencv4/opencv2/opencv.hpp"

#include "glm/glm.hpp"

#define VIDEO_DOWNSCALE_FACTOR 4
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define WINDOW_TITLE "Vulkan Compute Render Window"
#define IMAGE_PATH "../assets/image.jpg"
//#define VIDEO_PATH "/home/standa/3_1_1_1/camera_left_front/video.mp4"
#define VIDEO_PATH "/mnt/B0E0DAB9E0DA84CE/BUD/3_1_1_1/camera_left_front/video.mp4"
//#define VIDEO_PATH "../assets/video.mp4"
#define WORKGROUP_COUNT (1024 / 32)
#define PLAY_VIDEO true
#define RENDERDOC_ENABLED false
#define SWEEP_FRAMES 20

#define DARK_CHANNEL_PRIOR "DarkChannelPrior"
#define MAXIMUM_AIRLIGHT "MaximumAirLight"

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
    } uboVertexShader;

    struct {
        glm::int32_t groupCount;
        glm::float32_t omega;
    } computePushConstant;

    struct {
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSetPreCompute;
        VkDescriptorSet descriptorSetPostCompute;
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
    } graphics;

    struct Compute {
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSet;
        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
    };

    VulkanEngineEntryPoint();
    ~VulkanEngineEntryPoint() = default;

    void prepareInputImage();
    static bool loadImageFromFile(const std::string& file, void *pixels, size_t &size, int &width, int &height, int &channels);
    void generateQuad();
    void setupVertexDescriptions();
    void prepareVertexUniformBuffer();
    void updateVertexUniformBuffer();
    void setupDescriptorSetLayout();
    void prepareGraphicsPipeline();
    void setupDescriptorPool();
    void setupDescriptorSet();
    void prepareCompute();

    void updateComputeDescriptorSets();
    void updateGraphicsDescriptorSets();

    void render();
    void handleEvents();

    void saveScreenshot(const char *filename);

    bool isRunning = false;
    uint32_t frameIndex = 0;
private:

    VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);
    VkShaderModule loadShaderModule(const char *fileName, VkDevice device);

    void prepareComputePipeline(std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings, const std::string& shaderName);

    void getMaxAirlight();

    VulkanEngineWindow window{WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE};
    VulkanEngineDevice engineDevice{window, WINDOW_TITLE};
    VulkanEngineRenderer renderer{window, engineDevice};
    Camera camera{};

    cv::VideoCapture video{VIDEO_PATH };
    double lastReadFrame = -1;
    double totalFrames = video.get(cv::CAP_PROP_FRAME_COUNT);

    Texture2D inputTexture;
    Texture2D darkChannelTexture;
    Texture2D outputTexture;

    std::unique_ptr<VulkanEngineBuffer> vertexBuffer;
    std::unique_ptr<VulkanEngineBuffer> indexBuffer;

    std::unique_ptr<VulkanEngineBuffer> uniformBufferVertexShader;
    std::unique_ptr<VulkanEngineBuffer> uniformBufferComputeShader;
    std::unique_ptr<VulkanEngineBuffer> airLightGroupsBuffer;
    std::unique_ptr<VulkanEngineBuffer> airLightMaxBuffer;

    std::vector<VkShaderModule> shaderModules;

    std::vector<Compute> compute;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    std::vector<std::string> shaderNames;

    uint32_t indexCount;
};


