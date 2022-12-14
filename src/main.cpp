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
#include "algorithms/GeometryAssertion.h"

#include "threading/BS_thread_pool.h"

RENDERDOC_API_1_1_2 *rdoc_api = nullptr;

void runCameraAlgorithms(Dataset *dataset, BS::thread_pool &pool) {
    if (dataset != nullptr && dataset->frameIndex != 0) {
        Timer tim("All CPU algorithms", &dataset->allCPUAlgorithms);

        cv::Mat leftCameraFrameGray;
        cv::Mat rightCameraFrameGray;
        cv::cvtColor(dataset->leftCameraFrame, leftCameraFrameGray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(dataset->rightCameraFrame, rightCameraFrameGray, cv::COLOR_BGR2GRAY);

        estimateVanishingPointPosition(dataset);

        // Algorithms run in parallel pool
        pool.push_task(VisibilityCalculation::calculateVisibilityVp, leftCameraFrameGray, dataset,
                       dataset->vanishingPoint);
        pool.push_task(detectGlareAndOcclusion, leftCameraFrameGray, dataset);

        {
            Timer t("Fog detection", &dataset->fogDetection);
            for (int j = 0; j < DFT_BLOCK_COUNT; j++) {
                for (int i = 0; i < DFT_BLOCK_COUNT; i++) {
                    int w = (DFT_WINDOW_SIZE / 2) +
                            (i * ((dataset->cameraWidth - DFT_WINDOW_SIZE) / (DFT_BLOCK_COUNT - 1)));
                    int h = (DFT_WINDOW_SIZE / 2) +
                            (j * ((dataset->cameraHeight - DFT_WINDOW_SIZE) / (DFT_BLOCK_COUNT - 1)));

                    pool.push_task(VisibilityCalculation::calculateVisibility, leftCameraFrameGray, dataset,
                                   std::pair(w, h),
                                   std::pair(i, j));
                }
            }
            pool.wait_for_tasks();
        }

        {
            Timer timer("Assert camera geometry");
            assertCameraGeometry(dataset, leftCameraFrameGray, rightCameraFrameGray, pool);
        }

        VisibilityCalculation::calculateVisibilityScore(dataset);
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
    auto *datasetFileReader = new DatasetFileReader(dataset, pool);
    auto *entryPoint = new VulkanEngineEntryPoint(dataset);

    while (entryPoint->isRunning) {
        entryPoint->handleEvents();
        if (!entryPoint->isFinished) {
            if (!entryPoint->isPaused) {
                if (dataset->frameIndex > 0) {
                    entryPoint->isFinished = !datasetFileReader->readData(pool);
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