//
// Created by Stanislav Svědiroh on 14.07.2022.
//
#pragma once

#include "../GlobalConfiguration.h"

#include <chrono>
#include <fmt/core.h>
#include <string>

struct Timer {

    #if DEVICE_TYPE == 2
        std::chrono::time_point<std::chrono::steady_clock> start, end;
    #else
        std::chrono::time_point<std::chrono::system_clock> start, end;
    #endif
    std::chrono::duration<float> duration;


    std::string _name;

    Timer(const std::string &name) {
        if (TIMER_ON) {
            start = std::chrono::high_resolution_clock::now();

            _name = name;
        }
    }

    ~Timer() {
        if (TIMER_ON) {
            end = std::chrono::high_resolution_clock::now();
            duration = end - start;

            fmt::print("Execution of {} took {} ms\n", _name, duration.count() * 1000);
        }
    }
};