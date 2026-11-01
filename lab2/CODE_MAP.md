# Карта итогового кода

Номера получены программно из текущих файлов. После правок выполните `python tools/update_code_map.py`. Поисковая строка остаётся полезной при смещении строк. Ссылки относительные, опубликованная ветка не предполагается.

| Что показать | Файл и актуальные строки | Что объяснить |
| --- | --- | --- |
| Процедурные нормали цилиндра | [source/geometry.cpp:4–23](source/geometry.cpp#L4) — `Mesh generateCylinder()` | 202 записи: боковина и крышки имеют разные нормали; 600 индексов, шов замыкается по модулю 50. |
| Настоящая матрица V | [source/scene.cpp:17–21](source/scene.cpp#L17) — `Mat4 cameraView(` | Обратная T*Rz*Ry*Rx: Rx^-1*Ry^-1*Rz^-1*T^-1. |
| FOV и Vulkan projection | [source/scene.cpp:22–25](source/scene.cpp#L22) — `Mat4 perspective(` | Градусы → радианы, depth [0,1], один переворот Y. |
| Изменение камеры через ImGui | [source/controls.cpp:12–19](source/controls.cpp#L12) — `if(ImGui::CollapsingHeader("Camera",` | XYZ/Euler/FOV/orthographic/Reset camera. |
| Model / normal matrix / материал | [source/scene.cpp:39–59](source/scene.cpp#L39) — `void buildUniforms(` | M=T*R*S, normal=R*S^-1, VP=P*V; параметры материала и флаги. |
| CPU-параметры сцены и ламп | [source/scene.hpp:63–78](source/scene.hpp#L63) — `struct Scene {` | Именно эти значения меняет UI; начальные intensities, toggles и векторы. |
| Материал и shininess в UI | [source/controls.cpp:20–38](source/controls.cpp#L20) — `if(ImGui::CollapsingHeader("Model and material"` | Albedo, specular color, shininess, scale, анимация. |
| std140 Scene | [source/scene.hpp:83–88](source/scene.hpp#L83) — `struct alignas(16) SceneGpu` | 160 байт; offsets сверяются static_assert и SPIR-V. |
| std140 Model | [source/scene.hpp:89–93](source/scene.hpp#L89) — `struct alignas(16) ModelGpu` | 176 байт; две mat4 и материал, никаких C++ bool. |
| Создание двух descriptor sets | [source/renderer.cpp:54–84](source/renderer.cpp#L54) — `bool createDescriptors()` | Два layouts, pool 2 UBO + 2 SSBO, allocation и pipeline layouts. |
| Запись buffer descriptors | [source/renderer.cpp:48–53](source/renderer.cpp#L48) — `void writeDescriptor(` | VkDescriptorBufferInfo и VkWriteDescriptorSet с реальным buffer/range. |
| Загрузка UBO/SSBO | [source/renderer.cpp:261–272](source/renderer.cpp#L261) — `bool uploadScene(` | buildUniforms/pack → ensureStorage → writeBuffer. |
| Map/flush/unmap | [source/renderer.cpp:29–39](source/renderer.cpp#L29) — `bool writeBuffer(` | Проверки VkResult и vmaFlushAllocation для noncoherent памяти. |
| Рост capacity и защита in-flight SSBO | [source/renderer.cpp:86–97](source/renderer.cpp#L86) — `bool ensureStorage(` | Новый буфер, update descriptor, destroy старого только после fence; предел устройства. |
| Добавление/удаление point | [source/controls.cpp:50–68](source/controls.cpp#L50) — `if(ImGui::CollapsingHeader("Point lights"` | Вектор, устойчивый PushID, erase после использования ссылки, Clear/Add. |
| Добавление/удаление spot и полууглы | [source/controls.cpp:69–91](source/controls.cpp#L69) — `if(ImGui::CollapsingHeader("Spot lights"` | Независимый список; inner≤outer<90°, направление и intensity. |
| CPU → GPU параметры прожектора | [source/scene.cpp:65–73](source/scene.cpp#L65) — `std::vector<SpotGpu> packSpots(` | Normalize с fallback, enabled через intensity=0, косинусы углов. |
| Когда можно менять GPU-данные | [source/graphics_internal.cpp:780–818](source/graphics_internal.cpp#L780) — `FrameData prepare()` | Ждём fence до записи buffers/sets; пустой кадр при свёрнутой поверхности. |
| Submit/present и сброс fence | [source/graphics_internal.cpp:820–864](source/graphics_internal.cpp#L820) — `bool submitAndPresent()` | Fence сбрасывается перед submit; rebuild откладывается до prepare. |
| Защита от нулевого swapchain extent | [source/graphics_internal.cpp:271–424](source/graphics_internal.cpp#L271) — `bool rebuildSwapchain(` | Caps snapshot, нулевой extent откладывается; пересоздаются depth/framebuffers/semaphores. |
| Привязка двух sets и draw цилиндра | [source/renderer.cpp:332–368](source/renderer.cpp#L332) — `bool render(const Scene&` | vkCmdBindDescriptorSets(...,0,2,sets,...), lit draw, затем помощники. |
| Блинн–Фонг: N/L/V/H | [shaders/lab2.frag:13–22](shaders/lab2.frag#L13) — `vec3 evaluateBlinnPhong(` | NdotL, half vector L+V, pow(NdotH,shininess), запрет блика с обратной стороны. |
| Направленный источник | [shaders/lab2.frag:23–26](shaders/lab2.frag#L23) — `vec3 evaluateDirectionalLight(` | L=-direction, без позиции и затухания. |
| Где прибавляется ambient | [shaders/lab2.frag:51–51](shaders/lab2.frag#L51) — `vec3 result=scene.ambient` | Один раз, до циклов по источникам. |
| Где суммируется весь свет | [shaders/lab2.frag:46–56](shaders/lab2.frag#L46) — `void main()` | Ambient → directional → point loop → spot loop → displayColor. |
| Закон обратных квадратов | [shaders/lab2.frag:27–27](shaders/lab2.frag#L27) — `float inverseSquare(` | 1/max(distanceSquared,0.01), ограничение только около нуля. |
| Освещение точечным источником | [shaders/lab2.frag:28–32](shaders/lab2.frag#L28) — `vec3 evaluatePointLight(` | Разность мировых позиций определяет L и d²; цвет × intensity × 1/r². |
| Мягкие края / равные углы | [shaders/lab2.frag:33–37](shaders/lab2.frag#L33) — `float spotFactor(` | smoothstep(cosOuter,cosInner,cosTheta); step при совпадении границ. |
| Прожектор и знак направления | [shaders/lab2.frag:38–45](shaders/lab2.frag#L38) — `vec3 evaluateSpotLight(` | dot(-L,direction); прямой свет умножается на attenuation и cone. |
| Мировые позиции и нормали | [shaders/lab2.vert:11–17](shaders/lab2.vert#L11) — `void main()` | Model и normalMatrix; worldNormal нормализуется после интерполяции во fragment. |
| Unlit маркеры источников | [source/renderer.cpp:282–296](source/renderer.cpp#L282) — `void drawMarkers(` | Те же позиции/цвета; marker pipeline и отдельные push constants. |
| Процедурные конусы | [source/geometry.cpp:30–48](source/geometry.cpp#L30) — `std::vector<LineVertex> generateSpotCones(` | Ось, устойчивый базис, радиус length*tan(angle), два кольца. |
| Unlit vertex shader | [shaders/unlit.vert:8–11](shaders/unlit.vert#L8) — `void main()` | VP*markerModel*position; постоянный цвет, без normal/material. |
| Unlit fragment shader | [shaders/unlit.frag:7–7](shaders/unlit.frag#L7) — `void main()` | Только displayColor(constantColor), без evaluateBlinnPhong. |
| Pipeline, атрибуты, depth test, линии | [source/renderer.cpp:137–258](source/renderer.cpp#L137) — `bool createPipeline(` | Lit/cube/line pipelines, lineWidth=1, depth writes отключены только у линий. |
| Линейный цвет и sRGB | [shaders/display.glsl:2–6](shaders/display.glsl#L2) — `vec3 displayColor(` | Одно кодирование: аппаратное у sRGB attachment или ручное у UNORM. |
| Компиляция GLSL при сборке | [CMakeLists.txt:89–104](CMakeLists.txt#L89) — `macro(compile_shader` | glslc --target-env=vulkan1.1; outputs только в собственном build/shaders. |
| Освобождение ресурсов сцены | [source/renderer.cpp:317–331](source/renderer.cpp#L317) — `void shutdown()` | Device idle, pipelines/layouts/pool, buffers, безопасно при частичной инициализации. |
| Освобождение Vulkan-каркаса | [source/graphics_internal.cpp:737–765](source/graphics_internal.cpp#L737) — `void shutdown()` | ImGui backend, sync, framebuffers, depth, swapchain, allocator/device/instance. |
| Сценарий динамических проверок | [source/application.cpp:21–50](source/application.cpp#L21) — `void smokeStep(` | 0/33/удаления, нулевые векторы/равные углы, camera/scale, reset. |
