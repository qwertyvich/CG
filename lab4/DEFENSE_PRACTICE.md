# Тренировка перед защитой

Не оставляйте учебные изменения в финальном проекте: для упражнений используйте UI и возвращайтесь демо-кнопкой к исходной сцене. Если меняете исходники, верните изменение и пересоберите. `cmake --build lab4/build-vs2022 --config Debug` автоматически пересобирает изменённые C++ и GLSL.

## Короткие определения

| Термин | Объяснение своими словами |
|---|---|
| Shadow map | Depth-текстура, хранящая ближайшую к свету поверхность для каждого текселя камеры света |
| Depth | Глубина после проекции; здесь обычная Vulkan depth от 0 (near) до 1 (far) |
| Caster / receiver | Объект, закрывающий свет / поверхность, получающая тень |
| Offscreen | Рендеринг в отдельное изображение вместо цветного кадра окна |
| Dynamic Rendering | Attachments передаются в BeginRendering; shadow VkRenderPass/VkFramebuffer не нужны |
| Depth-only pipeline | Graphics pipeline без color attachments и fragment shader, с записью depth |
| LookAt | View matrix, строящая систему координат из eye и направления на target |
| Orthographic / perspective | Параллельные лучи без уменьшения с расстоянием / расходящиеся лучи из позиции |
| Clip / NDC / UV | Координаты после матрицы / после деления на w / координаты текстуры 0..1 |
| Comparison sampler | Sampler, сравнивающий переданный reference depth с depth карты |
| Visibility | Доля видимого прямого света: при выбранном NEAREST 0 или 1 |
| Bias | Смещение depth для уменьшения ложного самозатенения |
| Acne / peter-panning | Ложные пятна на поверхности / отрыв тени из-за слишком большого bias |

## Вопросы и ответы

**Чем тень отличается от N·L?** N·L определяет ориентацию поверхности к свету. Shadow map дополнительно обнаруживает препятствие между ней и источником. Для проверки выключите shadow toggle, оставив источник включённым.

**Почему солнце использует ortho, а spot — perspective?** Солнце моделируется параллельными лучами, spot расходится из одной позиции. Полный FOV spot равен удвоенному внешнему полууглу конуса.

**Почему Z не преобразуется как XY?** Матрицы строятся под Vulkan depth [0,1]. OpenGL-преобразование z×0.5+0.5 здесь сломало бы сравнение.

**Почему проверяется w?** Для перспективы точка с w≤0 находится позади камеры света. Её нельзя корректно проецировать на видимую карту.

**Почему visibility=1 вне карты?** Там нет данных о перекрытии. Такой выбор не растягивает край карты на весь мир; небольшой охват может видимо обрезать тень. Spot cone factor отдельно обнуляет свет вне конуса.

**LESS означает тень или свет?** `reference < stored` — проверка прошла, свет виден, visibility=1. Это не shadow amount.

**Почему не тёмный весь результат?** Каждая visibility умножает только прямой вклад своего источника. Другие лампы, ambient и emissive независимы.

**Откуда берётся мягкий край конуса?** Из smoothstep между cos outer и cos inner в старом cone factor. Это не фильтрация карты и не мягкие физические тени.

**Что происходит при удалении выбранного spot?** Stable ID ищется заново. Выбор переходит на первый оставшийся, иначе отключается. Старый индекс не используется.

**Почему descriptors валидны при Off?** Обе карты существуют до shutdown и в первый кадр очищаются/переводятся в читаемый layout. Shader флаг отключает чтение неактивной карты.

**Можно ли убрать barriers после включения Dynamic Rendering?** Нет. Он не выполняет переход depth attachment → sampled read и обратную зависимость за приложение.

**Почему UBO/SSBO можно обновлять?** В `prepare` ожидается fence; в работе один кадр. CPU не перезаписывает данные, которые ещё читает GPU.

**Что сохранено из ЛР3?** Геометрия цилиндров, UV/текстуры, staging/device-local buffers, dynamic model UBO, specular mask/emission, камера, списки света, конусы/маркеры и sampling. Новое — пол, light matrices, depth resources/pipeline/passes, comparison sampling, shadow UI и стабильный выбор spot.

## 12 упражнений «найти — изменить — предсказать»

