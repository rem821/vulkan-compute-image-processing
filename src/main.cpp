#include "VulkanEngineEntryPoint.h"

#include <string>
#include <dlfcn.h>

#include "GlobalConfiguration.h"
#include "../external/renderdoc/renderdoc_app.h"
#include "algorithms/ImuExtract.h"

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

    auto *imu = new ImuExtract();
    auto *entryPoint = new VulkanEngineEntryPoint();
    while (entryPoint->isRunning) {
        imu->extractData(entryPoint->frameIndex);
        entryPoint->setImuPose(imu->heading, imu->headingDif, imu->attitude, imu->attitudeDif);
        entryPoint->handleEvents();
        if (rdoc_api && RENDERDOC_ENABLED) rdoc_api->StartFrameCapture(nullptr, nullptr);
        entryPoint->render();
        if (rdoc_api && RENDERDOC_ENABLED) rdoc_api->EndFrameCapture(nullptr, nullptr);
    }
    return 0;
}