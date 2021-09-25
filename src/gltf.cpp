#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "gltf.hpp"
#include "common.hpp"
#include "utils.hpp"

namespace rei::assets::gltf {

uint8_t countComponents (AccessorType accessorType) noexcept {
  switch (accessorType) {
    case AccessorType::Vec2: return 2;
    case AccessorType::Vec3: return 3;
    case AccessorType::Vec4: return 4;
    case AccessorType::Mat2: return 4;
    case AccessorType::Mat3: return 9;
    case AccessorType::Mat4: return 16;
    case AccessorType::Scalar: return 1;
    default: return 0;
  }
}

AccessorType parseAccessorType (const char* rawType) noexcept {
  if (!strcmp (rawType, "VEC2")) return AccessorType::Vec2;
  if (!strcmp (rawType, "VEC3")) return AccessorType::Vec3;
  if (!strcmp (rawType, "VEC4")) return AccessorType::Vec4;
  if (!strcmp (rawType, "MAT2")) return AccessorType::Mat2;
  if (!strcmp (rawType, "MAT3")) return AccessorType::Mat3;
  if (!strcmp (rawType, "MAT4")) return AccessorType::Mat4;
  if (!strcmp (rawType, "SCALAR")) return AccessorType::Scalar;
  return AccessorType::Unknown;
}

TopologyType parsePrimitiveMode (const char* rawMode) noexcept {
  if (!strcmp (rawMode, "LINES")) return TopologyType::Lines;
  if (!strcmp (rawMode, "POINTS")) return TopologyType::Points;
  if (!strcmp (rawMode, "LINE_LOOP")) return TopologyType::LineLoop;
  if (!strcmp (rawMode, "TRIANGLES")) return TopologyType::Triangles;
  if (!strcmp (rawMode, "LINE_STRIP")) return TopologyType::LineStrip;
  if (!strcmp (rawMode, "TRIANGLE_FAN")) return TopologyType::TriangleFan;
  if (!strcmp (rawMode, "TRIANGLE_STRIP")) return TopologyType::TriangleStrip;
  return TopologyType::Unknown;
}

AccessorComponentType parseAccessorComponentType (uint64_t rawType) noexcept {
  switch (rawType) {
    case 5120: return AccessorComponentType::Int8;
    case 5121: return AccessorComponentType::Uint8;
    case 5122: return AccessorComponentType::Int16;
    case 5126: return AccessorComponentType::Float;
    case 5123: return AccessorComponentType::Uint16;
    case 5125: return AccessorComponentType::Uint32;
    default: return AccessorComponentType::Unknown;
  }
}

void load (const char* relativePath, Data& output) {
  printf ("Loading a gltf model from %s\n", relativePath);

  { // Load buffer data from .bin file
    char binaryPath[256] {};
    strcpy (binaryPath, relativePath);
    char* extension = strrchr (binaryPath, '.');
    strcpy (extension, ".bin");

    utils::File binaryFile;
    utils::readFile (binaryPath, "rb", binaryFile);

    output.buffer = MALLOC (uint8_t, binaryFile.size);
    memcpy (output.buffer, SCAST <uint8_t*> (binaryFile.contents), binaryFile.size);

    free (binaryFile.contents);
  }
}

void destroy (Data& data) {
  free (data.mesh.primitives);
  free (data.accessors);
  free (data.bufferViews);
  free (data.buffer);
}

}
