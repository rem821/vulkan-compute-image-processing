//
// Created by standa on 8.12.22.
//
#pragma once

#include "../GlobalConfiguration.h"
#include "../util/Dataset.h"

#define SHI_TOMASI_MAX_CORNERS 50;
#define SHI_TOMASI_QUALITY_LEVEL 0.1;

void assertCameraGeometry(Dataset *dataset, const cv::Mat &leftCameraFrameGray, const cv::Mat &rightCameraFrameGray) {

    std::vector<cv::Point2f> leftCorners;
    //cv::goodFeaturesToTrack(leftCameraFrameGray, leftCorners, SHI_TOMASI_MAX_CORNERS, SHI_TOMASI_QUALITY_LEVEL, )
}