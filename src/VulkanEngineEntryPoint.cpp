//
// Created by Stanislav Svědiroh on 27.09.2022.
//
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "VulkanEngineEntryPoint.h"
#include "profiling/Timer.h"
#include "../external/stb/stb_image.h"
#include "../external/stb/stb_image_write.h"
#include <vulkan/vulkan.hpp>
#include <fmt/core.h>
#include <vector>

VulkanEngineEntryPoint::VulkanEngineEntryPoint() {

    // Init resources
    prepareInputImage();
    darkChannelPriorTexture.createTextureTarget(engineDevice, inputTexture);
    transmissionTexture.createTextureTarget(engineDevice, inputTexture);
    filteredTransmissionTexture.createTextureTarget(engineDevice, inputTexture);
    radianceTexture.createTextureTarget(engineDevice, inputTexture);

    // Buffer holding maximum airlight components for every workgroup
    airLightGroupsBuffer = std::make_unique<VulkanEngineBuffer>(engineDevice, sizeof(float),
                                                                WORKGROUP_COUNT * WORKGROUP_COUNT * 3,
                                                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Buffer holding maximum airlight components of the whole image
    airLightMaxBuffer = std::make_unique<VulkanEngineBuffer>(engineDevice, sizeof(float), 3,
                                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);


    // Graphics
    generateQuad();
    setupVertexDescriptions();
    prepareVertexUniformBuffer();
    setupDescriptorSetLayout();
    prepareGraphicsPipeline();
    setupDescriptorPool();
    setupDescriptorSet();

    camera.setViewYXZ(glm::vec3(0.0f, 0.0f, -2.0f), glm::vec3(0.0f));
    camera.setPerspectiveProjection(glm::radians(60.f), (float) WINDOW_WIDTH * 0.5f / (float) WINDOW_HEIGHT, 1.0f,
                                    256.0f);

    // Compute
    prepareCompute();

    isRunning = true;
}

void VulkanEngineEntryPoint::prepareInputImage() {
    Timer timer("Preparing next video frame");

    if (PLAY_VIDEO && frameIndex < totalFrames) {
        while (lastReadFrame < frameIndex) {
            video.read(cameraFrame);
            lastReadFrame += 1;
        }

        if(!heading.empty()) {
            cameraWindowFrame = cv::Mat(DFT_WINDOW_SIZE, DFT_WINDOW_SIZE * 3, CV_8U, cv::Scalar::all(0));
            cv::Mat windowFrame = getDFTWindow();
            size_t currInd = 0;
            cameraWindowFrame.rows = windowFrame.rows;
            cameraWindowFrame.cols = windowFrame.cols;
            for(size_t i = 0; i < windowFrame.cols * windowFrame.rows * 3; i += 3) {
                cameraWindowFrame.data[i + 0] = windowFrame.data[currInd];
                cameraWindowFrame.data[i + 1] = windowFrame.data[currInd];
                cameraWindowFrame.data[i + 2] = windowFrame.data[currInd];
                currInd ++;
            }
            cameraFrame = cameraWindowFrame;
        }

        int32_t width = cameraFrame.cols;
        int32_t height = cameraFrame.rows;
        int32_t size = width * height * 3;
        /*
        cv::Mat d_frame;
        int32_t d_width = width / VIDEO_DOWNSCALE_FACTOR;
        int32_t d_height = height / VIDEO_DOWNSCALE_FACTOR;

        size_t d_size_rgba = d_width * d_height * 4;
        auto *d_pixels_rgba = new uint8_t[d_size_rgba];

        // Downscale the video according to VIDEO_DOWNSCALE_FACTOR
        cv::resize(cameraFrame, d_frame, cv::Size(d_width, d_height), cv::INTER_LINEAR);

        // Convert from BGR to RGBA
        size_t currInd = 0;
        for (size_t i = 0; i < d_size_rgba; i += 4) {
            d_pixels_rgba[i + 0] = d_frame.data[currInd + 2];
            d_pixels_rgba[i + 1] = d_frame.data[currInd + 1];
            d_pixels_rgba[i + 2] = d_frame.data[currInd + 0];
            d_pixels_rgba[i + 3] = 255;
            currInd += 3;
        }
        */

        size_t d_size_rgba = width * height * 4;
        auto *d_pixels_rgba = new uint8_t[d_size_rgba];

        // Convert from BGR to RGBA
        size_t currInd = 0;
        for (size_t i = 0; i < width * height * 4; i += 4) {
            d_pixels_rgba[i + 0] = cameraFrame.data[currInd + 2];
            d_pixels_rgba[i + 1] = cameraFrame.data[currInd + 1];
            d_pixels_rgba[i + 2] = cameraFrame.data[currInd + 0];
            d_pixels_rgba[i + 3] = 255;
            currInd += 3;
        }

        inputTexture.fromImageFile(d_pixels_rgba, d_size_rgba, VK_FORMAT_R8G8B8A8_UNORM, width, height,
                                   engineDevice,
                                   engineDevice.graphicsQueue(), VK_FILTER_LINEAR,
                                   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                   VK_IMAGE_LAYOUT_GENERAL);

        delete[] d_pixels_rgba;
    } else {
        size_t size;
        int32_t width, height, channels;
        loadImageFromFile(IMAGE_PATH, nullptr, size, width, height, channels);

        auto *pixels = new int8_t[size];
        loadImageFromFile(IMAGE_PATH, pixels, size, width, height, channels);

        inputTexture.fromImageFile(pixels, size, VK_FORMAT_R8G8B8A8_UNORM, width, height, engineDevice,
                                   engineDevice.graphicsQueue(), VK_FILTER_LINEAR,
                                   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                   VK_IMAGE_LAYOUT_GENERAL);

        delete[] pixels;
    }
}

bool VulkanEngineEntryPoint::loadImageFromFile(const std::string &file, void *pixels, size_t &size, int &width,
                                               int &height, int &channels) {
    stbi_uc *px = stbi_load(file.c_str(), &width, &height, nullptr, STBI_rgb_alpha);

    channels = 4;
    size = width * height * channels;
    if (!pixels) return false;
    if (!px) {
        std::cout << "Failed to load texture file " << file << std::endl;
        return false;
    }
    memcpy(pixels, px, size * sizeof(int8_t));

    return true;
}

void VulkanEngineEntryPoint::generateQuad() {
    // Setup vertices for a single uv-mapped quad made from two triangles
    std::vector<Vertex> _vertices =
            {
                    {{1.0f,  1.0f,  0.0f}, {1.0f, 1.0f}},
                    {{-1.0f, 1.0f,  0.0f}, {0.0f, 1.0f}},
                    {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
                    {{1.0f,  -1.0f, 0.0f}, {1.0f, 0.0f}}
            };

    // Setup indices
    std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
    indexCount = static_cast<uint32_t>(indices.size());


    // Vertex buffer
    vertexBuffer = std::make_unique<VulkanEngineBuffer>(engineDevice, sizeof(Vertex), _vertices.size(),
                                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vertexBuffer->map();
    vertexBuffer->writeToBuffer((void *) _vertices.data());


    // Index buffer
    indexBuffer = std::make_unique<VulkanEngineBuffer>(engineDevice, sizeof(uint32_t), indices.size(),
                                                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    indexBuffer->map();
    indexBuffer->writeToBuffer((void *) indices.data());
}

void VulkanEngineEntryPoint::setupVertexDescriptions() {

    // Binding description
    VkVertexInputBindingDescription vInputBindDescription{};
    vInputBindDescription.binding = 0;
    vInputBindDescription.stride = sizeof(Vertex);
    vInputBindDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertices.bindingDescriptions = {vInputBindDescription};

    // Attribute descriptions
    // Describes memory layout and shader positions
    // Location 0: Position
    VkVertexInputAttributeDescription vInputAttribDescription0{};
    vInputAttribDescription0.location = 0;
    vInputAttribDescription0.binding = 0;
    vInputAttribDescription0.format = VK_FORMAT_R32G32B32_SFLOAT;
    vInputAttribDescription0.offset = offsetof(Vertex, pos);

    // Location 1: Texture coordinates
    VkVertexInputAttributeDescription vInputAttribDescription1{};
    vInputAttribDescription1.location = 1;
    vInputAttribDescription1.binding = 0;
    vInputAttribDescription1.format = VK_FORMAT_R32G32_SFLOAT;
    vInputAttribDescription1.offset = offsetof(Vertex, uv);

    vertices.attributeDescriptions = {vInputAttribDescription0, vInputAttribDescription1};

    // Assign to vertex buffer
    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
    pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertices.inputState = pipelineVertexInputStateCreateInfo;
    vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
    vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
    vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
    vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
}

void VulkanEngineEntryPoint::prepareVertexUniformBuffer() {
    uniformBufferVertexShader = std::make_unique<VulkanEngineBuffer>(engineDevice, sizeof(uboVertexShader), 1,
                                                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    uniformBufferVertexShader->map();

    updateVertexUniformBuffer();
}

void VulkanEngineEntryPoint::updateVertexUniformBuffer() {
    uboVertexShader.projection = camera.getProjection();
    uboVertexShader.modelView = camera.getView();
    memcpy(uniformBufferVertexShader->getMappedMemory(), &uboVertexShader, sizeof(uboVertexShader));
}

void VulkanEngineEntryPoint::setupDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding setLayoutBindingVertex{};
    setLayoutBindingVertex.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    setLayoutBindingVertex.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    setLayoutBindingVertex.binding = 0;
    setLayoutBindingVertex.descriptorCount = 1;

    VkDescriptorSetLayoutBinding setLayoutBindingFragment{};
    setLayoutBindingFragment.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    setLayoutBindingFragment.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    setLayoutBindingFragment.binding = 1;
    setLayoutBindingFragment.descriptorCount = 1;

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            // Binding 0: Vertex shader uniform buffer
            setLayoutBindingVertex,
            // Binding 1: Fragment shader input image
            setLayoutBindingFragment
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pBindings = setLayoutBindings.data();
    descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());

    VkDescriptorSetLayoutCreateInfo descriptorLayout = descriptorSetLayoutCreateInfo;
    VK_CHECK(vkCreateDescriptorSetLayout(engineDevice.getDevice(), &descriptorLayout, nullptr,
                                         &graphics.descriptorSetLayout));

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &graphics.descriptorSetLayout;

    VK_CHECK(vkCreatePipelineLayout(engineDevice.getDevice(), &pipelineLayoutCreateInfo, nullptr,
                                    &graphics.pipelineLayout));
}

void VulkanEngineEntryPoint::prepareGraphicsPipeline() {
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{};
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyState.flags = 0;
    inputAssemblyState.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo rasterizationState{};
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode = VK_CULL_MODE_NONE;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.flags = 0;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState blendAttachmentState{};
    blendAttachmentState.colorWriteMask = 0xf;
    blendAttachmentState.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlendState{};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = &blendAttachmentState;

    VkPipelineDepthStencilStateCreateInfo depthStencilState{};
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    viewportState.flags = 0;

    VkPipelineMultisampleStateCreateInfo multisampleState{};
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.flags = 0;

    std::vector<VkDynamicState> dynamicStateEnables = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = dynamicStateEnables.data();
    dynamicState.dynamicStateCount = dynamicStateEnables.size();
    dynamicState.flags = 0;

    // Rendering pipeline
    // Load shaders
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

    shaderStages[0] = loadShader("../shaders/texture.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader("../shaders/texture.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.layout = graphics.pipelineLayout;
    pipelineCreateInfo.renderPass = renderer.getSwapChainRenderPass();
    pipelineCreateInfo.flags = 0;
    pipelineCreateInfo.basePipelineIndex = -1;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

    pipelineCreateInfo.pVertexInputState = &vertices.inputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = shaderStages.size();
    pipelineCreateInfo.pStages = shaderStages.data();
    pipelineCreateInfo.renderPass = renderer.getSwapChainRenderPass();

    VK_CHECK(vkCreateGraphicsPipelines(engineDevice.getDevice(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
                                       &graphics.pipeline));
}

void VulkanEngineEntryPoint::setupDescriptorPool() {
    VkDescriptorPoolSize uniformBuffers{};
    uniformBuffers.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBuffers.descriptorCount = 100;

    VkDescriptorPoolSize imageSamplers{};
    imageSamplers.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    imageSamplers.descriptorCount = 100;

    VkDescriptorPoolSize storageImage{};
    storageImage.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    storageImage.descriptorCount = 100;

    VkDescriptorPoolSize storageBuffer{};
    storageBuffer.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    storageBuffer.descriptorCount = 100;

    std::vector<VkDescriptorPoolSize> poolSizes = {
            uniformBuffers,
            imageSamplers,
            storageImage,
            storageBuffer,
    };

    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolInfo.pPoolSizes = poolSizes.data();
    descriptorPoolInfo.maxSets = 50;

    VK_CHECK(vkCreateDescriptorPool(engineDevice.getDevice(), &descriptorPoolInfo, nullptr, &descriptorPool));
}

void VulkanEngineEntryPoint::setupDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.pSetLayouts = &graphics.descriptorSetLayout;
    allocInfo.descriptorSetCount = 1;

    // Input image (before compute post-processing)
    VK_CHECK(vkAllocateDescriptorSets(engineDevice.getDevice(), &allocInfo, &graphics.descriptorSetPreCompute));

    // Image processing stage one
    VK_CHECK(
            vkAllocateDescriptorSets(engineDevice.getDevice(), &allocInfo, &graphics.descriptorSetPostComputeStageOne));

    // Image processing stage two
    VK_CHECK(
            vkAllocateDescriptorSets(engineDevice.getDevice(), &allocInfo, &graphics.descriptorSetPostComputeStageTwo));

    // Image processing stage three
    VK_CHECK(vkAllocateDescriptorSets(engineDevice.getDevice(), &allocInfo,
                                      &graphics.descriptorSetPostComputeStageThree));

    // Image processing stage four
    VK_CHECK(vkAllocateDescriptorSets(engineDevice.getDevice(), &allocInfo,
                                      &graphics.descriptorSetPostComputeStageFour));

    // Final image (after compute shader processing)
    VK_CHECK(vkAllocateDescriptorSets(engineDevice.getDevice(), &allocInfo, &graphics.descriptorSetPostComputeFinal));

    updateGraphicsDescriptorSets();
}

void VulkanEngineEntryPoint::prepareCompute() {
    // DarkChannelPrior calculation
    {
        VkDescriptorSetLayoutBinding inputImageLayoutBinding{};
        inputImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        inputImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        inputImageLayoutBinding.binding = 0;
        inputImageLayoutBinding.descriptorCount = 1;

        VkDescriptorSetLayoutBinding outputImageLayoutBinding{};
        outputImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        outputImageLayoutBinding.binding = 1;
        outputImageLayoutBinding.descriptorCount = 1;

        VkDescriptorSetLayoutBinding outputAtmosphericLightLayoutBinding{};
        outputAtmosphericLightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        outputAtmosphericLightLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        outputAtmosphericLightLayoutBinding.binding = 2;
        outputAtmosphericLightLayoutBinding.descriptorCount = 1;

        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
                // Binding 0: Input image (read-only)
                inputImageLayoutBinding,
                // Binding 1: Output image (write)
                outputImageLayoutBinding,
                // Binding 2: Output atmospheric light buffer (write)
                outputAtmosphericLightLayoutBinding,
        };

        prepareComputePipeline(setLayoutBindings, (std::string) DARK_CHANNEL_PRIOR_SHADER);
    }

    // Maximum airLight calculation
    {
        VkDescriptorSetLayoutBinding inputAtmosphericLightLayoutBinding{};
        inputAtmosphericLightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        inputAtmosphericLightLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        inputAtmosphericLightLayoutBinding.binding = 0;
        inputAtmosphericLightLayoutBinding.descriptorCount = 1;

        VkDescriptorSetLayoutBinding outputAtmosphericLightLayoutBinding{};
        outputAtmosphericLightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        outputAtmosphericLightLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        outputAtmosphericLightLayoutBinding.binding = 1;
        outputAtmosphericLightLayoutBinding.descriptorCount = 1;

        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
                // Binding 0: Output image (read-only)
                inputAtmosphericLightLayoutBinding,
                // Binding 1: Output atmospheric light buffer (write)
                outputAtmosphericLightLayoutBinding,
        };

        prepareComputePipeline(setLayoutBindings, (std::string) MAXIMUM_AIRLIGHT_SHADER);
    }

    // Transmission calculation
    {
        VkDescriptorSetLayoutBinding inputImageLayoutBinding{};
        inputImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        inputImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        inputImageLayoutBinding.binding = 0;
        inputImageLayoutBinding.descriptorCount = 1;

        VkDescriptorSetLayoutBinding outputImageLayoutBinding{};
        outputImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        outputImageLayoutBinding.binding = 1;
        outputImageLayoutBinding.descriptorCount = 1;

        VkDescriptorSetLayoutBinding inputMaxAtmosphericLightLayoutBinding{};
        inputMaxAtmosphericLightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        inputMaxAtmosphericLightLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        inputMaxAtmosphericLightLayoutBinding.binding = 2;
        inputMaxAtmosphericLightLayoutBinding.descriptorCount = 1;

        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
                // Binding 0: Input image (read-only)
                inputImageLayoutBinding,
                // Binding 1: Output image (write)
                outputImageLayoutBinding,
                // Binding 2: Input max atmospheric light buffer (read-only)
                inputMaxAtmosphericLightLayoutBinding
        };

        prepareComputePipeline(setLayoutBindings, (std::string) TRANSMISSION_SHADER);
    }

    // Guided Filter
    {
        VkDescriptorSetLayoutBinding guideImageLayoutBinding{};
        guideImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        guideImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        guideImageLayoutBinding.binding = 0;
        guideImageLayoutBinding.descriptorCount = 1;

        VkDescriptorSetLayoutBinding filterInputImageLayoutBinding{};
        filterInputImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        filterInputImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        filterInputImageLayoutBinding.binding = 1;
        filterInputImageLayoutBinding.descriptorCount = 1;

        VkDescriptorSetLayoutBinding outputImageLayoutBinding{};
        outputImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        outputImageLayoutBinding.binding = 2;
        outputImageLayoutBinding.descriptorCount = 1;

        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
                // Binding 0: Input Guide Image (read-only)
                guideImageLayoutBinding,
                // Binding 1: Input Filter Image (read-only)
                filterInputImageLayoutBinding,
                // Binding 2: Output Image (write)
                outputImageLayoutBinding,
        };

        prepareComputePipeline(setLayoutBindings, (std::string) GUIDED_FILTER_SHADER);
    }

    // Radiance calculation
    {
        VkDescriptorSetLayoutBinding inputImageLayoutBinding{};
        inputImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        inputImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        inputImageLayoutBinding.binding = 0;
        inputImageLayoutBinding.descriptorCount = 1;

        VkDescriptorSetLayoutBinding transmissionImageLayoutBinding{};
        transmissionImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        transmissionImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        transmissionImageLayoutBinding.binding = 1;
        transmissionImageLayoutBinding.descriptorCount = 1;

        VkDescriptorSetLayoutBinding outputImageLayoutBinding{};
        outputImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        outputImageLayoutBinding.binding = 2;
        outputImageLayoutBinding.descriptorCount = 1;

        VkDescriptorSetLayoutBinding inputAtmosphericLightLayoutBinding{};
        inputAtmosphericLightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        inputAtmosphericLightLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        inputAtmosphericLightLayoutBinding.binding = 3;
        inputAtmosphericLightLayoutBinding.descriptorCount = 1;

        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
                // Binding 0: Input image (read-only)
                inputImageLayoutBinding,
                // Binding 1: Transmission image (read-only)
                transmissionImageLayoutBinding,
                // Binding 2: Output image (write)
                outputImageLayoutBinding,
                // Binding 3: Input max atmospheric light buffer (read-only)
                inputAtmosphericLightLayoutBinding
        };

        prepareComputePipeline(setLayoutBindings, (std::string) RADIANCE_SHADER);
    }

    updateComputeDescriptorSets();

    // Push constants
    computePushConstant.groupCount = WORKGROUP_COUNT * WORKGROUP_COUNT;
    computePushConstant.imageWidth = glm::int32_t(inputTexture.width);
    computePushConstant.imageHeight = glm::int32_t(inputTexture.height);
    computePushConstant.omega = 0.98;
    computePushConstant.epsilon = 0.000001;
}

