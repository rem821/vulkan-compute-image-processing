//
// Created by Stanislav Svědiroh on 14.06.2022.
//

#include "VulkanEngineRenderer.h"
#include "../profiling/Timer.h"
#include <fmt/core.h>

VulkanEngineRenderer::VulkanEngineRenderer(VulkanEngineWindow &window, VulkanEngineDevice &device) : window(window),
                                                                                                     engineDevice(
                                                                                                             device) {
    recreateSwapChain();
    createCommandBuffers();
}

VulkanEngineRenderer::~VulkanEngineRenderer() { freeCommandBuffers(); }

CommandBufferPair VulkanEngineRenderer::beginFrame() {
    assert(!isFrameStarted && "Can't call beginFrame while already in progress!");

    auto result = engineSwapChain->acquireNextImage(&currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return CommandBufferPair{};
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    isFrameStarted = true;

    // Begin compute command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_CHECK(vkBeginCommandBuffer(computeCommandBuffer, &beginInfo));

    // Begin graphics command buffer
    auto graphicsCommandBuffer = getCurrentGraphicsCommandBuffer();
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_CHECK(vkBeginCommandBuffer(graphicsCommandBuffer, &beginInfo));
    return CommandBufferPair{graphicsCommandBuffer, computeCommandBuffer};
}

void VulkanEngineRenderer::endFrame(Dataset *dataset) {
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress!");
    auto commandBuffer = getCurrentGraphicsCommandBuffer();

    VK_CHECK(vkEndCommandBuffer(computeCommandBuffer));
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    {
        Timer timer("Frame submission", dataset->frameSubmission);

        auto result = engineSwapChain->submitCommandBuffers(&commandBuffer, &computeCommandBuffer, &currentImageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.wasWindowResized()) {
            window.resetWindowsResizedFlag();
            recreateSwapChain();
        }


        if (result != VK_SUCCESS) {
            fmt::print("Failed to present swap chain image!");
        }
    }

    isFrameStarted = false;
    currentFrameIndex = (currentFrameIndex + 1) % VulkanEngineSwapChain::MAX_FRAMES_IN_FLIGHT;
}

void VulkanEngineRenderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer, VkImage &outputImage) {
    assert(isFrameStarted && "Can't call beginSwapChainRenderPass if frame is not in progress!");
    assert(commandBuffer == getCurrentGraphicsCommandBuffer() &&
           "Can't begin render pass on command buffer from a different frame");

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = engineSwapChain->getRenderPass();
    renderPassInfo.framebuffer = engineSwapChain->getFrameBuffer(currentImageIndex);

    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = engineSwapChain->getSwapChainExtent();

    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    // Image memory barrier to make sure that compute shader writes are finished before sampling from the texture
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // We won't be changing the layout of the image
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.image = outputImage;
    imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrier);

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanEngineRenderer::endSwapChainRenderPass(VkCommandBuffer graphicsCommandBuffer) {
    assert(isFrameStarted && "Can't call endSwapChainRenderPass if frame is not in progress!");
    assert(graphicsCommandBuffer == getCurrentGraphicsCommandBuffer() &&
           "Can't end render pass on command buffer from a different frame");

    vkCmdEndRenderPass(graphicsCommandBuffer);
}

void VulkanEngineRenderer::createCommandBuffers() {
    graphicsCommandBuffers.resize(VulkanEngineSwapChain::MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = engineDevice.getGraphicsCommandPool();
    allocInfo.commandBufferCount = static_cast<uint32_t>(graphicsCommandBuffers.size());

    VK_CHECK(vkAllocateCommandBuffers(engineDevice.getDevice(), &allocInfo, graphicsCommandBuffers.data()));

    // Create a command buffer for compute operations
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = engineDevice.getComputeCommandPool();
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(engineDevice.getDevice(), &commandBufferAllocateInfo, &computeCommandBuffer));
}

void VulkanEngineRenderer::freeCommandBuffers() {

}

void VulkanEngineRenderer::recreateSwapChain() {
    auto extent = window.getExtent();
    while (extent.width == 0 || extent.height == 0) {
        extent = window.getExtent();
        SDL_WaitEvent(nullptr);
    }

    vkDeviceWaitIdle(engineDevice.getDevice());

    if (engineSwapChain == nullptr) {
        engineSwapChain = std::make_unique<VulkanEngineSwapChain>(engineDevice, extent);
    } else {
        std::shared_ptr<VulkanEngineSwapChain> oldSwapChain = std::move(engineSwapChain);
        engineSwapChain = std::make_unique<VulkanEngineSwapChain>(engineDevice, extent, oldSwapChain);

        if (!oldSwapChain->compareSwapFormats(*engineSwapChain)) {
            throw std::runtime_error("Swap chain image(or depth) format has changed!");
        }
    }
}
