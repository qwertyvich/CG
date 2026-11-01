# Как устроена ЛР2

## Смысл работы

В ЛР1 мы передавали готовую MVP-матрицу и цвет. В ЛР2 положение наблюдателя и ламп влияет на каждую точку поверхности: бок цилиндра становится светлее при повороте к лампе, а блик перемещается при движении камеры. CPU хранит параметры и отправляет их GPU; fragment shader рассчитывает освещение каждого видимого фрагмента.

Цилиндр остался тем же вариантом 8. Его позиции повторяются на границах боковины и крышек, потому что одна запись вершины содержит одну нормаль. Геометрически это один замкнутый объект, но нормали на жёстком ребре различаются. Получилось 202 вершины, 200 треугольников и 600 индексов.

## Карта проекта

| Файл / каталог | Ответственность |
| --- | --- |
| `CMakeLists.txt`, `CMakePresets.json` | Отдельная x64 сборка, зависимости, glslc, target и CPU-тест |
| `source/main.cpp` | GLFW, ImGui, порядок кадра, minimize, ошибки, режим smoke |
| `source/graphics_internal.hpp/.cpp` | Vulkan instance/device, allocator, swapchain/depth, render passes, fence/semaphores, present, ImGui backend, validation |
| `source/application.hpp/.cpp` | initialize/update/render/shutdown; CPU Scene; сценарий smoke |
| `source/scene.hpp/.cpp` | Математика, камера, параметры и GPU-структуры, упаковка UBO/SSBO |
| `source/geometry.cpp` | Цилиндр с нормалями, маркер-куб, два кольца конуса и ось |
| `source/controls.cpp` | Все секции ImGui, Add/Remove/Clear/Reset |
| `source/renderer.hpp/.cpp` | VMA buffers, descriptors, три pipelines, загрузка и запись команд сцены |
| `shaders/scene.glsl`, `model.glsl` | Общие раскладки данных C++ ↔ GLSL |
| `shaders/display.glsl` | Ограничение результата и единственное sRGB-кодирование при необходимости |
| `shaders/lab2.vert/.frag` | Мировые позиции/нормали и освещение Блинн–Фонга |
| `shaders/unlit.vert/.frag` | Постоянные цвета помощников |
| `tests/scene_tests.cpp` | Геометрия, матрицы, граничные случаи освещения |
| `tools/verify_lab1.py` | Сравнение SHA-256 оригинала с исходным снимком |
| `tools/update_code_map.py` | Актуализация CODE_MAP и строк в сценарии защиты |
| `docs/` | Снимок исходного состояния, реальные логи и 3 скриншота |
| `README.md` | Быстрые команды и управление |
| `CODE_MAP.md`, `DEFENSE_15MIN.md`, `DEFENSE_PRACTICE.md` | Навигация, доклад, тренировка |
| `REPORT_DRAFT.md` | Черновик отчёта без придуманных персональных данных |
| `codex_lab2_prompt.md` | Пользовательское техническое поручение |
| `LICENSE`, `NOTICE` | Лицензия и авторство каркаса |
| `build-vs2022/` | Генерируемые файлы, скачанные зависимости, бинарники; Git их исключает |

## Что действительно сделано

Это последовательность работы в текущей сессии, а не история отдельных коммитов. Сначала зафиксированы Git status и хеши 15 файлов `lab1`; существовавший комментарий в `lab1/source/application.cpp` сохранён. Затем исходники и лицензии скопированы в `lab2` без build, `.spv`, `.git` и настроек редактора.

Копия CMake переименована в `cg-lab2`, выход шейдеров перенесён в собственный build. Собственная математика ЛР1 развита в `scene.cpp`; добавлены камера, нормали, материал и GPU-структуры. Отрисовка отделена от UI, созданы два набора дескрипторов, динамические SSBO и unlit пути. Подключены численные тесты, затем выполнены сборка и запуск на настоящем GPU.

Во время проверки найден и исправлен случай сворачивания между проверкой размера окна и rebuild swapchain. Теперь prepare проверяет capabilities поверхности, нулевой кадр пропускается, а пересоздание использует ненулевой снимок extent. После уничтожения старых объектов handles очищаются: частичная неудача не приводит к повторному освобождению.

## Старт, кадр, завершение

При старте `main` создаёт окно и ImGui context; `graphics::internal::initialize` создаёт instance с Khronos validation и synchronization validation, выбирает device/depth format, создаёт allocator, swapchain, render passes и синхронизацию. Затем `application::initialize` создаёт начальную сцену и renderer. Renderer выделяет геометрию, UBO/SSBO, layouts, pool, sets и pipelines. При частичной ошибке уже созданные ресурсы освобождаются.

