#version 450

layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 pixelColor;

layout (set = 0, binding = 0) uniform sampler2D albedo;
layout (set = 0, binding = 1) uniform sampler2D normal;
layout (set = 0, binding = 2) uniform sampler2D position;

void main () {
  pixelColor = texture (albedo, uv);
}
