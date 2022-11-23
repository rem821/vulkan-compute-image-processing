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

void DebugGui::showWindow(SDL_Window *window, long frameIndex, const std::vector<double> &visibility,
                          const cv::Mat &histograms, const cv::Mat &glareAmounts) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);

    ImGui::NewFrame();

    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
    window_flags |= ImGuiWindowFlags_NoScrollbar;
    window_flags |= ImGuiWindowFlags_MenuBar;
    window_flags |= ImGuiWindowFlags_NoNav;
    window_flags |= ImGuiWindowFlags_NoBackground;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;

    const ImGuiViewport *main_viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);


    if (!ImGui::Begin("Runtime info", nullptr, window_flags)) {
        // Early out if the window is collapsed, as an optimization.
        ImGui::End();
        return;
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

    std::vector<double> x(frameIndex);
    std::iota(std::begin(x), std::end(x), 0);
    if (ImPlot::BeginPlot("##Visibility")) {
        ImPlot::SetupAxis(ImAxis_X1, "Frame", ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxis(ImAxis_Y1, "Visibility", ImPlotAxisFlags_AutoFit);

        ImPlot::PlotLine("", x.data(), visibility.data(), int(frameIndex), ImPlotLineFlags_NoClip);
        ImPlot::EndPlot();
    }

    if (ImPlot::BeginPlot("##Histogram")) {
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 256);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, HISTOGRAM_BINS);

        uchar *xData = histograms.row(0).data;
        ImPlot::PlotBars("", xData, HISTOGRAM_BINS);
        ImPlot::EndPlot();
    }

    ImPlot::PushColormap(ImPlotColormap_Plasma);
    if (ImPlot::BeginPlot("##GlareHeatmap")) {
        static int axesFlags = ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks |
                ImPlotAxisFlags_NoLabel;
        ImPlot::SetupAxes(nullptr, nullptr, axesFlags, axesFlags);
        ImPlot::SetupAxisTicks(ImAxis_X1, 0.0 + 1.0 / HISTOGRAM_COUNT, 1.0 - 1.0 / HISTOGRAM_COUNT, HISTOGRAM_COUNT, nullptr);
        ImPlot::SetupAxisTicks(ImAxis_Y1, 1.0 - 1.0 / HISTOGRAM_COUNT, 0.0 + 1.0 / HISTOGRAM_COUNT, HISTOGRAM_COUNT, nullptr);

        ImPlot::PlotHeatmap("", glareAmounts.data, HISTOGRAM_COUNT, HISTOGRAM_COUNT, 0, 1, nullptr, ImPlotPoint(0, 0), ImPlotPoint(1, 1), 0);
        ImPlot::EndPlot();
    }

    ImGui::End();

    ImGui::Render();
}

void DebugGui::render(VkCommandBuffer &commandBuffer) {
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void DebugGui::processEvent(SDL_Event event) {
    ImGui_ImplSDL2_ProcessEvent(&event);
}