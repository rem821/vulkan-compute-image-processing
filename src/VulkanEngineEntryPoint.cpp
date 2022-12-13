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

VulkanEngineEntryPoint::VulkanEngineEntryPoint(Dataset *_dataset) : dataset(_dataset) {

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
    prepareGraphicsUniformBuffers();
    setupDescriptorSetLayout();
    prepareGraphicsPipeline();
    setupDescriptorPool();
    setupDescriptorSet();

    camera.setViewYXZ(glm::vec3(0.0f, 0.0f, -2.0f), glm::vec3(0.0f));
    camera.setPerspectiveProjection(glm::radians(50.f), (float) WINDOW_WIDTH * 0.5f / (float) WINDOW_HEIGHT, 1.0f,
                                    256.0f);

    // Compute
    prepareCompute();

    isRunning = true;
}

void VulkanEngineEntryPoint::prepareInputImage() {
    Timer timer("Texture generation", dataset->textureGeneration);
    cv::Mat rgba;

    cv::cvtColor(dataset->leftCameraFrame, rgba, cv::COLOR_BGR2RGBA);

    inputTexture.fromImageFile(rgba.data, rgba.cols * rgba.rows * rgba.channels(), VK_FORMAT_R8G8B8A8_UNORM,
                               rgba.cols, rgba.rows,
                               engineDevice,
                               engineDevice.graphicsQueue(), VK_FILTER_LINEAR,
                               VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                               VK_IMAGE_LAYOUT_GENERAL);
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

void VulkanEngineEntryPoint::prepareGraphicsUniformBuffers() {
    uniformBufferVertexShader = std::make_unique<VulkanEngineBuffer>(engineDevice, sizeof(uboVertexShader), 1,
                                                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    uniformBufferFragmentShader = std::make_unique<VulkanEngineBuffer>(engineDevice, sizeof(uboFragmentShader), 1,
                                                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    uniformBufferVertexShader->map();
    uniformBufferFragmentShader->map();

    updateGraphicsUniformBuffers();
}

void VulkanEngineEntryPoint::updateGraphicsUniformBuffers() {
    uboVertexShader.projection = camera.getProjection();
    uboVertexShader.modelView = camera.getView();
    memcpy(uniformBufferVertexShader->getMappedMemory(), &uboVertexShader, sizeof(uboVertexShader));

    if (dataset != nullptr) {
        uboFragmentShader.showVanishingPoint = false;
        uboFragmentShader.showKeypoints = true;
        uboFragmentShader.vanishingPoint = glm::vec3(dataset->vanishingPoint.first, dataset->vanishingPoint.second,
                                                     DFT_WINDOW_SIZE);
        int i = 0;
        float x_ratio = float(window.getExtent().width) / float(dataset->cameraWidth);
        float y_ratio =  float(window.getExtent().height) / float(dataset->cameraHeight);
        for (const auto &point: dataset->leftKeypoints) {
            float x = point.pt.x * x_ratio;
            float y = point.pt.y * y_ratio;
            uboFragmentShader.keyPoints[i] = glm::vec2(x, y);
            i++;
        }
    }

    memcpy(uniformBufferFragmentShader->getMappedMemory(), &uboFragmentShader, sizeof(uboFragmentShader));
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

    VkDescriptorSetLayoutBinding setLayoutBindingFragmentUbo{};
    setLayoutBindingFragmentUbo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    setLayoutBindingFragmentUbo.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    setLayoutBindingFragmentUbo.binding = 2;
    setLayoutBindingFragmentUbo.descriptorCount = 1;

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            // Binding 0: Vertex shader uniform buffer
            setLayoutBindingVertex,
            // Binding 1: Fragment shader input image
            setLayoutBindingFragment,
            // Binding 2: Fragment shader uniform buffer
            setLayoutBindingFragmentUbo,
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
    Timer timer("Rendering", dataset->rendering);

    CommandBufferPair bufferPair = renderer.beginFrame();
    if (bufferPair.computeCommandBuffer != nullptr && bufferPair.graphicsCommandBuffer != nullptr) {
        // Record compute command buffer

#if DEBUG_GUI_ENABLED
        debugGui.showWindow(window.sdlWindow(), dataset->frameIndex, dataset);
#endif

        // First ComputeShader call -> Calculate DarkChannelPrior + maxAirLight channels for each workgroup
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
        vkCmdPipelineBarrier(bufferPair.computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);

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
        vkCmdPipelineBarrier(bufferPair.computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);

        // Third ComputeShader call -> Calculate transmission
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
        vkCmdPipelineBarrier(bufferPair.computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);

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
        vkCmdPipelineBarrier(bufferPair.computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);

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

        VkMemoryBarrier memoryBarrier = {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        // Wait
        vkCmdPipelineBarrier(bufferPair.computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

        renderer.beginSwapChainRenderPass(bufferPair.graphicsCommandBuffer, radianceTexture.image);
        // Record graphics commandBuffer
#if SINGLE_VIEW_MODE
        uint32_t preWidth = renderer.getEngineSwapChain()->getSwapChainExtent().width;
        uint32_t preHeight = renderer.getEngineSwapChain()->getSwapChainExtent().height;

        VkViewport viewport = {};
        viewport.x = panPosition.x;
        viewport.y = panPosition.y;
        viewport.width = float(preWidth);
        viewport.height = float(preHeight);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{{0, 0}, renderer.getEngineSwapChain()->getSwapChainExtent()};

        vkCmdSetViewport(bufferPair.graphicsCommandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(bufferPair.graphicsCommandBuffer, 0, 1, &scissor);

        VkDeviceSize offsets[1] = {0};
        vkCmdBindVertexBuffers(bufferPair.graphicsCommandBuffer, 0, 1, vertexBuffer->getBuffer(), offsets);
        vkCmdBindIndexBuffer(bufferPair.graphicsCommandBuffer, *indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindPipeline(bufferPair.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipeline);

        vkCmdBindDescriptorSets(bufferPair.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                graphics.pipelineLayout, 0, 1,
                                &graphics.descriptorSetPreCompute, 0, nullptr);

        vkCmdDrawIndexed(bufferPair.graphicsCommandBuffer, indexCount, 1, 0, 0, 0);
#else
        // Render first half of the screen
        float preWidth = float(renderer.getEngineSwapChain()->getSwapChainExtent().width) * 0.5f + zoom;
        float preHeight = (preWidth / float(inputTexture.width)) * float(inputTexture.height);

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
#endif

#if DEBUG_GUI_ENABLED
        debugGui.render(bufferPair.graphicsCommandBuffer);
#endif

        renderer.endSwapChainRenderPass(bufferPair.graphicsCommandBuffer);
        renderer.endFrame(dataset);

        vkQueueWaitIdle(engineDevice.graphicsQueue());

    }
}

void VulkanEngineEntryPoint::prepareNextFrame() {
    dataset->vanishingPoint.first = int(
            float(dataset->vanishingPoint.first * window.getExtent().width) / float(dataset->cameraWidth));
    dataset->vanishingPoint.second = int(
            float(dataset->vanishingPoint.second * window.getExtent().height) / float(dataset->cameraHeight));

    prepareInputImage();

    updateGraphicsUniformBuffers();
    updateComputeDescriptorSets();
    updateGraphicsDescriptorSets();
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
        VkWriteDescriptorSet vertexUniformDescriptorSet{};
        vertexUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vertexUniformDescriptorSet.dstSet = graphics.descriptorSetPreCompute;
        vertexUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vertexUniformDescriptorSet.dstBinding = 0;
        vertexUniformDescriptorSet.pBufferInfo = &uniformBufferVertexShader->getBufferInfo();
        vertexUniformDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet imageDescriptorSet{};
        imageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        imageDescriptorSet.dstSet = graphics.descriptorSetPreCompute;
        imageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        imageDescriptorSet.dstBinding = 1;
        imageDescriptorSet.pImageInfo = &inputTexture.descriptor;
        imageDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet fragmentUniformDescriptorSet{};
        fragmentUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        fragmentUniformDescriptorSet.dstSet = graphics.descriptorSetPreCompute;
        fragmentUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        fragmentUniformDescriptorSet.dstBinding = 2;
        fragmentUniformDescriptorSet.pBufferInfo = &uniformBufferFragmentShader->getBufferInfo();
        fragmentUniformDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> baseImageWriteDescriptorSets = {
                vertexUniformDescriptorSet,
                imageDescriptorSet,
                fragmentUniformDescriptorSet,
        };
        vkUpdateDescriptorSets(engineDevice.getDevice(), baseImageWriteDescriptorSets.size(),
                               baseImageWriteDescriptorSets.data(), 0, nullptr);
    }

    // Post-Compute first stage
    {
        VkWriteDescriptorSet inProgressVertexUniformDescriptorSet{};
        inProgressVertexUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressVertexUniformDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageOne;
        inProgressVertexUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        inProgressVertexUniformDescriptorSet.dstBinding = 0;
        inProgressVertexUniformDescriptorSet.pBufferInfo = &uniformBufferVertexShader->getBufferInfo();
        inProgressVertexUniformDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet inProgressImageDescriptorSet{};
        inProgressImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressImageDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageOne;
        inProgressImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        inProgressImageDescriptorSet.dstBinding = 1;
        inProgressImageDescriptorSet.pImageInfo = &darkChannelPriorTexture.descriptor;
        inProgressImageDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet inProgressFragmentUniformDescriptorSet{};
        inProgressFragmentUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressFragmentUniformDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageOne;
        inProgressFragmentUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        inProgressFragmentUniformDescriptorSet.dstBinding = 2;
        inProgressFragmentUniformDescriptorSet.pBufferInfo = &uniformBufferFragmentShader->getBufferInfo();
        inProgressFragmentUniformDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
                inProgressVertexUniformDescriptorSet,
                inProgressImageDescriptorSet,
                inProgressFragmentUniformDescriptorSet
        };

        vkUpdateDescriptorSets(engineDevice.getDevice(), writeDescriptorSets.size(), writeDescriptorSets.data(), 0,
                               nullptr);
    }

    // Post-Compute second stage
    {
        VkWriteDescriptorSet inProgressVertexUniformDescriptorSet{};
        inProgressVertexUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressVertexUniformDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageTwo;
        inProgressVertexUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        inProgressVertexUniformDescriptorSet.dstBinding = 0;
        inProgressVertexUniformDescriptorSet.pBufferInfo = &uniformBufferVertexShader->getBufferInfo();
        inProgressVertexUniformDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet inProgressImageDescriptorSet{};
        inProgressImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressImageDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageTwo;
        inProgressImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        inProgressImageDescriptorSet.dstBinding = 1;
        inProgressImageDescriptorSet.pImageInfo = &transmissionTexture.descriptor;
        inProgressImageDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet inProgressFragmentUniformDescriptorSet{};
        inProgressFragmentUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressFragmentUniformDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageTwo;
        inProgressFragmentUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        inProgressFragmentUniformDescriptorSet.dstBinding = 2;
        inProgressFragmentUniformDescriptorSet.pBufferInfo = &uniformBufferFragmentShader->getBufferInfo();
        inProgressFragmentUniformDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
                inProgressVertexUniformDescriptorSet,
                inProgressImageDescriptorSet,
                inProgressFragmentUniformDescriptorSet
        };

        vkUpdateDescriptorSets(engineDevice.getDevice(), writeDescriptorSets.size(), writeDescriptorSets.data(), 0,
                               nullptr);
    }

    // Post-Compute third stage
    {
        VkWriteDescriptorSet inProgressVertexUniformDescriptorSet{};
        inProgressVertexUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressVertexUniformDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageThree;
        inProgressVertexUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        inProgressVertexUniformDescriptorSet.dstBinding = 0;
        inProgressVertexUniformDescriptorSet.pBufferInfo = &uniformBufferVertexShader->getBufferInfo();
        inProgressVertexUniformDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet inProgressImageDescriptorSet{};
        inProgressImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressImageDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageThree;
        inProgressImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        inProgressImageDescriptorSet.dstBinding = 1;
        inProgressImageDescriptorSet.pImageInfo = &filteredTransmissionTexture.descriptor;
        inProgressImageDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet inProgressFragmentUniformDescriptorSet{};
        inProgressFragmentUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressFragmentUniformDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageThree;
        inProgressFragmentUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        inProgressFragmentUniformDescriptorSet.dstBinding = 2;
        inProgressFragmentUniformDescriptorSet.pBufferInfo = &uniformBufferFragmentShader->getBufferInfo();
        inProgressFragmentUniformDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
                inProgressVertexUniformDescriptorSet,
                inProgressImageDescriptorSet,
                inProgressFragmentUniformDescriptorSet
        };

        vkUpdateDescriptorSets(engineDevice.getDevice(), writeDescriptorSets.size(), writeDescriptorSets.data(), 0,
                               nullptr);
    }

    // Post-Compute fourth stage
    {
        VkWriteDescriptorSet inProgressVertexUniformDescriptorSet{};
        inProgressVertexUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressVertexUniformDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageFour;
        inProgressVertexUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        inProgressVertexUniformDescriptorSet.dstBinding = 0;
        inProgressVertexUniformDescriptorSet.pBufferInfo = &uniformBufferVertexShader->getBufferInfo();
        inProgressVertexUniformDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet inProgressImageDescriptorSet{};
        inProgressImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressImageDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageFour;
        inProgressImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        inProgressImageDescriptorSet.dstBinding = 1;
        inProgressImageDescriptorSet.pImageInfo = &filteredTransmissionTexture.descriptor;
        inProgressImageDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet inProgressFragmentUniformDescriptorSet{};
        inProgressFragmentUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        inProgressFragmentUniformDescriptorSet.dstSet = graphics.descriptorSetPostComputeStageFour;
        inProgressFragmentUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        inProgressFragmentUniformDescriptorSet.dstBinding = 2;
        inProgressFragmentUniformDescriptorSet.pBufferInfo = &uniformBufferFragmentShader->getBufferInfo();
        inProgressFragmentUniformDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
                inProgressVertexUniformDescriptorSet,
                inProgressImageDescriptorSet,
                inProgressFragmentUniformDescriptorSet
        };

        vkUpdateDescriptorSets(engineDevice.getDevice(), writeDescriptorSets.size(), writeDescriptorSets.data(), 0,
                               nullptr);
    }

    // Post-Compute final image
    {
        VkWriteDescriptorSet postComputeVertexUniformDescriptorSet{};
        postComputeVertexUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        postComputeVertexUniformDescriptorSet.dstSet = graphics.descriptorSetPostComputeFinal;
        postComputeVertexUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        postComputeVertexUniformDescriptorSet.dstBinding = 0;
        postComputeVertexUniformDescriptorSet.pBufferInfo = &uniformBufferVertexShader->getBufferInfo();
        postComputeVertexUniformDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet postImageDescriptorSet{};
        postImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        postImageDescriptorSet.dstSet = graphics.descriptorSetPostComputeFinal;
        postImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        postImageDescriptorSet.dstBinding = 1;
        postImageDescriptorSet.pImageInfo = &radianceTexture.descriptor;
        postImageDescriptorSet.descriptorCount = 1;

        VkWriteDescriptorSet postComputeFragmentUniformDescriptorSet{};
        postComputeFragmentUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        postComputeFragmentUniformDescriptorSet.dstSet = graphics.descriptorSetPostComputeFinal;
        postComputeFragmentUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        postComputeFragmentUniformDescriptorSet.dstBinding = 2;
        postComputeFragmentUniformDescriptorSet.pBufferInfo = &uniformBufferFragmentShader->getBufferInfo();
        postComputeFragmentUniformDescriptorSet.descriptorCount = 1;

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
                postComputeVertexUniformDescriptorSet,
                postImageDescriptorSet,
                postComputeFragmentUniformDescriptorSet
        };

        vkUpdateDescriptorSets(engineDevice.getDevice(), writeDescriptorSets.size(), writeDescriptorSets.data(), 0,
                               nullptr);
    }
}

void VulkanEngineEntryPoint::saveScreenshot(const char *filename) {
    bool supportsBlit = true;

    // Check blit support for source and destination
    VkFormatProperties formatProps;

    // Check if the device supports blitting from optimal images (the swap-chain images are in optimal format)
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

    // Source for the copy is the last rendered swap-chain image
    VkImage srcImage = renderer.getEngineSwapChain()->getImage(0);
    uint32_t width = renderer.getEngineSwapChain()->getSwapChainExtent().width;
    uint32_t height = renderer.getEngineSwapChain()->getSwapChainExtent().height;
    uint32_t channels = 4;

    // Create the linear tiled destination image to copy to and to read the memory from
    VkImageCreateInfo imageCreateCI{};
    imageCreateCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
    // Note that vkCmdBlitImage (if supported) will also do format conversions if the swap-chain color format would differ
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

    // Do the actual blit from the swap-chain image to our host visible destination image
    VkCommandBuffer copyCmd = engineDevice.beginSingleTimeCommands();

    // Transition destination image to transfer destination layout
    setImageLayout(copyCmd,
                   dstImage,
                   VK_IMAGE_ASPECT_COLOR_BIT,
                   VK_IMAGE_LAYOUT_UNDEFINED,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Transition swap-chain image from present to transfer source layout
    setImageLayout(copyCmd,
                   srcImage,
                   VK_IMAGE_ASPECT_COLOR_BIT,
                   VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT);

    // If source and destination support blit we'll blit as this also does automatic format conversion (e.g. from BGR to RGB)
    if (supportsBlit) {
        // Define the region to blit (we will blit the whole swap-chain image)
        VkOffset3D blitSize;
        blitSize.x = int(width);
        blitSize.y = int(height);
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
    const char *data;
    vkMapMemory(engineDevice.getDevice(), dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void **) &data);
    data += subResourceLayout.offset;

#if DEVICE_TYPE == 2
    size_t size = width * height * channels;

    auto *dataRgb = new uint8_t[size];
    for (size_t i = 0; i < size; i += channels) {
        dataRgb[i + 0] = data[i + 2];
        dataRgb[i + 1] = data[i + 1];
        dataRgb[i + 2] = data[i + 0];
        dataRgb[i + 3] = data[i + 3];
    }

    stbi_write_png(filename, int(width), int(height), int(channels), dataRgb, 0);
#else
    stbi_write_png(filename, int(width), int(height), int(channels), data, 0);
#endif
    fmt::print("Screenshot saved to disk\n");

    // Clean up resources
    vkUnmapMemory(engineDevice.getDevice(), dstImageMemory);
    vkFreeMemory(engineDevice.getDevice(), dstImageMemory, nullptr);
    vkDestroyImage(engineDevice.getDevice(), dstImage, nullptr);
}

VkPipelineShaderStageCreateInfo
VulkanEngineEntryPoint::loadShader(const std::string &fileName, VkShaderStageFlagBits stage) {
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
        is.read(shaderCode, std::streamsize(size));
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
    const Uint8 *keystate = SDL_GetKeyboardState(nullptr);

    if (isStepping) {
        isPaused = true;
        isStepping = false;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
#if DEBUG_GUI_ENABLED
        debugGui.processEvent(event);
#endif
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
                if (event.button.button == SDL_BUTTON_LEFT && keystate[SDL_SCANCODE_LCTRL]) {
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
                    glm::vec2 difPos = glm::vec2(mouseDragOrigin.x - float(event.button.x),
                                                 mouseDragOrigin.y - float(event.button.y));

                    panPosition += glm::vec2(-difPos.x, -difPos.y);
                    mouseDragOrigin = glm::vec2(event.button.x, event.button.y);
                }

            default:
                break;
        }
    }

    if (keystate[SDL_SCANCODE_ESCAPE]) {
        isRunning = false;
    } else if (keystate[SDL_SCANCODE_P]) {
        saveScreenshot(fmt::format("../screenshots/screenshot_frame_{}.png", dataset->frameIndex).c_str());
    } else if (keystate[SDL_SCANCODE_A]) {
        isPaused = false;
    } else if (keystate[SDL_SCANCODE_S]) {
        isPaused = true;
    } else if (keystate[SDL_SCANCODE_SPACE]) {
        isPaused = false;
        isStepping = true;
    }
}