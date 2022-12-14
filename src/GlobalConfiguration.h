//
// Created by Stanislav Svědiroh on 12.07.2022.
//
#pragma once

// Device type switch
//  0 --> Home PC (NVIDIA RTX 3080)
//  1 --> Work PC (AMD Radeon card)
//  2 --> Macbook Pro  16" 2019 (AMD Radeon card)

#define DEVICE_TYPE 0

#if DEVICE_TYPE == 0

// Graphics setting section
#define INTEGRATED_GRAPHICS false
#define VALIDATION_LAYER_NAME "VK_LAYER_LUNARG_standard_validation"

// Content section
#define VIDEO_DOWNSCALE_FACTOR 1
#define SESSION_PATH "/home/standa/3_1_1_1/"
#define IMAGE_PATH "../assets/haze.jpg"

// Shaders section
#define WORKGROUP_COUNT 64

#elif DEVICE_TYPE == 1

// Graphics setting section
#define INTEGRATED_GRAPHICS false
#define VALIDATION_LAYER_NAME "VK_LAYER_LUNARG_standard_validation"

// Content section
#define VIDEO_DOWNSCALE_FACTOR 1
#define SESSION_PATH "/mnt/B0E0DAB9E0DA84CE/BUD/3_1_1_1/"
#define IMAGE_PATH "../assets/image.jpg"

// Shaders section
#define WORKGROUP_COUNT 64

#elif DEVICE_TYPE == 2

// Graphics setting section
#define INTEGRATED_GRAPHICS false
#define VALIDATION_LAYER_NAME "VK_LAYER_KHRONOS_validation"

// Content section
#define VIDEO_DOWNSCALE_FACTOR 1
#define SESSION_PATH "/Users/stanislavsvediroh/Downloads/1_1_1_1/"

// Shaders section
#define WORKGROUP_COUNT 24

#endif

// Common section
#define WINDOW_TITLE "Vulkan Compute Render Window"
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1200

#define SINGLE_VIEW_MODE true

#define TIMEZONE_OFFSET 1

#define DARK_CHANNEL_PRIOR_SHADER "ImageDarkChannelPrior"
#define TRANSMISSION_SHADER "ImageTransmission"
#define MAXIMUM_AIRLIGHT_SHADER "MaximumAirLight"
#define GUIDED_FILTER_SHADER "GuidedFilter"
#define RADIANCE_SHADER "ImageRadiance"

// Files
#define LEFT_VIDEO_PATH "camera_left_front/video.mp4"
#define RIGHT_VIDEO_PATH "camera_right_front/video.mp4"
#define THERMAL_VIDEO_PATH "camera_ir/video.mp4"
#define THERMAL_TIMESTAMPS_PATH "camera_ir/timestamps.txt"
#define CAMERA_TIMESTAMPS_PATH "camera_left_front/timestamps.txt"
#define IMU_PATH "imu/imu.txt"
#define TIME_PATH "gnss/time.txt"
#define POSE_PATH "gnss/pose.txt"

// IMU
#define MAX_HEADING_DIF 3
#define MAX_ATTITUDE_DIF 3

// VISIBILITY CALCULATION
#define DFT_WINDOW_SIZE 128
#define HORIZONTAL_OFFSET 0
#define VERTICAL_OFFSET (-50)
#define HORIZONTAL_SENSITIVITY 150
#define VERTICAL_SENSITIVITY 2500

#define MAX_VISIBILITY_THRESHOLD 3400.0
#define MIN_VISIBILITY_THRESHOLD 2600.0
#define MOVING_AVERAGE_FORGET_RATE 0.005

// GLARE DETECTION
#define HISTOGRAM_COUNT 12
#define HISTOGRAM_BINS 16
#define GLARE_THRESHOLD 0.6
#define OCCLUSION_THRESHOLD 0.0625
#define OCCLUSION_MIN_FRAMES 100

// DFT BLOCK ANALYSIS
#define DFT_BLOCK_COUNT 8

// KEYPOINT MATCHING
#define MAX_KEYPOINTS 200 // Don't forget to mirror this setting into texture.frag shader
#define KEYPOINT_HIST_BINS 32

// Debugging section
#define TIMER_ON true
#define RENDERDOC_ENABLED false
#define DEBUG_GUI_ENABLED true

// We want to immediately abort when there is an error. In normal engines this would give an error message to the user, or perform a dump of state.
using namespace std;
#define VK_CHECK(x)                                                     \
    do                                                                  \
    {                                                                   \
        VkResult err = x;                                               \
        if (err)                                                        \
        {                                                               \
            std::cout <<"Detected Vulkan error: " << err << std::endl;  \
            abort();                                                    \
        }                                                               \
    } while (0)

