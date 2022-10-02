//
// Created by Stanislav Svědiroh on 13.06.2022.
//

#include "VulkanEngineWindow.h"

VulkanEngineWindow::VulkanEngineWindow(const char *title, uint32_t width, uint32_t height, int flags) :
        windowName{title}, width_{width}, height_{height}, flags_{flags} {
    initWindow();
}

VulkanEngineWindow::~VulkanEngineWindow() {
    SDL_DestroyWindow(window);
}

void VulkanEngineWindow::initWindow() {
    if (SDL_Init(SDL_INIT_EVERYTHING) == 0) {
        window = SDL_CreateWindow(windowName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (int) width_, (int) height_, flags_);
    } else {
        throw std::runtime_error("Failed to initialize SDL subsystem!");
    }
}