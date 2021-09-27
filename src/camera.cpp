#include <stdio.h>

#include "camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace rei {

void Camera::update () noexcept {
  front.x = cosf (glm::radians (yaw)) * cosf (glm::radians (pitch));
  front.y = sinf (glm::radians (pitch));
  front.z = sinf (glm::radians (yaw)) * cosf (glm::radians (pitch));

  front = glm::normalize (front);
  right = glm::normalize (glm::cross (front, worldUp));
  up = glm::normalize (glm::cross (right, front));
}

void Camera::move (Direction direction, float deltaTime) noexcept {
  float velocity = speed * deltaTime;
  switch (direction) {
    case Direction::Left: position -= right * velocity; break;
    case Direction::Right: position += right * velocity; break;
    case Direction::Forward: position += front * velocity; break;
    case Direction::Backward: position -= front * velocity; break;
  }
}

Camera::Camera (const glm::vec3& up, const glm::vec3& position, float yaw, float pitch)
  : firstMouse {true}, zoom {45.f}, speed {530.f}, sensitivity {0.1f},
  zFar {3000.f}, yaw {yaw}, pitch {pitch}, worldUp {up}, position {position} {

  update ();
  projection = glm::perspective (glm::radians (zoom), 1680.f / 1050.f, 0.1f, zFar);
  projection[1][1] *= -1;
}

}
