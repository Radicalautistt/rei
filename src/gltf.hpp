#ifndef GLTF_HPP
#define GLTF_HPP

#include <stddef.h>

#include "math.hpp"

namespace rei::assets::gltf {

enum class AccessorComponentType : u32 {
  Int8,
  Int16,
  Float,
  Uint8,
  Uint16,
  Uint32,

  Unknown
};

enum class AccessorType : u32 {
  Vec2,
  Vec3,
  Vec4,
  Mat2,
  Mat3,
  Mat4,
  Scalar,

  Unknown
};

enum class TopologyType : u32 {
  Lines,
  Points,
  LineLoop,
  LineStrip,
  Triangles,
  TriangleFan,
  TriangleStrip,

  Unknown
};

enum class AlphaMode : u32 {
  Mask,
  Blend,
  Opaque,

  Unknown
};

enum class MimeType : u8 {
  Png,
  Jpeg,

  Unknown
};

struct Accessor {
  AccessorType type;
  AccessorComponentType componentType;

  u32 count;
  u32 bufferView;
  u32 byteOffset;
};

struct BufferView {
  u32 buffer;
  u32 byteLength;
  u32 byteOffset;
};

struct Image {
  char uri[256];
  MimeType mimeType;
};

struct Texture {
  u32 source;
};

struct Material {
  AlphaMode alphaMode;
  // PBR metallic rougness
  u32 baseColorTexture;
};

struct Primitive {
  u32 indices;
  u32 material;

  struct {
    u32 uv;
    u32 normal;
    u32 tangent;
    u32 position;
  } attributes;
  TopologyType mode;
};

struct Mesh {
  Primitive* primitives;
  size_t primitivesCount;
};

struct Data {
  u8* buffer;
  size_t bufferSize;

  BufferView* bufferViews;
  size_t bufferViewsCount;

  Accessor* accessors;
  size_t accessorsCount;

  Mesh mesh;

  Image* images;
  size_t imagesCount;

  Texture* textures;
  size_t texturesCount;

  Material* materials;
  size_t materialsCount;

  math::Vector3 scaleVector;
};

[[nodiscard]] MimeType parseMimeType (const char* rawType) noexcept;
[[nodiscard]] AlphaMode parseAlphaMode (const char* rawMode) noexcept;
[[nodiscard]] TopologyType parsePrimitiveMode (u64 mode) noexcept;
[[nodiscard]] u8 countComponents (AccessorType accessorType) noexcept;
[[nodiscard]] AccessorType parseAccessorType (const char* rawType) noexcept;
[[nodiscard]] AccessorComponentType parseAccessorComponentType (u64 type) noexcept;

void load (const char* relativePath, Data* output);
void destroy (Data* data);

}

#endif /* GLTF_HPP */
