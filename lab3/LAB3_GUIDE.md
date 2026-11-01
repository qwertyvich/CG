# Как устроена ЛР3

В ЛР2 основной цвет задавался числом или цветом вершины. В ЛР3 цвет приходит
из файла, а вершины объясняют шейдеру, какую часть изображения выбрать.
Освещение осталось Блинн-Фонгом. Из текущей рабочей ЛР2 скопированы исходники,
математика, инфраструктура окна, синхронизация, камера и источники света.
Затем в копии добавлены UV, decoder, загрузка GPU, материалы, динамический
буфер и эффект. Это порядок работы над файлами, а не история Git-коммитов.

## Файлы и жизненный цикл

`main.cpp` создаёт GLFW/ImGui/Vulkan, запускает цикл и освобождает ресурсы.
`graphics_internal.cpp` отвечает за swapchain, depth, командные буферы, fence,
submit/present, resize и validation. `application.cpp` содержит CPU-сцену,
считает время и вызывает UI. `scene.cpp` строит матрицы и упаковывает свет,
`geometry.cpp` строит цилиндр и помощники. `controls.cpp` редактирует CPU-сцену.
`renderer.cpp` связывает GPU-ресурсы с pipeline и выполняет draw.
`image.cpp` читает PNG, `gpu_upload.cpp` копирует данные на устройство.

При initialize загружаются изображения, геометрия, descriptor sets и pipelines.
В update UI меняет только CPU-состояние. Затем prepare ждёт fence предыдущего
кадра; только после него renderer записывает UBO/SSBO и при необходимости
заменяет линии конусов. В render записываются команды. Submit отправляет
их GPU; present показывает кадр. Shutdown ждёт device idle, уничтожает
pipelines/layouts/pool, текстуры, буферы и upload pool, затем Vulkan и окно.
Неудачная частичная инициализация renderer проходит через тот же cleanup.

## От файла до пикселя

Файл PNG -> `stbi_info`/`stbi_load` -> CPU RGBA -> host-visible staging ->
`vkCmdCopyBufferToImage` -> optimal device-local VkImage -> VkImageView +
VkSampler -> combined image sampler descriptor -> `texture()`.

Размеры проверяются до декодирования: положительные, не больше лимита GPU,
без переполнения объёма RGBA. Строки переворачиваются ровно один раз.
Staging имеет TRANSFER_SRC, CPU map/memcpy и flush. Изображение имеет
SAMPLED|TRANSFER_DST и required DEVICE_LOCAL. Барьер переводит UNDEFINED
в TRANSFER_DST_OPTIMAL с разрешением transfer write. После копии второй
барьер переводит его в SHADER_READ_ONLY_OPTIMAL и обеспечивает shader read.
Upload fence подтверждает завершение до освобождения staging.

VkImage — ресурс с пикселями и памятью. View задаёт, какой формат/слой/mip
видит шейдер. Sampler задаёт repeat/linear и LOD. Descriptor связывает их
с binding шейдера. Здесь mipLevels=1 и minLod=maxLod=0; anisotropy не нужна.

## UV и геометрия

Position определяет место вершины, normal — ориентацию поверхности для света,
UV — адрес изображения. Rasterizer интерполирует UV внутри треугольника.
Для 50 сегментов сохранён дополнительный 51-й набор вершин: позиции первого
и последнего совпадают, но u равны 1 и 0. Использовано
u=1-angleCCW/(2*pi), чтобы текст читался снаружи. Последний участок
интерполирует 0.02..0, без скачка через всю текстуру. На крышках свои вершины, нормали и плоские UV.
Итого 4*51+2=206 вершин, 50*12=600 индексов. View поворачивается независимо
от модели; normal matrix R*S^-1 сохраняет свет при неравномерном масштабе.

Вершины/индексы цилиндра, кубы маркеров и линии конусов проходят через
staging и `vkCmdCopyBuffer`. Финальные буферы имеют TRANSFER_DST и
VERTEX_BUFFER/INDEX_BUFFER, VMA требует DEVICE_LOCAL. Transfer write
синхронизирован с vertex/index read. Финальный буфер CPU не заполняет.
На дискретной GPU это обычно VRAM; на UMA DEVICE_LOCAL может одновременно
быть HOST_VISIBLE. В логах печатаются реальные memory property flags.

