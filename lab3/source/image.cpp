#include "image.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
namespace lab3 {
ImagePixels decodeImage(const std::filesystem::path& path,uint32_t maxDimension) {
    const auto name=path.string();
    int w=0,h=0,channels=0;
    if(!stbi_info(name.c_str(),&w,&h,&channels) || w<=0 || h<=0 ||
       static_cast<uint32_t>(w)>maxDimension || static_cast<uint32_t>(h)>maxDimension ||
       static_cast<size_t>(w)>std::numeric_limits<size_t>::max()/4/static_cast<size_t>(h))
        throw std::runtime_error("Invalid/missing image or size exceeds limit: "+name);
    stbi_set_flip_vertically_on_load(1);
    std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> data(stbi_load(name.c_str(),&w,&h,&channels,4),stbi_image_free);
    if(!data) throw std::runtime_error("Cannot decode image: "+name+" ("+stbi_failure_reason()+")");
    return {w,h,std::vector<uint8_t>(data.get(),data.get()+static_cast<size_t>(w)*static_cast<size_t>(h)*4)};
}
}
