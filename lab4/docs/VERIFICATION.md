# Фактическая проверка ЛР4

Проверено 20 ноября 2026 года в Windows, в самостоятельной `lab4`. Основа — копия текущих исходников ЛР3. Исходный PDF не был предоставлен в доступных файлах; соответствие проверялось по подробным требованиям из [TASK_REQUIREMENTS.md](TASK_REQUIREMENTS.md). Оценку определяет преподаватель.

## Окружение и API

- MSVC 19.44.35229, Visual Studio 17 2022 Build Tools, x64 Debug; CMake 3.31.6; Vulkan SDK 1.4.321.1.
- Instance запрошен и создан с API 1.3.
- **Выбрана NVIDIA GeForce RTX 3050 Laptop GPU, device API 1.3.289**, driver 566.07 по vulkaninfo. Проверен core Dynamic Rendering, feature явно включена через vk-bootstrap features chain, core entry points загружены и проверены.
- Аудит также обнаружил AMD Radeon(TM) Graphics API 1.2.196: расширение VK_KHR_dynamic_rendering присутствует, feature=true. Однако KHR execution path в этом проекте не реализован, запуск на AMD не проверялся. Это не вывод о неспособности AMD поддерживать функцию.
- Shadow format: **D32_SFLOAT** (VkFormat=126), проверены optimal attachment + sampled-image features и image-format query. Две карты **2048×2048**, отдельные comparison samplers LESS/NEAREST.
- Один model UBO: sizeof(ModelGpu)=176, minUniformBufferOffsetAlignment=64, stride=192, три записи со смещениями 0/192/384. Пол: 4 вершины / 6 индексов; цилиндр: 206 вершин / 600 индексов.

Реальные сообщения старта: [portable-combined-stdout.log](portable-combined-stdout.log).

## Сборка и численные проверки

Выполнено из корня CG:

```bat
cmake -S lab4 -B lab4/build-vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build lab4/build-vs2022 --config Debug --parallel 6
ctest --test-dir lab4/build-vs2022 -C Debug --output-on-failure
```

Использовался полный путь к установленному в VS `cmake.exe`/`ctest.exe`, поскольку CMake не в PATH. Конфигурация FetchContent загружает собственные зависимости, закреплённые по тегам, через shallow clone. Прежние build/cache/shaders ЛР3 не использовались.

Результат: сборка прошла; **1/1 CPU test passed**. Внутри этого теста проверены geometry/UV/PNG/material regression из ЛР3, floor winding/count, inverse-transpose normals, LookAt eye/forward/ортонормальность и вертикальное/нулевое направление, Vulkan near/far depth, spot FOV=2×outer при углах 0.1/0.5/38/89°, независимость light VP от observer camera, неподвижность пола при общей анимации, стабильный spot ID после erase/clear/re-add, отключённый свет и нулевой конус.

Все **6 SPIR-V файлов** прошли `spirv-val --target-env vulkan1.3`: lab4.vert, lab4.frag, lab4-base.frag, shadow.vert, unlit.vert, unlit.frag.

Логи: [configure.log](configure.log), [build.log](build.log), [cpu-tests.log](cpu-tests.log), [spirv-val.log](spirv-val.log).

## Требование → реализация → проверка → результат

| Требование | Реализация | Проверка | Фактический результат |
|---|---|---|---|
| Depth-карта каждый кадр вне экрана | `shadows::record` | 60 кадров для каждого режима + counters | Sun-only: (60,0), spot-only: (0,60), combined: (60,60); без теней (0,0) |
| Dynamic Rendering без shadow RenderPass/Framebuffer | `initializePipeline`, `record` | Код, загрузка entry points, runtime validation | Pipeline renderPass=null, stageCount=1 vertex, colorAttachmentCount=0; core Begin/EndRendering |
| Shadow pass до главного | `renderer::render` | Порядок command recording и synchronization validation | Sun → spot → основной render pass → ImGui; ошибок validation нет |
| LookAt + orthographic солнца | `buildShadows`, `lookAt` | Численные тесты и реальные directional/model/camera/sun-shift кадры | Устойчивая матрица; тени видны на полу; изменения состояния не вызывают NaN/ошибок API |
| Comparison sampler LESS / sampler2DShadow | `initializeImages`, `shadows.glsl` | Runtime validation + A/B shadow On/Off | Настоящее hardware comparison sampling; 1=visible, 0=blocked |
| UI coverage и bias | `drawControls`, `vkCmdSetDepthBias` | Small coverage, zero/large bias кадры; smoke крайних значений | Покрытие обрезает данные; bias=0 даёт видимое acne; default устраняет полосы. Чрезмерный slope демонстрируется отдельно |
| Пол принимает тень цилиндров | `generateFloor`, общий `drawModels` | Снимки directional и spot без солнца | Тень на настоящих треугольниках пола, не текстурная имитация |
| Доп: выбираемый spot shadow | `selectedShadowSpot`, `buildShadows` | Spot-only A/B; smoke выбора/erase/clear/re-add/intensity/toggle | Отдельная perspective карта, FOV=2×outer, без invalid descriptors/crash |
| Обе карты одновременно | `ShadowGpu`, два sampler binding | Combined A/B, оба counters=60 | Разные тени двух источников видны одновременно; visibility применяется раздельно |
| Сохранение ЛР3 | Shared lighting + model UBO + textures | Оба lit variants, emission, SSBO growth, CPU regression | Sampling получает тени; эмиссия остаётся при выключенном освещении; point/другие spot не читают чужую карту |
| Синхронизация и lifetime | Fence + image barriers + cleanup | 240-frame smoke, repeats, штатное закрытие | validation_errors=0, general_errors=0, status=0 в изолированном запуске |
| Resize/minimize/restore | Прежний swapchain flow + shadow maps фиксированного размера | Smoke resize на кадрах 100/120, minimize/restore на 200 | Завершение без зависания и validation errors |
| Неизменность lab1–lab3 | SHA-256 baseline | `verify_runtime.py --check-baseline` | 114 исходных файлов совпадают с baseline |