Каждая итерация сначала обрабатывает события GLFW. Для минимизированного или нулевого окна программа ждёт событий. ImGui строит UI; `application::update` меняет только CPU Scene и время анимации. `ImGui::Render` формирует UI draw data, но ещё ничего не отправляет GPU.

`prepare` ждёт fence прошлого кадра, при необходимости пересоздаёт swapchain и получает изображение acquire. На свёрнутой поверхности возвращает пустой кадр без ошибки. При настоящей ошибке выставляет `FrameData.failed`; такой кадр не отправляется. После prepare `renderer::render` обновляет буферы и записывает команды. `submitAndPresent` дописывает ImGui-команды, сбрасывает fence непосредственно перед submit, отправляет оба command buffers и вызывает present.

`update` — изменение CPU-состояния. `render` — подготовка данных и запись команд. `submit` — передача команд очереди GPU. `present` — передача готового изображения системе показа. Эти действия имеют разные задачи и не являются четырьмя названиями одной операции.

При выходе renderer ждёт idle и освобождает pipelines, layouts, pool, descriptor layouts и VMA buffers. Затем графический каркас освобождает backend ImGui, framebuffers, depth, sync, swapchain, allocator, device, surface, debug messenger и instance. После этого закрываются GLFW backend и окно.

## Путь данных

`ImGui → Scene (Camera, Material, points, spots) → buildUniforms/packPoints/packSpots → writeBuffer → UBO/SSBO → descriptor sets → vertex shader → fragment shader → swapchain → present`.

В `Scene` лежат удобные для UI структуры с `bool` и `Vec3`. В GPU уходят отдельные структуры с `mat4`, `vec4` и `uvec4`. Например, `enabled=false` превращается в `colorIntensity.w=0`, а не в C++ bool внутри буфера. В counts записана **длина списка**, включая выключенные записи; их нулевая интенсивность даёт нулевой вклад. Capacity может быть больше длины списка; shader не читает хвост capacity.

## Геометрия и математика

**Нормаль** показывает направление наружу от поверхности. Для боковины `N=(x,0,z)` при радиусе 1, для крышек `(0,±1,0)`. `generateCylinder` дублирует края крышек. Модуль нормалей равен 1; тест дополнительно проверяет winding, отсутствие вырожденных треугольников и замыкание после объединения одинаковых позиций. Back-face culling включён у цилиндра, но доказательство корректности индексов — численный тест.

**Model** переводит локальные вершины в мир: `M=T*Rz*Ry*Rx*S`. Для column vectors первым применяется S, затем Rx/Ry/Rz, затем T. Масштаб ограничен положительными значениями не меньше 0.05, поэтому обратная матрица существует. Анимация изменяет позицию/угол модели; её можно остановить.

**Камера** задаёт положение наблюдателя в мире. Оси: +X вправо, +Y вверх, локальное направление взгляда −Z. CameraWorld=`T*Rz*Ry*Rx`. `cameraView` вычисляет `V=Rx(-x)*Ry(-y)*Rz(-z)*T(-position)`: обратные действия идут в обратном порядке. Градусы UI умножаются на π/180 перед sin/cos. Старт: `(0,1.2,9)`, pitch −7.6°, взгляд слегка вниз. Камера не перемещает сам цилиндр; shader отдельно получает её мировую позицию для блика.

**Projection** превращает видовой объём в clip-space. В перспективе FOV ограничен 10…120°, near=0.1, far=100. В ортографии настраивается половина вертикального размера области, FOV не используется. Aspect берётся из актуального swapchain extent уже после возможного rebuild. Обе матрицы дают глубину Vulkan [0,1]; Y переворачивается один раз в projection, viewport остаётся с положительной высотой.

**Normal matrix** сохраняет перпендикулярность поверхности после растяжения. Общая формула `transpose(inverse(mat3(M)))`. В нашем TRS это `R*S⁻¹`; именно её считает `buildUniforms`. Перенос исключён, normalMatrix хранится как mat4 ради простой раскладки. Vertex shader применяет её с w=0, fragment shader заново нормализует интерполированную нормаль.

Все векторы освещения находятся в **мировом пространстве**: worldPosition, worldNormal, cameraPosition и позиции/направления света. Смешивать локальную нормаль и мировое направление нельзя: результат будет поворачиваться неправильно.

## Освещение по шагам

`evaluateBlinnPhong` отвечает за поверхность, а функции `evaluateDirectionalLight`, `evaluatePointLight`, `evaluateSpotLight` — за конкретную лампу.

