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

    auto detector = cv::ORB::create(MAX_KEYPOINTS);
    detector->detectAndCompute(imageROI, cv::Mat(), isLeft ? dataset->leftKeypoints : dataset->rightKeypoints,
                               isLeft ? dataset->leftDescriptors : dataset->rightDescriptors);

    if (isLeft) {
        // Offset for visualization
        for (auto &corner: dataset->leftKeypoints) {
            corner.pt.x += float(dataset->cameraWidth) / 2.0f;
        }
    }
}

// ORB Feature detector and descriptor, matched using Bruteforce matcher.
// There needs to be certain number of matched points and their y offset needs to be sufficiently low to qualify for proper geometry
void assertCameraGeometry(Dataset *dataset, const cv::Mat &leftCameraFrameGray, const cv::Mat &rightCameraFrameGray,
                          BS::thread_pool &pool) {

    // Detect features
    pool.push_task(detectKeypoints, leftCameraFrameGray, dataset, true);
    pool.push_task(detectKeypoints, rightCameraFrameGray, dataset, false);
    pool.wait_for_tasks();

    // Match features
    cv::BFMatcher bf = cv::BFMatcher(cv::NORM_HAMMING, true);
    std::vector<cv::DMatch> matches;
    bf.match(dataset->leftDescriptors, dataset->rightDescriptors, matches);

    // Filter out only matched keypoints
    std::vector<cv::KeyPoint> _leftKeyp, _rightKeyp;
    cv::Mat _leftDesc, _rightDesc;
    cv::Mat_<float> x_dists, y_dists;
    for (auto match: matches) {
        _leftKeyp.push_back(dataset->leftKeypoints.at(match.queryIdx));
        _rightKeyp.push_back(dataset->rightKeypoints.at(match.trainIdx));

        _leftDesc.push_back(dataset->leftDescriptors.row(match.queryIdx));
        _rightDesc.push_back(dataset->rightDescriptors.row(match.trainIdx));

        x_dists.push_back(dataset->leftKeypoints.at(match.queryIdx).pt.x - dataset->rightKeypoints.at(match.trainIdx).pt.x);
        y_dists.push_back(dataset->leftKeypoints.at(match.queryIdx).pt.y - dataset->rightKeypoints.at(match.trainIdx).pt.y);
    }

    dataset->leftKeypoints = _leftKeyp;
    dataset->rightKeypoints = _rightKeyp;
    dataset->leftDescriptors = _leftDesc;
    dataset->rightDescriptors = _rightDesc;

    if (matches.size() > MAX_KEYPOINTS / 10) {
        cv::Mat histRoi;
        int histSize = KEYPOINT_HIST_BINS;
        float range[] = {0, 256};
        const float *histRange[] = {range};
        cv::calcHist(&y_dists, 1, nullptr, cv::Mat(), histRoi, 1, &histSize, histRange, true, false);
        histRoi = histRoi.t();

        float sum_l = 0, sum_h = 0;
        for (int k = 0; k < histSize; k++) {
            if (k <= histSize / 2) {
                sum_l += histRoi.at<float>(k);
            } else {
                sum_h += histRoi.at<float>(k);
            }
        }
        dataset->geometryOk = sum_l > sum_h;
    } else {
        dataset->geometryOk = false;
    }
}

