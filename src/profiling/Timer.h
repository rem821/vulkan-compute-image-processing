//
// Created by Stanislav Svědiroh on 14.07.2022.
//
#pragma once

#include "../GlobalConfiguration.h"

#include <chrono>
#include <fmt/core.h>
#include <string>
#include <utility>

struct Timer {

#if DEVICE_TYPE == 2
    std::chrono::time_point<std::chrono::steady_clock> _start, _end;
#else
    std::chrono::time_point<std::chrono::system_clock> _start, _end;
#endif
    std::chrono::duration<float> _duration{};


    std::string _name;
    float *_timerValue;

    Timer(std::string name, float *timerValue = nullptr) : _name(std::move(name)), _timerValue(timerValue),
                                                           _start(std::chrono::high_resolution_clock::now()) {}

    ~Timer() {
#if TIMER_ON
        _end = std::chrono::high_resolution_clock::now();
        _duration = _end - _start;

        float ms = _duration.count() * 1000;
        if(_timerValue != nullptr) *_timerValue = ms;
        fmt::print("Execution of {} took {} ms\n", _name, ms);
#endif
    }
};