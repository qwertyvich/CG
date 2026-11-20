# Отличия лабораторных 3 и 4 по коду

Главное изменение ЛР4 — проверка препятствий между поверхностью и источником света. ЛР3 уже рассчитывала освещение и читала текстуры. ЛР4 дополнительно строит две карты глубины и использует их при расчёте вклада солнца и выбранного прожектора.

Ниже готовый текст от первого лица на примерно 15 минут вместе с открытием файлов и показом указанных строк. Инструкции «Открыть» выполняются в редакторе; основной текст можно говорить преподавателю. Материалы, камеру, SSBO, normal matrix и dynamic UBO следует называть сохранённой основой ЛР3. Номера строк обновляются командой `python lab4/tools/update_code_map.py`.

## 0:00–1:00 Что изменилось в расчёте света

Открыть ЛР3: <!--code:lab3-lighting-->[../lab3/shaders/lighting.glsl:74](../lab3/shaders/lighting.glsl#L74)<!--/code-->. Затем ЛР4: <!--code:sun-visibility-->[shaders/lighting.glsl:76](shaders/lighting.glsl#L76)<!--/code-->. В обоих файлах это функция `main`.

В третьей лабораторной я рассчитывал цвет поверхности по модели Блинна–Фонга. Учитывались направление света, нормаль, положение камеры и свойства материала. Также уже были diffuse, specular, ambient и emissive. Однако расчёт не проверял, находится ли между источником и поверхностью другой объект.

В `lab3/shaders/lighting.glsl`, в функции `main`, результат `evaluateDirectionalLight` сразу прибавлялся к цвету. Поэтому освещённость зависела от ориентации поверхности, но цилиндр не закрывал солнце для пола.

В четвёртой работе перед этим вкладом появился множитель `sunVisibility`. Он показывает, виден ли источник из данной точки. Чтобы получить этот множитель, добавлены карты глубины, камеры света и дополнительные проходы. Дальше я покажу, где именно каждый из этих этапов находится в коде.

## 1:00–2:00 Включение Dynamic Rendering

Открыть функцию `graphics::internal::initialize`: ЛР3 <!--code:lab3-instance-->[../lab3/source/graphics_internal.cpp:433](../lab3/source/graphics_internal.cpp#L433)<!--/code-->, ЛР4 <!--code:feature-query-->[source/graphics_internal.cpp:465](source/graphics_internal.cpp#L465)<!--/code-->, затем <!--code:feature-enable-->[source/graphics_internal.cpp:477](source/graphics_internal.cpp#L477)<!--/code--> и <!--code:entry-points-->[source/graphics_internal.cpp:505](source/graphics_internal.cpp#L505)<!--/code-->.

В файле `graphics_internal.cpp` функция `initialize` создаёт Vulkan instance и device. В третьей работе она запрашивала Vulkan 1.1. В четвёртой я использую core Vulkan 1.3, поскольку проходы карт теней выполняются через Dynamic Rendering.

При этом недостаточно просто поменять номер версии. Здесь `VkPhysicalDeviceFeatures2` вместе со структурой dynamic rendering проверяет реальную поддержку функции устройством. Затем в структуре требуемых Vulkan 1.3 features выставляется `dynamicRendering=VK_TRUE`. Метод `set_required_features_13` передаёт это требование в vk-bootstrap для выбора и создания устройства.

Ниже через `vkGetDeviceProcAddr` загружаются `vkCmdBeginRendering` и `vkCmdEndRendering`, а указатели проверяются. Ими будет записываться новый depth-проход. Основной экранный проход и ImGui сохранены на прежних render passes.

## 2:00–3:00 Новые данные сцены

Открыть `scene.hpp`: ЛР3 <!--code:lab3-models-->[../lab3/source/scene.hpp:74](../lab3/source/scene.hpp#L74)<!--/code-->, ЛР4 <!--code:scene-models-->[source/scene.hpp:86](source/scene.hpp#L86)<!--/code-->, <!--code:shadow-settings-->[source/scene.hpp:79](source/scene.hpp#L79)<!--/code--> и <!--code:shadow-gpu-->[source/scene.hpp:121](source/scene.hpp#L121)<!--/code-->.

В `scene.hpp` описано состояние сцены. В ЛР3 массив `models` содержал два цилиндра. Теперь моделей три: добавлен пол. Поле `MeshKind` сообщает рендереру, какую геометрию рисовать для каждой записи — цилиндр или плоскость.

Структура `ShadowSettings` хранит настройки на CPU: включены ли тени солнца и прожектора, половину размера покрытия, constant и slope bias, а также ID выбранного прожектора. Когда я меняю настройки в интерфейсе, сначала меняются именно эти значения.

Отдельная структура `ShadowGpu` предназначена для передачи в шейдер. В ней две матрицы — `sunViewProjection` и `spotViewProjection` — и вектор флагов. Флаги показывают, какие карты активны и к какому индексу списка spot относится вторая карта. Прежние структуры данных камеры и материала сохранены; данные теней добавлены отдельным UBO.

## 3:00–4:00 Пол и общая геометрия

Открыть `geometry.cpp::generateFloor`: <!--code:floor-->[source/geometry.cpp:24](source/geometry.cpp#L24)<!--/code-->. Затем `scene.cpp::defaultModel`: <!--code:floor-model-->[source/scene.cpp:48](source/scene.cpp#L48)<!--/code--> и `renderer.cpp::initialize`: <!--code:mesh-ranges-->[source/renderer.cpp:360](source/renderer.cpp#L360)<!--/code-->.

В `geometry.cpp` появилась функция `generateFloor`. Она создаёт четыре вершины с нормалью вверх и UV, а шесть индексов образуют два треугольника. Старый `generateCylinder` сохранён: цилиндр по-прежнему имеет пятьдесят сегментов окружности.

В `scene.cpp` функция `defaultModel` задаёт положение и материал каждого объекта. Пол размещён на высоте минус 1.16, чуть ниже нижних точек начальных цилиндров, и получает светлый материал.

В `renderer.cpp`, в `initialize`, вершины и индексы пола добавляются в общие буферы геометрии. Массив `meshRanges` хранит диапазоны: у цилиндра шестьсот индексов, у пола шесть. Геометрия цилиндра хранится один раз и используется двумя моделями с разными матрицами. Пол участвует в настоящем depth-проходе и получает тень в основном шейдере.

## 4:00–5:30 Матрицы камер света

Открыть `scene.cpp`: `lookAt` <!--code:lookat-->[source/scene.cpp:30](source/scene.cpp#L30)<!--/code-->, `lightPerspective` <!--code:light-perspective-->[source/scene.cpp:41](source/scene.cpp#L41)<!--/code-->, `buildShadows` <!--code:light-matrices-->[source/scene.cpp:64](source/scene.cpp#L64)<!--/code-->.

В третьей работе была камера наблюдателя. В четвёртой дополнительно нужны камеры света, чтобы построить карты глубины. Функция `lookAt` создаёт матрицу вида из положения `eye` и точки `target`, на которую направлен взгляд. Для устойчивости около вертикального направления выбирается другой опорный вектор.

Функция `buildShadows` собирает две итоговые матрицы. Для солнца используется `orthographic` вместе с `lookAt`. Ортографическая проекция подходит для параллельных лучей: дальние объекты в ней не уменьшаются. Положение виртуальной камеры вычисляется как `target - direction * 25`. Параметр `halfExtent` задаёт половину ширины и высоты покрытия карты в единицах мира.

Для прожектора используется перспективная проекция из `lightPerspective`. Положение камеры совпадает с положением spot, а взгляд направлен вдоль его `direction`. Полный FOV равен двум внешним полууглам: `2 * outerDegrees`. Например, при outer в 38 градусов камера карты имеет угол 76 градусов.

`buildShadows` также решает, нужна ли карта сейчас: источник должен быть включён, иметь положительную интенсивность и разрешённую тень. Слишком узкий spot пропускается. Камера наблюдателя здесь не используется, поэтому её движение не переносит тень относительно мира.

## 5:30–7:00 Создание карт глубины

Открыть новый файл `shadows.cpp`, функцию `initializeImages`: <!--code:shadow-images-function-->[source/shadows.cpp:27](source/shadows.cpp#L27)<!--/code-->. Показать <!--code:depth-format-->[source/shadows.cpp:31](source/shadows.cpp#L31)<!--/code-->, <!--code:depth-image-->[source/shadows.cpp:43](source/shadows.cpp#L43)<!--/code--> и <!--code:compare-sampler-->[source/shadows.cpp:55](source/shadows.cpp#L55)<!--/code-->.

В ЛР3 были изображения материалов: albedo, specular и emissive. В ЛР4 к ним добавились изображения другого назначения — карты глубины света. Их создаёт новая функция `initializeImages` в `shadows.cpp`.

Сначала она проверяет depth-формат: GPU должен уметь использовать изображение и как depth attachment, и как текстуру для чтения в шейдере. Затем создаются две независимые карты размером 2048 на 2048: одна для солнца, другая для выбранного прожектора. Две карты нужны, потому что источники смотрят на сцену из разных направлений. Если использовать одну и перезаписать её, первый источник потеряет свой результат.

Для изображения задаются флаги `DEPTH_STENCIL_ATTACHMENT_BIT` и `SAMPLED_BIT`. `VkImageView` позволяет обращаться к depth-компоненте изображения, а sampler задаёт, как её читать.

Здесь sampler имеет `compareEnable=VK_TRUE` и операцию `LESS`. Это comparison sampler: при выборке он сравнивает переданную глубину поверхности с глубиной карты. В отличие от albedo sampler он возвращает результат сравнения, а не цвет материала. Выбрана фильтрация NEAREST.

## 7:00–8:00 Pipeline и вершинный шейдер теней

Открыть `shadows.cpp::initializePipeline`: <!--code:shadow-pipeline-function-->[source/shadows.cpp:61](source/shadows.cpp#L61)<!--/code-->, затем <!--code:depth-pipeline-->[source/shadows.cpp:88](source/shadows.cpp#L88)<!--/code--> и `shaders/shadow.vert`: <!--code:light-transform-->[shaders/shadow.vert:7](shaders/shadow.vert#L7)<!--/code-->.

Новая функция `initializePipeline` создаёт отдельный graphics pipeline для карт глубины. В отличие от основного pipeline ЛР3 здесь нет цветовых attachments и fragment shader. Единственный шейдер — вершинный. Depth test и запись depth включены, чтобы сохранялась ближайшая к свету поверхность.

В `VkPipelineRenderingCreateInfo` указан depth-формат, а `renderPass` равен `VK_NULL_HANDLE`. Это позволяет использовать pipeline с Dynamic Rendering.

Файл `shadow.vert` очень короткий. Его функция `main` вычисляет:

```glsl
gl_Position = light.viewProjection * object.model * vec4(position, 1.0);
```

Сначала локальная вершина переводится в мир матрицей модели, затем в пространство камеры света. Шейдер сам не записывает текстуру глубины отдельной командой: глубина записывается при растеризации геометрии с включённым depth test/write.

Матрица света приходит в этот vertex shader через push constants. Основной fragment shader получает те же light VP через отдельный UBO `ShadowGpu`: это два способа передать согласованные данные для разных проходов.

## 8:00–9:30 Descriptors и общий draw моделей

Открыть `renderer.cpp::createDescriptors`: <!--code:create-descriptors-->[source/renderer.cpp:34](source/renderer.cpp#L34)<!--/code-->, затем <!--code:shadow-descriptors-->[source/renderer.cpp:79](source/renderer.cpp#L79)<!--/code--> и `drawModels`: <!--code:draw-models-function-->[source/renderer.cpp:303](source/renderer.cpp#L303)<!--/code-->.

В третьей работе уже были descriptor sets сцены и материалов, а также один dynamic model UBO. В четвёртой эта схема расширена. В scene set добавлен binding 3 для `ShadowGpu`, binding 4 для карты солнца и binding 5 для карты прожектора. Именно через эти привязки fragment shader получает новые данные.

Буфер моделей теперь выделяется по фактическому количеству моделей, поскольку добавился пол. Выравнивание и `modelStride` сохранены из ЛР3. Dynamic offset — смещение, которое выбирает запись конкретной модели в общем буфере.

Новая функция `drawModels` содержит общий цикл отрисовки. Раньше цикл был внутри `render` и всегда рисовал шестьсот индексов цилиндра. Теперь для каждого объекта выбираются его dynamic offset и диапазон `meshRanges`. Благодаря этому пол рисуется шестью индексами.

Эта же функция вызывается в shadow pass и в основном проходе. Поэтому цилиндр имеет одинаковые положение, поворот и масштаб при построении карты и при показе на экране. В основном проходе дополнительно выбирается обычный или sampling pipeline; в shadow pass уже привязан depth-only pipeline.

## 9:30–11:00 Порядок кадра и запись depth

Сравнить `renderer.cpp::render`: ЛР3 <!--code:lab3-render-->[../lab3/source/renderer.cpp:361](../lab3/source/renderer.cpp#L361)<!--/code-->, ЛР4 <!--code:renderer-render-->[source/renderer.cpp:404](source/renderer.cpp#L404)<!--/code--> и <!--code:pass-order-->[source/renderer.cpp:422](source/renderer.cpp#L422)<!--/code-->. Затем открыть `shadows.cpp::record`: <!--code:shadow-record-function-->[source/shadows.cpp:97](source/shadows.cpp#L97)<!--/code--> и `transition`: <!--code:barriers-->[source/shadows.cpp:12](source/shadows.cpp#L12)<!--/code-->.

В ЛР3 функция `render` после загрузки данных начинала основной render pass окна. Теперь перед ним добавлены два вызова `shadows::record`: сначала для солнца, затем для spot. Эти вызовы получают разные карты и разные light VP.

`record` записывает команды одного прохода. Карта переводится в состояние depth attachment, очищается значением один, затем начинается Dynamic Rendering. В активном проходе привязывается shadow pipeline, задаются viewport и scissor по размеру карты, передаются матрица света и bias, после чего вызывается общий `drawModels`. Затем выполняются `EndRendering` и переход карты в состояние для чтения шейдером.

Переходы и зависимости задаёт функция `transition`. Она отдельно учитывает первый кадр с состоянием `UNDEFINED`, запись depth и последующее чтение fragment shader. Dynamic Rendering не выполняет эти барьеры автоматически.

Перед повторным использованием карты выполняется обратная зависимость чтение — запись. CPU обновляет общие буферы после ожидания fence в `prepare`, как и в ЛР3. Активные карты заполняются каждый кадр, даже когда анимация стоит на паузе. Перед основным проходом viewport и scissor возвращаются к размерам окна.

## 11:00–13:00 Как шейдер получает настоящую тень

Открыть новый `shaders/shadows.glsl`, функцию `shadowVisibility`: <!--code:shadow-samplers-->[shaders/shadows.glsl:6](shaders/shadows.glsl#L6)<!--/code-->, <!--code:perspective-divide-->[shaders/shadows.glsl:12](shaders/shadows.glsl#L12)<!--/code-->, <!--code:comparison-->[shaders/shadows.glsl:17](shaders/shadows.glsl#L17)<!--/code-->. Затем `lighting.glsl::main`: <!--code:sun-visibility-->[shaders/lighting.glsl:76](shaders/lighting.glsl#L76)<!--/code-->, <!--code:spot-visibility-->[shaders/lighting.glsl:79](shaders/lighting.glsl#L79)<!--/code-->, <!--code:emissive-->[shaders/lighting.glsl:82](shaders/lighting.glsl#L82)<!--/code-->.

Функция `shadowVisibility` получает мировую координату текущей поверхности, матрицу света и нужный `sampler2DShadow`. Она отвечает на вопрос: перекрыта ли эта точка по данным карты?

Сначала `worldPosition` умножается на light VP. После проверки `w` координаты делятся на него. Это особенно важно для перспективного spot. Полученные XY переводятся из диапазона минус один — плюс один в координаты текстуры от нуля до единицы. Z уже построен под диапазон Vulkan от нуля до единицы, поэтому дополнительное преобразование к нему не применяется.

Если координаты выходят за карту, функция возвращает один: данных о перекрытии там нет. Внутри карты вызывается `texture(map, vec3(uv, ndc.z))`. Sampler сравнивает глубину текущей точки с глубиной ближайшей к свету поверхности. Если наша точка глубже, перед ней есть препятствие и visibility равна нулю. Если сравнение LESS проходит, возвращается один и прямой свет сохраняется.

Результат применяется в `main` файла `lighting.glsl`. В ЛР3 вклад солнца прибавлялся напрямую. Теперь он умножается на `sunVisibility`. В цикле spot аналогичная проверка действует только для выбранного индекса прожектора. Остальные spot и все point продолжают рассчитываться независимо.

Ambient добавляется отдельно, а emissive — после прямого освещения. Они не умножаются на shadow visibility. Общий `lighting.glsl` включён и обычным, и sampling fragment shader, поэтому новая логика работает в обоих вариантах ЛР3.

## 13:00–14:00 Bias и безопасный выбор прожектора

Открыть `controls.cpp::drawControls`: <!--code:bias-ui-->[source/controls.cpp:32](source/controls.cpp#L32)<!--/code-->, затем `shadows.cpp::record`: <!--code:bias-->[source/shadows.cpp:115](source/shadows.cpp#L115)<!--/code-->. Для выбора spot показать `scene.cpp::selectedShadowSpot` <!--code:stable-selection-->[source/scene.cpp:57](source/scene.cpp#L57)<!--/code--> и `reconcileShadowSpot` <!--code:reconcile-->[source/scene.cpp:61](source/scene.cpp#L61)<!--/code-->.

В `drawControls` появилась секция `Shadows`. Её значения попадают в `ShadowSettings`, а затем используются при построении матриц и записи прохода.

В `record` вызов `vkCmdSetDepthBias` задаёт constant и slope bias. Небольшой сдвиг сохранённой глубины уменьшает ложное самозатенение, возникающее из-за точности карты. Такие полосы называют acne. Слишком большой bias может сокращать и отрывать тень от объекта. Это отдельный параметр, который не меняет яркость источника.

`selectedShadowSpot` ищет актуальный индекс по стабильному ID. `reconcileShadowSpot` выбирает первый оставшийся spot, если прежний удалён, либо записывает минус один при пустом списке. Поэтому удаление элемента `vector` не оставляет старую ссылку и не заставляет применять карту к другому источнику случайно.

## 14:00–15:00 Сборка очистка и границы изменений

Открыть `CMakeLists.txt`: <!--code:cmake-->[CMakeLists.txt:123](CMakeLists.txt#L123)<!--/code-->, затем `renderer.cpp::shutdown`: <!--code:cleanup-->[source/renderer.cpp:390](source/renderer.cpp#L390)<!--/code--> и `shadows.cpp::shutdown`: <!--code:shadow-cleanup-->[source/shadows.cpp:124](source/shadows.cpp#L124)<!--/code-->.

В `CMakeLists.txt` добавлены новый `shadows.cpp` и компиляция `shadow.vert`. Также учитывается зависимость от общего `shadows.glsl`, чтобы после его изменения пересобирались оба fragment shader.

Проект имеет собственный target `cg-lab4` и свои shader outputs, поэтому сборка ЛР4 не заменяет файлы ЛР3.

В `renderer::shutdown` после ожидания GPU вызывается `shadows::shutdown`. Эта функция уничтожает pipeline, его layout и обе карты вместе с views и samplers. Прежняя очистка материалов и буферов сохранена.

Модель Блинна–Фонга, текстуры, specular mask, emissive, dynamic UBO, staging-загрузка и управление камерой были в ЛР3. Новая цепочка ЛР4 состоит из камер света, depth-карт, дополнительных проходов, comparison sampling и применения visibility к конкретному источнику. Базовая часть — тень солнца; дополнительная — тень одного выбранного прожектора. Тени точечных источников в этой реализации не добавлялись.

## Маршрут для повторения

`lighting.glsl` до и после → `graphics_internal.cpp::initialize` → `scene.hpp` → `generateFloor` → `lookAt` и `buildShadows` → `initializeImages` → `initializePipeline` и `shadow.vert` → `createDescriptors` и `drawModels` → `render` и `record` → `shadowVisibility` → `drawControls` и stable ID → `shutdown` и CMake.

На вопрос «что было в ЛР3» отвечать: камера, освещение, материалы и данные моделей. На вопрос «что сделано в ЛР4» показывать путь от создания карты глубины до умножения прямого света на visibility.
