#include "gpu_upload.hpp"
#include "image.hpp"
#include <cstring>
#include <functional>
#include <iostream>
namespace lab3::gpu {
namespace {
VkCommandPool uploadPool{};
bool upload(const std::function<void(VkCommandBuffer)>& record) {
    auto& c=graphics::internal::context;
    VkCommandBuffer cmd{};
    VkCommandBufferAllocateInfo ai{.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool=uploadPool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};
    if(vkAllocateCommandBuffers(c.device,&ai,&cmd)!=VK_SUCCESS) return false;
    VkCommandBufferBeginInfo bi{.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    bool ok=vkBeginCommandBuffer(cmd,&bi)==VK_SUCCESS;
    if(ok) {record(cmd);ok=vkEndCommandBuffer(cmd)==VK_SUCCESS;}
    VkFence fence{};
    VkFenceCreateInfo fi{.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if(ok) ok=vkCreateFence(c.device,&fi,nullptr,&fence)==VK_SUCCESS;
    if(ok) {
        VkSubmitInfo si{.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&cmd};
        ok=vkQueueSubmit(c.graphics_queue,1,&si,fence)==VK_SUCCESS;
        if(ok) ok=vkWaitForFences(c.device,1,&fence,VK_TRUE,UINT64_MAX)==VK_SUCCESS;
    }
    if(fence) vkDestroyFence(c.device,fence,nullptr);
    vkFreeCommandBuffers(c.device,uploadPool,1,&cmd);
    return ok;
}
void logMemory(VmaAllocation allocation,const char* kind) {
    VkMemoryPropertyFlags flags{};
    vmaGetAllocationMemoryProperties(graphics::internal::context.allocator,allocation,&flags);
    std::cout<<kind<<" memory_flags="<<flags<<" DEVICE_LOCAL="<<((flags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)!=0)<<"\n";
}
}
bool initializeUploads() {
    auto& c=graphics::internal::context;
    VkCommandPoolCreateInfo pi{.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags=VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,.queueFamilyIndex=c.graphics_queue_index};
    return vkCreateCommandPool(c.device,&pi,nullptr,&uploadPool)==VK_SUCCESS;
}
void shutdownUploads() {
    if(uploadPool) vkDestroyCommandPool(graphics::internal::context.device,uploadPool,nullptr);
    uploadPool={};
}
void destroyBuffer(Buffer& b) {
    if(b.handle) vmaDestroyBuffer(graphics::internal::context.allocator,b.handle,b.allocation);
    b={};
}
bool writeBuffer(Buffer& b,const void* data,VkDeviceSize size,VkDeviceSize offset) {
    if(!size) return true;
    if(offset>b.bytes || size>b.bytes-offset || !data) return false;
    auto allocator=graphics::internal::context.allocator;
    void* mapped=nullptr;
    if(vmaMapMemory(allocator,b.allocation,&mapped)!=VK_SUCCESS) return false;
    std::memcpy(static_cast<char*>(mapped)+offset,data,static_cast<size_t>(size));
    VkResult result=vmaFlushAllocation(allocator,b.allocation,offset,size);
    vmaUnmapMemory(allocator,b.allocation);
    return result==VK_SUCCESS;
}
bool createBuffer(VkDeviceSize size,VkBufferUsageFlags usage,Buffer& b,const void* data) {
    VkBufferCreateInfo info{.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=size,.usage=usage,.sharingMode=VK_SHARING_MODE_EXCLUSIVE};
    VmaAllocationCreateInfo allocation{.flags=VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,.usage=VMA_MEMORY_USAGE_AUTO};
    if(vmaCreateBuffer(graphics::internal::context.allocator,&info,&allocation,&b.handle,&b.allocation,nullptr)!=VK_SUCCESS) return false;
    b.bytes=size;
    if(data && !writeBuffer(b,data,size)) {destroyBuffer(b);return false;}
    return true;
}
bool uploadGeometry(VkDeviceSize size,VkBufferUsageFlags usage,Buffer& b,const void* data) {
    auto& c=graphics::internal::context;
    Buffer staging;
    if(!createBuffer(size,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,staging,data)) return false;
    VkBufferCreateInfo bi{.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=size,
        .usage=usage|VK_BUFFER_USAGE_TRANSFER_DST_BIT,.sharingMode=VK_SHARING_MODE_EXCLUSIVE};
    VmaAllocationCreateInfo ai{.usage=VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,.requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
    bool ok=vmaCreateBuffer(c.allocator,&bi,&ai,&b.handle,&b.allocation,nullptr)==VK_SUCCESS;
    if(ok) {
        b.bytes=size;
        ok=upload([&](VkCommandBuffer cmd) {
            VkBufferCopy copy{0,0,size};
            vkCmdCopyBuffer(cmd,staging.handle,b.handle,1,&copy);
            VkBufferMemoryBarrier barrier{.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask=static_cast<VkAccessFlags>((usage&VK_BUFFER_USAGE_INDEX_BUFFER_BIT)?VK_ACCESS_INDEX_READ_BIT:VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT),
                .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
                .buffer=b.handle,.offset=0,.size=size};
            vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,0,0,nullptr,1,&barrier,0,nullptr);
        });
    }
    destroyBuffer(staging); // upload fence has completed before freeing staging.
    if(!ok) destroyBuffer(b); else logMemory(b.allocation,"Geometry");
    return ok;
}
void destroyTexture(Texture& t) {
    auto& c=graphics::internal::context;
    if(t.sampler) vkDestroySampler(c.device,t.sampler,nullptr);
    if(t.view) vkDestroyImageView(c.device,t.view,nullptr);
    if(t.image) vmaDestroyImage(c.allocator,t.image,t.allocation);
    t={};
}
bool loadTexture(const std::filesystem::path& path,VkFormat format,Texture& t) {
    auto& c=graphics::internal::context;
    VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties(c.physical_device,&props);
    ImagePixels pixels;
    try {pixels=decodeImage(path,props.limits.maxImageDimension2D);}
    catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return false;}
    Buffer staging;
    if(!createBuffer(pixels.rgba.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,staging,pixels.rgba.data())) return false;
    const VkExtent3D extent{static_cast<uint32_t>(pixels.width),static_cast<uint32_t>(pixels.height),1};
    VkImageCreateInfo ii{.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,.imageType=VK_IMAGE_TYPE_2D,.format=format,
        .extent=extent,.mipLevels=1,.arrayLayers=1,.samples=VK_SAMPLE_COUNT_1_BIT,.tiling=VK_IMAGE_TILING_OPTIMAL,
        .usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout=VK_IMAGE_LAYOUT_UNDEFINED};
    VmaAllocationCreateInfo ai{.usage=VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,.requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
    bool ok=vmaCreateImage(c.allocator,&ii,&ai,&t.image,&t.allocation,nullptr)==VK_SUCCESS;
    if(ok) ok=upload([&](VkCommandBuffer cmd) {
        VkImageMemoryBarrier barrier{.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask=0,.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
            .image=t.image,.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
        vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        VkBufferImageCopy copy{.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},.imageExtent=extent};
        vkCmdCopyBufferToImage(cmd,staging.handle,t.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    });
    destroyBuffer(staging);
    if(ok) {
        VkImageViewCreateInfo vi{.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,.image=t.image,.viewType=VK_IMAGE_VIEW_TYPE_2D,
            .format=format,.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
        ok=vkCreateImageView(c.device,&vi,nullptr,&t.view)==VK_SUCCESS;
    }
    if(ok) {
        VkSamplerCreateInfo si{.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter=VK_FILTER_LINEAR,.minFilter=VK_FILTER_LINEAR,.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU=VK_SAMPLER_ADDRESS_MODE_REPEAT,.addressModeV=VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT,.minLod=0,.maxLod=0};
        ok=vkCreateSampler(c.device,&si,nullptr,&t.sampler)==VK_SUCCESS;
    }
    if(!ok) destroyTexture(t);
    else {std::cout<<"Decoded + uploaded "<<path.string()<<" "<<pixels.width<<"x"<<pixels.height<<"\n";logMemory(t.allocation,"Texture");}
    return ok;
}
}
