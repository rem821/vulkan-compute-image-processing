//
// Created by standa on 14.11.22.
//
#pragma once

#include "../profiling/Timer.h"
#include "opencv4/opencv2/opencv.hpp"
#include <fmt/core.h>

void detectGlareAndOcclusion(const cv::Mat &cameraFrameGray, Dataset *dataset) {
    Timer timer("Glare and occlusion detection", &dataset->glareAndOcclusionDetection);

    // Step 1: Convert the frame to a color space that maximizes resolution in luminance
    // Step 2: Divide the camera frame into meaningful regions to detect glare in
    int roiWidth = cameraFrameGray.cols / HISTOGRAM_COUNT;
    int roiHeight = cameraFrameGray.rows / HISTOGRAM_COUNT;
    int glareBinThreshold = int(HISTOGRAM_BINS * GLARE_THRESHOLD);
    int occlusionBinThreshold = int(HISTOGRAM_BINS * OCCLUSION_THRESHOLD);

    for (int i = 0, j = 0; i < HISTOGRAM_COUNT && j < HISTOGRAM_COUNT;) {
        int histIndex = i + (j * HISTOGRAM_COUNT);
        cv::Rect roi = cv::Rect(i * roiWidth, j * roiHeight, roiWidth, roiHeight);
        cv::Mat imageRoi = cv::Mat(cameraFrameGray, roi);

        // Step 3: Calculate histogram of every camera frame region
        cv::Mat histRoi;
        int histSize = HISTOGRAM_BINS;
        float range[] = {0, 256};
        const float *histRange[] = {range};
        cv::calcHist(&imageRoi, 1, nullptr, cv::Mat(), histRoi, 1, &histSize, histRange, true, false);
        histRoi = histRoi.t();

        for (int k = 0; k < histSize; k++) {
            dataset->histograms.at<float>(histIndex, k) = histRoi.at<float>(k);
        }

        // Step 4: Detect glare and occlusions in each histogram
        float occlusionLuminance = 0, regularLuminance = 0, glareLuminance = 0;
        for (int k = 0; k < HISTOGRAM_BINS; k++) {
            if (k >= glareBinThreshold) {
                glareLuminance += dataset->histograms.at<float>(histIndex, k);
            } else if (k <= occlusionBinThreshold) {
                occlusionLuminance += dataset->histograms.at<float>(histIndex, k);
            } else {
                regularLuminance += dataset->histograms.at<float>(histIndex, k);
            }
        }

        if (glareLuminance >= (regularLuminance + occlusionLuminance)) {
            dataset->occlusionBuffers.at(histIndex).push_back(true);
            dataset->glareAmounts.at<float>(i, j) =
                    (glareLuminance * 255.0f) / (regularLuminance + glareLuminance + occlusionLuminance);
        } else if (occlusionLuminance >= (regularLuminance + glareLuminance)) {
            dataset->occlusionBuffers.at(histIndex).push_back(false);
            if (dataset->occlusionBuffers.at(histIndex).dataSum() == 0) {
                dataset->glareAmounts.at<float>(i, j) = 0;
            } else {
                dataset->glareAmounts.at<float>(i, j) = 64;
            }
        } else {
            dataset->occlusionBuffers.at(histIndex).push_back(true);
            dataset->glareAmounts.at<float>(i, j) = 64;
        }

        // For daylight images ignore skylight
        if (j < HISTOGRAM_COUNT / 2 && dataset->isDaylight) {
            if (dataset->glareAmounts.at<float>(i, j) > 64) {
                dataset->glareAmounts.at<float>(i, j) = 64;
            }
        }

        i++;
        if (i == HISTOGRAM_COUNT) {
            i = 0;
            j++;
        }
    }

    // Step 5: Calculate a global score for the image based on the detected glares in various regions
}

