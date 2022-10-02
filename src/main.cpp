#include <iostream>

#include "VulkanEngineEntryPoint.h"
#include <chrono>
#include <thread>

int main() {
    VulkanEngineEntryPoint* entryPoint = new VulkanEngineEntryPoint();
    while(entryPoint->isRunning) {
        entryPoint->handleEvents();
        entryPoint->render();
        //std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}