void VulkanEngineEntryPoint::prepareComputePipeline(std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings,
                                                    const std::string &shaderName) {
    compute.emplace_back();
    uint32_t pipelineIndex = compute.size() - 1;

    VkDescriptorSetLayoutCreateInfo descriptorLayout{};
    descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout.pBindings = setLayoutBindings.data();
    descriptorLayout.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());

    VK_CHECK(vkCreateDescriptorSetLayout(engineDevice.getDevice(), &descriptorLayout, nullptr,
                                         &compute.at(pipelineIndex).descriptorSetLayout));
    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(computePushConstant);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &compute.at(pipelineIndex).descriptorSetLayout;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(engineDevice.getDevice(), &pipelineLayoutCreateInfo, nullptr,
                                    &compute.at(pipelineIndex).pipelineLayout));

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.pSetLayouts = &compute.at(pipelineIndex).descriptorSetLayout;
    allocInfo.descriptorSetCount = 1;

    VK_CHECK(vkAllocateDescriptorSets(engineDevice.getDevice(), &allocInfo,
                                      &compute.at(pipelineIndex).descriptorSet));

    // Create compute shader pipelines
    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.layout = compute.at(pipelineIndex).pipelineLayout;
    computePipelineCreateInfo.flags = 0;

    std::string fileName = "../shaders/" + shaderName + ".comp.spv";
    computePipelineCreateInfo.stage = loadShader(fileName, VK_SHADER_STAGE_COMPUTE_BIT);
    VkPipeline pipeline;
    VK_CHECK(vkCreateComputePipelines(engineDevice.getDevice(), VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr,
                                      &pipeline));
    compute.at(pipelineIndex).pipeline = pipeline;
}

