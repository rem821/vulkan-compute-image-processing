//
// Created by standa on 14.11.22.
//
#pragma once

#include "../profiling/Timer.h"
#include "opencv4/opencv2/opencv.hpp"

void detectGlare(const cv::Mat &cameraFrame, std::pair<int, int> &vanishingPoint) {
    // Step 1: Convert the frame to a color space that maximizes resolution in luminance
    // Step 2: Divide the camera frame into meaningful regions to detect glare in
    // Step 3: Calculate histogram of every camera frame region
    // Step 4: Detect glare in each histogram
    // Step 5: Calculate a global score for the image based on the detected glares in various regions


}
