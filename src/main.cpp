#include <iostream>

#include "VulkanEngineEntryPoint.h"

#include <dlfcn.h>
#include "GlobalConfiguration.h"
#include "../external/renderdoc/renderdoc_app.h"

RENDERDOC_API_1_1_2 *rdoc_api = nullptr;

int main() {
    // Initialize RenderDoc API
    if (RENDERDOC_ENABLED) {
        if (void *mod = dlopen("../external/renderdoc/librenderdoc.so", RTLD_NOW)) {
            auto RENDERDOC_GetAPI = (pRENDERDOC_GetAPI) dlsym(mod, "RENDERDOC_GetAPI");
            int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_5_0, (void **) &rdoc_api);
            assert(ret == 1);
        }
        if (rdoc_api) {
            rdoc_api->TriggerCapture();
        }
    }


    VulkanEngineEntryPoint* entryPoint = new VulkanEngineEntryPoint();
    while(entryPoint->isRunning) {
        entryPoint->handleEvents();
        if (rdoc_api && RENDERDOC_ENABLED) rdoc_api->StartFrameCapture(nullptr, nullptr);
        entryPoint->render();
        if (rdoc_api && RENDERDOC_ENABLED) rdoc_api->EndFrameCapture(nullptr, nullptr);

        //std::this_thread::sleep_for(std::chrono::milliseconds(500));

    }

    return 0;
}