#include "VulkanEngineEntryPoint.h"

#include <string>
#include <dlfcn.h>

#include "GlobalConfiguration.h"
#include "util/Dataset.h"
#include "../external/renderdoc/renderdoc_app.h"
#include "algorithms/DatasetFileReader.h"

#include "algorithms/VisibilityCalculation.h"
#include "algorithms/GlareAndOcclusionDetection.h"
#include "algorithms/VanishingPointEstimation.h"

RENDERDOC_API_1_1_2 *rdoc_api = nullptr;

void runCameraAlgorithms(Dataset *dataset) {
    if (dataset != nullptr && dataset->frameIndex != 0) {
        cv::Mat cameraFrameGray;
        cv::cvtColor(dataset->cameraFrame, cameraFrameGray, cv::COLOR_BGR2GRAY);

        estimateVanishingPointPosition(dataset);
        calculateVisibility(cameraFrameGray, dataset);
        detectGlareAndOcclusion(cameraFrameGray, dataset);
    }
}

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
    std::unique_ptr<Dataset> dataset = std::make_unique<Dataset>();

    std::unique_ptr<DatasetFileReader> datasetFileReader = std::make_unique<DatasetFileReader>(dataset.get());
    std::unique_ptr<VulkanEngineEntryPoint> entryPoint = std::make_unique<VulkanEngineEntryPoint>(dataset.get());

    while (entryPoint->isRunning) {
        entryPoint->handleEvents();
        if (!entryPoint->isFinished) {
            if (!entryPoint->isPaused) {
                if (dataset->frameIndex > 0) {
                    datasetFileReader->readData();
                    runCameraAlgorithms(dataset.get());
                    entryPoint->prepareNextFrame();
                }

#if RENDERDOC_ENABLED
                if (rdoc_api) rdoc_api->StartFrameCapture(nullptr, nullptr);
#endif

                entryPoint->render();

#if RENDERDOC_ENABLED
                if (rdoc_api) rdoc_api->EndFrameCapture(nullptr, nullptr);
#endif
                dataset->frameIndex++;
#if TIMER_ON
                fmt::print("--------------------------------------------------------------------------------------------\n");
#endif
            }
        }
    }
    return 0;
}