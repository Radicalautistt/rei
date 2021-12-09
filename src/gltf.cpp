#include <string.h>
#include <stdlib.h>

#include "gltf.hpp"
#include "common.hpp"

#include <rapidjson/document.h>

namespace rei::assets::gltf {

MimeType parseMimeType (const char* rawType) noexcept {
  if (!strcmp (rawType, "image/png")) return MimeType::Png;
  if (!strcmp (rawType, "image/jpeg")) return MimeType::Jpeg;
  return MimeType::Unknown;
}

AlphaMode parseAlphaMode (const char* rawMode) noexcept {
  if (!strcmp (rawMode, "MASK")) return AlphaMode::Mask;
  if (!strcmp (rawMode, "BLEND")) return AlphaMode::Blend;
  if (!strcmp (rawMode, "OPAQUE")) return AlphaMode::Opaque;
  return AlphaMode::Unknown;
}

Uint8 countComponents (AccessorType accessorType) noexcept {
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

TopologyType parsePrimitiveMode (Uint64 mode) noexcept {
  switch (mode) {
    case 1: return TopologyType::Lines;
    case 0: return TopologyType::Points;
    case 2: return TopologyType::LineLoop;
    case 4: return TopologyType::Triangles;
    case 3: return TopologyType::LineStrip;
    case 6: return TopologyType::TriangleFan;
    case 5: return TopologyType::TriangleStrip;
    default: return TopologyType::Unknown;
  }
}

AccessorComponentType parseAccessorComponentType (Uint64 type) noexcept {
  switch (type) {
    case 5120: return AccessorComponentType::Int8;
    case 5121: return AccessorComponentType::Uint8;
    case 5122: return AccessorComponentType::Int16;
    case 5126: return AccessorComponentType::Float;
    case 5123: return AccessorComponentType::Uint16;
    case 5125: return AccessorComponentType::Uint32;
    default: return AccessorComponentType::Unknown;
  }
}

void load (const char* relativePath, Data* output) {
  LOG_INFO ("Loading a gltf model from " ANSI_YELLOW "%s", relativePath);

  { // Load buffer data from .bin file
    char binaryPath[256] {};
    strcpy (binaryPath, relativePath);
    char* extension = strrchr (binaryPath, '.');
    memcpy (extension + 1, "bin", 4);

    File binaryFile;
    REI_CHECK (readFile (binaryPath, True, &binaryFile));

    output->buffer = MALLOC (Uint8, binaryFile.size);
    memcpy (output->buffer, (Uint8*) binaryFile.contents, binaryFile.size);

    free (binaryFile.contents);
  }

  File gltf;
  REI_CHECK (readFile (relativePath, False, &gltf));

  rapidjson::Document parsedGLTF;
  parsedGLTF.Parse ((const char*) gltf.contents);
  free (gltf.contents);

  // Load buffer views
  const auto& bufferViews = parsedGLTF["bufferViews"].GetArray ();
  output->bufferViewsCount = bufferViews.Size ();
  output->bufferViews = MALLOC (BufferView, output->bufferViewsCount);

  Uint32 offset = 0;
  for (const auto& bufferView : bufferViews) {
    auto newBufferView = &output->bufferViews[offset++];
    newBufferView->buffer = bufferView["buffer"].GetUint ();
    newBufferView->byteLength = bufferView["byteLength"].GetUint ();
    newBufferView->byteOffset = bufferView["byteOffset"].GetUint ();
  }

  // Load accessors
  const auto& accessors = parsedGLTF["accessors"].GetArray ();
  output->accessorsCount = accessors.Size ();
  output->accessors = MALLOC (Accessor, output->accessorsCount);

  offset = 0;
  for (const auto& accessor : accessors) {
    auto newAccessor = &output->accessors[offset++];
    newAccessor->count = accessor["count"].GetUint ();
    newAccessor->bufferView = accessor["bufferView"].GetUint ();
    newAccessor->byteOffset = accessor["byteOffset"].GetUint ();
    newAccessor->type = parseAccessorType (accessor["type"].GetString ());
    newAccessor->componentType = parseAccessorComponentType (accessor["componentType"].GetUint ());
  }

  // Load scale vector
  const auto& defaultNode = *parsedGLTF["nodes"].GetArray().Begin ();
  const auto& scaleVector = defaultNode["scale"].GetArray ();

  output->scaleVector.x = scaleVector[0].GetFloat ();
  output->scaleVector.y = scaleVector[1].GetFloat ();
  output->scaleVector.z = scaleVector[2].GetFloat ();

  // Load primitives
  const auto& defaultMesh = *parsedGLTF["meshes"].GetArray().Begin ();
  const auto& primitives = defaultMesh["primitives"].GetArray ();
  output->mesh.primitivesCount = primitives.Size ();
  output->mesh.primitives = MALLOC (Primitive, output->mesh.primitivesCount);

  #define GET_ATTRIBUTE(name, fieldName) do {                                        \
    if (primitive["attributes"].HasMember(name))                                     \
      newPrimitive->attributes.fieldName = primitive["attributes"][name].GetUint (); \
  } while (0)

  offset = 0;
  for (const auto& primitive : primitives) {
    auto newPrimitive = &output->mesh.primitives[offset++];
    GET_ATTRIBUTE ("TEXCOORD_0", uv);
    GET_ATTRIBUTE ("NORMAL", normal);
    GET_ATTRIBUTE ("TANGENT", tangent);
    GET_ATTRIBUTE ("POSITION", position);

    newPrimitive->mode = parsePrimitiveMode (primitive["mode"].GetUint ());
    newPrimitive->indices = primitive["indices"].GetUint ();
    newPrimitive->material = primitive["material"].GetUint ();
  }

  #undef GET_ATTRIBUTE

  // Load images
  const auto& images = parsedGLTF["images"].GetArray ();
  output->imagesCount = images.Size ();
  output->images = MALLOC (Image, output->imagesCount);

  offset = 0;
  for (const auto& image : images) {
    auto newImage = &output->images[offset++];
    strcpy (newImage->uri, image["uri"].GetString ());
    newImage->mimeType = parseMimeType (image["mimeType"].GetString ());
  }

  // Load textures
  const auto& textures = parsedGLTF["textures"].GetArray ();
  output->texturesCount = textures.Size ();
  output->textures = MALLOC (Texture, output->texturesCount);

  offset = 0;
  for (const auto& texture : textures) {
    auto newTexture = &output->textures[offset++];
    newTexture->source = texture["source"].GetUint ();
  }

  // Load materials
  const auto& materials = parsedGLTF["materials"].GetArray ();
  output->materialsCount = materials.Size ();
  output->materials = MALLOC (Material, output->materialsCount);

  offset = 0;
  for (const auto& material : materials) {
    auto newMaterial = &output->materials[offset++];

    if (material.HasMember ("alphaMode")) {
      newMaterial->alphaMode = parseAlphaMode (material["alphaMode"].GetString ());
    } else {
      newMaterial->alphaMode = AlphaMode::Opaque;
    }

    newMaterial->baseColorTexture =
      material["pbrMetallicRoughness"]["baseColorTexture"]["index"].GetUint ();
  }
}

void destroy (Data* data) {
  free (data->materials);
  free (data->textures);
  free (data->images);
  free (data->mesh.primitives);
  free (data->accessors);
  free (data->bufferViews);
  free (data->buffer);
}

}
