#version 450

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 pixelColor;

//layout (set = 0, binding = 0) uniform sampler2D albedo;

void main () {
  //pixelColor = texture (albedo, uv);
  pixelColor = vec4 (uv, 0.f, 1.f);
}
