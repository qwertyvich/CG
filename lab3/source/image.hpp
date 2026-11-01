#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>
namespace lab3 {
struct ImagePixels { int width=0,height=0; std::vector<uint8_t> rgba; };
// File rows are flipped once: UV v=0 is the bottom of the original PNG.
ImagePixels decodeImage(const std::filesystem::path& path, uint32_t maxDimension=16384);
}
