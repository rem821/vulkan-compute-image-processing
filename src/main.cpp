#include <iostream>

#include "VulkanEngineEntryPoint.h"

int main() {
    VulkanEngineEntryPoint* entryPoint = new VulkanEngineEntryPoint();
    while(entryPoint->isRunning) {
        entryPoint->handleEvents();
        entryPoint->render();
    }

    return 0;
}