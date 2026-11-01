# Лабораторная работа №3: текстурирование

Самостоятельная копия текущей ЛР2: C++20, Vulkan 1.1, GLFW, Dear ImGui, VMA,
vk-bootstrap. Вариант 8, цилиндр: 50 сегментов, 206 вершин, 600 индексов.
Два экземпляра имеют независимые трансформации и материалы. `lab1` и `lab2`
не изменяются; проверка SHA-256 описана в [VERIFICATION](docs/VERIFICATION.md).

Реализованы файловые albedo-текстуры и UV, staging/device-local геометрия,
один dynamic model UBO, три материала, анимированное многократное сэмплирование
с push constants, specular/emissive карты и чёрный fallback 1x1.
Фактические проверки и ограничения приведены в таблице верификации.
В PDF три дополнительных задания; неоднозначная формулировка оценки 4
не меняет выбранный объём. Окончательную оценку ставит преподаватель.

## Сборка Windows x64

Нужны VS 2022 Build Tools с C++, Windows SDK, CMake, Git, Vulkan SDK.
Команды CMD по одной строке, из корня репозитория:

```bat
cd lab3
cmake -S . -B build-vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2022 --config Debug --parallel
ctest --test-dir build-vs2022 -C Debug --output-on-failure
cd build-vs2022
Debug\cg-lab3.exe
```

Если CMake не в PATH, используйте **x64 Native Tools Command Prompt for VS 2022**
или полный путь из установленного Visual Studio. Если SDK не обнаружен,
добавьте к configure `-DVulkan_ROOT=C:/VulkanSDK/1.4.321.1`, подставив свою версию.
Альтернатива из папки `lab3`: `cmake --preset msvc-debug`, затем
`cmake --build --preset msvc-debug` и `ctest --preset msvc-debug`.

Рабочая директория приложения — **build-vs2022**. CMake компилирует пять
шейдеров в `build-vs2022/shaders`, копирует `assets` в `build-vs2022/assets`.
В Visual Studio эта рабочая директория уже настроена. Для переноса готового
приложения сохраните рядом папки `shaders` и `assets`; запускайте из их родителя.
При первом configure FetchContent получает четыре зависимости по закреплённым
версиям. `stb_image` поставляется в `third_party` с лицензией и точным commit;
его скачивать при сборке не нужно. Готовое приложение работает без сети.
Изменения GLSL автоматически вызывают glslc, C++ компилирует MSVC,
линкер собирает `cg-lab3.exe`. `find_package(Vulkan)` находит SDK и loader.

## Управление

- `Base demo`: оба цилиндра с UV grid, обычное освещение, без эффекта.
- `Sampling demo`: эффект на первом, обычная выборка на втором.
- `Emission demo`: первый Panels, второй UV grid, все источники выключены.
- `Specular demo`: одинаковые Panels и свет; слева маска блика, справа
  specular color обнулён как явный эталон; emissive strength обоих равна 0.
- `Object` выбирает цилиндр; `Material` меняет только его материал.
- `Model XYZ`, `Model Euler XYZ`, `Scale XYZ` задают положение, углы и масштаб.
- `Special sampling`, `Play sampling effect`, `Effect speed`, `Effect strength`
  управляют эффектом. `Play animation` отдельно управляет движением геометрии.
- `UV repeat`, `Emissive strength`, `Shininess` доступны для выбранного объекта.
- `All lights off` и `Restore lights` выключают/включают источники;
  `Reset scene` восстанавливает начальные значения, включая списки источников.
- Камера, ambient/directional/point/spot и их маркеры/конусы сохранены из ЛР2.

Повторяемые запуски из build-каталога:

```bat
Debug\cg-lab3.exe --smoke-test
Debug\cg-lab3.exe --demo=sampling
Debug\cg-lab3.exe --demo=emission
Debug\cg-lab3.exe --demo=specular
Debug\cg-lab3.exe --demo=caps
Debug\cg-lab3.exe --demo=seam
Debug\cg-lab3.exe --demo=base --frames=120
```

`--smoke-test` сам завершает работу после 240 итераций, меняет материалы,
эффекты, свет, камеру, размер окна, сворачивает/восстанавливает его и проверяет
ошибки validation. Обычный запуск продолжается до закрытия окна.

На этой машине обычный запуск обнаружил две отсутствующие JSON-записи
Steam overlay. Они сохранены в `docs/smoke-system-stderr.log`. Из папки
`lab3` можно запускать `tools\run.cmd` (или `tools\run.cmd --smoke-test`):
он ограничивает поиск сторонних implicit layers окружением этого процесса,
сохраняя explicit Khronos validation и synchronization validation.
Реестр, SDK и драйвер не меняются. Контрольный запуск на AMD Radeon прошёл
240 итераций с нулевыми validation/general errors; NVIDIA и FPS не проверялись.

## Ресурсы и ограничения

В `assets/textures` шесть оригинальных PNG: `uv_grid`, `stripes`, `panels`,
`panels_specular`, `panels_emissive`, `black`. Первые три — albedo.
Grid и Stripes действительно не имеют дополнительных файлов и используют
общий чёрный ресурс в обоих slots. Для отсутствующих optional-карт Panels
также назначается чёрный fallback с сообщением в логе. Повреждённая карта
или отсутствующая обязательная albedo дают ошибку и завершение с cleanup.

PNG уже включены в Git. Необязательный генератор
`python tools/generate_textures.py` требует Pillow, приложение — нет.
Авторство, закрепление decoder и лицензии: [ATTRIBUTION](assets/ATTRIBUTION.md).
Один mip level, repeat/linear, без anisotropy. Эмиссия не создаёт bloom или свет
на соседях. Синхронный upload при редактировании конусов выбран ради простоты;
при неизменной геометрии конусов повторной загрузки нет. FPS и проверка на
второй GPU не заявляются без измерений.

Материалы защиты: [руководство](LAB3_GUIDE.md), [карта кода](CODE_MAP.md),
[показ по пунктам](DEMO_CHECKLIST.md), [защита 15 минут](DEFENSE_15MIN.md),
[тренировка](DEFENSE_PRACTICE.md), [черновик отчёта](REPORT_DRAFT.md).
