# Фактическая проверка ЛР2

Дата: 01.11.2026. Рабочий каталог: `C:\Users\user\Desktop\CG\lab2`.

## Среда и результат

- MSVC 19.44.35229, Visual Studio 17 2022 Build Tools, Windows SDK 10.0.26100.0; `-A x64`, Debug.
- CMake из установленного VS; Vulkan SDK 1.4.321.1.
- Фактически выбранный GPU: **AMD Radeon(TM) Graphics**.
- `maxStorageBufferRange=4294967295`; stride PointGpu=32, SpotGpu=64. Это только аппаратный предел binding, а не обещание доступной памяти или скорости для такого числа ламп.
- Configure выполнен в новом `lab2/build-vs2022`; затем выполнена полная `--clean-first` сборка. Результат успешный; [лог](build.log).
- CTest: **1/1 PASS**. Тест проверяет 202/600, единичные и раздельные нормали, winding, невырожденность, замкнутость welded mesh, обратную камеру, глубину [0,1], ортогональность normal matrix, `1/r²`, smoothstep/step, zero direction, вертикальный конус и пустые списки.
- `spirv-val --target-env vulkan1.1`: PASS для `lab2.vert`, `lab2.frag`, `unlit.vert`, `unlit.frag`.
- `spirv-dis`: Scene offsets 0/64/80/96/112/128/144, Model 0/64/128/144/160, array stride Point=32, Spot=64; совпадают с C++ static_assert.
- Финальный smoke: **240 итераций, validation_errors=0, general_errors=2, status=0**, exit code 0. [stdout](smoke-stdout.log), [stderr](smoke-stderr.log).
- Сохранение ЛР1: **все 15 SHA-256 совпадают** с [исходным снимком](lab1-baseline.json); проверка `python tools/verify_lab1.py`.

## Динамические сценарии

Smoke выполняет: оба пустых списка; рост до 33 point + 33 spot; удаление первого, среднего, последнего с 30 оставшимися; нулевые direction/intensity; inner=outer; ненулевой Euler и неравномерный масштаб; камеру XYZ/Euler/FOV и ортографию; скрытие помощников; ambient-only, directional specular-only и point+spot без ambient; reset и повторное добавление; размеры 1024×768 и 1400×800; iconify/restore с ожиданием события.

Отдельно через реальные события интерфейса выполнены:

- Ambient on/off. Сравнение области цилиндра меняется; область unlit point marker сохраняется.
- Show light markers и Show spot cones выключены. Помощники исчезают; свет на центральной области цилиндра сохраняется.
- FOV изменён, затем Reset camera.
- Через Ctrl+щелчок введены Model Scale=(0.5,1.8,1.2) и Y Rotation=40°; получен реальный кадр.
- Для point и spot: Add два раза, Clear, Add. В UI наблюдался рост capacity с 1 до 4 и валидный повторный источник. Конечная запись spot имела новый устойчивый ID.
- Три отдельных цикла minimize с паузой **500 мс** и restore: окно остаётся доступным и продолжает рисовать; в stderr этих запусков нет сообщений validation.
- Штатное закрытие WM_CLOSE и несколько повторных запусков. Сообщений validation/VMA о неосвобождённых объектах не получено; внешний профилировщик утечек не использовался.

Реальные скриншоты: [старт](screenshots/01-initial.png), [помощники скрыты](screenshots/02-light-without-helpers.png), [масштаб и поворот](screenshots/03-nonuniform-scale.png). Это кадры приложения; генерация изображений не применялась.

## Сообщения и найденная ошибка

В итоговом stderr остались два сообщения **GENERAL**, полностью:

```text
GENERAL: loader_get_json: Failed to open JSON file C:\Program Files (x86)\Steam\SteamOverlayVulkanLayer64.json
GENERAL: loader_get_json: Failed to open JSON file C:\Program Files (x86)\Steam\SteamFossilizeVulkanLayer64.json
```

Это отсутствующие JSON установленных ранее implicit layers Steam. Они не являются VUID команд приложения. Сообщения не фильтруются и не подавляются; общие ERROR-сообщения имеют отдельный счётчик. Khronos validation и synchronization validation включены. Реестр/Steam не менялись. В некоторых промежуточных resize-запусках встречалось `Swapchain is suboptimal for rendering!`; это сигнал пересоздать swapchain, а не ошибка проверки Vulkan-команд.

Первое ручное сворачивание выявило гонку: vk-bootstrap мог прочитать уже нулевой currentExtent при rebuild. Возникал `VUID-VkSwapchainCreateInfoKHR-imageExtent-01689`, а ошибочное повторное освобождение старых handles давало `VUID-vkDestroyFramebuffer-framebuffer-parameter`. Исправлено: события и iconified проверяются до кадра; prepare откладывает rebuild; сам rebuild берёт один снимок caps и не создаёт нулевые изображения; удалённые handles/векторы очищаются. Итоговые логи получены **после** исправления.

## Границы проверки

NVIDIA, Linux, долгий stress на предельном числе ламп и субъективная визуальная оценка всех сочетаний цветов/ширины блика не проверялись. Математические границы конуса/attenuation и код формулы проверены, но это не заменяет ручную демонстрацию на компьютере защиты. Большая интенсивность намеренно может насыщать изображение: тон-маппинга нет.

## Повторение

Из `lab2`, в Windows CMD с CMake в PATH:

```bat
cmake --build build-vs2022 --config Debug --parallel 6
ctest --test-dir build-vs2022 -C Debug --output-on-failure
build-vs2022\Debug\cg-lab2.exe --smoke-test
python tools\verify_lab1.py
C:\VulkanSDK\1.4.321.1\Bin\spirv-val.exe --target-env vulkan1.1 build-vs2022\shaders\lab2.vert.spv
C:\VulkanSDK\1.4.321.1\Bin\spirv-val.exe --target-env vulkan1.1 build-vs2022\shaders\lab2.frag.spv
C:\VulkanSDK\1.4.321.1\Bin\spirv-val.exe --target-env vulkan1.1 build-vs2022\shaders\unlit.vert.spv
C:\VulkanSDK\1.4.321.1\Bin\spirv-val.exe --target-env vulkan1.1 build-vs2022\shaders\unlit.frag.spv
```
