#include <stdio.h>

#include "asset_baker.hpp"

#include <lz4/lib/lz4.h>
#include <stb/stb_image.h>
#include <stb/stb_sprintf.h>

namespace rei::assets {

void bakeImage (const char* relativePath) {
  int width, height, channels;
  auto pixels = stbi_load (relativePath, &width, &height, &channels, STBI_rgb_alpha);
  REI_ASSERT (pixels);

  char outputPath[256] {};
  strcpy (outputPath, relativePath);
  char* extension = strrchr (outputPath, '.');
  strcpy (extension + 1, "rtex");

  FILE* outputFile = fopen (outputPath, "wb");

  int imageSize = width * height * 4;
  char* compressed = MALLOC (char, imageSize);
  int compressedBound = LZ4_compressBound (imageSize);

  int compressedActual = LZ4_compress_default (
    RCAST <const char*> (pixels),
    compressed,
    imageSize,
    compressedBound
  );

  stbi_image_free (pixels);

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
  "binarySize":%d
})";

  char metadata[128] {};
  stbsp_sprintf (
    metadata,
    metadataFormat,
    width,
    height,
    compressedActual
  );

  size_t metadataSize = strlen (metadata);
  fwrite (&metadataSize, sizeof (size_t), 1, outputFile);
  fwrite (metadata, 1, metadataSize, outputFile);
  fwrite (compressed, 1, compressedActual, outputFile);

  free (compressed);
  fclose (outputFile);
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
  output->size = SCAST <size_t> (metadata["binarySize"].get_uint64 ());

  output->data = MALLOC (char, output->size);
  fread (output->data, 1, output->size, assetFile);

  fclose (assetFile);
  return Result::Success;
}

}
