#include <math.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include "vkutils.hpp"
#include "asset_baker.hpp"

#include <lz4/lib/lz4.h>
#include <stb/stb_image.h>
#include <stb/stb_sprintf.h>
#include <rapidjson/document.h>
#include <stb/stb_image_resize.h>

namespace rei::assets {

void bakeImage (const char* relativePath) {
  Int32 width, height, channels;
  auto pixels = stbi_load (relativePath, &width, &height, &channels, STBI_rgb_alpha);
  LOGS_WARNING (relativePath);
  REI_ASSERT (pixels);

  char outputPath[256] {};
  strcpy (outputPath, relativePath);
  char* extension = strrchr (outputPath, '.');
  memcpy (extension + 1, "rtex", 5);

  FILE* outputFile = fopen (outputPath, "wb");

  Int32 resultSize = 0;
  Int32 originalSize = width * height * 4;
  Uint32 mipLevels = (Uint32) floorf (log2f ((Float32) MAX (width, height))) + 1;

  auto mipSizes = ALLOCA (size_t, mipLevels);

  // Calculate resulting image size, which contains all mip levels
  for (Uint32 mipLevel = 0; mipLevel < mipLevels; ++mipLevel) {
    // Cache sizes
    mipSizes[mipLevel] = (width >> mipLevel) * (height >> mipLevel) * 4;
    resultSize += (Int32) mipSizes[mipLevel];
  }

  Uint8* resultImage = MALLOC (Uint8, (size_t) resultSize);

  size_t mipOffset = 0;
  memcpy (resultImage, pixels, originalSize);
  mipOffset += (size_t) originalSize;

  for (Uint32 mipLevel = 1; mipLevel < mipLevels; ++mipLevel) {
    stbir_resize_uint8 (
      pixels,
      width,
      height,
      1,
      resultImage + mipOffset,
      width >> mipLevel,
      height >> mipLevel,
      1,
      4
    );

    mipOffset += mipSizes[mipLevel];
  }

  stbi_image_free (pixels);

  char* compressed = MALLOC (char, originalSize);
  Int32 compressedBound = LZ4_compressBound (resultSize);

  Int32 compressedActual = LZ4_compress_default (
    (const char*) resultImage,
    compressed,
    resultSize,
    compressedBound
  );

  if (compressedActual < compressedBound) {
    char* temp = compressed;
    compressed = MALLOC (char, compressedActual);
    memcpy (compressed, temp, compressedActual);
    free (temp);
  }

  const char metadataFormat[] =
R"({
  "width":%d,
  "height":%d,
  "mipLevels":%u,
  "binarySize":%d
})";

  char metadata[128] {};
  stbsp_sprintf (
    metadata,
    metadataFormat,
    width,
    height,
    mipLevels,
    compressedActual
  );

  size_t metadataSize = strlen (metadata);
  fwrite (&metadataSize, sizeof (size_t), 1, outputFile);
  fwrite (metadata, 1, metadataSize, outputFile);
  fwrite (compressed, 1, compressedActual, outputFile);

  free (compressed);
  fclose (outputFile);
}

void bakeImages (const char* relativePath) {
  DIR* directory = opendir (relativePath);
  REI_ASSERT (directory);

  dirent* current = nullptr;
  while ((current = readdir (directory))) {
    if (current->d_type != DT_DIR) {
      char completePath[256] {};
      strcpy (completePath, relativePath);
      const char* extension = strrchr (current->d_name, '.');

      if (strcmp (extension + 1, "rtex") && strcmp (extension + 1, "bin") && strcmp (extension + 1, "gltf")) {
        strcpy (completePath + strlen (relativePath), current->d_name);
        bakeImage (completePath);
      }
    }
  }

  closedir (directory);
}

Result readImage (const char* relativePath, vku::TextureAllocationInfo* output) {
  FILE* assetFile = fopen (relativePath, "rb");
  if (!assetFile) return Result::FileDoesNotExist;

  size_t metadataSize = 0;
  fread (&metadataSize, sizeof (size_t), 1, assetFile);

  char* metadata = ALLOCA (char, metadataSize);
  fread (metadata, 1, metadataSize, assetFile);
  metadata[metadataSize] = '\0';

  rapidjson::Document parsedMetadata;
  parsedMetadata.Parse (metadata);

  output->compressed = True;
  output->width = parsedMetadata["width"].GetUint ();
  output->height = parsedMetadata["height"].GetUint ();
  output->mipLevels = parsedMetadata["mipLevels"].GetUint ();
  output->compressedSize = (size_t) parsedMetadata["binarySize"].GetUint64 ();

  output->pixels = MALLOC (char, output->compressedSize);
  fread (output->pixels, 1, output->compressedSize, assetFile);

  fclose (assetFile);
  return Result::Success;
}

}
