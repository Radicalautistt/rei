#include "camera.hpp"
#include "rei_math.inl"

namespace rei {

void Camera::update () noexcept {
  front.x = cosf (math::radians (yaw)) * cosf (math::radians (pitch));
  front.y = sinf (math::radians (pitch));
  front.z = sinf (math::radians (yaw)) * cosf (math::radians (pitch));

  math::vec3::normalize (&front);

  math::vec3::cross (&front, &worldUp, &right);
  math::vec3::normalize (&right);

  math::vec3::cross (&right, &front, &up);
  math::vec3::normalize (&up);
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
      math::Vec3 temp;
      math::vec3::mulScalar (&right, velocity, &temp);
      math::vec3::sub (&position, &temp, &position);
    } break;

    case Direction::Right: {
      math::Vec3 temp;
      math::vec3::mulScalar (&right, velocity, &temp);
      math::vec3::add (&position, &temp, &position);
    } break;

    case Direction::Forward: {
      math::Vec3 temp;
      math::vec3::mulScalar (&front, velocity, &temp);
      math::vec3::add (&position, &temp, &position);
    } break;

    case Direction::Backward: {
      math::Vec3 temp;
      math::vec3::mulScalar (&front, velocity, &temp);
      math::vec3::sub (&position, &temp, &position);
    } break;
  }
}

Camera::Camera (const math::Vec3& up, const math::Vec3& position, f32 yaw, f32 pitch)
  : firstMouse {REI_TRUE}, zoom {45.f}, speed {20.f}, sensitivity {0.1f},
  yaw {yaw}, pitch {pitch}, worldUp {up}, position {position} {

  update ();
  math::perspective (math::radians (zoom), 1680.f / 1050.f, 0.1f, 100.f, &projection);
}

}
