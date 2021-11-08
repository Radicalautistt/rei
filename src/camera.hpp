#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "math.hpp"
#include "common.hpp"

namespace rei {

struct Camera {
  enum class Direction : uint8_t {
    Left,
    Right,
    Forward,
    Backward,
  };

  Bool32 firstMouse;
  float zoom, speed, sensitivity;
  float zFar, yaw, pitch, lastX, lastY;

  math::Vector3 up, front, right, worldUp, position;
  math::Matrix4 projection;

  void update () noexcept;
  void handleMouseMovement (float x, float y) noexcept;
  void move (Direction direction, float deltaTime) noexcept;

  Camera (const math::Vector3& up, const math::Vector3& position, float yaw, float pitch);
};

};

#endif /* CAMERA_HPP */
