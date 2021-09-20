#version 450

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 uv;

layout (location = 0) out vec2 outUv;

layout (push_constant) uniform Mvp {
  mat4 mvp;
} mvp;

void main () {
  outUv = uv;
  gl_Position = mvp.mvp * vec4 (position, 0.f, 1.f);
}