`N` — нормаль; `L` — единичный вектор от фрагмента к лампе; `V` — от фрагмента к камере. `H=normalize(L+V)` — половинный вектор. Диффузная доля равна `albedo*max(dot(N,L),0)`. Она ярче, когда поверхность смотрит к лампе. Зеркальная доля равна `specularColor*pow(max(dot(N,H),0),shininess)`, только при `NdotL>0`. Больший shininess сужает блик; цвет блика и интенсивность лампы — другие параметры. Почти нулевые векторы дают нулевой безопасный результат.

`main` добавляет `ambientColor*ambientIntensity*albedo` ровно один раз, затем directional, затем циклы point и spot. Ambient — условный постоянный фон; он не зависит от направления камеры и не затухает по расстоянию до каждой лампы. В UI можно выключить ambient, diffuse и specular независимо, чтобы проверить их смысл.

Directional задаёт **направление движения лучей**; к свету направлен `L=-direction`. Позиции и затухания у него нет. Нулевое направление на CPU получает запасное `(0,-1,0)`.

Point задаёт `delta=lightPosition-worldPosition`, `L=normalize(delta)`, `attenuation=1/max(dot(delta,delta),0.01)`. Это закон `1/r²` с ограничением вблизи нуля для численной устойчивости. При фиксированных N/L/V удвоение расстояния уменьшает линейный вклад вчетверо. Произвольный скриншот может изменить углы и попасть в насыщение, поэтому от его яркости такое отношение не требуется.

Spot дополнительно ограничивает свет конусом. Direction направлен **от источника в сцену**, поэтому `cosTheta=dot(-L,direction)`. UI задаёт полууглы в градусах; CPU передаёт их косинусы. Так как inner≤outer, cosInner≥cosOuter. При разных границах `smoothstep(cosOuter,cosInner,cosTheta)` плавно повышает вклад от 0 до 1; при равных используется step без деления на ноль. Итоговый прямой свет умножается и на attenuation, и на cone factor. Углы ограничены `0≤inner≤outer≤89°`.

Цвета трактуются как линейные RGB, сумма считается до отображения, тон-маппинга нет. Для sRGB attachment кодирование выполняет формат; для UNORM `displayColor` делает явное кодирование. Двойной gamma-коррекции нет. Яркие результаты ограничиваются [0,1], поэтому могут терять детали при слишком большой интенсивности.

## Два descriptor sets и раскладка памяти

| Set / binding | Тип / layout | Размер и поля (offset в байтах) |
| --- | --- | --- |
| 0 / 0 | UBO / std140, vertex+fragment | SceneGpu 160: VP 0; camera 64; ambient 80; dirDirection 96; dirColorIntensity 112; counts 128; display 144 |
| 0 / 1 | readonly SSBO / std430, fragment | PointGpu stride 32: position 0; colorIntensity 16 |
| 0 / 2 | readonly SSBO / std430, fragment | SpotGpu stride 64: position 0; direction 16; colorIntensity 32; cone cosines 48 |
| 1 / 0 | UBO / std140, vertex+fragment | ModelGpu 176: M 0; normalMatrix 64; albedo 128; specular+shininess 144; options 160 |

Counts: point length, spot length, diffuse flag, specular flag. Display.x: требуется ли ручное sRGB-кодирование. Model.options.x: умножать ли albedo на vertex color. Position.w=1; направления имеют w=0. Все матрицы column-major. В `scene.hpp` размеры и offsets проверяются static_assert; SPIR-V отдельно проверен spirv-val.

`createDescriptors` создаёт два layout и pool ровно для 2 uniform + 2 storage descriptors и 2 sets. `writeDescriptor` записывает VkDescriptorBufferInfo. Lit pipeline layout содержит оба set layout; `renderer::render` привязывает оба set одной командой перед draw. ImGui имеет собственный pool и к этим двум наборам не относится. Vertex locations: 0 position, 1 color, 2 normal; set/binding — адрес ресурса, location — адрес vertex attribute или shader varying.

`ensureStorage` начинает с capacity 1, увеличивает до `max(count,2*capacity)`, ограничивая `maxStorageBufferRange/stride`. Пустой список сохраняет валидный ненулевой buffer, а count=0. Буферы не сжимаются при удалении; это уменьшает лишние перевыделения. Сначала успешно создаётся новый buffer, затем переписывается descriptor и уничтожается старый. При неудаче выделения старый остаётся валидным, кадр прерывается с ошибкой.

В работе один кадр. Ожидание fence в prepare завершает предыдущее GPU-чтение до write/map/flush/unmap, перевыделения SSBO и обновления sets. UI может менять CPU-векторы до fence: это другая память. VMA flush вызывается и для noncoherent memory. Добавление frames in flight потребовало бы отдельных буферов и sets на кадр либо другой схемы синхронизации; сейчас этого усложнения нет.

## Визуализация

