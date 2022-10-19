//
// Created by Stanislav Svědiroh on 12.07.2022.
//
#pragma once

// Device type switch
//  0 --> Home PC (NVIDIA RTX 3080)
//  1 --> Work PC (AMD Radeon card)
//  2 --> Macbook Pro  16" 2019 (AMD Radeon card)

#define DEVICE_TYPE 1

#if DEVICE_TYPE == 0

// Graphics setting section
#define INTEGRATED_GRAPHICS false
#define VALIDATION_LAYER_NAME "VK_LAYER_LUNARG_standard_validation"

// Content section
#define VIDEO_DOWNSCALE_FACTOR 1
#define VIDEO_PATH "/home/standa/3_1_1_1/camera_left_front/video.mp4"
#define IMAGE_PATH "../assets/haze.jpg"

// Shaders section
#define WORKGROUP_COUNT 64

#elif DEVICE_TYPE == 1

// Graphics setting section
#define INTEGRATED_GRAPHICS false
#define VALIDATION_LAYER_NAME "VK_LAYER_LUNARG_standard_validation"

// Content section
#define VIDEO_DOWNSCALE_FACTOR 3
#define VIDEO_PATH "/mnt/B0E0DAB9E0DA84CE/BUD/3_1_1_2/camera_left_front/video.mp4"
//#define VIDEO_PATH "../assets/foggy_ride.mp4"
#define IMAGE_PATH "../assets/image.jpg"

// Shaders section
#define WORKGROUP_COUNT 32

#elif DEVICE_TYPE == 2

// Graphics setting section
#define INTEGRATED_GRAPHICS false
#define VALIDATION_LAYER_NAME "VK_LAYER_KHRONOS_validation"

// Content section
#define VIDEO_DOWNSCALE_FACTOR 4
#define VIDEO_PATH "../assets/video.mp4"
#define IMAGE_PATH "../assets/image.jpg"

// Shaders section
#define WORKGROUP_COUNT 16

#endif

// Common section
#define WINDOW_TITLE "Vulkan Compute Render Window"
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

#define PLAY_VIDEO true

#define SWEEP_FRAMES 40

#define DARK_CHANNEL_PRIOR_SHADER "ImageDarkChannelPrior"
#define TRANSMISSION_SHADER "ImageTransmission"
#define MAXIMUM_AIRLIGHT_SHADER "MaximumAirLight"
#define GUIDED_FILTER_SHADER "GuidedFilter"
#define RADIANCE_SHADER "ImageRadiance"

// Debugging section
#define TIMER_ON false
#define RENDERDOC_ENABLED true

