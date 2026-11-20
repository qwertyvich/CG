# Что добавлено поверх ЛР3

ЛР3 уже умела считать свет по нормали поверхности и читать текстуры. Это отвечает на вопрос «насколько ярко поверхность отражает источник», но не проверяет, не стоит ли на пути света другой объект. Поэтому обратная сторона цилиндра темнела от N·L, а цилиндр ещё не закрывал свет для пола.

В ЛР4 появилась **shadow map** — текстура расстояний в форме глубины, увиденная из положения света. Объект, закрывающий свет, называют caster, а поверхность, на которой видна его тень, — receiver. Цилиндры и пол участвуют в обоих проходах. В `geometry.cpp::generateFloor` пол состоит из двух настоящих треугольников с нормалью вверх и UV. `renderer.cpp::drawModels` использует диапазон сетки: 600 индексов цилиндра или 6 индексов пола. Все три модели используют один dynamic UBO, с отдельными выровненными смещениями. Геометрия по-прежнему попадает в device-local буферы через staging из ЛР3.

## Как устроен кадр

1. `application::update` обрабатывает UI, время и анимацию на CPU.
2. `graphics::internal::prepare` ждёт fence предыдущего кадра. В работе один кадр: после этого можно менять общие uniform/SSBO и descriptors.
3. `renderer::render` вызывает `uploadScene` и `buildShadows`: записывает фактические model matrices, нормальные матрицы и две light VP в GPU-буферы. Одна и та же model запись используется во всех проходах. Observer camera влияет на основной SceneGpu, но не на light VP.
4. `shadows::record` заполняет карту солнца, затем отдельную карту выбранного spot. Это **offscreen rendering**: результатом является depth-текстура, а не цвет окна. Активная карта перерисовывается каждый кадр, даже если анимация остановлена. Неактивная карта в первый кадр очищается и переводится в читаемый layout, чтобы descriptors оставались валидны.
5. Основной проход вызывает общий `drawModels`: lit/sampling pipelines читают карты в `shaders/shadows.glsl`. После shadow passes viewport, scissor, pipeline, sets и push constants выставляются для основного прохода заново.
6. Отдельный прежний render pass рисует ImGui; submit/present выводит кадр. Перед уничтожением ресурсов программа дожидается GPU.

## Что видит камера света

`scene.cpp::lookAt` переводит мир в систему координат наблюдателя света. Eye — положение этого наблюдателя, target — точка, на которую он смотрит. Вперёд в этой системе направлен −Z. Около вертикального направления функция выбирает другой опорный вектор, чтобы векторное произведение не выродилось. Нулевое направление света заменяется на (0,−1,0).

Для солнца лучи параллельны. В `buildShadows` eye = target − normalize(direction) × 25, а projection — ортографическая. Она не уменьшает далёкие предметы. `Shadow half extent=7` означает полную ширину и высоту 14 единиц мира. Центр фиксирован около сцены; near/far 0.1–60.

Для прожектора лучи расходятся из одной позиции. `lookAt(position, position + normalized(direction))` сочетается с перспективной `lightPerspective`. Полный FOV равен **2 × Outer half angle**, aspect=1, near/far 0.1–50. Inner и outer из ЛР3 остаются полууглами. Почти нулевой outer отключает карту, а projection защищена от вырождения.

Light VP = projection × view. В `shadow.vert` вычисляется `lightVP × object.model × position`. Отдельный **depth-only pipeline** содержит только vertex shader, не считает материалы и не пишет цвет. Depth test/write сохраняют ближайшую к свету поверхность. `shadows.cpp::initializePipeline` указывает `VkPipelineRenderingCreateInfo`, depth-формат, ноль цветовых attachments и `renderPass=VK_NULL_HANDLE`.

## Dynamic Rendering и синхронизация

`graphics_internal.cpp::initialize` действительно запрашивает и включает feature dynamicRendering, а не только меняет номер SDK. В этой реализации выбран core Vulkan 1.3. В `shadows::record` вызываются загруженные core `vkCmdBeginRendering`/`vkCmdEndRendering`, без shadow VkRenderPass или VkFramebuffer. Depth attachment очищается в 1 и сохраняется с STORE.

Справка API: [Vulkan 1.3 features](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceVulkan13Features.html), [VkPipelineRenderingCreateInfo](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineRenderingCreateInfo.html).

