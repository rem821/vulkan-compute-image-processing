//
// Created by standa on 8.12.22.
//
#pragma once

#include "../GlobalConfiguration.h"
#include "../util/Dataset.h"
#include <fmt/core.h>
#include "../threading/BS_thread_pool.h"

void detectKeypoints(const cv::Mat &frame, Dataset *dataset, bool isLeft) {
    auto leftROI = cv::Rect(dataset->cameraWidth / 2, 0, dataset->cameraWidth / 2, dataset->cameraHeight);
    auto rightROI = cv::Rect(0, 0, dataset->cameraWidth / 2, dataset->cameraHeight);
    auto imageROI = cv::Mat(frame, isLeft ? leftROI : rightROI);

    auto detector = cv::ORB::create(SHI_TOMASI_MAX_CORNERS);
    detector->detectAndCompute(imageROI, cv::Mat(), isLeft ? dataset->leftKeypoints : dataset->rightKeypoints,
                               isLeft ? dataset->leftDescriptors : dataset->rightDescriptors);

    if (isLeft) {
        for (auto &corner: dataset->leftKeypoints) {
            corner.pt.x += float(dataset->cameraWidth) / 2.0f;
        }
    }
}

void assertCameraGeometry(Dataset *dataset, const cv::Mat &leftCameraFrameGray, const cv::Mat &rightCameraFrameGray,
                          BS::thread_pool &pool) {

    pool.push_task(detectKeypoints, leftCameraFrameGray, dataset, true);
    pool.push_task(detectKeypoints, rightCameraFrameGray, dataset, false);
    pool.wait_for_tasks();
}

