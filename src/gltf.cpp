#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "gltf.hpp"
#include "common.hpp"
#include "utils.hpp"

#include <simdjson/simdjson.h>

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

TopologyType parsePrimitiveMode (uint64_t mode) noexcept {
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

AccessorComponentType parseAccessorComponentType (uint64_t type) noexcept {
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

[[nodiscard]] static inline size_t jsonArraySize (simdjson::ondemand::array&& array) noexcept {
  size_t result = 0;
  for (auto current = array.begin (); current != array.end (); ++current)
    ++result;
  return result;
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

  // TODO Either migrate to json parser written in C (without the C++ bullshit),
  // or write my own. 25 FREAKING seconds to compile this translation unit is not a joke.
  // Sure, this thing's fast, but I'm not willing to wait almost two quarters of a minute each time I make a small change.
  // Also, it forces me to use STL and exceptions (kinda), so, yeah...
  simdjson::ondemand::parser parser;
  simdjson::padded_string rawGLTF = simdjson::padded_string::load (relativePath);
  simdjson::ondemand::document parsedGLTF = parser.iterate (rawGLTF);

  { // Load buffer views
    output.bufferViewsCount = jsonArraySize (parsedGLTF["bufferViews"]);
    output.bufferViews = MALLOC (BufferView, output.bufferViewsCount);

    size_t index = 0;
    for (auto bufferView : parsedGLTF["bufferViews"].get_array ()) {
      auto& newBufferView = output.bufferViews[index++];
      newBufferView.buffer = SCAST <uint32_t> (bufferView["buffer"].get_uint64 ());
      newBufferView.byteLength = SCAST <uint32_t> (bufferView["byteLength"].get_uint64 ());
      newBufferView.byteOffset = SCAST <uint32_t> (bufferView["byteOffset"].get_uint64 ());
    }
  }

  { // Load accessors
    output.accessorsCount = jsonArraySize (parsedGLTF["accessors"]);
    output.accessors = MALLOC (Accessor, output.accessorsCount);

    size_t index = 0;
    for (auto accessor : parsedGLTF["accessors"].get_array ()) {
      char accessorType[7] {};
      std::string_view accessorTypeView = accessor["type"];
      strncpy (accessorType, accessorTypeView.data (), accessorTypeView.size ());

      auto& newAccessor = output.accessors[index++];
      newAccessor.type = parseAccessorType (accessorType);
      newAccessor.count = SCAST <uint32_t> (accessor["count"].get_uint64 ());
      newAccessor.bufferView = SCAST <uint32_t> (accessor["bufferView"].get_uint64 ());
      newAccessor.byteOffset = SCAST <uint32_t> (accessor["byteOffset"].get_uint64 ());
      newAccessor.componentType = parseAccessorComponentType (accessor["componentType"].get_uint64 ());
    }
  }

  { // Load primitives
    auto defaultMesh = *parsedGLTF["meshes"].get_array().begin();
    output.mesh.primitivesCount = jsonArraySize (defaultMesh["primitives"]);
    output.mesh.primitives = MALLOC (Primitive, output.mesh.primitivesCount);

    #define GET_ATTRIBUTE(name, fieldName) do {                                                     \
      auto result = primitive["attributes"][name];                                                  \
      if (result.error () == simdjson::SUCCESS)                                                     \
        newPrimitive.attributes.fieldName = (uint32_t) primitive["attributes"][name].get_uint64 (); \
    } while (false)

    size_t index = 0;
    for (auto primitive : defaultMesh["primitives"].get_array ()) {
      auto& newPrimitive = output.mesh.primitives[index++];
      GET_ATTRIBUTE ("TEXCOORD_0", uv);
      GET_ATTRIBUTE ("NORMAL", normal);
      GET_ATTRIBUTE ("TANGENT", tangent);
      GET_ATTRIBUTE ("POSITION", position);

      newPrimitive.mode = parsePrimitiveMode (primitive["mode"].get_uint64 ());
      newPrimitive.indices = SCAST <uint32_t> (primitive["indices"].get_uint64 ());
      newPrimitive.material = SCAST <uint32_t> (primitive["material"].get_uint64 ());
    }

    #undef GET_ATTRIBUTE
  }

  {  // Load images
     output.imagesCount = jsonArraySize (parsedGLTF["images"]);
     output.images = MALLOC (Image, output.imagesCount);

     size_t index = 0;
     for (auto image : parsedGLTF["images"].get_array ()) {
       std::string_view uriView = image["uri"];
       std::string_view mimeTypeView = image["mimeType"];

       auto& newImage = output.images[index++];
       memset (newImage.uri, 0, sizeof (newImage.uri));
       strncpy (newImage.uri, uriView.data (), uriView.size ());

       char mimeType[11] {};
       strncpy (mimeType, mimeTypeView.data (), mimeTypeView.size ());
       newImage.mimeType = parseMimeType (mimeType);
     }
  }

  { // Load textures
    output.texturesCount = jsonArraySize (parsedGLTF["textures"]);
    output.textures = MALLOC (Texture, output.texturesCount);

    size_t index = 0;
    for (auto texture : parsedGLTF["textures"].get_array ()) {
      auto& newTexture = output.textures[index++];
      newTexture.source = SCAST <uint32_t> (texture["source"].get_uint64 ());
    }
  }

  { // Load materials
    output.materialsCount = jsonArraySize (parsedGLTF["materials"]);
    output.materials = MALLOC (Material, output.materialsCount);

    size_t index = 0;
    for (auto material : parsedGLTF["materials"].get_array ()) {
      auto& newMaterial = output.materials[index++];

      {
      auto result = material["alphaMode"];
        if (result.error () != simdjson::SUCCESS) {
          newMaterial.alphaMode = AlphaMode::Opaque;
        } else {
          char alphaMode[7] {};

          std::string_view alphaModeView = result.value_unsafe ();
          strncpy (alphaMode, alphaModeView.data (), alphaModeView.size ());

          newMaterial.alphaMode = parseAlphaMode (alphaMode);
        }
      }

      newMaterial.pbrMetallicRoughness.baseColorTexture.index =
	SCAST <uint32_t> (material["pbrMetallicRoughness"]["baseColorTexture"]["index"].get_uint64());
    }
  }
}

void destroy (Data& data) {
  free (data.materials);
  free (data.textures);
  free (data.images);
  free (data.mesh.primitives);
  free (data.accessors);
  free (data.bufferViews);
  free (data.buffer);
}

}
