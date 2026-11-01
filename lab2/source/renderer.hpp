#pragma once
#include "scene.hpp"
#include "graphics_internal.hpp"
namespace lab2::renderer {
bool initialize();
void shutdown();
bool render(const Scene& scene,const graphics::internal::FrameData& frame);
size_t pointCapacity();
size_t spotCapacity();
size_t maxPoints();
size_t maxSpots();
}
