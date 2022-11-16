//
// Created by standa on 14.11.22.
//
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "../GlobalConfiguration.h"
#include "../../external/fastcsv/csv.h"

class ImuExtract {
public:
    ImuExtract() {
        io::CSVReader<1> in_timestamps(std::string(SESSION_PATH) + std::string(TIMESTAMPS_PATH));
        in_timestamps.read_header(io::ignore_extra_column, "timestamp");

        long _t;
        while (in_timestamps.read_row(_t)) {
            timestamps.emplace_back(_t);
        }

        io::CSVReader<11> in_imu(std::string(SESSION_PATH) + std::string(IMU_PATH));
        in_imu.read_header(io::ignore_extra_column, "timestamp", "acc_x", "acc_y", "acc_z", "ang_vel_x", "ang_vel_y", "ang_vel_z", "quat_x", "quat_y", "quat_z", "quat_w");
        float _acc_x, _acc_y, _acc_z, _ang_vel_x, _ang_vel_y, _ang_vel_z, _quat_x, _quat_y, _quat_z, _quat_w;
        while (in_imu.read_row(_t, _acc_x, _acc_y, _acc_z, _ang_vel_x, _ang_vel_y, _ang_vel_z, _quat_x, _quat_y, _quat_z, _quat_w)) {
            imu_timestamps.emplace_back(_t);

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
    }

    void extractData(const uint32_t &frameIndex) {
        long i = frameIndex;
        // Find IMU positions that corresponds to the actual camera frame timestamp
        do {
            previousTimeDif = abs(timestamps[frameIndex] - imu_timestamps[imu_index]);
            imu_index += 1;
        } while (abs(timestamps[frameIndex] - imu_timestamps[imu_index]) < previousTimeDif);

        glm::quat q = glm::quat(quat_w[imu_index], quat_x[imu_index], quat_y[imu_index], quat_z[imu_index]);
        glm::vec3 euler = glm::eulerAngles(q);
        heading.emplace_back(((euler[2] + M_PI) * 360) / (2 * M_PI));
        attitude.emplace_back(euler[1]);

        if (i > 0) {
            headingDif.emplace_back(heading[i - 1] - heading[i]);
            if (abs(headingDif[i]) > MAX_HEADING_DIF) {
                headingDif[i] = headingDif[i - 1];
            }
            attitudeDif.emplace_back(attitude[i - 1] - attitude[i]);
            if (abs(attitudeDif[i]) > MAX_ATTITUDE_DIF) {
                attitudeDif[i] = 0;
            }
        } else {
            headingDif.emplace_back(0);
            attitudeDif.emplace_back(0);
        }
    }

    std::vector<double> heading;
    std::vector<double> headingDif;

    std::vector<double> attitude;
    std::vector<double> attitudeDif;

private:
    std::vector<long> timestamps;
    std::vector<long> imu_timestamps;

    std::vector<double> acc_x, acc_y, acc_z, ang_vel_x, ang_vel_y, ang_vel_z, quat_x, quat_y, quat_z, quat_w;

    long imu_index = 0;
    long previousTimeDif = INT_MAX;
};


