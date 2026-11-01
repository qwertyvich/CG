# Лабораторная №2: камера и освещение

Вариант 8, порядковый номер 20: цилиндр радиуса 1, высоты 2, 50 сегментов. Самостоятельный проект `cg-lab2`, полученный из текущего `lab1`. Исходный каркас: [Vladimir Bakharev / vulkan-starter-app](https://github.com/vladeemerr/vulkan-starter-app). Авторство, LICENSE и NOTICE сохранены.

| Задание | Что сделано | Статус |
| --- | --- | --- |
| База | Камера XYZ/Euler/FOV, процедурные нормали, материал Блинн–Фонга, ambient и directional через ImGui | Реализовано, собрано, запущено |
| Доп 1 | Scene: UBO + два SSBO; Model: отдельный UBO, два настоящих descriptor sets | Реализовано, validation проверена |
| Доп 2 | Динамический массив point, добавление/удаление, цвет/позиция/intensity/enabled, `1/r²` | Реализовано, рост и пустой список проверены |
| Доп 3 | Динамический массив spot, направление, полууглы, мягкие края, `1/r²` | Реализовано, граничные случаи проверены |
| Доп 4 | Unlit кубы, белые внешние/синие внутренние конусы и оси; независимое скрытие | Реализовано, реальные кадры сохранены |

Это технический объём базы и всех четырёх дополнений. Оценку выставляет преподаватель.

## Сборка и запуск в Windows CMD

Проверено 01.11.2026: VS 2022 Build Tools, MSVC 19.44, Windows SDK 10.0.26100.0, Vulkan SDK 1.4.321.1, архитектура **x64**, конфигурация Debug. Команды по одной строке:

```bat
cd /d C:\Users\user\Desktop\CG\lab2
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
cmake -S . -B build-vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2022 --config Debug --target cg-lab2 --parallel 6
build-vs2022\Debug\cg-lab2.exe
```

Первая конфигурация требует интернета для FetchContent. Не используйте в этом каталоге Ninja или старый cache ЛР1. Альтернатива из `lab2`: `cmake --preset msvc-debug`, затем `cmake --build --preset msvc-debug`.

Шейдеры автоматически собираются в `build-vs2022/shaders`. Путь передаётся приложению через `LAB2_SHADER_DIR`: оно не читает `.spv` из ЛР1 и может запускаться из другой рабочей папки. После переноса build-каталога выполните configure/build заново.

## Зависимости

| Компонент | Роль | Откуда берётся |
| --- | --- | --- |
| C++20 / MSVC | CPU-логика и Vulkan-вызовы | Установленные VS Build Tools |
| Vulkan | Устройство, buffers, descriptors, pipelines, команды | `find_package(Vulkan REQUIRED)` находит установленный SDK |
| GLFW 3.5.1 | Окно, события, Vulkan surface | FetchContent из Git, закреплённый tag |
| vk-bootstrap v1.4.321 | Создание instance/device и начального swapchain | FetchContent |
| VMA v3.4.0 | Выделение/отображение/flush памяти | FetchContent |
| Dear ImGui v1.92.9 | Панель параметров | FetchContent |
| glslc | GLSL → SPIR-V при обычной сборке | `find_program` в PATH / Vulkan SDK |

Версии унаследованы из ЛР1. GLM не добавлен: собственных проверенных Vec3/Mat4 достаточно для TRS, камеры и проекций. Текстуры, staging buffer, тени и PBR не входят в эту работу.

## Управление

Откройте нужную секцию панели. Для точного ввода в Drag/Slider используйте Ctrl+щелчок. Камера полностью управляется ImGui, отдельные клавиши или мышиный захват не нужны.

- **Camera**: XYZ, Euler XYZ в градусах, Perspective, FOV или Ortho half height; Reset camera.
- **Model and material**: позиция/поворот/положительный масштаб, линейные albedo/specular color, shininess, необязательная окраска вершин и анимация. Старт неподвижный.
- **Ambient and directional**: цвет, интенсивность, направление лучей, enabled; отдельные переключатели ambient/diffuse/specular.
- **Point lights / Spot lights**: Add, Clear, раскрываемые записи с устойчивыми ID; Remove внутри записи. Spot задаётся полууглами: полный угол вдвое больше.
- **Light visualization**: Show light markers / Show spot cones. Они меняют только изображение помощников. Белый контур — внешний угол, синий — внутренний. Длина конуса условная и не ограничивает дальность света.
- **Reset scene** возвращает камеру, материал, один point и один spot. Выключенные лампы остаются видимыми условными маркерами.

Все цвета UI трактуются как линейные RGB. Свет складывается линейно, затем ограничивается диапазоном [0,1]. sRGB attachment кодирует результат аппаратно; для UNORM используется одно явное sRGB-кодирование. Тон-маппинга нет: очень яркий свет может насыщаться.

## Проверки

```bat
cmake --build build-vs2022 --config Debug --target lab2-math-tests
ctest --test-dir build-vs2022 -C Debug --output-on-failure
build-vs2022\Debug\cg-lab2.exe --smoke-test
python tools\verify_lab1.py
python tools\update_code_map.py
```

`--smoke-test` завершает приложение после 240 итераций: пустые массивы, 33 point + 33 spot, удаление первого/среднего/последнего, равные углы, нулевые направления/intensity, масштаб, камера, resize, сворачивание, reset. При ошибках Vulkan validation или приложения код завершения ненулевой. Общие ошибки loader считаются отдельно и **выводятся полностью**.

На этой машине выбран **AMD Radeon(TM) Graphics**; проверка на NVIDIA отдельно не выполнялась. После исправления обработки minimize: 0 ошибок validation, 2 общих сообщения loader о недостающих Steam JSON. Точные тексты и границы проверки — в [таблице результатов](LAB2_GUIDE.md#проверки-и-фактические-результаты), [stdout](docs/smoke-stdout.log), [stderr](docs/smoke-stderr.log). Эти логи нельзя называть полностью чистыми.

Количество источников не ограничено константой GLSL. UI показывает count/capacity и предел по `maxStorageBufferRange`; память CPU/GPU и время кадра также конечны. При ошибке выделения приложение сообщает об ошибке и корректно завершает работу. Работа на других GPU/драйверах и длительный stress-тест на пределе памяти не проверялись.

## Подготовка к защите

[Понятное руководство](LAB2_GUIDE.md), [карта кода с номерами строк](CODE_MAP.md), [доклад на 15 минут](DEFENSE_15MIN.md), [вопросы и упражнения](DEFENSE_PRACTICE.md), [черновик отчёта с реальными кадрами](REPORT_DRAFT.md).

Источник задания — предоставленный PDF «Lab #2 Camera and Lighting», все три страницы. Дополнительное объяснение типов света: [LearnOpenGL: Light casters](https://learnopengl.com/Lighting/Light-casters), ссылка преподавателя. В статье есть полиномиальное затухание; здесь по PDF используется именно `1/r²`. Файлы лекций №3/№4 и образец отчёта не приложены, их содержание не реконструировалось.
