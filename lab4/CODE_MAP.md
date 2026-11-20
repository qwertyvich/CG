# Карта кода ЛР4

Строки вычислены из текущих исходников скриптом `python lab4/tools/update_code_map.py`. После правок запустите его повторно. Поисковые строки позволяют найти место и при смещении номеров.

| Что показать | Файл / точная строка | Функция или поисковая строка | Что сказать |
|---|---|---|---|
| Проверка фактической поддержки | [source/graphics_internal.cpp:465](source/graphics_internal.cpp#L465) | `VkPhysicalDeviceFeatures2 features` | Query features2/pNext и аудит device API/расширения для всех GPU. |
| Включение Dynamic Rendering | [source/graphics_internal.cpp:477](source/graphics_internal.cpp#L477) | `.set_required_features_13(required)` | vk-bootstrap запрашивает dynamicRendering=true при vkCreateDevice; выбран core 1.3. |
| Core команды | [source/graphics_internal.cpp:505](source/graphics_internal.cpp#L505) | `context.beginRendering=reinterpret_cast` | vkGetDeviceProcAddr загружает и проверяет core entry points. KHR fallback не реализован. |
| Выбор depth-формата | [source/shadows.cpp:31](source/shadows.cpp#L31) | `constexpr auto needed=` | Формат должен поддерживать optimal attachment и sampled image; дополнительно image-format query. |
| Создание depth image/view | [source/shadows.cpp:43](source/shadows.cpp#L43) | `VkImageCreateInfo image` | Две независимые DEVICE_LOCAL карты, DEPTH_ATTACHMENT\|SAMPLED, depth-aspect view. |
| Comparison sampler LESS | [source/shadows.cpp:55](source/shadows.cpp#L55) | `.compareEnable=VK_TRUE` | Отдельный sampler: compareEnable, LESS, NEAREST, белый border. |
| Shadow descriptor bindings | [source/renderer.cpp:79](source/renderer.cpp#L79) | `writeDescriptor(0,3,` | Set 0: binding 3 ShadowGpu; binding 4/5 — sun/spot combined image samplers. |
| sampler2DShadow в GLSL | [shaders/shadows.glsl:6](shaders/shadows.glsl#L6) | `uniform sampler2DShadow sunShadowSampler` | Самpler ресурсы совпадают с CPU binding 4 и 5. |
| Depth-only pipeline | [source/shadows.cpp:88](source/shadows.cpp#L88) | `VkPipelineRenderingCreateInfo rendering` | Ноль цветовых attachments, depth format; ниже stageCount=1, renderPass=null. |
| Dynamic Rendering begin/end | [source/shadows.cpp:109](source/shadows.cpp#L109) | `c.beginRendering(cmd,&rendering)` | Загруженный vkCmdBeginRendering; затем bind/draw и endRendering; без shadow framebuffer. |
| Light VP для depth-only vertex | [source/shadows.cpp:116](source/shadows.cpp#L116) | `vkCmdPushConstants(cmd,layout,VK_SHADER_STAGE_VERTEX_BIT` | Отдельный shadow layout, vertex push 64 bytes; sampling push имеет другой layout/stage. |
| Shadow/main draws с dynamic offsets | [source/renderer.cpp:311](source/renderer.cpp#L311) | `const uint32_t dynamicOffset=` | Одна модельная запись UBO выбирается для каждого объекта и каждого прохода. |
| Диапазоны цилиндра/пола | [source/renderer.cpp:318](source/renderer.cpp#L318) | `vkCmdDrawIndexed(cmd,mesh.indexCount` | indexCount/firstIndex/vertexOffset берутся по MeshKind, пол не рисуется как 600 индексов. |
| Сборка общей геометрии | [source/renderer.cpp:360](source/renderer.cpp#L360) | `meshRanges[1]=` | Floor имеет отдельный firstIndex и vertexOffset в общем device-local buffer. |
| Image barriers | [source/shadows.cpp:12](source/shadows.cpp#L12) | `void transition(` | UNDEFINED отдельно; EARLY/LATE depth writes ↔ fragment shader read. Descriptor layout совпадает. |
| LookAt и устойчивый базис | [source/scene.cpp:30](source/scene.cpp#L30) | `Mat4 lookAt(` | Вид вдоль -Z; альтернативный up около вертикали и fallback нулевого направления. |
| World light matrices | [source/scene.cpp:64](source/scene.cpp#L64) | `ShadowGpu buildShadows(` | Солнце и spot имеют собственные VP; observer camera не участвует. |
| Ортографическое покрытие солнца | [source/scene.cpp:67](source/scene.cpp#L67) | `g.sunViewProjection=multiply` | Ortho half extent в world units; eye=target-direction*25; near/far 0.1/60. |
| Перспектива spot: FOV=2*outer | [source/scene.cpp:76](source/scene.cpp#L76) | `const float fullFov=2*` | Выбранный enabled spot с валидным outer использует perspective * LookAt; near/far 0.1/50. |
| Динамический raster depth bias | [source/shadows.cpp:115](source/shadows.cpp#L115) | `vkCmdSetDepthBias(` | Constant/slope из UI, clamp=0, положительный bias при обычной LESS depth. |
| Управление bias/coverage | [source/controls.cpp:32](source/controls.cpp#L32) | `ImGui::SliderFloat("Shadow half extent"` | Обязательные настройки UI: половина покрытия и два depth bias. |
| Преобразование depth vertex | [shaders/shadow.vert:7](shaders/shadow.vert#L7) | `gl_Position=` | lightViewProjection * model * position, один vertex shader. |
| Clip → NDC → UV | [shaders/shadows.glsl:12](shaders/shadows.glsl#L12) | `vec3 ndc=clip.xyz/clip.w` | w>0; XY scale/bias; Vulkan Z уже 0..1; Y не переворачивается повторно. |
| Выход за карту | [shaders/shadows.glsl:15](shaders/shadows.glsl#L15) | `if(any(lessThan(uv` | UV/depth bounds проверяются до sampling; вне карты visibility=1. |
| Сравнение глубины | [shaders/shadows.glsl:17](shaders/shadows.glsl#L17) | `return texture(map,vec3(uv,ndc.z))` | Comparison sampler возвращает visibility: LESS успех = свет виден. |
| Visibility только для солнца | [shaders/lighting.glsl:76](shaders/lighting.glsl#L76) | `result+=sunVisibility*` | Shadow ограничивает directional diffuse+specular, не общий результат. |
| Visibility только для выбранного spot | [shaders/lighting.glsl:79](shaders/lighting.glsl#L79) | `float visibility=shadows.flags.y` | GPU packed index вычислен по stable ID; остальные spot/point независимы. |
| Независимые ambient/emissive | [shaders/lighting.glsl:82](shaders/lighting.glsl#L82) | `result+=texture(emissiveSampler` | Emissive прибавляется после света, ambient — до него без visibility. |
| Пол: два треугольника | [source/geometry.cpp:24](source/geometry.cpp#L24) | `Mesh generateFloor()` | 4 вершины, 6 индексов, нормаль +Y, UV; настоящий receiver. |
| Третья модель | [source/scene.cpp:48](source/scene.cpp#L48) | `ModelState defaultModel(` | Пол y=-1.16, под цилиндрами y_min=-1.15, без общей анимации. |
| Выбор spot по ID | [source/scene.cpp:57](source/scene.cpp#L57) | `int selectedShadowSpot(` | Каждый раз поиск актуального индекса; не хранится ссылка в vector. |
| Удаление/clear/re-add/reset spot | [source/scene.cpp:61](source/scene.cpp#L61) | `void reconcileShadowSpot(` | Первый оставшийся либо -1; новый spot выбирается при отсутствии прежнего. |
| UI erase выбранного spot | [source/controls.cpp:140](source/controls.cpp#L140) | `if(remove) s.spots.erase` | Erase после последнего обращения к ссылке; reconcile выполняется в конце UI. |
| Валидная неактивная карта | [source/shadows.cpp:101](source/shadows.cpp#L101) | `if(!active && map.initialized)` | Первый кадр даже неактивной карты: clear+layout; позже pass пропускается. |
| Порядок проходов кадра | [source/renderer.cpp:422](source/renderer.cpp#L422) | `shadows::record(cmd,i,` | Sun затем spot до vkCmdBeginRenderPass основного окна; карты каждый активный кадр. |
| Ожидание GPU до upload | [source/graphics_internal.cpp:811](source/graphics_internal.cpp#L811) | `vkWaitForFences(` | В prepare ожидается fence одного кадра; затем renderer пишет общие UBO/SSBO. |
| Обычный и sampling shader | [source/renderer.cpp:309](source/renderer.cpp#L309) | `if(!shadowPass) vkCmdBindPipeline` | Выбор base/sampling pipeline; оба включают общий lighting.glsl с shadow logic. |
| Освобождение ресурсов | [source/renderer.cpp:390](source/renderer.cpp#L390) | `shadows::shutdown();` | Renderer ждёт GPU; уничтожает shadow pipeline/layout/maps, descriptors, buffers и textures. |
| Сборка новых shaders | [CMakeLists.txt:123](CMakeLists.txt#L123) | `compile_shader(shadow.vert)` | Автоматическая GLSL compilation + tracking shadows.glsl, свой target cg-lab4. |
