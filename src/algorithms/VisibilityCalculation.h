//
// Created by standa on 14.11.22.
//
#pragma once

#include "../profiling/Timer.h"
#include "opencv4/opencv2/opencv.hpp"
#include "../util/polyfit.h"

void calculateVisibility(const int frameIndex, const cv::Mat &cameraFrame, const std::vector<float> &headingDif,
                         const std::vector<float> &attitudeDif, std::pair<int, int> &vanishingPoint,
                         std::vector<float> &visibilityCoeffs, std::vector<float> &visibility) {
    Timer timer("Calculating DFT window");

    // Estimate position of the road vanishing point
    int32_t r_vp_x = cameraFrame.cols / 2 + int32_t(HORIZONTAL_SENSITIVITY * headingDif[headingDif.size() - 1]);
    int32_t r_vp_y = cameraFrame.rows / 2 + int32_t(VERTICAL_SENSITIVITY * attitudeDif[attitudeDif.size() - 1]);
    vanishingPoint = std::pair(r_vp_x, r_vp_y);

    int32_t window_top_left_x = r_vp_x - (DFT_WINDOW_SIZE / 2);
    int32_t window_top_left_y = r_vp_y - (DFT_WINDOW_SIZE / 2);
    cv::Rect window_rect(window_top_left_x, window_top_left_y, DFT_WINDOW_SIZE, DFT_WINDOW_SIZE);

    cv::Mat cameraFrameGray;
    cv::cvtColor(cameraFrame, cameraFrameGray, cv::COLOR_BGR2GRAY);

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
    cv::warpPolar(magINorm, magINormPolar, cv::Size(magI.cols, magI.rows), cv::Point2f(magI.cols / 2, magI.rows / 2),
                  magI.rows / 2, cv::WARP_POLAR_LINEAR);
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

    cout << frameIndex << endl;
    /*
    cout << "PSS = [ ";
    for (int i = 0; i < intMagINormPolar.cols; i++) {
        cout << pss[i];
        cout << " ";
    }
    cout << " ]" << endl;

    cout << "Freq = [ ";
    for (int i = 0; i < intMagINormPolar.cols; i++) {
        cout << freq[i];
        cout << " ";
    }
    cout << " ]" << endl;
    */
    std::vector<double> coeffs(3);
    polyfit(freq, pss, coeffs, 2);
    visibilityCoeffs.push_back(coeffs[0]);
    if (visibility.empty()) visibility.push_back(1);
    else
        visibility.push_back(
                0.9 * visibility[visibility.size() - 1] + 0.1 * (4000.0 - min(4000.0, max(3400.0, coeffs[0]))) / 600.0);

    cout << "VisibilityCoeffs = [ ";
    for (float visibilityCoeff : visibilityCoeffs) {
        cout << visibilityCoeff;
        cout << " ";
    }
    cout << " ]" << endl;

    cout << "Visibility = [ ";
    for (float i : visibility) {
        cout << i;
        cout << " ";
    }
    cout << " ]" << endl;
}