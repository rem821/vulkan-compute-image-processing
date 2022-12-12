#include "DebugGui.h"
#include <numeric>

DebugGui::DebugGui(VulkanEngineDevice &engineDevice, VulkanEngineRenderer &renderer, SDL_Window *window) {
    imguiPool = VulkanEngineDescriptorPool::Builder(engineDevice)
            .setMaxSets(1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000)
            .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
            .build();

    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForVulkan(window);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = engineDevice.getInstance();
    init_info.PhysicalDevice = engineDevice.getPhysicalDevice();
    init_info.Device = engineDevice.getDevice();
    init_info.QueueFamily = engineDevice.findPhysicalQueueFamilies().graphicsFamily;
    init_info.Queue = engineDevice.graphicsQueue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imguiPool->getPool();
    init_info.Subpass = 0;
    init_info.MinImageCount = VulkanEngineSwapChain::MAX_FRAMES_IN_FLIGHT;
    init_info.ImageCount = VulkanEngineSwapChain::MAX_FRAMES_IN_FLIGHT;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    ImGui_ImplVulkan_Init(&init_info, renderer.getSwapChainRenderPass());

    VkCommandBuffer commandBuffer = engineDevice.beginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
    engineDevice.endSingleTimeCommands(commandBuffer, engineDevice.graphicsQueue());

    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

DebugGui::~DebugGui() {
    ImGui_ImplVulkan_Shutdown();
}

bool
DebugGui::showWindow(SDL_Window *window, long frameIndex, const Dataset &dataset) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);

    ImGui::NewFrame();

    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_MenuBar;
    window_flags |= ImGuiWindowFlags_NoBackground;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;

    const ImGuiViewport *main_viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);


    if (!ImGui::Begin("Runtime info", nullptr, window_flags)) {
        // Early out if the window is collapsed, as an optimization.
        ImGui::End();
        ImGui::EndFrame();

        return false;
    }

    // Frame index
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Frame: %ld", frameIndex);

    // Running time, Frame time and FPS (Moving Average)

    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Running time: %f s", ImGui::GetTime());
    double frameTime = (ImGui::GetTime() - lastFrameTimestamp) * 1000;
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Frame time: %f ms", frameTime);
    movingFPSAverage = (0.08f * (1000.0f / frameTime)) + (1.0f - 0.08f) * movingFPSAverage;
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "FPS: %f", movingFPSAverage);
    lastFrameTimestamp = ImGui::GetTime();

    ImGui::Text(" ");

    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Time: %02d.%02d.%d %02d:%02d:%02d", dataset.day, dataset.month,
                       dataset.year,
                       dataset.hours, dataset.minutes, dataset.seconds);

    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Position: LAT:%f, LONG:%f, ALT:%f", dataset.latitude, dataset.longitude,
                       dataset.altitude);
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Azimuth: %f", dataset.azimuth);
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Sunrise: %dh %02dmin", int(dataset.sunrise),
                       int((dataset.sunrise - int(dataset.sunrise)) * 60.0));
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Sunset: %dh %02dmin", int(dataset.sunset),
                       int((dataset.sunset - int(dataset.sunset)) * 60.0));

    ImGui::Text(" ");

    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Fog: %d", int(dataset.visibilityScore));
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Camera geometry: %d", int(dataset.geometryOk));

    std::vector<double> x(frameIndex);
    std::iota(std::begin(x), std::end(x), 0);
    if (ImPlot::BeginPlot("##Visibility (FFT)")) {
        ImPlot::SetupAxis(ImAxis_X1, "Frame", ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxis(ImAxis_Y1, "Visibility (FFT)", ImPlotAxisFlags_AutoFit);

        ImPlot::PlotLine("", x.data(), dataset.vp_visibility.data(), int(frameIndex), ImPlotLineFlags_NoClip);
        ImPlot::EndPlot();
    }

    /*
    if (ImPlot::BeginPlot("##Histogram")) {
        ImPlot::SetupAxis(ImAxis_Y1, "Count", ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxis(ImAxis_X1, "Bin", 0);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, KEYPOINT_HIST_BINS);

        ImPlot::PlotBars("", dataset.y_dists.data(), KEYPOINT_HIST_BINS);
        ImPlot::EndPlot();
    }
    */

    ImPlot::PushColormap(ImPlotColormap_Plasma);
    if (ImPlot::BeginPlot("##VisibilityHeatmap")) {
        static int axesFlags = ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks |
                               ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels;
        ImPlot::SetupAxes(nullptr, nullptr, axesFlags, axesFlags);
        ImPlot::SetupAxisTicks(ImAxis_X1, 0.0 + 1.0 / DFT_BLOCK_COUNT, 1.0 - 1.0 / DFT_BLOCK_COUNT, DFT_BLOCK_COUNT,
                               nullptr);
        ImPlot::SetupAxisTicks(ImAxis_Y1, 1.0 - 1.0 / DFT_BLOCK_COUNT, 0.0 + 1.0 / DFT_BLOCK_COUNT, DFT_BLOCK_COUNT,
                               nullptr);
        ImPlot::PlotHeatmap("", &dataset.visibility.at<float>(0), DFT_BLOCK_COUNT, DFT_BLOCK_COUNT, 0, 1, nullptr,
                            ImPlotPoint(0, 0), ImPlotPoint(1, 1), ImPlotHeatmapFlags_ColMajor);
        ImPlot::EndPlot();
    }

    ImPlot::PushColormap(ImPlotColormap_Plasma);
    if (ImPlot::BeginPlot("##GlareHeatmap")) {
        static int axesFlags = ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks |
                               ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels;
        ImPlot::SetupAxes(nullptr, nullptr, axesFlags, axesFlags);
        ImPlot::SetupAxisTicks(ImAxis_X1, 0.0 + 1.0 / HISTOGRAM_COUNT, 1.0 - 1.0 / HISTOGRAM_COUNT, HISTOGRAM_COUNT,
                               nullptr);
        ImPlot::SetupAxisTicks(ImAxis_Y1, 1.0 - 1.0 / HISTOGRAM_COUNT, 0.0 + 1.0 / HISTOGRAM_COUNT, HISTOGRAM_COUNT,
                               nullptr);
        ImPlot::PlotHeatmap("", &dataset.glareAmounts.at<float>(0), HISTOGRAM_COUNT, HISTOGRAM_COUNT, 0, 256, nullptr,
                            ImPlotPoint(0, 0), ImPlotPoint(1, 1), ImPlotHeatmapFlags_ColMajor);
        ImPlot::EndPlot();
    }

    ImGui::End();

    ImGui::Render();
    return true;
}

void DebugGui::render(VkCommandBuffer &commandBuffer) {
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void DebugGui::processEvent(SDL_Event event) {
    ImGui_ImplSDL2_ProcessEvent(&event);
}