void VulkanEngineEntryPoint::render() {
    Timer timer("Rendering");

    CommandBufferPair bufferPair = renderer.beginFrame();
    if (bufferPair.computeCommandBuffer != nullptr && bufferPair.graphicsCommandBuffer != nullptr) {
        // Record compute command buffer
        if (PLAY_VIDEO || frameIndex == 0) {
            // First ComputeShader call -> calculate DarkChannelPrior + maxAirLight channels for each workgroup
            {
                vkCmdBindPipeline(bufferPair.computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                  compute.at(0).pipeline);
                vkCmdBindDescriptorSets(bufferPair.computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                        compute.at(0).pipelineLayout,
                                        0, 1, &compute.at(0).descriptorSet, 0,
                                        nullptr);

                vkCmdPushConstants(bufferPair.computeCommandBuffer, compute.at(0).pipelineLayout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(computePushConstant), &computePushConstant);
                vkCmdDispatch(bufferPair.computeCommandBuffer, WORKGROUP_COUNT, WORKGROUP_COUNT, 1);
            }

            // Wait
            vkCmdPipelineBarrier(bufferPair.computeCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);

            // Second ComputeShader call -> Calculate maximum airLight channels on a single thread
            {
                vkCmdBindPipeline(bufferPair.computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                  compute.at(1).pipeline);
                vkCmdBindDescriptorSets(bufferPair.computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                        compute.at(1).pipelineLayout,
                                        0, 1, &compute.at(1).descriptorSet, 0,
                                        nullptr);
                vkCmdPushConstants(bufferPair.computeCommandBuffer, compute.at(1).pipelineLayout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(computePushConstant), &computePushConstant);
                vkCmdDispatch(bufferPair.computeCommandBuffer, 1, 1, 1);
            }

            // Wait
            vkCmdPipelineBarrier(bufferPair.computeCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);

            // Third ComputeShader call -> calculate transmission
            {
                vkCmdBindPipeline(bufferPair.computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                  compute.at(2).pipeline);
                vkCmdBindDescriptorSets(bufferPair.computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                        compute.at(2).pipelineLayout,
                                        0, 1, &compute.at(2).descriptorSet, 0,
                                        nullptr);
                vkCmdPushConstants(bufferPair.computeCommandBuffer, compute.at(2).pipelineLayout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(computePushConstant), &computePushConstant);
                vkCmdDispatch(bufferPair.computeCommandBuffer, WORKGROUP_COUNT, WORKGROUP_COUNT, 1);
            }

            // Wait
            vkCmdPipelineBarrier(bufferPair.computeCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);

            // Fourth ComputeShader call -> Refine transmission with Guided filter
            {
                vkCmdBindPipeline(bufferPair.computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                  compute.at(3).pipeline);
                vkCmdBindDescriptorSets(bufferPair.computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                        compute.at(3).pipelineLayout,
                                        0, 1, &compute.at(3).descriptorSet, 0,
                                        nullptr);
                vkCmdPushConstants(bufferPair.computeCommandBuffer, compute.at(3).pipelineLayout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(computePushConstant), &computePushConstant);
                vkCmdDispatch(bufferPair.computeCommandBuffer, WORKGROUP_COUNT, WORKGROUP_COUNT, 1);
            }

            // Wait
            vkCmdPipelineBarrier(bufferPair.computeCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);

            // Fifth ComputeShader call -> calculate radiance
            {
                vkCmdBindPipeline(bufferPair.computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                  compute.at(4).pipeline);
                vkCmdBindDescriptorSets(bufferPair.computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                        compute.at(4).pipelineLayout,
                                        0, 1, &compute.at(4).descriptorSet, 0,
                                        nullptr);

                vkCmdPushConstants(bufferPair.computeCommandBuffer, compute.at(4).pipelineLayout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(computePushConstant),
                                   &computePushConstant);
                vkCmdDispatch(bufferPair.computeCommandBuffer, WORKGROUP_COUNT, WORKGROUP_COUNT, 1);
            }
        }

        // Record graphics commandBuffer
        {
            renderer.beginSwapChainRenderPass(bufferPair.graphicsCommandBuffer, radianceTexture.image);

            // Render first half of the screen
            float preWidth = renderer.getEngineSwapChain()->getSwapChainExtent().width * 0.5f + zoom;
            float preHeight = (preWidth / inputTexture.width) * inputTexture.height;

            VkViewport viewport = {};
            viewport.x = panPosition.x;
            viewport.y = panPosition.y;
            viewport.width = preWidth;
            viewport.height = preHeight;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            VkRect2D scissor{{0, 0}, renderer.getEngineSwapChain()->getSwapChainExtent()};

            vkCmdSetViewport(bufferPair.graphicsCommandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(bufferPair.graphicsCommandBuffer, 0, 1, &scissor);

            VkDeviceSize offsets[1] = {0};
            vkCmdBindVertexBuffers(bufferPair.graphicsCommandBuffer, 0, 1, vertexBuffer->getBuffer(), offsets);
            vkCmdBindIndexBuffer(bufferPair.graphicsCommandBuffer, *indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindPipeline(bufferPair.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipeline);

            // Top Left (pre compute)

            vkCmdBindDescriptorSets(bufferPair.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    graphics.pipelineLayout, 0, 1,
                                    &graphics.descriptorSetPreCompute, 0, nullptr);

            vkCmdDrawIndexed(bufferPair.graphicsCommandBuffer, indexCount, 1, 0, 0, 0);

            /*
            // Top Right (final image)
            vkCmdBindDescriptorSets(bufferPair.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    graphics.pipelineLayout, 0, 1,
                                    &graphics.descriptorSetPostComputeFinal, 0, nullptr);

            viewport.x = panPosition.x + preWidth;
            vkCmdSetViewport(bufferPair.graphicsCommandBuffer, 0, 1, &viewport);
            vkCmdDrawIndexed(bufferPair.graphicsCommandBuffer, indexCount, 1, 0, 0, 0);

            // Middle Left (compute first stage)
            vkCmdBindDescriptorSets(bufferPair.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    graphics.pipelineLayout, 0, 1,
                                    &graphics.descriptorSetPostComputeStageOne, 0, nullptr);

            viewport.x = panPosition.x;
            viewport.y = panPosition.y + preHeight;
            vkCmdSetViewport(bufferPair.graphicsCommandBuffer, 0, 1, &viewport);
            vkCmdDrawIndexed(bufferPair.graphicsCommandBuffer, indexCount, 1, 0, 0, 0);

            // Middle Right (compute second stage)
            vkCmdBindDescriptorSets(bufferPair.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    graphics.pipelineLayout, 0, 1,
                                    &graphics.descriptorSetPostComputeStageTwo, 0, nullptr);

            viewport.x = panPosition.x + preWidth;
            viewport.y = panPosition.y + preHeight;
            vkCmdSetViewport(bufferPair.graphicsCommandBuffer, 0, 1, &viewport);
            vkCmdDrawIndexed(bufferPair.graphicsCommandBuffer, indexCount, 1, 0, 0, 0);

            // Bottom Left (compute third stage)
            vkCmdBindDescriptorSets(bufferPair.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    graphics.pipelineLayout, 0, 1,
                                    &graphics.descriptorSetPostComputeStageThree, 0, nullptr);

            viewport.x = panPosition.x;
            viewport.y = panPosition.y + (preHeight * 2.0f);
            vkCmdSetViewport(bufferPair.graphicsCommandBuffer, 0, 1, &viewport);
            vkCmdDrawIndexed(bufferPair.graphicsCommandBuffer, indexCount, 1, 0, 0, 0);

            // Bottom Right (final fourth stage)
            vkCmdBindDescriptorSets(bufferPair.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    graphics.pipelineLayout, 0, 1,
                                    &graphics.descriptorSetPostComputeStageFour, 0, nullptr);

            viewport.x = panPosition.x + preWidth;
            viewport.y = panPosition.y + (preHeight * 2.0f);
            vkCmdSetViewport(bufferPair.graphicsCommandBuffer, 0, 1, &viewport);
            vkCmdDrawIndexed(bufferPair.graphicsCommandBuffer, indexCount, 1, 0, 0, 0);
            */
            renderer.endSwapChainRenderPass(bufferPair.graphicsCommandBuffer);
        }

        renderer.endFrame();

        vkQueueWaitIdle(engineDevice.graphicsQueue());

        // Prepare next frame
        frameIndex += 1;
        prepareInputImage();

        updateComputeDescriptorSets();
        updateGraphicsDescriptorSets();
    }
}

void VulkanEngineEntryPoint::updateComputeDescriptorSets() {
    // DarkChannelPrior calculation
    {
        VkWriteDescriptorSet inputImageDescriptorSet{};
        inputImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inputImageDescriptorSet.dstSet = compute.at(0).descriptorSet;
        inputImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        inputImageDescriptorSet.dstBinding = 0;
        inputImageDescriptorSet.pImageInfo = &inputTexture.descriptor;
        inputImageDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet outputImageDescriptorSet{};
        outputImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        outputImageDescriptorSet.dstSet = compute.at(0).descriptorSet;
        outputImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputImageDescriptorSet.dstBinding = 1;
        outputImageDescriptorSet.pImageInfo = &darkChannelPriorTexture.descriptor;
        outputImageDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet outputAirLightBufferDescriptorSet{};
        outputAirLightBufferDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        outputAirLightBufferDescriptorSet.dstSet = compute.at(0).descriptorSet;
        outputAirLightBufferDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        outputAirLightBufferDescriptorSet.dstBinding = 2;
        outputAirLightBufferDescriptorSet.pBufferInfo = &airLightGroupsBuffer->getBufferInfo();
        outputAirLightBufferDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
                inputImageDescriptorSet,
                outputImageDescriptorSet,
                outputAirLightBufferDescriptorSet,
        };
        vkUpdateDescriptorSets(engineDevice.getDevice(), computeWriteDescriptorSets.size(),
                               computeWriteDescriptorSets.data(), 0, nullptr);
    }

    // Maximum airLight calculation
    {
        VkWriteDescriptorSet inputAirLightBufferDescriptorSet{};
        inputAirLightBufferDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inputAirLightBufferDescriptorSet.dstSet = compute.at(1).descriptorSet;
        inputAirLightBufferDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        inputAirLightBufferDescriptorSet.dstBinding = 0;
        inputAirLightBufferDescriptorSet.pBufferInfo = &airLightGroupsBuffer->getBufferInfo();
        inputAirLightBufferDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet outputAirLightBufferDescriptorSet{};
        outputAirLightBufferDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        outputAirLightBufferDescriptorSet.dstSet = compute.at(1).descriptorSet;
        outputAirLightBufferDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        outputAirLightBufferDescriptorSet.dstBinding = 1;
        outputAirLightBufferDescriptorSet.pBufferInfo = &airLightMaxBuffer->getBufferInfo();
        outputAirLightBufferDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
                inputAirLightBufferDescriptorSet,
                outputAirLightBufferDescriptorSet,
        };
        vkUpdateDescriptorSets(engineDevice.getDevice(), computeWriteDescriptorSets.size(),
                               computeWriteDescriptorSets.data(), 0, nullptr);
    }

    // Transmission calculation
    {
        VkWriteDescriptorSet inputImageDescriptorSet{};
        inputImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inputImageDescriptorSet.dstSet = compute.at(2).descriptorSet;
        inputImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        inputImageDescriptorSet.dstBinding = 0;
        inputImageDescriptorSet.pImageInfo = &inputTexture.descriptor;
        inputImageDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet outputImageDescriptorSet{};
        outputImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        outputImageDescriptorSet.dstSet = compute.at(2).descriptorSet;
        outputImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputImageDescriptorSet.dstBinding = 1;
        outputImageDescriptorSet.pImageInfo = &transmissionTexture.descriptor;
        outputImageDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet inputMaxAirLightBufferDescriptorSet{};
        inputMaxAirLightBufferDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inputMaxAirLightBufferDescriptorSet.dstSet = compute.at(2).descriptorSet;
        inputMaxAirLightBufferDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        inputMaxAirLightBufferDescriptorSet.dstBinding = 2;
        inputMaxAirLightBufferDescriptorSet.pBufferInfo = &airLightMaxBuffer->getBufferInfo();
        inputMaxAirLightBufferDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
                inputImageDescriptorSet,
                outputImageDescriptorSet,
                inputMaxAirLightBufferDescriptorSet,
        };
        vkUpdateDescriptorSets(engineDevice.getDevice(), computeWriteDescriptorSets.size(),
                               computeWriteDescriptorSets.data(), 0, nullptr);
    }

    // Guided filter
    {
        VkWriteDescriptorSet guideImageDescriptorSet{};
        guideImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        guideImageDescriptorSet.dstSet = compute.at(3).descriptorSet;
        guideImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        guideImageDescriptorSet.dstBinding = 0;
        guideImageDescriptorSet.pImageInfo = &inputTexture.descriptor;
        guideImageDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet filterInputImageDescriptorSet{};
        filterInputImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        filterInputImageDescriptorSet.dstSet = compute.at(3).descriptorSet;
        filterInputImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        filterInputImageDescriptorSet.dstBinding = 1;
        filterInputImageDescriptorSet.pImageInfo = &transmissionTexture.descriptor;
        filterInputImageDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet outputImageDescriptorSet{};
        outputImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        outputImageDescriptorSet.dstSet = compute.at(3).descriptorSet;
        outputImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputImageDescriptorSet.dstBinding = 2;
        outputImageDescriptorSet.pImageInfo = &filteredTransmissionTexture.descriptor;
        outputImageDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
                guideImageDescriptorSet,
                filterInputImageDescriptorSet,
                outputImageDescriptorSet,
        };
        vkUpdateDescriptorSets(engineDevice.getDevice(), computeWriteDescriptorSets.size(),
                               computeWriteDescriptorSets.data(), 0, nullptr);
    }

    // Radiance calculation
    {
        VkWriteDescriptorSet inputImageDescriptorSet{};
        inputImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inputImageDescriptorSet.dstSet = compute.at(4).descriptorSet;
        inputImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        inputImageDescriptorSet.dstBinding = 0;
        inputImageDescriptorSet.pImageInfo = &inputTexture.descriptor;
        inputImageDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet transmissionImageDescriptorSet{};
        transmissionImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        transmissionImageDescriptorSet.dstSet = compute.at(4).descriptorSet;
        transmissionImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        transmissionImageDescriptorSet.dstBinding = 1;
        transmissionImageDescriptorSet.pImageInfo = &filteredTransmissionTexture.descriptor;
        transmissionImageDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet outputImageDescriptorSet{};
        outputImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        outputImageDescriptorSet.dstSet = compute.at(4).descriptorSet;
        outputImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputImageDescriptorSet.dstBinding = 2;
        outputImageDescriptorSet.pImageInfo = &radianceTexture.descriptor;
        outputImageDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet maxAirLightBufferDescriptorSet{};
        maxAirLightBufferDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        maxAirLightBufferDescriptorSet.dstSet = compute.at(4).descriptorSet;
        maxAirLightBufferDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        maxAirLightBufferDescriptorSet.dstBinding = 3;
        maxAirLightBufferDescriptorSet.pBufferInfo = &airLightMaxBuffer->getBufferInfo();
        maxAirLightBufferDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
                inputImageDescriptorSet,
                transmissionImageDescriptorSet,
                outputImageDescriptorSet,
                maxAirLightBufferDescriptorSet,
        };
        vkUpdateDescriptorSets(engineDevice.getDevice(), computeWriteDescriptorSets.size(),
                               computeWriteDescriptorSets.data(), 0, nullptr);
    }
}

