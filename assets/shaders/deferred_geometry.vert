#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUv;

layout (push_constant) uniform PushConstants {
  mat4 mvp;
  mat4 model;
} pushConstants;

void main () {
  const vec4 _position = vec4 (position, 1.f);
  gl_Position = pushConstants.mvp * _position;

  outUv = uv;
  outNormal = (pushConstants.model * vec4 (normal, 0.f)).xyz;
  outPosition = (pushConstants.model * _position).xyz;
}
