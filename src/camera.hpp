#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "math.hpp"
#include "common.hpp"

namespace rei {

struct Camera {
  enum class Direction : Uint8 {
    Left,
    Right,
    Forward,
    Backward,
  };

  Bool32 firstMouse;
  Float32 zoom, speed, sensitivity;
  math::Vector3 up;
  Float32 yaw, pitch, lastX, lastY;

  math::Vector3 front, right, worldUp, position;
  math::Matrix4 projection;

  void update () noexcept;
  void handleMouseMovement (Float32 x, Float32 y) noexcept;
  void move (Direction direction, Float32 deltaTime) noexcept;

  Camera (const math::Vector3& up, const math::Vector3& position, Float32 yaw, Float32 pitch);
};

};

#endif /* CAMERA_HPP */
