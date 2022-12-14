//
// Created by standa on 5.12.22.
//
#pragma once

#include "DatasetFileReader.h"

void estimateVanishingPointPosition(Dataset* dataset) {
    Timer timer("Vanishing point estimation", &dataset->vanishingPointEstimation);

    // Estimate position of the road vanishing point
    int32_t r_vp_x = (dataset->cameraWidth / 2 + int32_t(HORIZONTAL_SENSITIVITY * dataset->headingDif.back()) +
                      HORIZONTAL_OFFSET);
    int32_t r_vp_y = (dataset->cameraHeight / 2 + int32_t(VERTICAL_SENSITIVITY * dataset->attitudeDif.back()) +
                      VERTICAL_OFFSET);
    dataset->vanishingPoint = std::pair(r_vp_x, r_vp_y);
}