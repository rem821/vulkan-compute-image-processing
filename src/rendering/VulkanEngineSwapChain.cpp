//
// Created by Stanislav Svědiroh on 13.06.2022.
//

#include "VulkanEngineSwapChain.h"

VulkanEngineSwapChain::VulkanEngineSwapChain(VulkanEngineDevice &engineDevice, VkExtent2D extent) : engineDevice{
        engineDevice}, windowExtent{extent} {
    init();
}

VulkanEngineSwapChain::VulkanEngineSwapChain(VulkanEngineDevice &engineDevice, VkExtent2D extent,
                                             std::shared_ptr<VulkanEngineSwapChain> previous) : engineDevice{
        engineDevice}, windowExtent{extent}, oldSwapChain{previous} {
    init();

    oldSwapChain = nullptr;
}

void VulkanEngineSwapChain::init() {
    createSwapChain();
    createImageViews();
    createRenderPass();
    createDepthResources();
    createFrameBuffers();
    createSyncObjects();
}


VulkanEngineSwapChain::~VulkanEngineSwapChain() {
    for (auto imageView: swapChainImageViews) {
        vkDestroyImageView(engineDevice.getDevice(), imageView, nullptr);
    }
    swapChainImageViews.clear();

    if (swapChain != nullptr) {
        vkDestroySwapchainKHR(engineDevice.getDevice(), swapChain, nullptr);
        swapChain = nullptr;
    }

    for (int i = 0; i < depthImages.size(); i++) {
        vkDestroyImageView(engineDevice.getDevice(), depthImageViews[i], nullptr);
        vkDestroyImage(engineDevice.getDevice(), depthImages[i], nullptr);
        vkFreeMemory(engineDevice.getDevice(), depthImageMemory[i], nullptr);
    }

    for (auto frameBuffer: swapChainFramebuffers) {
        vkDestroyFramebuffer(engineDevice.getDevice(), frameBuffer, nullptr);
    }

    vkDestroyRenderPass(engineDevice.getDevice(), renderPass, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(engineDevice.getDevice(), renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(engineDevice.getDevice(), imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(engineDevice.getDevice(), inFlightFences[i], nullptr);
    }

    vkDestroySemaphore(engineDevice.getDevice(), computeSemaphore, nullptr);
}

VkFormat VulkanEngineSwapChain::findDepthFormat() {
    return engineDevice.findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkResult VulkanEngineSwapChain::acquireNextImage(uint32_t *imageIndex) {
    vkWaitForFences(
            engineDevice.getDevice(),
            1,
            &inFlightFences[currentFrame],
            VK_TRUE,
            std::numeric_limits<uint64_t>::max()
    );

    VkResult result = vkAcquireNextImageKHR(
            engineDevice.getDevice(),
            swapChain,
            std::numeric_limits<uint64_t>::max(),
            imageAvailableSemaphores[currentFrame],
            VK_NULL_HANDLE,
            imageIndex
    );
    return result;
}

VkResult
VulkanEngineSwapChain::submitCommandBuffers(const VkCommandBuffer *buffers, const VkCommandBuffer *computeCommandBuffer,
                                            const uint32_t *imageIndex) {
    if (imagesInFlight[*imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(engineDevice.getDevice(), 1, &imagesInFlight[*imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight[*imageIndex] = inFlightFences[currentFrame];

    // Wait for rendering finished
    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    VkSubmitInfo computeSubmitInfo = {};
    computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    computeSubmitInfo.commandBufferCount = 1;
    computeSubmitInfo.pCommandBuffers = computeCommandBuffer;
    computeSubmitInfo.waitSemaphoreCount = 1;
    computeSubmitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
    computeSubmitInfo.pWaitDstStageMask = &waitStageMask;
    computeSubmitInfo.signalSemaphoreCount = 1;
    computeSubmitInfo.pSignalSemaphores = &computeSemaphore;

    if (vkQueueSubmit(engineDevice.computeQueue(), 1, &computeSubmitInfo, VK_NULL_HANDLE)) {
        throw std::runtime_error("Failed to submit compute queue!");
    }

    VkPipelineStageFlags graphicsWaitStageMasks[] = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore graphicsWaitSemaphores[] = {computeSemaphore};
    VkSemaphore graphicsSignalSemaphores[] = {renderFinishedSemaphores[currentFrame]};

    // Submit graphics commands
    VkSubmitInfo graphicsSubmitInfo = {};
    graphicsSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    graphicsSubmitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];
    graphicsSubmitInfo.commandBufferCount = 1;
    graphicsSubmitInfo.pCommandBuffers = buffers;
    graphicsSubmitInfo.waitSemaphoreCount = 1;
    graphicsSubmitInfo.pWaitSemaphores = graphicsWaitSemaphores;
    graphicsSubmitInfo.pWaitDstStageMask = graphicsWaitStageMasks;
    graphicsSubmitInfo.signalSemaphoreCount = 1;
    graphicsSubmitInfo.pSignalSemaphores = graphicsSignalSemaphores;

    vkResetFences(engineDevice.getDevice(), 1, &inFlightFences[currentFrame]);
    if (vkQueueSubmit(engineDevice.graphicsQueue(), 1, &graphicsSubmitInfo, inFlightFences[currentFrame])) {
        throw std::runtime_error("Failed to submit graphics queue!");
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = imageIndex;

    auto result = vkQueuePresentKHR(engineDevice.presentQueue(), &presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    return result;
}

void VulkanEngineSwapChain::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = engineDevice.getSwapChainSupport();

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = engineDevice.surface();

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    QueueFamilyIndices indices = engineDevice.findPhysicalQueueFamilies();
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = oldSwapChain == nullptr ? VK_NULL_HANDLE : oldSwapChain->swapChain;

    VK_CHECK(vkCreateSwapchainKHR(engineDevice.getDevice(), &createInfo, nullptr, &swapChain));

    vkGetSwapchainImagesKHR(engineDevice.getDevice(), swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(engineDevice.getDevice(), swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void VulkanEngineSwapChain::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapChainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapChainImageFormat;
        viewInfo.components = {
                VK_COMPONENT_SWIZZLE_R,
                VK_COMPONENT_SWIZZLE_G,
                VK_COMPONENT_SWIZZLE_B,
                VK_COMPONENT_SWIZZLE_A
        };
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(engineDevice.getDevice(), &viewInfo, nullptr, &swapChainImageViews[i]));
    }
}

void VulkanEngineSwapChain::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();
    swapChainDepthFormat = depthFormat;
    VkExtent2D swapChainExt = getSwapChainExtent();

    depthImages.resize(imageCount());
    depthImageMemory.resize(imageCount());
    depthImageViews.resize(imageCount());

    for (int i = 0; i < depthImages.size(); i++) {
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = swapChainExt.width;
        imageInfo.extent.height = swapChainExt.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.flags = 0;

        engineDevice.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImages[i],
                                         depthImageMemory[i]);

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(engineDevice.getDevice(), &viewInfo, nullptr, &depthImageViews[i]));
    }
}

void VulkanEngineSwapChain::createRenderPass() {
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = getSwapChainImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcAccessMask = 0;
    dependency.srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstSubpass = 0;
    dependency.dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(engineDevice.getDevice(), &renderPassInfo, nullptr, &renderPass));
}

void VulkanEngineSwapChain::createFrameBuffers() {
    swapChainFramebuffers.resize(imageCount());
    for (size_t i = 0; i < imageCount(); i++) {
        std::array<VkImageView, 2> attachments = {swapChainImageViews[i], depthImageViews[i]};

        VkExtent2D swapChainExt = getSwapChainExtent();
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExt.width;
        framebufferInfo.height = swapChainExt.height;
        framebufferInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(engineDevice.getDevice(), &framebufferInfo, nullptr, &swapChainFramebuffers[i]));
    }
}

void VulkanEngineSwapChain::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(imageCount(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateSemaphore(engineDevice.getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(engineDevice.getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(engineDevice.getDevice(), &fenceInfo, nullptr, &inFlightFences[i]));
    }

    VK_CHECK(vkCreateSemaphore(engineDevice.getDevice(), &semaphoreInfo, nullptr, &computeSemaphore));
}

VkSurfaceFormatKHR
VulkanEngineSwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
    for (const auto &availableFormat: availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR
VulkanEngineSwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
    for (const auto &availablePresentMode: availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            fmt::print("Present mode: Mailbox\n");
            return availablePresentMode;
        }

        if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            fmt::print("Present mode: Immediate\n");
            return availablePresentMode;
        }
    }

    fmt::print("Present mode: V-Sync\n");
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanEngineSwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = windowExtent;
        actualExtent.width = std::max(capabilities.minImageExtent.width,
                                      std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height,
                                       std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}