| № | Действие | Предсказать до нажатия | Где найти код | Что пересобирать |
|---|---|---|---|---|
| 1 | Directional demo → `Directional shadows` Off | Солнце останется включённым, его тень исчезнет | <!--code:sun-visibility-->[shaders/lighting.glsl:76](shaders/lighting.glsl#L76)<!--/code--> | UI, сборка не нужна |
| 2 | Half extent 7→14 | Покрытие вдвое шире; мировой размер текселя удвоится при тех же 2048² | <!--code:ortho-->[source/scene.cpp:67](source/scene.cpp#L67)<!--/code-->; `shadows.hpp::resolution` | UI; изменение resolution в коде потребует C++ build |
| 3 | Bias 0/0, затем 16/64, потом Reset shadows | Малый bias допускает acne, большой может сокращать/отрывать тень | <!--code:bias-->[source/shadows.cpp:115](source/shadows.cpp#L115)<!--/code--> | UI |
| 4 | Model X −1.35→−2, затем Camera X 7→8 | Модель изменит мировую тень; камера изменит только ракурс | <!--code:light-matrices-->[source/scene.cpp:64](source/scene.cpp#L64)<!--/code-->; <!--code:dynamic-draw-->[source/renderer.cpp:311](source/renderer.cpp#L311)<!--/code--> | UI |
| 5 | Sun direction (0,−1,0), затем нулевой | Вертикальная тень без вырождения; ноль использует fallback | <!--code:lookat-->[source/scene.cpp:30](source/scene.cpp#L30)<!--/code--> | UI |
| 6 | Spot demo → Position X −3→−1, Direction (1,−5,−3) | Изменятся световой конус и перспективная карта выбранного spot | <!--code:spot-fov-->[source/scene.cpp:76](source/scene.cpp#L76)<!--/code--> | UI |
| 7 | Outer 38→45, Inner 25→45 | FOV станет 90°, конус жёстче; это не PCF | <!--code:spot-fov-->[source/scene.cpp:76](source/scene.cpp#L76)<!--/code-->; `lighting.glsl::spotFactor` | UI |
| 8 | Add spot → выбрать новый → удалить предыдущий | Новый ID сохранится, packed индекс уменьшится | <!--code:stable-selection-->[source/scene.cpp:57](source/scene.cpp#L57)<!--/code--> | UI |
| 9 | Удалить выбранный, Clear spots, Add spot | Нет dangling reference; после Add выбирается новый ID | <!--code:reconcile-->[source/scene.cpp:61](source/scene.cpp#L61)<!--/code-->; <!--code:remove-spot-->[source/controls.cpp:140](source/controls.cpp#L140)<!--/code--> | UI |
| 10 | Найти return texture в GLSL; объяснить 1 и 0 | 1 сохраняет direct light; 0 убирает только вклад данного источника | <!--code:comparison-->[shaders/shadows.glsl:17](shaders/shadows.glsl#L17)<!--/code-->; <!--code:spot-visibility-->[shaders/lighting.glsl:79](shaders/lighting.glsl#L79)<!--/code--> | Только чтение; изменение GLSL → shader build через CMake |
| 11 | Panels + Emissive strength=1; показать Emission demo | Самосвечение остаётся при отсутствии прямого света | <!--code:emissive-->[shaders/lighting.glsl:82](shaders/lighting.glsl#L82)<!--/code--> | UI |
| 12 | Найти feature enable, BeginRendering и барьер | Feature включена в device; проход не использует shadow render pass; depth writes видимы shader | <!--code:feature-enable-->[source/graphics_internal.cpp:477](source/graphics_internal.cpp#L477)<!--/code-->; <!--code:begin-rendering-->[source/shadows.cpp:109](source/shadows.cpp#L109)<!--/code-->; <!--code:barriers-->[source/shadows.cpp:12](source/shadows.cpp#L12)<!--/code--> | Только чтение; изменение C++ → C++ build |

После упражнения 12 закройте программу штатно, а не прерывайте процесс. Полный сценарий доклада — [DEFENSE_15MIN.md](DEFENSE_15MIN.md), пошаговый показ — [DEMO_CHECKLIST.md](DEMO_CHECKLIST.md).
