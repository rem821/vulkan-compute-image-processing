//
// Created by Stanislav Svědiroh on 27.09.2022.
//
#include "VulkanEngineEntryPoint.h"
#include <vulkan/vulkan.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb/stb_image.h"

char *inputFile = "../assets/input.png";

VulkanEngineEntryPoint::VulkanEngineEntryPoint() {
    prepareInputImage();
    generateQuad();
    setupVertexDescriptions();

    camera.setViewYXZ(glm::vec3(0.0f, 0.0f, -2.0f), glm::vec3(0.0f));
    camera.setPerspectiveProjection(glm::radians(60.f), (float) WINDOW_WIDTH * 0.5f / (float) WINDOW_HEIGHT, 1.0f, 256.0f);

    prepareUniformBuffers();
    outputTexture.createTextureTarget(engineDevice, inputTexture);
    setupDescriptorSetLayout();
    preparePipelines();
    setupDescriptorPool();
    setupDescriptorSet();
    prepareCompute();

    isRunning = true;
}

VulkanEngineEntryPoint::~VulkanEngineEntryPoint() {

}

void VulkanEngineEntryPoint::prepareInputImage() {
    size_t size;
    int32_t width, height, channels;
    loadImageFromFile(inputFile, nullptr, size, width, height, channels);
    auto *pixels = new int8_t[size];
    loadImageFromFile(inputFile, pixels, size, width, height, channels);

    inputTexture.fromImageFile(pixels, size, VK_FORMAT_R8G8B8A8_UNORM, width, height, engineDevice, engineDevice.graphicsQueue(), VK_FILTER_LINEAR,
                               VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_LAYOUT_GENERAL);
}

