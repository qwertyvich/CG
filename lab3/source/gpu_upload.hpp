#pragma once
#include "graphics_internal.hpp"
#include <filesystem>
namespace lab3::gpu {
struct Buffer { VkBuffer handle{}; VmaAllocation allocation{}; VkDeviceSize bytes{}; };
struct Texture { VkImage image{}; VmaAllocation allocation{}; VkImageView view{}; VkSampler sampler{}; };
bool initializeUploads();
void shutdownUploads();
void destroyBuffer(Buffer& buffer);
void destroyTexture(Texture& texture);
bool writeBuffer(Buffer& buffer,const void* data,VkDeviceSize bytes,VkDeviceSize offset=0);
bool createBuffer(VkDeviceSize bytes,VkBufferUsageFlags usage,Buffer& buffer,const void* data=nullptr);
bool uploadGeometry(VkDeviceSize bytes,VkBufferUsageFlags usage,Buffer& buffer,const void* data);
bool loadTexture(const std::filesystem::path& path,VkFormat format,Texture& texture);
}
