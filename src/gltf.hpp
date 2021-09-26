#ifndef GLTF_HPP
#define GLTF_HPP

#include <stdint.h>
#include <stddef.h>

namespace rei::assets::gltf {

enum class AccessorComponentType : uint8_t {
  Int8,
  Int16,
  Float,
  Uint8,
  Uint16,
  Uint32,

  Unknown
};

enum class AccessorType : uint8_t {
  Vec2,
  Vec3,
  Vec4,
  Mat2,
  Mat3,
  Mat4,
  Scalar,

  Unknown
};

enum class TopologyType : uint8_t {
  Lines,
  Points,
  LineLoop,
  LineStrip,
  Triangles,
  TriangleFan,
  TriangleStrip,

  Unknown
};

enum class AlphaMode : uint8_t {
  Mask,
  Blend,
  Opaque,

  Unknown
};

struct Accessor {
  AccessorType type;
  AccessorComponentType componentType;

  uint32_t count;
  uint32_t bufferView;
  uint32_t byteOffset;
};

struct BufferView {
  uint32_t buffer;
  uint32_t byteLength;
  uint32_t byteOffset;
};

struct TextureInfo {
  uint32_t index;
};

struct PbrMetallicRoughness {
  TextureInfo baseColorTexture;
};

struct Material {
  AlphaMode alphaMode;
  PbrMetallicRoughness pbrMetallicRoughness;
};

struct Primitive {
  uint32_t indices;
  uint32_t material;

  struct {
    uint32_t uv;
    uint32_t normal;
    uint32_t tangent;
    uint32_t position;
  } attributes;
  TopologyType mode;
};

struct Mesh {
  Primitive* primitives;
  size_t primitivesCount;
};

struct Data {
  uint8_t* buffer;
  size_t bufferSize;

  BufferView* bufferViews;
  size_t bufferViewsCount;

  Accessor* accessors;
  size_t accessorsCount;

  Mesh mesh;

  Material* materials;
  size_t materialsCount;
};

[[nodiscard]] AlphaMode parseAlphaMode (const char* rawMode) noexcept;
[[nodiscard]] TopologyType parsePrimitiveMode (uint64_t mode) noexcept;
[[nodiscard]] uint8_t countComponents (AccessorType accessorType) noexcept;
[[nodiscard]] AccessorType parseAccessorType (const char* rawType) noexcept;
[[nodiscard]] AccessorComponentType parseAccessorComponentType (uint64_t type) noexcept;

void load (const char* relativePath, Data& output);
void destroy (Data& data);

}

#endif /* GLTF_HPP */
