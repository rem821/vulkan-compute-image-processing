#include <iostream>

#include "VulkanEngineEntryPoint.h"

#include <string>
#include <dlfcn.h>
#include "GlobalConfiguration.h"
#include "../external/renderdoc/renderdoc_app.h"
#include "../external/fastcsv/csv.h"

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

    io::CSVReader<1> in_timestamps("../assets/timestamps.txt");
    in_timestamps.read_header(io::ignore_extra_column, "timestamp");

    std::vector<long> timestamps;
    long _t;
    while (in_timestamps.read_row(_t)) {
        timestamps.emplace_back(_t);
    }

    io::CSVReader<11> in_imu("../assets/imu.txt");
    in_imu.read_header(io::ignore_extra_column, "timestamp", "acc_x", "acc_y", "acc_z", "ang_vel_x", "ang_vel_y", "ang_vel_z", "quat_x", "quat_y", "quat_z", "quat_w");
    std::vector<long> imu_timestamps;
    std::vector<float> acc_x, acc_y, acc_z, ang_vel_x, ang_vel_y , ang_vel_z, quat_x, quat_y, quat_z, quat_w;
    float _acc_x, _acc_y, _acc_z, _ang_vel_x, _ang_vel_y , _ang_vel_z, _quat_x, _quat_y, _quat_z, _quat_w;
    while (in_imu.read_row(_t, _acc_x, _acc_y, _acc_z, _ang_vel_x, _ang_vel_y , _ang_vel_z, _quat_x, _quat_y, _quat_z, _quat_w)) {
        imu_timestamps.emplace_back(_t);

        acc_x.emplace_back(_acc_x);
        acc_y.emplace_back(_acc_y);
        acc_z.emplace_back(_acc_z);

        ang_vel_x.emplace_back(_ang_vel_x);
        ang_vel_y.emplace_back(_ang_vel_y);
        ang_vel_z.emplace_back(_ang_vel_z);

        quat_x.emplace_back(_quat_x);
        quat_y.emplace_back(_quat_y);
        quat_z.emplace_back(_quat_z);
        quat_w.emplace_back(_quat_w);
    }

    auto *entryPoint = new VulkanEngineEntryPoint();
    while (entryPoint->isRunning) {
        entryPoint->handleEvents();
        if (rdoc_api && RENDERDOC_ENABLED) rdoc_api->StartFrameCapture(nullptr, nullptr);
        entryPoint->render();
        if (rdoc_api && RENDERDOC_ENABLED) rdoc_api->EndFrameCapture(nullptr, nullptr);

        entryPoint->frameIndex;
        //std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}