void VulkanEngineEntryPoint::updateGraphicsDescriptorSets() {
    // Pre Compute
    {
        VkWriteDescriptorSet uniformDescriptorSet{};
        uniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uniformDescriptorSet.dstSet = graphics.descriptorSetPreCompute;
        uniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniformDescriptorSet.dstBinding = 0;
        uniformDescriptorSet.pBufferInfo = &uniformBufferVertexShader->getBufferInfo();
        uniformDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet imageDescriptorSet{};
        imageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        imageDescriptorSet.dstSet = graphics.descriptorSetPreCompute;
        imageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        imageDescriptorSet.dstBinding = 1;
        imageDescriptorSet.pImageInfo = &inputTexture.descriptor;
        imageDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> baseImageWriteDescriptorSets = {
                uniformDescriptorSet,
                imageDescriptorSet
        };
        vkUpdateDescriptorSets(engineDevice.getDevice(), baseImageWriteDescriptorSets.size(), baseImageWriteDescriptorSets.data(), 0, nullptr);
    }

    // Post-Compute first stage
    {
        VkWriteDescriptorSet inProgressUniformDescriptorSet{};
        inProgressUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressUniformDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageOne;
        inProgressUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        inProgressUniformDescriptorSet.dstBinding = 0;
        inProgressUniformDescriptorSet.pBufferInfo = &uniformBufferVertexShader->getBufferInfo();
        inProgressUniformDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet inProgressImageDescriptorSet{};
        inProgressImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressImageDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageOne;
        inProgressImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        inProgressImageDescriptorSet.dstBinding = 1;
        inProgressImageDescriptorSet.pImageInfo = &darkChannelPriorTexture.descriptor;
        inProgressImageDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
                inProgressUniformDescriptorSet,
                inProgressImageDescriptorSet,
        };

        vkUpdateDescriptorSets(engineDevice.getDevice(), writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
    }

    // Post-Compute second stage
    {
        VkWriteDescriptorSet inProgressUniformDescriptorSet{};
        inProgressUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressUniformDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageTwo;
        inProgressUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        inProgressUniformDescriptorSet.dstBinding = 0;
        inProgressUniformDescriptorSet.pBufferInfo = &uniformBufferVertexShader->getBufferInfo();
        inProgressUniformDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet inProgressImageDescriptorSet{};
        inProgressImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressImageDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageTwo;
        inProgressImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        inProgressImageDescriptorSet.dstBinding = 1;
        inProgressImageDescriptorSet.pImageInfo = &transmissionTexture.descriptor;
        inProgressImageDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
                inProgressUniformDescriptorSet,
                inProgressImageDescriptorSet,
        };

        vkUpdateDescriptorSets(engineDevice.getDevice(), writeDescriptorSets.size(), writeDescriptorSets.data(), 0,
                               nullptr);
    }

    // Post-Compute third stage
    {
        VkWriteDescriptorSet inProgressUniformDescriptorSet{};
        inProgressUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressUniformDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageThree;
        inProgressUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        inProgressUniformDescriptorSet.dstBinding = 0;
        inProgressUniformDescriptorSet.pBufferInfo = &uniformBufferVertexShader->getBufferInfo();
        inProgressUniformDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet inProgressImageDescriptorSet{};
        inProgressImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressImageDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageThree;
        inProgressImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        inProgressImageDescriptorSet.dstBinding = 1;
        inProgressImageDescriptorSet.pImageInfo = &filteredTransmissionTexture.descriptor;
        inProgressImageDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
                inProgressUniformDescriptorSet,
                inProgressImageDescriptorSet,
        };

        vkUpdateDescriptorSets(engineDevice.getDevice(), writeDescriptorSets.size(), writeDescriptorSets.data(), 0,
                               nullptr);
    }

    // Post-Compute fourth stage
    {
        VkWriteDescriptorSet inProgressUniformDescriptorSet{};
        inProgressUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressUniformDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageFour;
        inProgressUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        inProgressUniformDescriptorSet.dstBinding = 0;
        inProgressUniformDescriptorSet.pBufferInfo = &uniformBufferVertexShader->getBufferInfo();
        inProgressUniformDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet inProgressImageDescriptorSet{};
        inProgressImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressImageDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageFour;
        inProgressImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        inProgressImageDescriptorSet.dstBinding = 1;
        inProgressImageDescriptorSet.pImageInfo = &filteredTransmissionTexture.descriptor;
        inProgressImageDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
                inProgressUniformDescriptorSet,
                inProgressImageDescriptorSet,
        };

        vkUpdateDescriptorSets(engineDevice.getDevice(), writeDescriptorSets.size(), writeDescriptorSets.data(), 0,
                               nullptr);
    }

    // Post-Compute final image
    {
        VkWriteDescriptorSet postComputeUniformDescriptorSet{};
        postComputeUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        postComputeUniformDescriptorSet.dstSet = graphics.descriptorSetPostComputeFinal;
        postComputeUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        postComputeUniformDescriptorSet.dstBinding = 0;
        postComputeUniformDescriptorSet.pBufferInfo = &uniformBufferVertexShader->getBufferInfo();
        postComputeUniformDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet postImageDescriptorSet{};
        postImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        postImageDescriptorSet.dstSet = graphics.descriptorSetPostComputeFinal;
        postImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        postImageDescriptorSet.dstBinding = 1;
        postImageDescriptorSet.pImageInfo = &radianceTexture.descriptor;
        postImageDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
                postComputeUniformDescriptorSet,
                postImageDescriptorSet,
        };

        vkUpdateDescriptorSets(engineDevice.getDevice(), writeDescriptorSets.size(), writeDescriptorSets.data(), 0,
                               nullptr);
    }
}

