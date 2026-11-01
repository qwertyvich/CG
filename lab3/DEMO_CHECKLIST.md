# Показ по требованиям

Запустите из build-каталога `Debug\cg-lab3.exe`. Если видны ошибки отсутствующих
Steam JSON, используйте описанную в README изоляцию implicit layers; explicit
Khronos и synchronization validation остаются включены. После каждого
самостоятельного опыта `Base demo` возвращает удобное исходное состояние.
Секции интерфейса можно сворачивать и прокручивать колесом.

| Что показать | Исходное состояние и точные действия UI | Ожидаемый результат / фраза | Код |
| --- | --- | --- | --- |
| База | `Base demo`. У обоих `Material` = `UV grid (black fallback)`, `Special sampling` выключен. `Object` = `Cylinder 1`, `Model XYZ` X=-1.7, `Scale XYZ`=(0.7,1.3,1). Затем `Object` = `Cylinder 2` | Второй не переместился. «Один material set, один model buffer, разные dynamic offsets» | [dynamic_bind](source/renderer.cpp#L389), [ubo_write](source/renderer.cpp#L277) |
| UV и нормали | Снова `Base demo`. `Model Euler XYZ` у первого: X=65, у второго: X=-65. Затем X=0, Y=-90; `Play animation` выключен. Угол камеры меняйте в `Camera Euler XYZ` малыми шагами | Видны обе крышки, различимые TOP/BOTTOM, шов без растянутой полосы. «У шва совпадают позиции, но различаются UV» | [cylinder](source/geometry.cpp#L4), [normal_matrix](source/scene.cpp#L70) |
| Память GPU | Открыть `docs/smoke-stdout.log`, строки `DEVICE_LOCAL=1`, `Dynamic model UBO`. Показать код allocation и copy | «Картинка не доказывает тип памяти: здесь required DEVICE_LOCAL и отдельный staging» | [geometry_upload](source/gpu_upload.cpp#L69), [buffer_copy](source/gpu_upload.cpp#L81) |
| Dynamic UBO | В том же логе показать buffers=1, sizeof, alignment, stride, offsets и range. Открыть bind | «Динамический offset передаётся командой Vulkan; шейдер видит одну структуру» | [ubo_allocate](source/renderer.cpp#L57), [dynamic_bind](source/renderer.cpp#L389) |
| Доп 1 | `Base demo`; первый `Material` = `Stripes (black fallback)`, второй = `Panels (specular + emissive)`. Затем обоим Panels, затем первому UV grid | Объекты переключаются независимо; одинаковый материал использует один set. «Выбор меняет привязку готового набора, а не переписывает активные descriptors» | [material_sets](source/renderer.cpp#L72), [material_combo](source/controls.cpp#L38) |
| Доп 2 | `Sampling demo`. Первый `Special sampling` включён, второй выключен. `Effect strength`=0.8, `Effect speed`=1.5. Выключить `Play sampling effect`, подождать, включить. Не включать `Play animation` | Слева движущийся смешанный узор и цветовая модуляция; справа обычный. На паузе фаза стоит. «Две выборки одного sampler по разным UV, время и флаг пришли через push constants» | [sampling_push](source/renderer.cpp#L392), [multi_sample](shaders/lighting.glsl#L61), [effect_time](source/application.cpp#L18) |
| Сила эффекта | В первом объекте `Effect strength`=0, затем 1 | При нуле обычное изображение; при единице полный эффект. `Base demo` восстанавливает сцену | [sampling_mix](shaders/lighting.glsl#L64) |
| Доп 3, блик | `Specular demo`. Выбран первый, оба материала Panels, внешний directional свет включён, эмиссия обоих равна 0. `Shininess` первого менять 16..96, `Camera XYZ` X менять -0.5..0.5. Для явной маски можно выключить `Diffuse contribution` в `Ambient and directional` | У первого блик есть только на разрешённых картой участках, у второго specular color=0 как эталон. «Маска умножает specular, shininess задаёт ширину» | [specular_mask](shaders/lighting.glsl#L25) |
| Доп 3, эмиссия | `Emission demo`; все внешние источники выключены, помощники скрыты. Первый Panels, второй UV grid. У первого `Emissive strength`=0, затем 1.5 | Голубые полосы пропадают при 0 и светятся при 1.5; второй тёмный. «Эмиссия прибавляется после освещения» | [emissive_add](shaders/lighting.glsl#L77) |
| Black fallback | В том же тёмном состоянии первый `Material` = `Stripes (black fallback)`. Вернуть Panels, затем `Restore lights` | У Stripes нет ни эмиссии, ни блика от карт. Лог maps=2,0,0 подтверждает оба чёрных bindings. «Ноль умножает блик и ничего не прибавляет к цвету» | [black_load](source/renderer.cpp#L325), [fallback_bind](source/renderer.cpp#L71) |
| Завершение | `Base demo`, несколько раз менять `Material`, изменить размер окна, свернуть/развернуть, закрыть | Окно восстанавливается, GPU-ресурсы освобождаются после ожидания. Ошибки validation должны оставаться нулевыми | [prepare_fence](source/graphics_internal.cpp#L781), [cleanup](source/renderer.cpp#L344) |

Кадры для отчёта уже находятся в `docs/screenshots`: base, sampling,
emission, specular, caps и seam. Это реальные снимки клиентской области
приложения, а не рендеры генератора изображений.
