//
// Created by standa on 14.11.22.
//
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "../GlobalConfiguration.h"
#include "../../external/fastcsv/csv.h"
#include "../../external/sunset/sunset.h"
#include <fmt/core.h>
#include "opencv4/opencv2/opencv.hpp"
#include "../util/circularbuffer.h"
#include "../util/Dataset.h"
#include "../profiling/Timer.h"

#include "../threading/BS_thread_pool.h"

class DatasetFileReader {
public:
    DatasetFileReader(Dataset *_dataset, BS::thread_pool &pool) : dataset(_dataset) {
        io::CSVReader<3> in_camera_timestamps(std::string(SESSION_PATH) + std::string(CAMERA_TIMESTAMPS_PATH));
        long _timestamp, _frame_index, _camera_timestamp;
        while (in_camera_timestamps.read_row(_timestamp, _frame_index, _camera_timestamp)) {
            timestamps.emplace_back(_timestamp);
        }

        io::CSVReader<4> in_thermal_timestamps(std::string(SESSION_PATH) + std::string(THERMAL_TIMESTAMPS_PATH));
        double _min_temp, _max_temp;
        while (in_thermal_timestamps.read_row(_timestamp, _frame_index, _min_temp, _max_temp)) {
            thermal_timestamps.emplace_back(_timestamp);
            min_temp.emplace_back(_min_temp);
            max_temp.emplace_back(_max_temp);
        }

        io::CSVReader<11> in_imu(std::string(SESSION_PATH) + std::string(IMU_PATH));
        float _acc_x, _acc_y, _acc_z, _ang_vel_x, _ang_vel_y, _ang_vel_z, _quat_x, _quat_y, _quat_z, _quat_w;
        while (in_imu.read_row(_timestamp, _acc_x, _acc_y, _acc_z, _ang_vel_x, _ang_vel_y, _ang_vel_z, _quat_x, _quat_y,
                               _quat_z, _quat_w)) {
            imu_timestamps.emplace_back(_timestamp);

            acc_x.emplace_back(_acc_x);
            acc_y.emplace_back(_acc_y);
            acc_z.emplace_back(_acc_z);

            ang_vel_x.emplace_back(_ang_vel_x);
            ang_vel_y.emplace_back(_ang_vel_y);
            ang_vel_z.emplace_back(_ang_vel_z);

            quat_x.emplace_back(_quat_x);
            quat_y.emplace_back(_quat_y);
            quat_z.emplace_back(_quat_z);
            quat_w.emplace_back(_quat_w);
        }

        io::CSVReader<8> in_time(std::string(SESSION_PATH) + std::string(TIME_PATH));
        float _year, _month, _day, _hours, _minutes, _seconds, _nanoseconds;
        while (in_time.read_row(_timestamp, _year, _month, _day, _hours, _minutes, _seconds, _nanoseconds)) {
            imu_gnss_timestamps.emplace_back(_timestamp);

            year.emplace_back(_year);
            month.emplace_back(_month);
            day.emplace_back(_day);

            hours.emplace_back(_hours);
            minutes.emplace_back(_minutes);
            seconds.emplace_back(_seconds);
            nanoseconds.emplace_back(_nanoseconds);
        }

        io::CSVReader<5> in_pose(std::string(SESSION_PATH) + std::string(POSE_PATH));
        float _latitude, _longitude, _altitude, _azimuth;
        while (in_pose.read_row(_timestamp, _latitude, _longitude, _altitude, _azimuth)) {
            gnss_timestamps.emplace_back(_timestamp);

            latitude.emplace_back(_latitude);
            longitude.emplace_back(_longitude);
            altitude.emplace_back(_altitude);
            azimuth.emplace_back(_azimuth);
        }

        for (int i = 0; i < HISTOGRAM_COUNT * HISTOGRAM_COUNT; i++) {
            dataset->occlusionBuffers.emplace_back(OCCLUSION_MIN_FRAMES);
        }

        readData(pool);
    }