Куб рисуется в позиции того же PointLight/SpotLight и имеет цвет источника. `generateSpotCones` использует тот же нормализованный direction и те же полууглы, что packSpots. Радиус кольца `length*tan(angle)`. Ортонормальный базис строится через cross; около вертикальной оси опорный вектор меняется, чтобы избежать нулевого результата.

Линии — line list, lineWidth=1, без fillModeNonSolid и wideLines. Цилиндр и кубы проходят depth test и пишут глубину; линии проходят depth test, но не пишут её. Поэтому часть конуса может быть скрыта цилиндром, а помощники не просвечивают сквозь него. Это не тени и не алгоритм распространения света. Длина конуса нужна только для удобства показа, закон 1/r² продолжается за его концом.

Unlit pipeline читает VP и собственные push constants 80 байт, но не материал цилиндра и не освещение. Ambient/intensity не меняют цвет куба. Скрытие помощников не выключает лампу; enabled выключает вклад лампы, сохраняя её условное обозначение.

## Проверки и фактические результаты

Проверки выполнены 01.11.2026 на AMD Radeon(TM) Graphics, Windows, Debug x64. Подробный итог: [VERIFICATION.md](docs/VERIFICATION.md). Все 15 исходных файлов ЛР1 совпадают с исходными SHA-256.

| Пункт PDF / сценарий | Реализация и проверка | Фактический результат |
| --- | --- | --- |
| Самостоятельная сборка | Новый build-vs2022, MSVC x64; затем clean-first build | Успешно, 4 GLSL компилируются при сборке |
| Нормали и замкнутость | CPU-test: unit normals, hard edges, outward winding, nondegenerate triangles, welded edges | PASS; 202 / 600 |
| Матрицы | View*CameraWorld≈I; depth near=0/far=1; normal perpendicular to scaled tangent | PASS |
| Масштаб/поворот | Изменены через реальный ImGui; [кадр](docs/screenshots/03-nonuniform-scale.png) | Видимое освещение сохраняется при (0.5,1.8,1.2), Y=40° |
| Камера | UI FOV/reset; smoke position/Euler/FOV/ortho; resize | Выполнено, ошибок validation нет; FOV проверен визуально |
| Компоненты света | UI ambient on/off; smoke ambient-only и directional-specular-only | Ambient меняет цилиндр; marker crop остаётся тем же |
| Материал | UI scale/rotation; shader формула, численные normal tests; исходные настройки видны на кадре | Реализовано; исчерпывающая визуальная проверка каждого оттенка/значения shininess не выполнялась |
| Доп 1 | Реальные set=0/set=1; SPIR-V layout; validation | PASS |
| Доп 2 | Smoke 0→33→30→0→1, рост capacity; UI Add/Clear/Add | PASS; UI capacity выросла 1→4; первый/средний/последний удалены в smoke |
| Закон 1/r² | Фиксированные углы, CPU linear contribution d²=4 и 16 | PASS: отношение 0.25 |
| Доп 3 | 33 spot; равные углы, zero direction, intensity=0; CPU границы/середина smoothstep | PASS; UI Add/Clear/Add работает |
| Доп 4 | [Старт](docs/screenshots/01-initial.png), [без помощников](docs/screenshots/02-light-without-helpers.png) | Кубы/оба контура видны; скрытие независимо от света |
| Вертикальный конус | CPU-test direction=(0,1,0) | Все позиции конечны, устойчивый базис |
| Resize/minimize/restore | Smoke два размера; отдельно 3 сворачивания по 500 мс | После исправления PASS, процесс продолжает рисовать |
| Закрытие/повторный запуск | Несколько запусков, штатное WM_CLOSE, shutdown с validation/VMA | Выполнено; сообщений об утечках объектов нет; внешнего профилировщика утечек не использовали |
| Проверки SPIR-V | spirv-val --target-env vulkan1.1 для четырёх файлов | PASS |

Есть два **общих сообщения loader**, не VUID приложения: отсутствуют `C:\Program Files (x86)\Steam\SteamOverlayVulkanLayer64.json` и `C:\Program Files (x86)\Steam\SteamFossilizeVulkanLayer64.json`. Они оставлены в stderr и отдельном счётчике general_errors. В smoke после исправлений validation_errors=0. Иногда resize даёт `Swapchain is suboptimal for rendering!`: это состояние WSI, после которого swapchain пересоздаётся. Реестр и установка Steam не изменялись.

NVIDIA, Linux, предельное число ламп и длительная нагрузка не проверялись. Для самостоятельной демонстрации: Reset scene, выключить point/spot и ambient, оставить directional; сравнить diffuse/specular и shininess; затем Add point/spot, менять направление/углы, удалить все и добавить снова, скрыть помощников, свернуть/восстановить. Для нового проблемного запуска нужен stderr с точным VUID и текущими GPU/драйвером.