void VulkanEngineEntryPoint::saveScreenshot(const char *filename) {
    bool supportsBlit = true;

    // Check blit support for source and destination
    VkFormatProperties formatProps;

    // Check if the device supports blitting from optimal images (the swapchain images are in optimal format)
    vkGetPhysicalDeviceFormatProperties(engineDevice.getPhysicalDevice(),
                                        renderer.getEngineSwapChain()->getSwapChainImageFormat(), &formatProps);
    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
        supportsBlit = false;
    }

    // Check if the device supports blitting to linear images
    vkGetPhysicalDeviceFormatProperties(engineDevice.getPhysicalDevice(), VK_FORMAT_R8G8B8A8_UNORM, &formatProps);
    if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
        supportsBlit = false;
    }

    // Source for the copy is the last rendered swapchain image
    VkImage srcImage = renderer.getEngineSwapChain()->getImage(0); //TODO: Find proper index
    uint32_t width = renderer.getEngineSwapChain()->getSwapChainExtent().width;
    uint32_t height = renderer.getEngineSwapChain()->getSwapChainExtent().height;
    uint32_t channels = 4;
    size_t size = width * height * channels;

    // Create the linear tiled destination image to copy to and to read the memory from
    VkImageCreateInfo imageCreateCI{};
    imageCreateCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
    // Note that vkCmdBlitImage (if supported) will also do format conversions if the swapchain color format would differ
    imageCreateCI.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreateCI.extent.width = width;
    imageCreateCI.extent.height = height;
    imageCreateCI.extent.depth = 1;
    imageCreateCI.arrayLayers = 1;
    imageCreateCI.mipLevels = 1;
    imageCreateCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateCI.tiling = VK_IMAGE_TILING_LINEAR;
    imageCreateCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // Create the image
    VkImage dstImage;
    VK_CHECK(vkCreateImage(engineDevice.getDevice(), &imageCreateCI, nullptr, &dstImage));

    // Create memory to back up the image
    VkMemoryRequirements memRequirements;
    VkMemoryAllocateInfo memAllocInfo{};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkDeviceMemory dstImageMemory;
    vkGetImageMemoryRequirements(engineDevice.getDevice(), dstImage, &memRequirements);
    memAllocInfo.allocationSize = memRequirements.size;
    // Memory must be host visible to copy from
    memAllocInfo.memoryTypeIndex = engineDevice.findMemoryType(memRequirements.memoryTypeBits,
                                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK_CHECK(vkAllocateMemory(engineDevice.getDevice(), &memAllocInfo, nullptr, &dstImageMemory));

    VK_CHECK(vkBindImageMemory(engineDevice.getDevice(), dstImage, dstImageMemory, 0));

    // Do the actual blit from the swapchain image to our host visible destination image
    VkCommandBuffer copyCmd = engineDevice.beginSingleTimeCommands();

    // Transition destination image to transfer destination layout
    setImageLayout(copyCmd,
                   dstImage,
                   VK_IMAGE_ASPECT_COLOR_BIT,
                   VK_IMAGE_LAYOUT_UNDEFINED,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Transition swapchain image from present to transfer source layout
    setImageLayout(copyCmd,
                   srcImage,
                   VK_IMAGE_ASPECT_COLOR_BIT,
                   VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT);

    // If source and destination support blit we'll blit as this also does automatic format conversion (e.g. from BGR to RGB)
    if (supportsBlit) {
        // Define the region to blit (we will blit the whole swapchain image)
        VkOffset3D blitSize;
        blitSize.x = width;
        blitSize.y = height;
        blitSize.z = 1;
        VkImageBlit imageBlitRegion{};
        imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.srcSubresource.layerCount = 1;
        imageBlitRegion.srcOffsets[1] = blitSize;
        imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.dstSubresource.layerCount = 1;
        imageBlitRegion.dstOffsets[1] = blitSize;

        // Issue the blit command
        vkCmdBlitImage(
                copyCmd,
                srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &imageBlitRegion,
                VK_FILTER_NEAREST);
    } else {
        // Otherwise use image copy (requires us to manually flip components)
        VkImageCopy imageCopyRegion{};
        imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.srcSubresource.layerCount = 1;
        imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.dstSubresource.layerCount = 1;
        imageCopyRegion.extent.width = renderer.getEngineSwapChain()->getSwapChainExtent().width;
        imageCopyRegion.extent.height = renderer.getEngineSwapChain()->getSwapChainExtent().height;
        imageCopyRegion.extent.depth = 1;

        // Issue the copy command
        vkCmdCopyImage(
                copyCmd,
                srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &imageCopyRegion);
    }

    // Transition destination image to general layout, which is the required layout for mapping the image memory later on
    setImageLayout(copyCmd,
                   dstImage,
                   VK_IMAGE_ASPECT_COLOR_BIT,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   VK_IMAGE_LAYOUT_GENERAL,
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Transition back the swap chain image after the blit is done
    setImageLayout(copyCmd,
                   srcImage,
                   VK_IMAGE_ASPECT_COLOR_BIT,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT);

    engineDevice.endSingleTimeCommands(copyCmd, engineDevice.graphicsQueue());

    // Get layout of the image (including row pitch)
    VkImageSubresource subResource{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
    VkSubresourceLayout subResourceLayout;
    vkGetImageSubresourceLayout(engineDevice.getDevice(), dstImage, &subResource, &subResourceLayout);

    // Map image memory so we can start copying from it
    const char *dataBgr;
    vkMapMemory(engineDevice.getDevice(), dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void **) &dataBgr);
    dataBgr += subResourceLayout.offset;

    auto *dataRgb = new uint8_t[size];
    for (size_t i = 0; i < size; i += channels) {
        dataRgb[i + 0] = dataBgr[i + 2];
        dataRgb[i + 1] = dataBgr[i + 1];
        dataRgb[i + 2] = dataBgr[i + 0];
        dataRgb[i + 3] = dataBgr[i + 3];
    }

    stbi_write_png(filename, width, height, channels, dataRgb, 0);

    fmt::print("Screenshot saved to disk\n");

    // Clean up resources
    vkUnmapMemory(engineDevice.getDevice(), dstImageMemory);
    vkFreeMemory(engineDevice.getDevice(), dstImageMemory, nullptr);
    vkDestroyImage(engineDevice.getDevice(), dstImage, nullptr);
    delete[] dataRgb;
}

VkPipelineShaderStageCreateInfo VulkanEngineEntryPoint::loadShader(std::string fileName, VkShaderStageFlagBits stage) {
    VkPipelineShaderStageCreateInfo shaderStage = {};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = stage;

    shaderStage.module = loadShaderModule(fileName.c_str(), engineDevice.getDevice());

    shaderStage.pName = "main";
    assert(shaderStage.module != VK_NULL_HANDLE);
    shaderModules.push_back(shaderStage.module);
    return shaderStage;
}

VkShaderModule VulkanEngineEntryPoint::loadShaderModule(const char *fileName, VkDevice device) {
    std::ifstream is(fileName, std::ios::binary | std::ios::in | std::ios::ate);

    if (is.is_open()) {
        size_t size = is.tellg();
        is.seekg(0, std::ios::beg);
        char *shaderCode = new char[size];
        is.read(shaderCode, size);
        is.close();

        assert(size > 0);

        VkShaderModule shaderModule;
        VkShaderModuleCreateInfo moduleCreateInfo{};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = size;
        moduleCreateInfo.pCode = (uint32_t *) shaderCode;

        VK_CHECK(vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &shaderModule));

        delete[] shaderCode;

        return shaderModule;
    } else {
        std::cerr << "Error: Could not open shader file \"" << fileName << "\"" << "\n";
        return VK_NULL_HANDLE;
    }
}

void VulkanEngineEntryPoint::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        switch (event.type) {
            case SDL_QUIT:
                isRunning = false;
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    SDL_Window *win = SDL_GetWindowFromID(event.window.windowID);
                    int w;
                    int h;
                    SDL_GetWindowSize(win, &w, &h);
                    window.onWindowResized(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
                }
            case SDL_MOUSEWHEEL: {
                if (event.wheel.y > 0) {
                    zoom += 30;
                } else if (event.wheel.y < 0) {
                    zoom -= 30;
                }
            }
            case SDL_MOUSEBUTTONDOWN: {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    if (mouseDragOrigin == glm::vec2(0.0f, 0.0f)) {
                        mouseDragOrigin = glm::vec2(event.button.x, event.button.y);
                    }
                }
            }
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT && event.button.state == SDL_RELEASED) {
                    mouseDragOrigin = glm::vec2(0.0f, 0.0f);
                }
            case SDL_MOUSEMOTION:
                if (mouseDragOrigin != glm::vec2(0.0f, 0.0f)) {
                    glm::vec2 difPos = glm::vec2(mouseDragOrigin.x - event.button.x,
                                                 mouseDragOrigin.y - event.button.y);

                    panPosition += glm::vec2(-difPos.x, -difPos.y);
                    mouseDragOrigin = glm::vec2(event.button.x, event.button.y);
                }

            default:
                break;
        }
    }

    const Uint8 *keystate = SDL_GetKeyboardState(nullptr);

    if (keystate[SDL_SCANCODE_ESCAPE]) {
        isRunning = false;
    } else if (keystate[SDL_SCANCODE_S]) {
        saveScreenshot(fmt::format("../screenshots/screenshot_frame_{}.png", frameIndex).c_str());
    } else if (keystate[SDL_SCANCODE_LEFT]) {
        if (frameIndex > SWEEP_FRAMES) {
            frameIndex -= SWEEP_FRAMES;
        }
    } else if (keystate[SDL_SCANCODE_RIGHT]) {
        if (frameIndex < totalFrames - SWEEP_FRAMES) {
            frameIndex += SWEEP_FRAMES;
        }
    }
}

