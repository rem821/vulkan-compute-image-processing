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

#include "threading/BS_thread_pool.h"

RENDERDOC_API_1_1_2 *rdoc_api = nullptr;

void runCameraAlgorithms(Dataset *dataset, BS::thread_pool &pool) {
    if (dataset != nullptr && dataset->frameIndex != 0) {
        cv::Mat cameraFrameGray;
        cv::cvtColor(dataset->cameraFrame, cameraFrameGray, cv::COLOR_BGR2GRAY);

        estimateVanishingPointPosition(dataset);
        VisibilityCalculation::calculateVisibilityVp(cameraFrameGray, dataset, dataset->vanishingPoint);
        detectGlareAndOcclusion(cameraFrameGray, dataset);

        pool.paused = true;
        for (int j = 0; j < DFT_BLOCK_COUNT; j++) {
            for (int i = 0; i < DFT_BLOCK_COUNT; i++) {
                int w = (DFT_WINDOW_SIZE / 2) +
                        (i * ((dataset->cameraWidth - DFT_WINDOW_SIZE) / (DFT_BLOCK_COUNT - 1)));
                int h = (DFT_WINDOW_SIZE / 2) +
                        (j * ((dataset->cameraHeight - DFT_WINDOW_SIZE) / (DFT_BLOCK_COUNT - 1)));

                pool.push_task(VisibilityCalculation::calculateVisibility, cameraFrameGray, dataset, std::pair(w, h),
                               std::pair(i, j));
            }
        }
        pool.paused = false;

        {
            Timer timer("Calculating visibility of multiple points asynchronously");
            while (pool.get_tasks_running() > 0);
        }
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

    BS::thread_pool pool(std::thread::hardware_concurrency() - 1);

    auto *dataset = new Dataset();
    auto *datasetFileReader = new DatasetFileReader(dataset);
    auto *entryPoint = new VulkanEngineEntryPoint(dataset);

    while (entryPoint->isRunning) {
        entryPoint->handleEvents();
        if (!entryPoint->isFinished) {
            if (!entryPoint->isPaused) {
                if (dataset->frameIndex > 0) {
                    entryPoint->isFinished = !datasetFileReader->readData();
                    runCameraAlgorithms(dataset, pool);
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

    delete entryPoint;
    delete datasetFileReader;
    delete dataset;

    return 0;
}