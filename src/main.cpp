#include "VulkanEngineEntryPoint.h"

#include <string>
#include <dlfcn.h>

#include "GlobalConfiguration.h"
#include "../external/renderdoc/renderdoc_app.h"
#include "algorithms/DatasetFileReader.h"

RENDERDOC_API_1_1_2 *rdoc_api = nullptr;

int main() {
#if RENDERDOC_ENABLED
    // Initialize RenderDoc API
    if (void *mod = dlopen("../external/renderdoc/librenderdoc.so", RTLD_NOW)) {
        auto RENDERDOC_GetAPI = (pRENDERDOC_GetAPI) dlsym(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_5_0, (void **) &rdoc_api);
        assert(ret == 1);
    }
    if (rdoc_api) {
        rdoc_api->TriggerCapture();
    }
#endif

    std::unique_ptr<VulkanEngineEntryPoint> entryPoint = std::make_unique<VulkanEngineEntryPoint>();
    std::unique_ptr<DatasetFileReader> dataset = std::make_unique<DatasetFileReader>();
    entryPoint->setDataset(dataset->getDataset());

    while (entryPoint->isRunning) {
        entryPoint->handleEvents();
        if (!entryPoint->isFinished) {
            if (!entryPoint->isPaused) {
                dataset->readData(entryPoint->frameIndex);
                entryPoint->prepareNextFrame();

#if RENDERDOC_ENABLED
                if (rdoc_api) rdoc_api->StartFrameCapture(nullptr, nullptr);
#endif

                entryPoint->render();

#if RENDERDOC_ENABLED
                if (rdoc_api) rdoc_api->EndFrameCapture(nullptr, nullptr);
#endif
            }
        }
    }
    return 0;
}