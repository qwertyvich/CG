# Карта кода ЛР3

Номера вычислены из итоговых файлов. Обновление: `python tools/update_code_map.py`.
Ссылки относительные; после публикации GitHub открывает соответствующую строку.

| Что показать | Файл и строка | Поисковая строка | Объяснение |
| --- | --- | --- | --- |
| decode | [source/image.cpp:9](source/image.cpp#L9) | `ImagePixels decodeImage` | Декодирование файла, проверка размеров и один переворот строк |
| vertex | [source/scene.hpp:120](source/scene.hpp#L120) | `struct Vertex {` | Position, normal, UV; цвет оставлен для помощников, offsets проверены |
| model_layout | [source/scene.hpp:101](source/scene.hpp#L101) | `struct alignas(16) ModelGpu` | Запись std140 на 176 байт без sampling flag |
| cylinder | [source/geometry.cpp:4](source/geometry.cpp#L4) | `Mesh generateCylinder` | 50 сегментов, дублированный шов, U по часовой стрелке и отдельные крышки |
| normal_matrix | [source/scene.cpp:70](source/scene.cpp#L70) | `mg.normalMatrix=` | Обратная транспонированная R*S^-1 при неравномерном масштабе |
| geometry_upload | [source/gpu_upload.cpp:69](source/gpu_upload.cpp#L69) | `bool uploadGeometry` | Staging и финальный буфер с обязательным DEVICE_LOCAL |
| buffer_copy | [source/gpu_upload.cpp:81](source/gpu_upload.cpp#L81) | `vkCmdCopyBuffer(cmd` | Копирование и барьер transfer write -> vertex/index read |
| upload_wait | [source/gpu_upload.cpp:24](source/gpu_upload.cpp#L24) | `vkWaitForFences` | Ожидание до освобождения staging и upload command buffer |
| image_barrier | [source/gpu_upload.cpp:117](source/gpu_upload.cpp#L117) | `VkImageMemoryBarrier barrier` | Переходы UNDEFINED -> TRANSFER_DST -> SHADER_READ |
| image_copy | [source/gpu_upload.cpp:124](source/gpu_upload.cpp#L124) | `vkCmdCopyBufferToImage` | Копирование RGBA из staging в изображение |
| image_view | [source/gpu_upload.cpp:131](source/gpu_upload.cpp#L131) | `VkImageViewCreateInfo vi` | Представление цветного изображения, один mip |
| sampler | [source/gpu_upload.cpp:136](source/gpu_upload.cpp#L136) | `VkSamplerCreateInfo si` | Linear/repeat, без anisotropy, LOD=0 |
| ubo_allocate | [source/renderer.cpp:57](source/renderer.cpp#L57) | `const auto alignment=` | Alignment, stride, range, лимиты и один общий model buffer |
| ubo_descriptor | [source/renderer.cpp:73](source/renderer.cpp#L73) | `VkDescriptorBufferInfo bufferInfo{modelBuffer.handle,0,sizeof(ModelGpu)}` | Range dynamic descriptor равен размеру одной записи |
| ubo_write | [source/renderer.cpp:277](source/renderer.cpp#L277) | `i*modelStride)) return false` | Запись полезных байт в каждый выровненный слот после frame fence |
| dynamic_bind | [source/renderer.cpp:389](source/renderer.cpp#L389) | `litLayout,0,2,bound,1,&dynamicOffset` | pDynamicOffsets выбирает запись модели перед draw |
| material_sets | [source/renderer.cpp:72](source/renderer.cpp#L72) | `for(size_t material=0;material<3;++material)` | Один set на материал, общий model VkBuffer |
| material_combo | [source/controls.cpp:38](source/controls.cpp#L38) | `ImGui::Combo("Material"` | Независимый выбор материала выбранного объекта |
| sampling_push | [source/renderer.cpp:392](source/renderer.cpp#L392) | `SamplingPush push` | 32 байта fragment push constants: флаг, время, сила |
| effect_time | [source/application.cpp:18](source/application.cpp#L18) | `if(scene.effectPlaying)` | Накопление dt*speed независимо от анимации геометрии |
| fragment_main | [shaders/lighting.glsl:51](shaders/lighting.glsl#L51) | `void main()` | Albedo -> эффект -> Блинн-Фонг -> emissive -> вывод |
| multi_sample | [shaders/lighting.glsl:61](shaders/lighting.glsl#L61) | `vec3 sampleA=texture` | Две texture() одного albedo sampler по uvA и uvB |
| sampling_mix | [shaders/lighting.glsl:64](shaders/lighting.glsl#L64) | `vec3 modulation=` | Смешивание и модуляция от времени, регулировка силы |
| specular_mask | [shaders/lighting.glsl:25](shaders/lighting.glsl#L25) | `object.specular.rgb*specularMask` | Маска умножает только specular; shininess отдельно |
| emissive_add | [shaders/lighting.glsl:77](shaders/lighting.glsl#L77) | `result+=texture(emissiveSampler` | Эмиссия прибавляется после света на исходных UV |
| black_load | [source/renderer.cpp:325](source/renderer.cpp#L325) | `const char* files[]={"black.png"` | Black загружается общим путём; отсутствие optional-карт разрешено |
| fallback_bind | [source/renderer.cpp:71](source/renderer.cpp#L71) | `const size_t maps[3][3]` | Отсутствующие specular/emissive привязаны к общему black |
| prepare_fence | [source/graphics_internal.cpp:781](source/graphics_internal.cpp#L781) | `if(graphicsFailed || vkWaitForFences` | Один кадр в работе: ожидание перед CPU write и заменой ресурсов |
| cleanup | [source/renderer.cpp:344](source/renderer.cpp#L344) | `void shutdown()` | Device idle, освобождение pipelines, descriptors, textures, buffers и upload pool |
| cmake_target | [CMakeLists.txt:6](CMakeLists.txt#L6) | `add_executable(${PROJECT_NAME}` | Самостоятельная цель, исходники decoder и GPU upload |
| cmake_shaders | [CMakeLists.txt:94](CMakeLists.txt#L94) | `macro(compile_shader` | glslc пишет SPIR-V только в build-каталог |