cv::Mat VulkanEngineEntryPoint::getDFTWindow() {
    int32_t width = cameraFrame.cols;
    int32_t height = cameraFrame.rows;

    // Estimate position of the road vanishing point
    int32_t r_vp_x = width / 2 + (HORIZONTAL_SENSITIVITY * headingDif[headingDif.size() - 1]);
    int32_t r_vp_y = height / 2 + (VERTICAL_SENSITIVITY * attitudeDif[attitudeDif.size() - 1]);

    int32_t window_top_left_x = r_vp_x - (DFT_WINDOW_SIZE / 2);
    int32_t window_top_left_y = r_vp_y - (DFT_WINDOW_SIZE / 2);
    cv::Rect window_rect(window_top_left_x, window_top_left_y, DFT_WINDOW_SIZE, DFT_WINDOW_SIZE);

    cv::Mat cameraFrameGray;
    cv::cvtColor(cameraFrame, cameraFrameGray, cv::COLOR_BGR2GRAY);

    cv::Mat cameraFrameWindow = cameraFrameGray(window_rect);

    int32_t optimalWindowSize = cv::getOptimalDFTSize(DFT_WINDOW_SIZE);
    cv::Mat paddedWindow;
    cv::copyMakeBorder(cameraFrameWindow, paddedWindow, 0, optimalWindowSize - cameraFrameWindow.rows, 0, optimalWindowSize - cameraFrameWindow.cols, cv::BORDER_CONSTANT, cv::Scalar::all(0));

    cv::Mat planes[] = {cv::Mat_<float>(paddedWindow), cv::Mat::zeros(paddedWindow.size(), CV_32F)};
    cv::Mat complexI;
    cv::merge(planes, 2, complexI);

    cv::dft(complexI, complexI);

    cv::split(complexI, planes);
    cv::magnitude(planes[0], planes[1], planes[0]);
    cv::Mat magI = planes[0];
    cv::Mat magIPow = magI.mul(magI);//(magI^2)/(DFT_WINDOW_SIZE^2);
    cv::Mat magINorm = magIPow.mul(DFT_WINDOW_SIZE^2);

    magINorm += cv::Scalar::all(1);                    // switch to logarithmic scale
    cv::log(magINorm, magINorm);

    // crop the spectrum, if it has an odd number of rows or columns
    magINorm = magINorm(cv::Rect(0, 0, magI.cols & -2, magI.rows & -2));
    // rearrange the quadrants of Fourier image  so that the origin is at the image center
    int cx = magINorm.cols/2;
    int cy = magINorm.rows/2;
    cv::Mat q0(magINorm, cv::Rect(0, 0, cx, cy));   // Top-Left - Create a ROI per quadrant
    cv::Mat q1(magINorm, cv::Rect(cx, 0, cx, cy));  // Top-Right
    cv::Mat q2(magINorm, cv::Rect(0, cy, cx, cy));  // Bottom-Left
    cv::Mat q3(magINorm, cv::Rect(cx, cy, cx, cy)); // Bottom-Right
    cv::Mat tmp;                           // swap quadrants (Top-Left with Bottom-Right)
    q0.copyTo(tmp);
    q3.copyTo(q0);
    tmp.copyTo(q3);
    q1.copyTo(tmp);                    // swap quadrant (Top-Right with Bottom-Left)
    q2.copyTo(q1);
    tmp.copyTo(q2);

    cv::normalize(magINorm, magINorm, 0, 255, cv::NORM_MINMAX);
    cv::Mat intMagI = cv::Mat_<uint8_t>(magINorm);
    return intMagI;
}