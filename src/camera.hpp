#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "math.hpp"
#include "rei_types.hpp"

namespace rei {

struct Camera {
  enum class Direction : u8 {
    Left,
    Right,
    Forward,
    Backward,
  };

  b32 firstMouse;
  f32 zoom, speed, sensitivity;
  math::Vector3 up;
  f32 yaw, pitch, lastX, lastY;

  math::Vector3 front, right, worldUp, position;
  math::Matrix4 projection;

  void update () noexcept;
  void handleMouseMovement (f32 x, f32 y) noexcept;
  void move (Direction direction, f32 deltaTime) noexcept;

  Camera (const math::Vector3& up, const math::Vector3& position, f32 yaw, f32 pitch);
};

};

#endif /* CAMERA_HPP */
