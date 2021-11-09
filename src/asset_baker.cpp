#include <math.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include "common.hpp"
#include "asset_baker.hpp"

#include <lz4/lib/lz4.h>
#include <stb/stb_image.h>
#include <stb/stb_sprintf.h>
#include <stb/stb_image_resize.h>

namespace rei::assets {

void bakeImage (const char* relativePath) {
  int width, height, channels;
  auto pixels = stbi_load (relativePath, &width, &height, &channels, STBI_rgb_alpha);
  LOGS_WARNING (relativePath);
  REI_ASSERT (pixels);

  char outputPath[256] {};
  strcpy (outputPath, relativePath);
  char* extension = strrchr (outputPath, '.');
  strcpy (extension + 1, "rtex");

  FILE* outputFile = fopen (outputPath, "wb");

  int resultSize = 0;
  int originalSize = width * height * 4;
  uint32_t mipLevels = (uint32_t) floorf (log2f ((float) MAX (width, height))) + 1;

  auto mipSizes = ALLOCA (size_t, mipLevels);

  // Calculate resulting image size, which contains all mip levels
  for (uint32_t mipLevel = 0; mipLevel < mipLevels; ++mipLevel) {
    // Cache sizes
    mipSizes[mipLevel] = (width >> mipLevel) * (height >> mipLevel) * 4;
    resultSize += (int) mipSizes[mipLevel];
  }

  uint8_t* resultImage = MALLOC (uint8_t, (size_t) resultSize);

  size_t mipOffset = 0;
  memcpy (resultImage, pixels, originalSize);
  mipOffset += (size_t) originalSize;

  for (uint32_t mipLevel = 1; mipLevel < mipLevels; ++mipLevel) {
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
  int compressedBound = LZ4_compressBound (resultSize);

  int compressedActual = LZ4_compress_default (
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

Result readAsset (const char* relativePath, simdjson::ondemand::parser* parser, Asset* output) {
  FILE* assetFile = fopen (relativePath, "rb");
  if (!assetFile) return Result::FileDoesNotExist;

  size_t metadataSize = 0;
  fread (&metadataSize, sizeof (size_t), 1, assetFile);

  output->metadata = MALLOC (char, metadataSize);
  fread (output->metadata, 1, metadataSize, assetFile);
  output->metadata[metadataSize] = '\0';

  simdjson::padded_string paddedMetadata {output->metadata, strlen (output->metadata)};
  simdjson::ondemand::document metadata = parser->iterate (paddedMetadata);
  output->size = (size_t) metadata["binarySize"].get_uint64 ();

  output->data = MALLOC (char, output->size);
  fread (output->data, 1, output->size, assetFile);

  fclose (assetFile);
  return Result::Success;
}

}
