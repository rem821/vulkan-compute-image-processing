//
// Created by standa on 14.11.22.
//
#pragma once

#include "../profiling/Timer.h"
#include "opencv4/opencv2/opencv.hpp"
#include <fmt/core.h>

void detectGlare(const cv::Mat &cameraFrameGray, std::pair<int, int> &vanishingPoint, cv::Mat &histograms) {
    Timer timer("Detecting glare");

    // Step 1: Convert the frame to a color space that maximizes resolution in luminance
    // Step 2: Divide the camera frame into meaningful regions to detect glare in
    int nRoi = int(sqrt(HISTOGRAM_COUNT));
    int roiWidth = cameraFrameGray.cols / nRoi;
    int roiHeight = cameraFrameGray.rows / nRoi;

    for (int i = 0, j = 0; i < nRoi && j < nRoi;) {
        cv::Rect roi = cv::Rect(i * roiWidth, j * roiHeight, roiWidth, roiHeight);
        cv::Mat imageRoi = cv::Mat(cameraFrameGray, roi);

        // Step 3: Calculate histogram of every camera frame region
        cv::Mat histRoi;
        int histSize = HISTOGRAM_BINS;
        float range[] = {0, 256};
        const float *histRange[] = {range};
        cv::calcHist(&imageRoi, 1, nullptr, cv::Mat(), histRoi, 1, &histSize, histRange, true, false);
        histRoi = histRoi.t();
        cv::Mat histRow = histograms.row(i + (j * nRoi));
        histRoi.copyTo(histRow);

        i++;
        if (i == nRoi) {
            i = 0;
            j++;
        }
    }
    // Step 4: Detect glare in each histogram
    // Step 5: Calculate a global score for the image based on the detected glares in various regions
}