Dynamic Rendering задаёт attachments непосредственно в команде; он **не заменяет барьеры**. `shadows.cpp::transition` переводит первое состояние UNDEFINED в depth attachment. После записи EARLY/LATE_FRAGMENT_TESTS + DEPTH_STENCIL_ATTACHMENT_WRITE синхронизируются с FRAGMENT_SHADER + SHADER_READ, layout становится DEPTH_STENCIL_READ_ONLY_OPTIMAL. Следующее использование выполняет обратную зависимость sampled read → depth read/write. Descriptor содержит тот же read-only layout. Fence и единственный кадр в работе защищают общие карты и UBO от конкурентного использования.

## Как карта превращается в тень

`shadowVisibility` в `shadows.glsl` получает world position и переводит её в **clip** координаты света: lightVP × vec4(world,1). После деления xyz/w получается **NDC**. Проверка w>0 особенно важна для spot: точка позади камеры света не должна читать карту.

В Vulkan NDC depth уже находится в [0,1]. Поэтому только XY переводятся из [−1,1] в **UV** [0,1]; Z дополнительно не преобразуется. Проекции света уже один раз переворачивают Y, второй переворот в GLSL не нужен. Если UV или Z вне диапазона, принимается visibility=1: карта там не содержит данных о тени. Для spot его прежний cone factor отдельно выключает свет вне конуса.

`sampler2DShadow` — **comparison sampler**. В `initializeImages` его VkSampler создаётся с compareEnable=TRUE и LESS. `texture(map, vec3(uv, receiverDepth))` сравнивает глубину получателя с сохранённой ближайшей глубиной. Результат — **visibility**: 1 означает, что свет виден; 0 означает, что закрыт. Выбрана NEAREST фильтрация, поэтому промежуточные значения PCF не обещаются.

В `lighting.glsl::main` sunVisibility умножает только diffuse+specular солнца. Выбранный spot получает свою visibility и свою карту; индекс для GPU заново определяется по стабильному ID. Point lights и остальные spot не читают эти карты. Ambient добавляется один раз, эмиссия — независимо в конце. Поэтому тень не должна делать самосветящуюся надпись чёрной и не должна отключать свет другого источника.

## Покрытие, разрешение и bias

Разрешение карты фиксировано: 2048² текселей. Покрытие задаёт, сколько единиц мира эти тексели описывают. При увеличении half extent те же 2048 текселей распределяются по большей области: тень покрывает больше мира, но её ступеньки в мире крупнее. Слишком маленькое покрытие обрезает данные карты; вне них visibility=1. Это видимая граница метода, а не область действия солнца.

**Depth bias** немного сдвигает сохранённую глубину при rasterization. Он компенсирует ошибки точности при сравнении поверхности с её собственной картой. Мелкие ложные полоски/пятна называют **shadow acne**. Слишком сильный сдвиг отрывает тень от объекта — **peter-panning**. `vkCmdSetDepthBias(constantFactor,0,slopeFactor)` применяется перед draw: constant зависит от представления depth, slope учитывает наклон глубины. Это не метры мира. Clamp=0 не требует feature depthBiasClamp. Receiver bias дополнительно не используется.

Настройки по умолчанию: constant=1.25, slope=1.75. Обе карты используют одинаковые значения. Они не меняют нормали, интенсивность, near/far или разрешение. Влияние bias показывается на тенях; конкретная заметность зависит от ракурса/формата/GPU.

## Где что лежит

| Файл | Назначение |
|---|---|
| `source/scene.hpp/.cpp` | Состояние сцены, структуры CPU/GLSL, матрицы, стабильный выбор spot, presets |
| `source/geometry.cpp` | Цилиндр варианта 8, пол, маркеры и конусы |
| `source/shadows.hpp/.cpp` | Depth image/view/sampler, depth pipeline, Dynamic Rendering и barriers |
| `source/renderer.cpp` | Descriptors, GPU upload, mesh ranges, общий draw моделей, порядок проходов |
| `source/graphics_internal.cpp` | Проверка/включение Vulkan features, fence, swapchain и ImGui |
| `source/controls.cpp` | Все подписи UI и действия |
| `shaders/shadow.vert` | Геометрия из камеры света |
| `shaders/shadows.glsl` | Shadow resources, преобразование координат и comparison sampling |
| `shaders/lighting.glsl` | Независимые вклады света, материалы/эмиссия из ЛР3 |
| `tests/scene_tests.cpp` | Численные проверки матриц, геометрии и выбора света |
| `tools/verify_runtime.py`, `capture_demos.py` | Запуск проверок и реальные кадры |

Точные строки и поисковые функции находятся в [CODE_MAP.md](CODE_MAP.md); они генерируются по итоговым исходникам.