bool VulkanEngineEntryPoint::loadImageFromFile(const char *file, void *pixels, size_t &size, int &width, int &height, int &channels) {
    stbi_uc *px = stbi_load(file, &width, &height, nullptr, STBI_rgb_alpha);

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

// Setup vertices for a single uv-mapped quad
void VulkanEngineEntryPoint::generateQuad() {
    // Setup vertices for a single uv-mapped quad made from two triangles
    std::vector<Vertex> vertices =
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
    vertexBuffer = std::make_unique<VulkanEngineBuffer>(engineDevice, sizeof(Vertex), vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vertexBuffer->map();
    vertexBuffer->writeToBuffer((void *) vertices.data());


    // Index buffer
    indexBuffer = std::make_unique<VulkanEngineBuffer>(engineDevice, sizeof(uint32_t), indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

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

// Prepare and initialize uniform buffer containing shader uniforms
void VulkanEngineEntryPoint::prepareUniformBuffers() {
    // Vertex shader uniform buffer block
    uniformBufferVS = std::make_unique<VulkanEngineBuffer>(engineDevice, sizeof(uboVS), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    uniformBufferVS->map();

    updateUniformBuffers();
}

void VulkanEngineEntryPoint::updateUniformBuffers() {
    uboVS.projection = camera.getProjection();
    uboVS.modelView = camera.getView();
    memcpy(uniformBufferVS->getMappedMemory(), &uboVS, sizeof(uboVS));
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
    if (vkCreateDescriptorSetLayout(engineDevice.getDevice(), &descriptorLayout, nullptr, &graphics.descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &graphics.descriptorSetLayout;

    if (vkCreatePipelineLayout(engineDevice.getDevice(), &pipelineLayoutCreateInfo, nullptr, &graphics.pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }
}

void VulkanEngineEntryPoint::preparePipelines() {
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
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

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

    if (vkCreateGraphicsPipelines(engineDevice.getDevice(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphics.pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }
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

        if (vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module!");
        }

        delete[] shaderCode;

        return shaderModule;
    } else {
        std::cerr << "Error: Could not open shader file \"" << fileName << "\"" << "\n";
        return VK_NULL_HANDLE;
    }
}

void VulkanEngineEntryPoint::setupDescriptorPool() {
    VkDescriptorPoolSize uniformBuffers{};
    uniformBuffers.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBuffers.descriptorCount = 2;

    VkDescriptorPoolSize imageSamplers{};
    imageSamplers.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    imageSamplers.descriptorCount = 2;

    VkDescriptorPoolSize storageImage{};
    storageImage.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    storageImage.descriptorCount = 2;

    std::vector<VkDescriptorPoolSize> poolSizes = {
            uniformBuffers, // Graphics pipelines uniform buffers
            imageSamplers, // Graphics pipelines image samplers for displaying compute output image
            storageImage, // Compute pipelines uses a storage image for image reads and writes
    };

    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolInfo.pPoolSizes = poolSizes.data();
    descriptorPoolInfo.maxSets = 3;

    if (vkCreateDescriptorPool(engineDevice.getDevice(), &descriptorPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

void VulkanEngineEntryPoint::setupDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.pSetLayouts = &graphics.descriptorSetLayout;
    allocInfo.descriptorSetCount = 1;

    // Input image (before compute post processing)
    if (vkAllocateDescriptorSets(engineDevice.getDevice(), &allocInfo, &graphics.descriptorSetPreCompute) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate pre-compute descriptor set!");
    }

    VkWriteDescriptorSet uniformDescriptorSet{};
    uniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    uniformDescriptorSet.dstSet = graphics.descriptorSetPreCompute;
    uniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformDescriptorSet.dstBinding = 0;
    uniformDescriptorSet.pBufferInfo = &uniformBufferVS->getBufferInfo();
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

    // Final image (after compute shader processing)
    if (vkAllocateDescriptorSets(engineDevice.getDevice(), &allocInfo, &graphics.descriptorSetPostCompute) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate post-compute descriptor set!");
    }

    VkWriteDescriptorSet postUniformDescriptorSet{};
    postUniformDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    postUniformDescriptorSet.dstSet = graphics.descriptorSetPostCompute;
    postUniformDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    postUniformDescriptorSet.dstBinding = 0;
    postUniformDescriptorSet.pBufferInfo = &uniformBufferVS->getBufferInfo();
    postUniformDescriptorSet.descriptorCount = 1;

    VkWriteDescriptorSet postImageDescriptorSet{};
    postImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    postImageDescriptorSet.dstSet = graphics.descriptorSetPostCompute;
    postImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    postImageDescriptorSet.dstBinding = 1;
    postImageDescriptorSet.pImageInfo = &outputTexture.descriptor;
    postImageDescriptorSet.descriptorCount = 1;


    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            postUniformDescriptorSet,
            postImageDescriptorSet,
    };
    vkUpdateDescriptorSets(engineDevice.getDevice(), writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}

void VulkanEngineEntryPoint::prepareCompute() {
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

    // Create compute pipeline
    // Compute pipelines are created separate from graphics pipelines even if they use the same queue
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            // Binding 0: Input image (read-only)
            inputImageLayoutBinding,
            // Binding 1: Output image (write)
            outputImageLayoutBinding,
    };

    VkDescriptorSetLayoutCreateInfo descriptorLayout{};
    descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout.pBindings = setLayoutBindings.data();
    descriptorLayout.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());

    if (vkCreateDescriptorSetLayout(engineDevice.getDevice(), &descriptorLayout, nullptr, &compute.descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout for compute pipeline!");
    }

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &compute.descriptorSetLayout;

    if (vkCreatePipelineLayout(engineDevice.getDevice(), &pipelineLayoutCreateInfo, nullptr, &compute.pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout for compute!");
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.pSetLayouts = &compute.descriptorSetLayout;
    allocInfo.descriptorSetCount = 1;

    if (vkAllocateDescriptorSets(engineDevice.getDevice(), &allocInfo, &compute.descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets for compute!");
    }

    VkWriteDescriptorSet inputImageDescriptorSet{};
    inputImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    inputImageDescriptorSet.dstSet = compute.descriptorSet;
    inputImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    inputImageDescriptorSet.dstBinding = 0;
    inputImageDescriptorSet.pImageInfo = &inputTexture.descriptor;
    inputImageDescriptorSet.descriptorCount = 1;

    VkWriteDescriptorSet outputImageDescriptorSet{};
    outputImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    outputImageDescriptorSet.dstSet = compute.descriptorSet;
    outputImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputImageDescriptorSet.dstBinding = 1;
    outputImageDescriptorSet.pImageInfo = &outputTexture.descriptor;
    outputImageDescriptorSet.descriptorCount = 1;


    std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
            inputImageDescriptorSet,
            outputImageDescriptorSet
    };
    vkUpdateDescriptorSets(engineDevice.getDevice(), computeWriteDescriptorSets.size(), computeWriteDescriptorSets.data(), 0, nullptr);

    // Create compute shader pipelines
    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.layout = compute.pipelineLayout;
    computePipelineCreateInfo.flags = 0;

    // One pipeline for each effect
    shaderNames = {"DarkChannelPrior"};
    for (auto &shaderName: shaderNames) {
        std::string fileName = "../shaders/" + shaderName + ".comp.spv";
        computePipelineCreateInfo.stage = loadShader(fileName, VK_SHADER_STAGE_COMPUTE_BIT);
        VkPipeline pipeline;
        if (vkCreateComputePipelines(engineDevice.getDevice(), VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipelines!");
        }
        compute.pipelines.push_back(pipeline);
    }
}

void VulkanEngineEntryPoint::render() {
    CommandBufferPair bufferPair = renderer.beginFrame();
    if (bufferPair.computeCommandBuffer != nullptr && bufferPair.graphicsCommandBuffer != nullptr) {
        // Record compute command buffer
        vkCmdBindPipeline(bufferPair.computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipelines[compute.pipelineIndex]);
        vkCmdBindDescriptorSets(bufferPair.computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipelineLayout, 0, 1, &compute.descriptorSet, 0,
                                nullptr);

        vkCmdDispatch(bufferPair.computeCommandBuffer, outputTexture.width / 16, outputTexture.height / 16, 1);

        // Record graphics commandBuffer
        renderer.beginSwapChainRenderPass(bufferPair.graphicsCommandBuffer, outputTexture.image);

        // Render first half of the screen
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(renderer.getEngineSwapChain()->getSwapChainExtent().width * 0.5f);
        viewport.height = static_cast<float>(renderer.getEngineSwapChain()->getSwapChainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{{0, 0}, renderer.getEngineSwapChain()->getSwapChainExtent()};

        vkCmdSetViewport(bufferPair.graphicsCommandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(bufferPair.graphicsCommandBuffer, 0, 1, &scissor);

        VkDeviceSize offsets[1] = {0};
        vkCmdBindVertexBuffers(bufferPair.graphicsCommandBuffer, 0, 1, vertexBuffer->getBuffer(), offsets);
        vkCmdBindIndexBuffer(bufferPair.graphicsCommandBuffer, *indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

        // Left (pre compute)
        vkCmdBindDescriptorSets(bufferPair.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipelineLayout, 0, 1,
                                &graphics.descriptorSetPreCompute, 0, nullptr);
        vkCmdBindPipeline(bufferPair.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipeline);

        vkCmdDrawIndexed(bufferPair.graphicsCommandBuffer, indexCount, 1, 0, 0, 0);

        // Right (post compute)
        vkCmdBindDescriptorSets(bufferPair.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipelineLayout, 0, 1,
                                &graphics.descriptorSetPostCompute, 0, nullptr);
        vkCmdBindPipeline(bufferPair.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipeline);

        viewport.x = static_cast<float>(renderer.getEngineSwapChain()->getSwapChainExtent().width * 0.5f);
        vkCmdSetViewport(bufferPair.graphicsCommandBuffer, 0, 1, &viewport);
        vkCmdDrawIndexed(bufferPair.graphicsCommandBuffer, indexCount, 1, 0, 0, 0);

        renderer.endSwapChainRenderPass(bufferPair.graphicsCommandBuffer);
        renderer.endFrame();
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
            default:
                break;
        }
    }

    const Uint8 *keystate = SDL_GetKeyboardState(nullptr);

    if (keystate[SDL_SCANCODE_ESCAPE]) {
        isRunning = false;
    }
}
