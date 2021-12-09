#version 450

layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 pixelColor;

layout (set = 0, binding = 0) uniform sampler2D albedo;
layout (set = 0, binding = 1) uniform sampler2D normal;
layout (set = 0, binding = 2) uniform sampler2D position;

struct Light {
  vec4 position;
  vec4 colorRadius;
};

layout (push_constant) uniform PushConstants {
  Light light;
  uint target;
  vec4 viewPosition;
} pushConstants;

// This shader is kinda stolen from Sascha Willems, I need to write my own
void main () {
  vec4 albedoAttachment = texture (albedo, uv);
  vec3 normalAttachment = texture (normal, uv).rgb;
  vec3 positionAttachment = texture (position, uv).rgb;

  if (pushConstants.target > 0) {
    switch (pushConstants.target) {
      case 1: pixelColor = albedoAttachment; return;
      case 2: pixelColor = vec4 (normalAttachment, 1.f); return;
      case 3: pixelColor = vec4 (positionAttachment, 1.f); return;
    }
  }

  vec3 color = albedoAttachment.rgb * 0.05f;

  vec3 L = pushConstants.light.position.xyz - positionAttachment;
  float distance = length (L);

  vec3 V = pushConstants.viewPosition.xyz - positionAttachment;
  V = normalize (V);

  L = normalize (L);
  float attenuation = pushConstants.light.colorRadius.w / (pow (distance, 2.f) + 1.f);

  vec3 N = normalize (normalAttachment);
  float NdotL = max (0.f, dot (N, L));
  vec3 diff = pushConstants.light.colorRadius.xyz * albedoAttachment.rgb * NdotL * attenuation;

  vec3 R = reflect (-L, N);
  float NdotR = max (0.0, dot(R, V));
  vec3 spec = pushConstants.light.colorRadius.xyz * albedoAttachment.a * pow (NdotR, 16.f) * attenuation;

  color += diff + spec;

  pixelColor = vec4 (color, 1.f);
}