    bool readData(BS::thread_pool &pool) {
        long i = dataset->frameIndex;
        bool isOk;

        // Find IMU positions that corresponds to the actual camera frame timestamp
        do {
            imu_index += 1;
            previousTimeDif = abs(timestamps[i] - imu_timestamps[imu_index]);
        } while (abs(timestamps[i] - imu_timestamps[imu_index + 1]) < previousTimeDif);

        // Find GNSS positions that corresponds to the actual camera frame timestamp
        previousTimeDif = INT_MAX;
        do {
            gnss_index += 1;
            previousTimeDif = abs(timestamps[i] - gnss_timestamps[gnss_index]);
        } while (abs(timestamps[i] - gnss_timestamps[gnss_index + 1]) < previousTimeDif);

        dataset->year = int(year[gnss_index]);
        dataset->month = int(month[gnss_index]);
        dataset->day = int(day[gnss_index]);

        dataset->hours = int(hours[gnss_index] + TIMEZONE_OFFSET);
        dataset->minutes = int(minutes[gnss_index]);
        dataset->seconds = int(seconds[gnss_index]);

        // Daylight savings time
        double dst = 0.0;
        if (dataset->month > 3 && dataset->month < 10) dst = 1.0;
        if (dataset->month == 3 && dataset->day >= 26 && dataset->hours >= 2) dst = 1.0;
        if (dataset->month == 10 && dataset->day <= 29 && dataset->hours <= 3) dst = 1.0;
        dataset->hours += int(dst);

        dataset->latitude = latitude[gnss_index];
        dataset->longitude = longitude[gnss_index];
        dataset->altitude = altitude[gnss_index];
        dataset->azimuth = azimuth[gnss_index];

        // Calculate heading and attitude
        glm::quat q = glm::quat(float(quat_w[imu_index]), float(quat_x[imu_index]), float(quat_y[imu_index]),
                                float(quat_z[imu_index]));
        glm::vec3 euler = glm::eulerAngles(q);
        dataset->heading.emplace_back(((euler[2] + M_PI) * 180) / M_PI);
        dataset->attitude.emplace_back(euler[1]);

        // Calculate sun position and sunrise/sunset
        sunCalc.setCurrentDate(dataset->year, dataset->month, dataset->day);
        sunCalc.setPosition(dataset->latitude, dataset->longitude, TIMEZONE_OFFSET + dst);
        dataset->sunrise = sunCalc.calcSunrise() / 60.0;
        dataset->sunset = sunCalc.calcSunset() / 60.0;
        double h = dataset->hours + (dataset->minutes / 60.0);
        dataset->isDaylight = h < dataset->sunset && h > dataset->sunrise;

        isOk = readCameraFrame(pool);

        // Save inferred variables
        if (i <= 0) return true;

        dataset->headingDif.emplace_back(dataset->heading[i - 1] - dataset->heading[i]);
        if (abs(dataset->headingDif.back()) > MAX_HEADING_DIF) {
            dataset->headingDif.back() = 0;
        }
        dataset->attitudeDif.emplace_back(dataset->attitude[i - 1] - dataset->attitude[i]);
        if (abs(dataset->attitudeDif[i]) > MAX_ATTITUDE_DIF) {
            dataset->attitudeDif.back() = 0;
        }

        return isOk;
    }

    bool readCameraFrame(BS::thread_pool &pool) {
        std::uint32_t &i = dataset->frameIndex;
        std::uint32_t &t_i = dataset->thermalFrameIndex;

        if (i < totalFrames) {
            Timer timer("Camera frame extraction", &dataset->cameraFrameExtraction);

            // Doesn't update the variable for some reason
            //pool.push_task(&cv::VideoCapture::set, &thermalVideo, cv::CAP_PROP_POS_FRAMES, i * 3);
            //pool.push_task(&cv::VideoCapture::read, &leftVideo, dataset->leftCameraFrame);
            //pool.push_task(&cv::VideoCapture::read, &rightVideo, dataset->rightCameraFrame);
            //while (pool.get_tasks_running() > 0);

            // This is actually quicker than advancing to specific frame via CAP_PROP_POS_FRAMES
            // It is done because the thermal camera has 3x the framerate of RGB camera
            for (int k = 0; k < 3; k++) {
                thermalVideo.read(dataset->thermalCameraFrame);
            }

            return leftVideo.read(dataset->leftCameraFrame) &&
                   rightVideo.read(dataset->rightCameraFrame) &&
                   thermalVideo.read(dataset->thermalCameraFrame);
        }
        return false;
    }

private:
    Dataset *dataset;

    std::vector<long> timestamps;
    std::vector<long> thermal_timestamps;
    std::vector<long> imu_timestamps;
    std::vector<long> imu_gnss_timestamps;
    std::vector<long> gnss_timestamps;

    std::vector<double> acc_x, acc_y, acc_z, ang_vel_x, ang_vel_y, ang_vel_z, quat_x, quat_y, quat_z, quat_w;
    std::vector<double> year, month, day, hours, minutes, seconds, nanoseconds;
    std::vector<double> latitude, longitude, altitude, azimuth;
    std::vector<double> min_temp, max_temp;

    SunSet sunCalc = SunSet();

    long imu_index = 0;
    long gnss_index = 0;
    long previousTimeDif = INT_MAX;

    // Camera
    cv::VideoCapture leftVideo{SESSION_PATH + std::string(LEFT_VIDEO_PATH)};
    cv::VideoCapture rightVideo{SESSION_PATH + std::string(RIGHT_VIDEO_PATH)};
    cv::VideoCapture thermalVideo{SESSION_PATH + std::string(THERMAL_VIDEO_PATH)};

    double totalFrames = leftVideo.get(cv::CAP_PROP_FRAME_COUNT);
};


