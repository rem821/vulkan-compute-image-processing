#include <iostream>

#include "VulkanEngineEntryPoint.h"

int main() {
    VulkanEngineEntryPoint* entryPoint = new VulkanEngineEntryPoint();
    while(entryPoint->isRunning) {
        entryPoint->render();
    }

    try {
        //VulkanCompute::VulkanEngine vulkanEngine = VulkanCompute::VulkanEngine();

        //vulkanEngine.submitCommandBuffer();

        //size_t imageSize = vulkanEngine.inputImage.width * vulkanEngine.inputImage.height * vulkanEngine.inputImage.channels;

        /*
        int32_t* inputImagePtr = static_cast<int32_t*>(vulkanEngine.getDevice().mapMemory(vulkanEngine.inputImage._imageMemory, 0, imageSize));
        for (uint32_t I = 0; I < imageSize; ++I)
        {
            std::cout << inputImagePtr[I] << " ";
        }
        std::cout << std::endl;
        vulkanEngine.getDevice().unmapMemory(vulkanEngine.inputImage._imageMemory);

        int32_t* outputImagePtr = static_cast<int32_t*>(vulkanEngine.getDevice().mapMemory(vulkanEngine.outputImage._imageMemory, 0, imageSize));
        for (uint32_t I = 0; I < imageSize; ++I)
        {
            std::cout << outputImagePtr[I] << " ";
        }
        std::cout << std::endl;
        vulkanEngine.getDevice().unmapMemory(vulkanEngine.outputImage._imageMemory);
*/
        /*
        const uint32_t NumElements = 10;
        const uint32_t BufferSize = NumElements * sizeof(int32_t);

        VulkanCompute::AllocatedBuffer inBuffer = vulkanEngine.createBuffer(BufferSize, vk::BufferUsageFlagBits::eStorageBuffer);
        VulkanCompute::AllocatedBuffer outBuffer = vulkanEngine.createBuffer(BufferSize, vk::BufferUsageFlagBits::eStorageBuffer);

        int32_t* InBufferPtr = static_cast<int32_t*>(vulkanEngine.getDevice().mapMemory(inBuffer._bufferMemory, 0, BufferSize));
        for (int32_t I = 0; I < NumElements; ++I) {
            InBufferPtr[I] = I;
        }
        vulkanEngine.getDevice().unmapMemory(inBuffer._bufferMemory);

        vulkanEngine.getDevice().bindBufferMemory(inBuffer._buffer, inBuffer._bufferMemory, 0);
        vulkanEngine.getDevice().bindBufferMemory(outBuffer._buffer, outBuffer._bufferMemory, 0);

        vulkanEngine.submitCommandBuffer();

        InBufferPtr = static_cast<int32_t*>(vulkanEngine.getDevice().mapMemory(inBuffer._bufferMemory, 0, BufferSize));
        for (uint32_t I = 0; I < NumElements; ++I)
        {
            std::cout << InBufferPtr[I] << " ";
        }
        std::cout << std::endl;
        vulkanEngine.getDevice().unmapMemory(inBuffer._bufferMemory);

        int32_t* OutBufferPtr = static_cast<int32_t*>(vulkanEngine.getDevice().mapMemory(outBuffer._bufferMemory, 0, BufferSize));
        for (uint32_t I = 0; I < NumElements; ++I)
        {
            std::cout << OutBufferPtr[I] << " ";
        }
        std::cout << std::endl;
        vulkanEngine.getDevice().unmapMemory(outBuffer._bufferMemory);

         */
    }
    catch (const std::exception &Exception) {
        std::cout << "Error: " << Exception.what() << std::endl;
    }

    return 0;
}