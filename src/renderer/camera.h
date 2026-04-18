#pragma once
#include <glm/glm.hpp>

struct CameraUBO {
    glm::vec4 position;
    glm::vec4 forward;
    glm::vec4 right;
    glm::vec4 up;
};

struct Camera {
    glm::vec3 position{ 0.0f, 0.0f, 5.0f };
    glm::vec3 forward{ 0.0f, 0.0f, -1.0f };
    glm::vec3 right{ 1.0f, 0.0f, 0.0f };
    glm::vec3 up{ 0.0f, 1.0f, 0.0f };

    float yaw = -90.0f;
    float pitch = 0.0f;
    float lastMouseX = 640.0f;
    float lastMouseY = 360.0f;
    bool firstMouse = true;
    float mouseSensitivity = 0.1f;

    void processMouseMovement(float xoffset, float yoffset) {
        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;

        yaw += xoffset;
        pitch += yoffset;

        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        
        forward = glm::normalize(front);
        right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        up = glm::normalize(glm::cross(right, forward));
    }
};