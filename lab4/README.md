# ЛР4 — наложение теней

Вариант 8: цилиндр, 50 сегментов окружности. Порядковый номер студента — 20.

Самостоятельная копия текущей ЛР3: два цилиндра, файловые albedo/specular/emissive карты, камера, Блинн–Фонг, динамические списки света, маркеры/конусы и sampling-эффект сохранены. Добавлены пол, тень направленного источника и **тень одного выбираемого прожектора**. Каждый активный источник каждый кадр заполняет свою depth-карту через Vulkan Dynamic Rendering. Оценку определяет преподаватель.

## Сборка в Windows (CMD)

Нужны Visual Studio 2022 Build Tools с C++, Windows SDK, CMake 3.20+, Git и Vulkan SDK с `glslc`. Первая конфигурация загружает закреплённые версии GLFW, vk-bootstrap, VMA и ImGui. После сборки запуск использует только локальные shaders/assets.

Из корня CG (если `cmake` отсутствует в PATH, используйте полный путь к cmake.exe из установленного VS):

```bat
cmake -S lab4 -B lab4/build-vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build lab4/build-vs2022 --config Debug
ctest --test-dir lab4/build-vs2022 -C Debug --output-on-failure
cd lab4\build-vs2022
Debug\cg-lab4.exe
```

Рабочая директория — **lab4/build-vs2022**: в ней лежат свои `assets/` и `shaders/*.spv`. CMake не читает соседние лабораторные. Для переноса готовой программы скопируйте `Debug/cg-lab4.exe`, `assets/` и `shaders/` в один каталог; для Debug нужен установленный Debug CRT Visual Studio. Для обычного распространения соберите Release с соответствующим MSVC runtime.

На этой машине CMake установлен по адресу:

```bat
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S lab4 -B lab4/build-vs2022 -G "Visual Studio 17 2022" -A x64
```

## GPU и API

Реализован **core Vulkan 1.3**. При старте проверяются device API, наличие `VK_KHR_dynamic_rendering` и `dynamicRendering` через features2; vk-bootstrap выбирает подходящее устройство и передаёт `dynamicRendering=VK_TRUE` при создании device. Core entry points загружаются через `vkGetDeviceProcAddr` и проверяются. KHR-путь Vulkan 1.2 не реализован. При отсутствии подходящей GPU программа выдаёт диагностику вместо подмены shadow pass обычным render pass.

Две карты 2048×2048, depth-формат выбирается по поддержке attachment + sampled image. Comparison sampler: `compareEnable=TRUE`, `LESS`, `NEAREST`. PCF, point-light/cubemap shadows и CSM не реализованы. Основной экранный проход и ImGui используют прежние render passes; только shadow passes переведены на Dynamic Rendering.

Проверено на NVIDIA GeForce RTX 3050 Laptop GPU, device API 1.3.289, core Dynamic Rendering. Debug build, CPU tests, 6 SPIR-V файлов и GPU smoke прошли. Полные результаты и ограничения приведены в [docs/VERIFICATION.md](docs/VERIFICATION.md).

## Управление

- `Directional shadow demo` — вся сцена сбрасывается в режим солнца и слабого ambient.
- `Spot shadow demo` — вся сцена сбрасывается; солнце выключено, включён выбранный прожектор.
- `Combined shadows` — вся сцена сбрасывается; солнце и прожектор работают одновременно.
- `Shadows`: тени отдельно от включения источников, половина размера покрытия солнца, constant/slope depth bias и выбор spot по стабильному ID.
- `Camera` меняет наблюдателя. `Model and material` выбирает цилиндр или пол, их трансформацию/материал. Анимация перемещает цилиндры; пол остаётся на месте.
- `Ambient and directional`, `Point lights`, `Spot lights` управляют освещением. Маркеры и конусы в `Light visualization` — отдельные подсказки, их видимость не включает свет или тени.

После удаления выбранного spot выбирается первый оставшийся. При пустом списке spot-тень отключена; карты/descriptors сохраняют валидные ресурсы. При добавлении автоматически выбирается первый доступный spot. У конуса меньше 0.1° тень пропускается; сам свет по-прежнему вычисляется по конусу ЛР3. Near/far света фиксированы: солнце 0.1–60, spot 0.1–50; направление нулевой длины заменяется на (0,−1,0).

## Проверки и материалы

```bat
python lab4\tools\verify_runtime.py --clean-implicit-layers --check-baseline
python lab4\tools\capture_demos.py --clean-implicit-layers
python lab4\tools\update_code_map.py
```

Первый скрипт использует только Python standard library. Второму нужен Pillow (`python -m pip install Pillow`) и доступный Windows desktop. `--check-baseline` применим только в исходной рабочей папке, поскольку хеши описывают её исходные lab1–lab3.

`tools/run.cmd` изолирует поиск сторонних implicit layers для запуска: это обход сломанных Steam overlay manifests на этой машине. Explicit Khronos validation и synchronization validation в программе включены. Обычный запуск и изолированный запуск различаются только поиском implicit layers; подробности и реальные логи — в проверках.

Для защиты: [объяснение ЛР4](LAB4_GUIDE.md), [карта кода](CODE_MAP.md), [все элементы UI](UI_REFERENCE.md), [показ базы и допа](DEMO_CHECKLIST.md), [выступление на 15 минут](DEFENSE_15MIN.md), [тренировка](DEFENSE_PRACTICE.md).
