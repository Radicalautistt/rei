#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

namespace rei {

struct Camera {
  enum class Direction : uint8_t {
    Left,
    Right,
    Forward,
    Backward,
  };

  bool firstMouse;
  float zoom, speed, sensitivity;
  float zFar, yaw, pitch, lastX, lastY;

  glm::vec3 up, front, right, worldUp, position;
  glm::mat4 projection;

  void update () noexcept;
  void handleMouseMovement (float x, float y) noexcept;
  void move (Direction direction, float deltaTime) noexcept;

  Camera (const glm::vec3& up, const glm::vec3& position, float yaw, float pitch);
};

};

#endif /* CAMERA_HPP */
