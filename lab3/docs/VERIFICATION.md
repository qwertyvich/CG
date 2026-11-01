# Фактическая проверка ЛР3, 01.11.2026

Среда: Windows x64, Visual Studio 2022 Build Tools / MSVC 19.44.35229,
CMake, Vulkan SDK 1.4.321.1. Фактический выбранный GPU:
**AMD Radeon(TM) Graphics**. Проверка на NVIDIA не выполнялась.

| Требование | Реализация | Проверка | Результат |
| --- | --- | --- | --- |
| Самостоятельная копия ЛР2 | source/shaders/assets/CMake внутри lab3 | Чистый configure в новом build-vs2022; перенос exe+assets+shaders в portable-runtime | Сборка и запуск успешны, соседние ресурсы не нужны |
| Прошлые работы неизменны | SHA-256 baseline до копирования | verify_runtime.py сверил 62 файла lab1/lab2 | Все хеши совпали; исходные незакоммиченные правки сохранены |
| PNG из файлов, сторонний decoder | stb_image, image.cpp | Все шесть PNG декодированы; тесты размера и ориентации | Успех; decoder vendored по точному commit |
| Ошибки обязательных изображений | Проверка заголовка/размера, диагностика пути | Отдельно отсутствующая и повреждённая albedo в portable-копии | Ожидаемый exit=1, cleanup без validation messages |
| UV боковины/крышек/шва | geometry.cpp, 206 вершин / 600 индексов | CPU UV interval/winding/manifold + реальные caps/seam кадры | Шов не растягивает всю текстуру; надписи читаются снаружи |
| Device-local изображения | optimal, SAMPLED + TRANSFER_DST, VMA required DEVICE_LOCAL | Реальные allocation flags и synchronization validation | memory_flags=1, DEVICE_LOCAL=1 для всех шести |
| Device-local vertex/index | Staging, copy, transfer -> vertex/index barrier | Лог цилиндра, кубов, линий; CPU финальный буфер не заполняет | DEVICE_LOCAL=1, ошибок синхронизации нет |
| Один dynamic model UBO | UNIFORM_BUFFER_DYNAMIC, два слота | Лог allocation + bind с pDynamicOffsets | buffers=1, sizeof=176, alignment=16, stride=176, offsets=0/176, range=176 |
| Независимые модели | Отдельные ModelState и UBO slots | CPU-тест независимости + два объекта на кадрах | Успех |
| Доп 1, три материала | Три готовых material sets | Smoke меняет материалы каждый кадр, назначает один обоим | Успех, descriptors не переписываются при выборе |
| Доп 2, две выборки/модуляция/время | lighting.glsl, SamplingPush=32 bytes | Шейдеры собраны/валидированы; sampling screenshot, smoke pause/resume | Эффект только у первого, paused phase совпадает в двух строках лога |
| Обычная база без sampling push | Отдельный BASE_ONLY shader/pipeline | Исходник и SPIR-V обычного режима, base screenshot | Push constant block в базовом fragment shader отсутствует |
| Доп 3, specular | Маска умножает только specular Blinn-Phong | Specular demo: слева разрешён, справа specular color=0, emissive=0 | Реальный кадр показывает ограниченный маской блик |
| Доп 3, emissive | Слагаемое после света | Emission demo: все lights off, рядом black-fallback объект | Голубые участки видны, прочие поверхности тёмные |
| Чёрный fallback | Один загруженный black.png 1x1 в обоих optional slots | Grid/Stripes + удаление обеих optional-карт Panels | maps=3,0,0 и два сообщения fallback; exit=0 |
| Цветовое пространство | Albedo/emissive SRGB, mask UNORM; swapchain encode | Анализ форматов и displayColor, графический запуск | Сложение линейное; двойного gamma encode нет |
| Resize/minimize/restore/закрытие | Инфраструктура из ЛР2, сохранён depth selection | Smoke: 1024x768 -> 1400x800, iconify/restore, 240 итераций | Контрольный exit=0, validation_errors=0, general_errors=0 |
| Компиляция шейдеров | Пять outputs glslc в build/shaders | spirv-val --target-env vulkan1.1 для всех пяти | Все exit=0 |

## Логи и кадры

- [build.log](build.log): фактическая успешная сборка Debug x64.
- [cpu-tests.log](cpu-tests.log): CTest, 1/1 тест, включает новые проверки ЛР3.
- [spirv-val.log](spirv-val.log): пять успешно проверенных SPIR-V файлов.
- [runtime-checks.log](runtime-checks.log): portable run, negative cases, хеши.
- [smoke-stdout.log](smoke-stdout.log), [smoke-stderr.log](smoke-stderr.log):
  контрольный GPU-запуск. Пустой stderr — реальный результат, файл сохранён.
- `optional-missing-*`, `albedo-missing-*`, `albedo-corrupt-*`: соответствующие
  реальные проверки, включая ожидаемые сообщения ошибок.
- [base](screenshots/base.png), [sampling](screenshots/sampling.png),
  [emission](screenshots/emission.png), [specular](screenshots/specular.png),
  [caps](screenshots/caps.png), [seam](screenshots/seam.png): реальные кадры
  клиентской области приложения. Скрипт не рисует и не редактирует содержимое.

## Особенность окружения и честные ограничения

Первый полный запуск в системном окружении дал **validation_errors=0,
general_errors=2, status=1**. Причина двух GENERAL messages — зарегистрированные,
но отсутствующие JSON SteamOverlay/SteamFossilize. Исходные сообщения сохранены
в [smoke-system-stderr.log](smoke-system-stderr.log), итог —
[smoke-system-stdout.log](smoke-system-stdout.log). Этот запуск не объявляется
полностью чистым. Callback и счётчики ошибок в программе не отключены.

Для контрольного запуска `verify_runtime.py --clean-implicit-layers` задаёт
**только окружению дочернего процесса** VK_IMPLICIT_LAYER_PATH на пустой
build-каталог. Explicit VK_LAYER_KHRONOS_validation, включая synchronization
validation, по-прежнему запрошен в initialize. Системные настройки/реестр,
SDK и драйвер не изменялись. Такой override пути описан в
[официальной документации loader](https://vulkan.lunarg.com/doc/view/latest/windows/LoaderLayerInterface.html#windows-layer-discovery).

CPU и GPU проверены на этой машине. Тестирование NVIDIA, измерение FPS,
максимального количества источников, длительная нагрузка и profiling памяти
не выполнялись. Сеть ОС физически не отключалась: проверен переносимый запуск
с локальными файлами, приложение не содержит получения ресурсов из сети.
Отсутствие validation messages подтверждает проверенные пути, но не заменяет
проверку драйвером/профайлером всех возможных сценариев.

## Воспроизведение

Из lab3 после сборки:

```bat
ctest --test-dir build-vs2022 -C Debug --output-on-failure
python tools/verify_runtime.py --clean-implicit-layers
python tools/capture_demos.py --clean-implicit-layers
python tools/update_code_map.py
```

Runtime-скрипт требует только Python stdlib; capture дополнительно Pillow.
Приложение этих зависимостей не требует. На машине без битых overlay-записей
можно убрать `--clean-implicit-layers`. Для проверки хешей добавьте
`--check-baseline` только в исходной рабочей папке: в свежем clone предыдущие лабы могут отличаться
от сохранённых локальных правок, поэтому факт неизменности относится именно
к этой сессии и исходному состоянию пользователя.
