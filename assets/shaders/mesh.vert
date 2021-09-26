#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

layout (location = 0) out vec2 outUv;

layout (push_constant) uniform Mvp {
  mat4 mvp;
} mvp;

void main () {
  outUv = uv;
  gl_Position = mvp.mvp * vec4 (position, 1.f);
}
