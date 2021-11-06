#ifndef ASSET_BAKER_HPP
#define ASSET_BAKER_HPP

#include <stddef.h>

#include "common.hpp"

#include <simdjson/simdjson.h>

namespace rei::assets {

struct Asset {
  char* data;
  size_t size;

  char* metadata;
};

void bakeImage (const char* relativePath);
Result readAsset (const char* relativePath, simdjson::ondemand::parser* parser, Asset* output);

}

#endif /* ASSET_BAKER_HPP */
