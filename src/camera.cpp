#include "camera.hpp"

namespace rei {

void Camera::update () noexcept {
  front.x = cosf (math::radians (yaw)) * cosf (math::radians (pitch));
  front.y = sinf (math::radians (pitch));
  front.z = sinf (math::radians (yaw)) * cosf (math::radians (pitch));

  math::Vector3::normalize (&front);

  math::Vector3::crossProduct (&front, &worldUp, &right);
  math::Vector3::normalize (&right);

  math::Vector3::crossProduct (&right, &front, &up);
  math::Vector3::normalize (&up);
}

void Camera::handleMouseMovement (Float32 x, Float32 y) noexcept {
  if (firstMouse) {
    lastX = x;
    lastY = y;
    firstMouse = False;
  }

  Float32 xOffset = x - lastX, yOffset = lastY - y;
  lastX = x;
  lastY = y;

  xOffset *= sensitivity;
  yOffset *= sensitivity;
  yaw += xOffset;
  pitch += yOffset;

  update ();
}

void Camera::move (Direction direction, Float32 deltaTime) noexcept {
  Float32 velocity = speed * deltaTime;
  switch (direction) {
    case Direction::Left: position -= right * velocity; break;
    case Direction::Right: position += right * velocity; break;
    case Direction::Forward: position += front * velocity; break;
    case Direction::Backward: position -= front * velocity; break;
  }
}

Camera::Camera (const math::Vector3& up, const math::Vector3& position, Float32 yaw, Float32 pitch)
  : firstMouse {True}, zoom {45.f}, speed {20.f}, sensitivity {0.1f},
  yaw {yaw}, pitch {pitch}, worldUp {up}, position {position} {

  update ();
  math::perspective (math::radians (zoom), 1680.f / 1050.f, 0.1f, 100.f, projection);
}

}
