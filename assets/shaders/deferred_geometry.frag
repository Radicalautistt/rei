#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

layout (location = 0) out vec3 outAlbedo;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outPosition;

layout (set = 0, binding = 0) uniform sampler2D albedo;

void main () {
  outAlbedo = (texture (albedo, uv)).rgb;
  outNormal = normalize (normal);
  outPosition = position;
}
