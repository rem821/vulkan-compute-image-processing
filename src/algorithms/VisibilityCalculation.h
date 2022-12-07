//
// Created by standa on 14.11.22.
//
#pragma once

#include "../profiling/Timer.h"
#include "opencv4/opencv2/opencv.hpp"
#include "../util/polyfit.h"
#include "DatasetFileReader.h"
#include <fmt/core.h>

class VisibilityCalculation {
public:
    static void
    calculateVisibility(const cv::Mat &cameraFrameGray, Dataset *dataset, std::pair<int, int> centerPoint,
                        std::pair<int, int> position) {
        _calculateVisibility(cameraFrameGray, dataset, false, centerPoint, position);
    }

    static void
    calculateVisibilityVp(const cv::Mat &cameraFrameGray, Dataset *dataset, std::pair<int, int> centerPoint) {
        Timer timer("Calculating vanishing point visibility");
        _calculateVisibility(cameraFrameGray, dataset, true, centerPoint, std::pair(-1, -1));
    }

    static void calculateVisibilityScore(Dataset *dataset) {
        float sum = 0.0f;
        for (int j = 0; j < DFT_BLOCK_COUNT; j++) {
            for (int i = 0; i < DFT_BLOCK_COUNT; i++) {
                sum += dataset->visibility.at<float>(i, j);
            }
        }
        float score = sum / (DFT_BLOCK_COUNT * DFT_BLOCK_COUNT);
        dataset->visibilityScore = score < 0.2 ? 1 : 0;
    }

private:
    static void _calculateVisibility(const cv::Mat &cameraFrameGray, Dataset *dataset, bool isVanishingPoint,
                                     std::pair<int, int> centerPoint,
                                     std::pair<int, int> position) {
        int32_t window_top_left_x = int(centerPoint.first) - (DFT_WINDOW_SIZE / 2);
        int32_t window_top_left_y = int(centerPoint.second) - (DFT_WINDOW_SIZE / 2);
        cv::Rect window_rect(window_top_left_x, window_top_left_y, DFT_WINDOW_SIZE, DFT_WINDOW_SIZE);

        cv::Mat cameraFrameWindow = cameraFrameGray(window_rect);

        int32_t optimalWindowSize = cv::getOptimalDFTSize(DFT_WINDOW_SIZE);
        cv::Mat paddedWindow;
        cv::copyMakeBorder(cameraFrameWindow, paddedWindow, 0, optimalWindowSize - cameraFrameWindow.rows, 0,
                           optimalWindowSize - cameraFrameWindow.cols,
                           cv::BORDER_CONSTANT, cv::Scalar::all(0));

        cv::Mat planes[] = {cv::Mat_<float>(paddedWindow), cv::Mat::zeros(paddedWindow.size(), CV_32F)};
        cv::Mat complexI;
        cv::merge(planes, 2, complexI);

        cv::dft(complexI, complexI);

        cv::split(complexI, planes);
        cv::magnitude(planes[0], planes[1], planes[0]);
        cv::Mat magI = planes[0];
        cv::Mat magIPow = magI.mul(magI);
        cv::Mat magINorm = magIPow.mul(DFT_WINDOW_SIZE ^ 2);

        magINorm += cv::Scalar::all(1);
        cv::log(magINorm, magINorm);

        // Crop the spectrum, if it has an odd number of rows or columns
        magINorm = magINorm(cv::Rect(0, 0, magI.cols & -2, magI.rows & -2));

        // Rearrange the quadrants of Fourier image so that the origin is at the image center
        int cx = magINorm.cols / 2;
        int cy = magINorm.rows / 2;
        cv::Mat q0(magINorm, cv::Rect(0, 0, cx, cy));
        cv::Mat q1(magINorm, cv::Rect(cx, 0, cx, cy));
        cv::Mat q2(magINorm, cv::Rect(0, cy, cx, cy));
        cv::Mat q3(magINorm, cv::Rect(cx, cy, cx, cy));
        cv::Mat tmp;
        q0.copyTo(tmp);
        q3.copyTo(q0);
        tmp.copyTo(q3);
        q1.copyTo(tmp);
        q2.copyTo(q1);
        tmp.copyTo(q2);

        cv::Mat magINormPolar;
        cv::warpPolar(magINorm, magINormPolar, cv::Size(magI.cols, magI.rows),
                      cv::Point2f(float(magI.cols) / 2, float(magI.rows) / 2), double(magI.rows) / 2.0,
                      cv::WARP_POLAR_LINEAR);
        cv::Mat intMagINormPolar = cv::Mat_<uint8_t>(magINormPolar);

        cv::normalize(magINorm, magINorm, 0, 255, cv::NORM_MINMAX);
        cv::Mat intMagINorm = cv::Mat_<uint8_t>(magINorm);

        std::vector<double> pss(DFT_WINDOW_SIZE);
        std::vector<double> freq(DFT_WINDOW_SIZE);
        for (int i = 0; i < intMagINormPolar.rows; i++) {
            for (int j = 0; j < intMagINormPolar.cols; j++) {
                pss[j] += intMagINormPolar.data[j + i * intMagINormPolar.cols];
                if (i == 0) freq[j] = (j + i * intMagINormPolar.cols) / 2.0;
            }
        }

        std::vector<double> coeffs(3);
        polyfit(freq, pss, coeffs, 2);

        if (isVanishingPoint) {
            if (dataset->vp_visibility.empty()) dataset->vp_visibility.push_back(1);
            else
                dataset->vp_visibility.push_back((1.0 - MOVING_AVERAGE_FORGET_RATE) * dataset->vp_visibility.back() +
                                                 MOVING_AVERAGE_FORGET_RATE * (1 - (MAX_VISIBILITY_THRESHOLD -
                                                                                    min(MAX_VISIBILITY_THRESHOLD,
                                                                                        max(MIN_VISIBILITY_THRESHOLD,
                                                                                            coeffs[0]))) /
                                                                                   (MAX_VISIBILITY_THRESHOLD -
                                                                                    MIN_VISIBILITY_THRESHOLD)));
        } else {
            double vis = (1.0 - (MAX_VISIBILITY_THRESHOLD -
                                 min(MAX_VISIBILITY_THRESHOLD, max(MIN_VISIBILITY_THRESHOLD, coeffs[0]))) /
                                (MAX_VISIBILITY_THRESHOLD - MIN_VISIBILITY_THRESHOLD));
            dataset->visibility.at<float>(position.first, position.second) = float(vis);

        }
    }
};
