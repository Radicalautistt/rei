#ifndef ASSET_BAKER_HPP
#define ASSET_BAKER_HPP

#include "common.hpp"

namespace rei::vku {
struct TextureAllocationInfo;
}

namespace rei::assets {

void bakeImage (const char* relativePath);
void bakeImages (const char* relativePath);
Result readImage (const char* relativePath, vku::TextureAllocationInfo* output);

}

#endif /* ASSET_BAKER_HPP */
