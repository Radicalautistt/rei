#version 450

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 pixelColor;

void main () {
  pixelColor = vec4 (uv, 1.f, 1.f);
}
