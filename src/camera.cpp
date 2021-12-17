#include <math.h>
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

void Camera::handleMouseMovement (f32 x, f32 y) noexcept {
  if (firstMouse) {
    lastX = x;
    lastY = y;
    firstMouse = REI_FALSE;
  }

  f32 xOffset = x - lastX, yOffset = lastY - y;
  lastX = x;
  lastY = y;

  xOffset *= sensitivity;
  yOffset *= sensitivity;
  yaw += xOffset;
  pitch += yOffset;

  update ();
}

void Camera::move (Direction direction, f32 deltaTime) noexcept {
  f32 velocity = speed * deltaTime;

  switch (direction) {
    case Direction::Left: {
      math::Vector3 temp;
      math::Vector3::mulScalar (&right, velocity, &temp);
      math::Vector3::sub (&position, &temp, &position);
    } break;

    case Direction::Right: {
      math::Vector3 temp;
      math::Vector3::mulScalar (&right, velocity, &temp);
      math::Vector3::add (&position, &temp, &position);
    } break;

    case Direction::Forward: {
      math::Vector3 temp;
      math::Vector3::mulScalar (&front, velocity, &temp);
      math::Vector3::add (&position, &temp, &position);
    } break;

    case Direction::Backward: {
      math::Vector3 temp;
      math::Vector3::mulScalar (&front, velocity, &temp);
      math::Vector3::sub (&position, &temp, &position);
    } break;
  }
}

Camera::Camera (const math::Vector3& up, const math::Vector3& position, f32 yaw, f32 pitch)
  : firstMouse {REI_TRUE}, zoom {45.f}, speed {20.f}, sensitivity {0.1f},
  yaw {yaw}, pitch {pitch}, worldUp {up}, position {position} {

  update ();
  math::perspective (math::radians (zoom), 1680.f / 1050.f, 0.1f, 100.f, &projection);
}

}
