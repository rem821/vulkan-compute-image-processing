//
// Created by standa on 14.11.22.
//
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "../GlobalConfiguration.h"
#include "../../external/fastcsv/csv.h"

struct Dataset {
    // Original variables
    std::vector<long> timestamps;
    std::vector<long> imu_timestamps;
    std::vector<long> imu_gnss_timestamps;
    std::vector<long> gnss_timestamps;

    std::vector<double> acc_x, acc_y, acc_z, ang_vel_x, ang_vel_y, ang_vel_z, quat_x, quat_y, quat_z, quat_w;
    int year, month, day, hours, minutes, seconds;
    std::vector<double> latitude, longitude, altitude, azimuth;

    // Inferred variables
    std::vector<double> heading;
    std::vector<double> headingDif;

    std::vector<double> attitude;
    std::vector<double> attitudeDif;
};

class DatasetFileReader {
public:
    DatasetFileReader() {
        io::CSVReader<1> in_timestamps(std::string(SESSION_PATH) + std::string(TIMESTAMPS_PATH));
        in_timestamps.read_header(io::ignore_extra_column, "epoch_timestamp");

        long _t;
        while (in_timestamps.read_row(_t)) {
            dataset.timestamps.emplace_back(_t);
        }

        io::CSVReader<11> in_imu(std::string(SESSION_PATH) + std::string(IMU_PATH));
        in_imu.read_header(io::ignore_extra_column, "epoch_timestamp", "acc_x", "acc_y", "acc_z", "ang_vel_x",
                           "ang_vel_y", "ang_vel_z", "quat_x", "quat_y", "quat_z", "quat_w");
        float _acc_x, _acc_y, _acc_z, _ang_vel_x, _ang_vel_y, _ang_vel_z, _quat_x, _quat_y, _quat_z, _quat_w;
        while (in_imu.read_row(_t, _acc_x, _acc_y, _acc_z, _ang_vel_x, _ang_vel_y, _ang_vel_z, _quat_x, _quat_y,
                               _quat_z, _quat_w)) {
            dataset.imu_timestamps.emplace_back(_t);

            dataset.acc_x.emplace_back(_acc_x);
            dataset.acc_y.emplace_back(_acc_y);
            dataset.acc_z.emplace_back(_acc_z);

            dataset.ang_vel_x.emplace_back(_ang_vel_x);
            dataset.ang_vel_y.emplace_back(_ang_vel_y);
            dataset.ang_vel_z.emplace_back(_ang_vel_z);

            dataset.quat_x.emplace_back(_quat_x);
            dataset.quat_y.emplace_back(_quat_y);
            dataset.quat_z.emplace_back(_quat_z);
            dataset.quat_w.emplace_back(_quat_w);
        }

        io::CSVReader<8> in_time(std::string(SESSION_PATH) + std::string(TIME_PATH));
        in_time.read_header(io::ignore_extra_column, "epoch_timestamp", "year", "month", "day", "hours", "minutes",
                            "seconds", "nanoseconds");
        float _year, _month, _day, _hours, _minutes, _seconds, _nanoseconds;
        while (in_time.read_row(_t, _year, _month, _day, _hours, _minutes, _seconds, _nanoseconds)) {
            dataset.imu_gnss_timestamps.emplace_back(_t);

            year.emplace_back(_year);
            month.emplace_back(_month);
            day.emplace_back(_day);

            hours.emplace_back(_hours);
            minutes.emplace_back(_minutes);
            seconds.emplace_back(_seconds);
            nanoseconds.emplace_back(_nanoseconds);
        }

        io::CSVReader<5> in_pose(std::string(SESSION_PATH) + std::string(POSE_PATH));
        in_pose.read_header(io::ignore_extra_column, "epoch_timestamp", "latitude", "longitude", "altitude", "azimuth");
        float _latitude, _longitude, _altitude, _azimuth;
        while (in_pose.read_row(_t, _latitude, _longitude, _altitude, _azimuth)) {
            dataset.gnss_timestamps.emplace_back(_t);

            dataset.latitude.emplace_back(_latitude);
            dataset.longitude.emplace_back(_longitude);
            dataset.altitude.emplace_back(_altitude);
            dataset.azimuth.emplace_back(_azimuth);
        }
    }

    void readData(const uint32_t &frameIndex) {
        long i = frameIndex;

        // Find IMU positions that corresponds to the actual camera frame timestamp
        do {
            previousTimeDif = abs(dataset.timestamps[i] - dataset.imu_timestamps[imu_index]);
            imu_index += 1;
        } while (abs(dataset.timestamps[i] - dataset.imu_timestamps[imu_index]) < previousTimeDif);

        // Find GNSS positions that corresponds to the actual camera frame timestamp
        previousTimeDif = INT_MAX;
        do {
            previousTimeDif = abs(dataset.timestamps[i] - dataset.gnss_timestamps[gnss_index]);
            gnss_index += 1;
        } while (abs(dataset.timestamps[i] - dataset.gnss_timestamps[gnss_index]) < previousTimeDif);

        // Calculate heading and attitude
        glm::quat q = glm::quat(dataset.quat_w[imu_index], dataset.quat_x[imu_index], dataset.quat_y[imu_index], dataset.quat_z[imu_index]);
        glm::vec3 euler = glm::eulerAngles(q);
        dataset.heading.emplace_back(((euler[2] + M_PI) * 360) / (2 * M_PI));
        dataset.attitude.emplace_back(euler[1]);

        // Save inferred variables
        if (i > 0) {
            dataset.headingDif.emplace_back(dataset.heading[i - 1] - dataset.heading[i]);
            if (abs(dataset.headingDif[i]) > MAX_HEADING_DIF) {
                dataset.headingDif[i] = dataset.headingDif[i - 1];
            }
            dataset.attitudeDif.emplace_back(dataset.attitude[i - 1] - dataset.attitude[i]);
            if (abs(dataset.attitudeDif[i]) > MAX_ATTITUDE_DIF) {
                dataset.attitudeDif[i] = 0;
            }

            dataset.year = int(year[gnss_index]);
            dataset.month = int(month[gnss_index]);
            dataset.day = int(day[gnss_index]);

            dataset.hours = int(hours[gnss_index]);
            dataset.minutes = int(minutes[gnss_index]);
            dataset.seconds = int(seconds[gnss_index]);
        }
    }

    Dataset* getDataset() { return &dataset; }

private:
    Dataset dataset{};

    std::vector<double> year, month, day, hours, minutes, seconds, nanoseconds;

    long imu_index = 0;
    long gnss_index = 0;
    long previousTimeDif = INT_MAX;
};


