//
// Created by Stanislav Svědiroh on 14.06.2022.
//
#pragma once

#include <cassert>
#include <limits>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>

class Camera {
public:

    void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);

    void setPerspectiveProjection(float fovy, float aspect, float near, float far);

    void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{.0f, -1.f, .0f});
    void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3{.0f, -1.f, .0f});
    void setViewYXZ(glm::vec3 position, glm::vec3 rotation);

    const glm::mat4 &getProjection() const { return projectionMatrix; };

    const glm::mat4 &getView() const { return viewMatrix; };

    const glm::mat4 &getInverseView() const { return inverseViewMatrix; };

    glm::vec3 getPosition() const { return _position; };

    glm::vec3 getRotation() const { return _rotation; };

private:
    glm::mat4 projectionMatrix{1.f};
    glm::mat4 viewMatrix{1.f};
    glm::mat4 inverseViewMatrix{1.f};

    glm::vec3 _position{1.f};
    glm::vec3 _rotation{1.f};
};
