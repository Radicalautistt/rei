#ifndef GLTF_HPP
#define GLTF_HPP

#include <stddef.h>

#include "math.hpp"
#include "common.hpp"

namespace rei::assets::gltf {

enum class AccessorComponentType : Uint32 {
  Int8,
  Int16,
  Float,
  Uint8,
  Uint16,
  Uint32,

  Unknown
};

enum class AccessorType : Uint32 {
  Vec2,
  Vec3,
  Vec4,
  Mat2,
  Mat3,
  Mat4,
  Scalar,

  Unknown
};

enum class TopologyType : Uint32 {
  Lines,
  Points,
  LineLoop,
  LineStrip,
  Triangles,
  TriangleFan,
  TriangleStrip,

  Unknown
};

enum class AlphaMode : Uint32 {
  Mask,
  Blend,
  Opaque,

  Unknown
};

enum class MimeType : Uint8 {
  Png,
  Jpeg,

  Unknown
};

struct Accessor {
  AccessorType type;
  AccessorComponentType componentType;

  Uint32 count;
  Uint32 bufferView;
  Uint32 byteOffset;
};

struct BufferView {
  Uint32 buffer;
  Uint32 byteLength;
  Uint32 byteOffset;
};

struct Image {
  char uri[256];
  MimeType mimeType;
};

struct Texture {
  Uint32 source;
};

struct Material {
  AlphaMode alphaMode;
  // PBR metallic rougness
  Uint32 baseColorTexture;
};

struct Primitive {
  Uint32 indices;
  Uint32 material;

  struct {
    Uint32 uv;
    Uint32 normal;
    Uint32 tangent;
    Uint32 position;
  } attributes;
  TopologyType mode;
};

struct Mesh {
  Primitive* primitives;
  size_t primitivesCount;
};

struct Data {
  Uint8* buffer;
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
[[nodiscard]] TopologyType parsePrimitiveMode (Uint64 mode) noexcept;
[[nodiscard]] Uint8 countComponents (AccessorType accessorType) noexcept;
[[nodiscard]] AccessorType parseAccessorType (const char* rawType) noexcept;
[[nodiscard]] AccessorComponentType parseAccessorComponentType (Uint64 type) noexcept;

void load (const char* relativePath, Data* output);
void destroy (Data* data);

}

#endif /* GLTF_HPP */