## Dynamic UBO и материалы

Есть один VkBuffer на две записи ModelGpu, а не по VkBuffer на объект.
Запись содержит model, normal matrix, tint, specular/shininess и surface.
Внутренний std140 проверен `alignas`, `sizeof` и `offsetof`. GPU alignment
между записями — отдельное ограничение minUniformBufferOffsetAlignment:
stride=alignUp(176,alignment). CPU копирует 176 полезных байт в слот i*stride.
Descriptor range равен **176**, offset в descriptor равен 0.

Перед draw `vkCmdBindDescriptorSets` получает один uint32 dynamicOffset:
0 для первого, stride для второго. Шейдер описывает одну запись uniform,
а выбранное место в общем буфере задаёт команда привязки. Это отличается
от обычного UBO и от shader-массива, индексируемого push constants.
Проверяются maxUniformBufferRange, uint32 offsets и границы общего буфера.
Один кадр в работе позволяет безопасно обновлять обе записи после fence.

| Set / binding | Содержимое |
| --- | --- |
| 0 / 0 | Scene UBO: камера, ambient, directional, режимы |
| 0 / 1,2 | Point и Spot SSBO |
| 1 / 0 | Общий UNIFORM_BUFFER_DYNAMIC с range одной модели |
| 1 / 1,2,3 | Albedo, specular и emissive combined samplers |

Три material sets созданы заранее. Один материал можно назначить обоим
объектам: set общий, dynamic offsets разные. Combo меняет CPU-индекс,
а renderer привязывает готовый set. В активном кадре descriptors материала
не переписываются. Пул учитывает scene set, три material sets, три dynamic
descriptors, девять samplers и два storage descriptors; ImGui имеет свой пул.

## Fragment shader

Сначала `main()` получает обычный albedo через `texture(albedoSampler,uv)`.
Базовый вариант шейдера собран с BASE_ONLY и не объявляет push constants.
Дополнительный вариант читает 32-байтный SamplingPush: uint32 flag и
time/strength. Если flag включён, uvA прокручивается, uvB поворачивается,
масштабируется и движется в другую сторону. Две выборки **того же** sampler
смешиваются с весом от времени и умножаются на цветовую модуляцию.
Это несколько texture samples, а не MSAA. Strength=0 возвращает обычный цвет.

Флаг не хранится в ModelGpu. Для каждого effect draw push constants записаны
явно; обычный объект использует base pipeline, который их не читает, поэтому
предыдущее значение флага не переносит эффект на соседний объект.
Время на CPU накапливается через dt*speed; на паузе не растёт и после resume
не прибавляет время паузы. Geometry animation имеет отдельный переключатель.
Specular/emissive карты намеренно остаются на исходных UV с учётом UV repeat.

Маска specular умножает только `object.specular.rgb*pow(NdotH,shininess)`.
Diffuse и ambient не маскируются. Белый разрешает блик, чёрный обнуляет его.
Shininess регулирует ширину блика, а не заменяет карту. Затем после суммы
Блинн-Фонга прибавляется `texture(emissiveSampler,uv).rgb*emissiveStrength`.
Эта сумма не зависит от NdotL и внешнего света. Чернота карты прибавляет 0.

`black.png` загружен тем же decoder/upload путём, что остальные файлы.
Grid/Stripes используют его view/sampler для обоих optional bindings;
один ресурс освобождается один раз. Отсутствующие optional-файлы Panels
дают тот же fallback, отсутствующая albedo — явную ошибку.

Свет вычисляется в линейном RGB. Albedo и художественная эмиссия используют
sRGB image format, specular mask — UNORM. Перед выводом учитывается формат
swapchain, чтобы не выполнить gamma-преобразование дважды.

Фактические результаты и непроведённые проверки находятся в
[docs/VERIFICATION.md](docs/VERIFICATION.md). Для поиска точных строк служит
[CODE_MAP.md](CODE_MAP.md), для показа — [DEMO_CHECKLIST.md](DEMO_CHECKLIST.md).
