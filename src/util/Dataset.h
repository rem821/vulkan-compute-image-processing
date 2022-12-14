//
// Created by standa on 5.12.22.
//
#pragma once

#include "../GlobalConfiguration.h"
#include "opencv4/opencv2/opencv.hpp"
#include "../util/circularbuffer.h"

struct Dataset {
    // Original variables
    int year, month, day, hours, minutes, seconds;
    double latitude, longitude, altitude, azimuth;

    int cameraWidth = 1920, cameraHeight = 1200;

    // Camera
    uint32_t frameIndex = 0;
    uint32_t thermalFrameIndex = 0;
    cv::Mat leftCameraFrame{};
    cv::Mat rightCameraFrame{};
    cv::Mat thermalCameraFrame{};

    // Inferred variables
    std::vector<double> heading;
    std::vector<double> headingDif;

    std::vector<double> attitude;
    std::vector<double> attitudeDif;

    std::pair<int, int> vanishingPoint{};

    double sunrise, sunset;
    bool isDaylight;

    // Visibility calculation
    std::vector<double> vp_visibility;
    cv::Mat visibility = cv::Mat(DFT_BLOCK_COUNT, DFT_BLOCK_COUNT, CV_32FC1, cv::Scalar(0.0f));
    double visibilityScore = 0.0;

    // Glare detection
    cv::Mat histograms = cv::Mat(HISTOGRAM_COUNT * HISTOGRAM_COUNT, HISTOGRAM_BINS, CV_32FC1, cv::Scalar(0.0f));
    cv::Mat glareAmounts = cv::Mat(HISTOGRAM_COUNT, HISTOGRAM_COUNT, CV_32FC1, cv::Scalar(0.0f));
    std::vector<CircularBuffer<bool>> occlusionBuffers;

    // Shi-Tomasi
    std::vector<cv::KeyPoint> leftKeypoints;
    cv::Mat leftDescriptors;

    std::vector<cv::KeyPoint> rightKeypoints;
    cv::Mat rightDescriptors;


    // Timers
    float cameraFrameExtraction, glareAndOcclusionDetection, vanishingPointEstimation, vanishingPointVisibilityCalculation, fogDetection, allCPUAlgorithms, textureGeneration, frameSubmission, rendering;

    // Configuration
    bool showVanishingPoint = true, showKeypoints = true;
};