## GPU acceptance

```bat
python lab4\tools\verify_runtime.py --clean-implicit-layers --check-baseline
```

Проверки выполнены на отдельной копии готовой программы в `lab4/build-vs2022/portable-runtime` со своими assets и shaders. Запуск не читает исходники/соседние лабораторные и не требует сети.

Smoke из 240 кадров программно меняет те же параметры сцены, что доступны в UI: nonuniform scale, движение и анимация, observer camera, вертикальное/нулевое направление, coverage, bias, growth SSBO до 34 источников, равные/нулевые/очень узкие/wide spot cones, выбор другого ID, удаление предшествующего и выбранного, clear/add, нулевую интенсивность, reset/toggles, emissive, resize/minimize/restore. Отдельные короткие запуски проверяют каждую активную карту **на каждом** из 60 кадров при статичной сцене.

Дополнительные проверки: отсутствие optional specular/emissive файлов приводит к чёрному fallback и exit=0; отсутствие/повреждение обязательной albedo даёт понятную ошибку и ожидаемый exit=1, без Vulkan validation errors. Проверки не являются ручным прокликиванием каждого UI-контрола; интерфейс сверён с исходниками и итоговыми кадрами.

Логи: [runtime-checks.log](runtime-checks.log), [smoke-stdout.log](smoke-stdout.log), [smoke-stderr.log](smoke-stderr.log). Отсутствие validation errors не является доказательством отсутствия любых возможных ошибок на других GPU.

## Реальные кадры и A/B

```bat
python lab4\tools\capture_demos.py --clean-implicit-layers
```

16 клиентских кадров захвачены из работающего Vulkan-приложения с нормальным закрытием через WM_CLOSE. Перед захватом скрипт ждёт реальный отрисованный кадр; стартовая белая поверхность не используется. Все кадры проверены на размер 1280×720, а режимы солнца/spot/combined/sampling/emission/coverage/bias/движения просмотрены визуально.

Для A/B сравнения вырезается UI (остаётся X=420…1279); положение камеры, материалы и свет одинаковы. Меняются только shadow toggles. Sampling для этой пары заморожен. Порог — изменение более 12 уровней хотя бы в одном RGB канале:

| Сравнение | Изменённых пикселей сцены | Неожиданно посветлевших пикселей при включении теней |
|---|---:|---:|
| Directional On / Off | 12013 | 0 |
| Spot On / Off, солнце выключено | 14611 | 0 |
| Combined On / Off | 24948 | 0 |
| Sampling On / Off | 12013 | 0 |

Прямые ссылки: [солнце](screenshots/directional.png), [без тени солнца](screenshots/no-shadows.png), [spot без солнца](screenshots/spot.png), [совместный режим](screenshots/combined.png), [sampling](screenshots/sampling-static.png), [эмиссия без света](screenshots/emission.png), [bias=0: acne](screenshots/zero-bias.png), [большой bias](screenshots/large-bias.png), [маленькое покрытие](screenshots/small-coverage.png), [движение модели](screenshots/model-shift.png), [движение камеры](screenshots/camera-shift.png), [изменение spot](screenshots/spot-shift.png). Метрики: [visual-checks.log](visual-checks.log).

## Особенность системного окружения

Обычный запуск без изоляции implicit layers завершился с exit=1 из-за **GENERAL loader messages** об отсутствующих SteamOverlayVulkanLayer64.json и SteamFossilizeVulkanLayer64.json. Также loader предупреждает о версии AMD switchable layer. Это не ошибки алгоритма теней и не исправление драйвера. Реальный вывод: [normal-stderr.log](normal-stderr.log).

`tools/run.cmd` задаёт VK_IMPLICIT_LAYER_PATH в пустой каталог только для дочернего процесса. Это изолирует сторонние implicit overlay layers. Явно запрошенные `VK_LAYER_KHRONOS_validation` и synchronization validation **не отключаются**. Все успешные acceptance/capture запуски сделаны именно с этой изоляцией; системные manifests/реестр не менялись.

## Ограничения результата

Проверен Windows Debug на указанной NVIDIA. Другие OS/GPU, KHR-путь Vulkan 1.2, Release packaging и длительное профилирование памяти отдельно не проверялись. Реализованы тени солнца и одного выбранного spot; point shadows, PCF, CSM и preview depth maps отсутствуют. Разрешение фиксировано, near/far фиксированы и могут не покрыть произвольно далёкие трансформации. Вне карты visibility=1. Слишком большой bias может отрывать/сокращать тени — это демонстрация ограничения метода.

Все обязательные технические пункты предоставленного Markdown-задания реализованы и проверены в описанном объёме; оригинальный PDF остаётся несверенным, поскольку его нет среди доступных файлов.
