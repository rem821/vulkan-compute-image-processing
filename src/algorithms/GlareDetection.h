//
// Created by standa on 14.11.22.
//
#pragma once

#include "../profiling/Timer.h"
#include "opencv4/opencv2/opencv.hpp"
#include <fmt/core.h>

void detectGlare(const cv::Mat &cameraFrameGray, std::pair<int, int> &vanishingPoint, cv::Mat &histograms,
                 cv::Mat &glareAmounts) {
    Timer timer("Detecting glare");

    // Step 1: Convert the frame to a color space that maximizes resolution in luminance
    // Step 2: Divide the camera frame into meaningful regions to detect glare in
    int roiWidth = cameraFrameGray.cols / HISTOGRAM_COUNT;
    int roiHeight = cameraFrameGray.rows / HISTOGRAM_COUNT;
    int glareBinThreshold = int(HISTOGRAM_BINS * GLARE_THRESHOLD);

    for (int i = 0, j = 0; i < HISTOGRAM_COUNT && j < HISTOGRAM_COUNT;) {
        cv::Rect roi = cv::Rect(i * roiWidth, j * roiHeight, roiWidth, roiHeight);
        cv::Mat imageRoi = cv::Mat(cameraFrameGray, roi);

        // Step 3: Calculate histogram of every camera frame region
        cv::Mat histRoi;
        int histSize = HISTOGRAM_BINS;
        float range[] = {0, 256};
        const float *histRange[] = {range};
        cv::calcHist(&imageRoi, 1, 0, cv::Mat(), histRoi, 1, &histSize, histRange, true, false);
        histRoi = histRoi.t();

        for (int k = 0; k < histSize; k++) {
            histograms.at<float>(i + (j * HISTOGRAM_COUNT), k) = histRoi.at<float>(k);
        }

        // Step 4: Detect glare in each histogram
        float nonGlareLuminance = 0, glareLuminance = 0;
        for (int k = 0; k < HISTOGRAM_BINS; k++) {
            if (k >= glareBinThreshold) {
                glareLuminance += histograms.at<float>(i + (j * HISTOGRAM_COUNT), k);
            } else {
                nonGlareLuminance += histograms.at<float>(i + (j * HISTOGRAM_COUNT), k);
            }
        }

        if (glareLuminance >= nonGlareLuminance) {
            glareAmounts.at<float>(i, j) = (glareLuminance * 255.f) / (nonGlareLuminance + glareLuminance);
        } else {
            glareAmounts.at<float>(i, j) = 0;
        }

        i++;
        if (i == HISTOGRAM_COUNT) {
            i = 0;
            j++;
        }
    }
    // Step 5: Calculate a global score for the image based on the detected glares in various regions
}
