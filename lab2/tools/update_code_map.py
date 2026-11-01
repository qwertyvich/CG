"""Generate exact source locations and the timed defense after code changes."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# key, request, file, unique search anchor, short explanation
ENTRIES = [
    ("geometry", "Процедурные нормали цилиндра", "source/geometry.cpp", "Mesh generateCylinder()", "202 записи: боковина и крышки имеют разные нормали; 600 индексов, шов замыкается по модулю 50."),
    ("camera", "Настоящая матрица V", "source/scene.cpp", "Mat4 cameraView(", "Обратная T*Rz*Ry*Rx: Rx^-1*Ry^-1*Rz^-1*T^-1."),
    ("fov", "FOV и Vulkan projection", "source/scene.cpp", "Mat4 perspective(", "Градусы → радианы, depth [0,1], один переворот Y."),
    ("camera_ui", "Изменение камеры через ImGui", "source/controls.cpp", 'if(ImGui::CollapsingHeader("Camera",', "XYZ/Euler/FOV/orthographic/Reset camera."),
    ("uniforms", "Model / normal matrix / материал", "source/scene.cpp", "void buildUniforms(", "M=T*R*S, normal=R*S^-1, VP=P*V; параметры материала и флаги."),
    ("cpu", "CPU-параметры сцены и ламп", "source/scene.hpp", "struct Scene {", "Именно эти значения меняет UI; начальные intensities, toggles и векторы."),
    ("material", "Материал и shininess в UI", "source/controls.cpp", 'if(ImGui::CollapsingHeader("Model and material"', "Albedo, specular color, shininess, scale, анимация."),
    ("scene_gpu", "std140 Scene", "source/scene.hpp", "struct alignas(16) SceneGpu", "160 байт; offsets сверяются static_assert и SPIR-V."),
    ("model_gpu", "std140 Model", "source/scene.hpp", "struct alignas(16) ModelGpu", "176 байт; две mat4 и материал, никаких C++ bool."),
    ("descriptors", "Создание двух descriptor sets", "source/renderer.cpp", "bool createDescriptors()", "Два layouts, pool 2 UBO + 2 SSBO, allocation и pipeline layouts."),
    ("write_descriptor", "Запись buffer descriptors", "source/renderer.cpp", "void writeDescriptor(", "VkDescriptorBufferInfo и VkWriteDescriptorSet с реальным buffer/range."),
    ("upload", "Загрузка UBO/SSBO", "source/renderer.cpp", "bool uploadScene(", "buildUniforms/pack → ensureStorage → writeBuffer."),
    ("map", "Map/flush/unmap", "source/renderer.cpp", "bool writeBuffer(", "Проверки VkResult и vmaFlushAllocation для noncoherent памяти."),
    ("growth", "Рост capacity и защита in-flight SSBO", "source/renderer.cpp", "bool ensureStorage(", "Новый буфер, update descriptor, destroy старого только после fence; предел устройства."),
    ("point_ui", "Добавление/удаление point", "source/controls.cpp", 'if(ImGui::CollapsingHeader("Point lights"', "Вектор, устойчивый PushID, erase после использования ссылки, Clear/Add."),
    ("spot_ui", "Добавление/удаление spot и полууглы", "source/controls.cpp", 'if(ImGui::CollapsingHeader("Spot lights"', "Независимый список; inner≤outer<90°, направление и intensity."),
    ("packing", "CPU → GPU параметры прожектора", "source/scene.cpp", "std::vector<SpotGpu> packSpots(", "Normalize с fallback, enabled через intensity=0, косинусы углов."),
    ("fence", "Когда можно менять GPU-данные", "source/graphics_internal.cpp", "FrameData prepare()", "Ждём fence до записи buffers/sets; пустой кадр при свёрнутой поверхности."),
    ("submit", "Submit/present и сброс fence", "source/graphics_internal.cpp", "bool submitAndPresent()", "Fence сбрасывается перед submit; rebuild откладывается до prepare."),
    ("resize", "Защита от нулевого swapchain extent", "source/graphics_internal.cpp", "bool rebuildSwapchain(", "Caps snapshot, нулевой extent откладывается; пересоздаются depth/framebuffers/semaphores."),
    ("draw", "Привязка двух sets и draw цилиндра", "source/renderer.cpp", "bool render(const Scene&", "vkCmdBindDescriptorSets(...,0,2,sets,...), lit draw, затем помощники."),
    ("blinn", "Блинн–Фонг: N/L/V/H", "shaders/lab2.frag", "vec3 evaluateBlinnPhong(", "NdotL, half vector L+V, pow(NdotH,shininess), запрет блика с обратной стороны."),
    ("directional", "Направленный источник", "shaders/lab2.frag", "vec3 evaluateDirectionalLight(", "L=-direction, без позиции и затухания."),
    ("ambient", "Где прибавляется ambient", "shaders/lab2.frag", "vec3 result=scene.ambient", "Один раз, до циклов по источникам."),
    ("sum", "Где суммируется весь свет", "shaders/lab2.frag", "void main()", "Ambient → directional → point loop → spot loop → displayColor."),
    ("attenuation", "Закон обратных квадратов", "shaders/lab2.frag", "float inverseSquare(", "1/max(distanceSquared,0.01), ограничение только около нуля."),
    ("point", "Освещение точечным источником", "shaders/lab2.frag", "vec3 evaluatePointLight(", "Разность мировых позиций определяет L и d²; цвет × intensity × 1/r²."),
    ("soft", "Мягкие края / равные углы", "shaders/lab2.frag", "float spotFactor(", "smoothstep(cosOuter,cosInner,cosTheta); step при совпадении границ."),
    ("spot", "Прожектор и знак направления", "shaders/lab2.frag", "vec3 evaluateSpotLight(", "dot(-L,direction); прямой свет умножается на attenuation и cone."),
    ("vertex", "Мировые позиции и нормали", "shaders/lab2.vert", "void main()", "Model и normalMatrix; worldNormal нормализуется после интерполяции во fragment."),
    ("markers", "Unlit маркеры источников", "source/renderer.cpp", "void drawMarkers(", "Те же позиции/цвета; marker pipeline и отдельные push constants."),
    ("cones", "Процедурные конусы", "source/geometry.cpp", "std::vector<LineVertex> generateSpotCones(", "Ось, устойчивый базис, радиус length*tan(angle), два кольца."),
    ("unlit_vertex", "Unlit vertex shader", "shaders/unlit.vert", "void main()", "VP*markerModel*position; постоянный цвет, без normal/material."),
    ("unlit_fragment", "Unlit fragment shader", "shaders/unlit.frag", "void main()", "Только displayColor(constantColor), без evaluateBlinnPhong."),
    ("pipeline", "Pipeline, атрибуты, depth test, линии", "source/renderer.cpp", "bool createPipeline(", "Lit/cube/line pipelines, lineWidth=1, depth writes отключены только у линий."),
    ("color", "Линейный цвет и sRGB", "shaders/display.glsl", "vec3 displayColor(", "Одно кодирование: аппаратное у sRGB attachment или ручное у UNORM."),
    ("cmake", "Компиляция GLSL при сборке", "CMakeLists.txt", "macro(compile_shader", "glslc --target-env=vulkan1.1; outputs только в собственном build/shaders."),
    ("cleanup", "Освобождение ресурсов сцены", "source/renderer.cpp", "void shutdown()", "Device idle, pipelines/layouts/pool, buffers, безопасно при частичной инициализации."),
    ("core_cleanup", "Освобождение Vulkan-каркаса", "source/graphics_internal.cpp", "void shutdown()", "ImGui backend, sync, framebuffers, depth, swapchain, allocator/device/instance."),
    ("smoke", "Сценарий динамических проверок", "source/application.cpp", "void smokeStep(", "0/33/удаления, нулевые векторы/равные углы, camera/scale, reset."),
]

def location(file, anchor):
    lines = (ROOT / file).read_text(encoding="utf-8-sig").splitlines()
    found = [i for i, line in enumerate(lines) if anchor in line]
    if len(found) != 1:
        raise ValueError(f"Expected unique anchor: {file}: {anchor}, found {found}")
    start = end = found[0]
    if file == "CMakeLists.txt":
        end = next(i for i in range(start, len(lines)) if "endmacro()" in lines[i])
    elif "{" in lines[start]:
        depth = 0
        for i in range(start, len(lines)):
            depth += lines[i].count("{") - lines[i].count("}")
            if depth == 0:
                end = i
                break
    return start + 1, end + 1

LOCATIONS = {key: (file, anchor, *location(file, anchor)) for key, _, file, anchor, _ in ENTRIES}

def ref(key):
    file, anchor, start, end = LOCATIONS[key]
    return f"[{file}:{start}–{end}]({file}#L{start}) — `{anchor}`"

rows = ["# Карта итогового кода", "", "Номера получены программно из текущих файлов. После правок выполните `python tools/update_code_map.py`. Поисковая строка остаётся полезной при смещении строк. Ссылки относительные, опубликованная ветка не предполагается.", "", "| Что показать | Файл и актуальные строки | Что объяснить |", "| --- | --- | --- |"]
for key, request, _, _, explanation in ENTRIES:
    rows.append(f"| {request} | {ref(key)} | {explanation} |")
(ROOT / "CODE_MAP.md").write_text("\n".join(rows) + "\n", encoding="utf-8")

# Речь + время на действия. Все интервалы суммарно 15:00.
SEGMENTS = [
    ("00:00–01:00", 60, "Цель и вариант", ["cpu", "geometry"],
     "Я продолжаю первую лабораторную: мой вариант — цилиндр с 50 сегментами, радиусом 1 и высотой 2. В этой работе изображение зависит от камеры, материала и нескольких видов источников света. Все параметры доступны через ImGui. Реализованы базовое задание и четыре дополнения: два набора дескрипторов, массив точечных источников, массив прожекторов и их отдельная визуализация. Покажу связь интерфейса, CPU-данных и шейдера, а также объясню, где обеспечивается безопасность обновления GPU-памяти.",
     "Запустить приложение, нажать Reset scene, показать цилиндр, два куба и контуры прожектора. Оставить 15 секунд на наблюдение.",
     "Почему цилиндр? Это мой вариант №8 из ЛР1; новую фигуру для ЛР2 выбирать не нужно."),
    ("01:00–02:10", 70, "Структура и CMake", ["cmake", "draw"],
     "ЛР2 находится в отдельной папке и собирается в cg-lab2. В main расположен цикл окна, application связывает UI и renderer, scene содержит математику и параметры, geometry генерирует вершины. Vulkan-каркас создаёт устройство и swapchain. CMake находит установленный Vulkan SDK через find_package, а FetchContent загружает закреплённые версии GLFW, ImGui, VMA и vk-bootstrap. Шейдеры — исходные GLSL, они при сборке компилируются glslc в SPIR-V. Эти файлы находятся в build ЛР2, поэтому приложение не может случайно использовать старые шейдеры ЛР1. Весь исходный lab1 сохранён, это подтверждено хешами.",
     "Показать список source и команду glslc в CMake. Указать фактическое имя target и путь build-vs2022/shaders; 15 секунд.",
     "Нужно ли пересобирать C++ после изменения GLSL? Достаточно обычного build: зависимость шейдера запустит glslc; затем приложение нужно перезапустить."),
    ("02:10–03:30", 80, "Нормали и преобразования модели", ["geometry", "uniforms", "vertex"],
     "Освещение требует нормали — вектора наружу от поверхности. У боковины цилиндра нормаль радиальная, у крышек строго вверх или вниз. На ребре позиции совпадают, но нормали разные, поэтому я разделил записи вершин. Получилось 202 вершины и прежние 600 индексов. Модель строится как перенос, вращение и масштаб. При неравномерном масштабе простое умножение нормали на model неверно: нормаль перестанет быть перпендикулярна касательной. Нужна inverse-transpose линейной части. Для нашего TRS она упрощается до R*S⁻¹. Vertex shader передаёт мировые позиции и преобразованные нормали; fragment shader нормализует их после интерполяции.",
     "Открыть Model and material, задать Scale (0.5,1.8,1.2), Y rotation 40°. Показать сохранение жёсткого ребра и освещения, затем Reset model; около 25 секунд.",
     "Почему не сохраняются 102 вершины ЛР1? Совпадение позиции не означает совпадения всех атрибутов: крышке нужна другая нормаль."),
    ("03:30–04:50", 80, "Камера и проекция", ["camera_ui", "camera", "fov"],
     "Камера — самостоятельная мировая трансформация, а view — её обратная. Я использую column vectors и порядок CameraWorld=T*Rz*Ry*Rx. Поэтому view имеет обратный порядок Rx⁻¹*Ry⁻¹*Rz⁻¹*T⁻¹. По умолчанию камера смотрит вдоль локальной минус Z; UI показывает градусы, а тригонометрия работает в радианах. FOV меняет перспективную проекцию, а в ортографии вместо него меняется размер видимой области. Aspect вычисляется из текущего swapchain extent после resize. Глубина соответствует Vulkan [0,1], Y переворачивается один раз в матрице проекции. Позиция камеры отдельно нужна fragment shader для направления V и расчёта блика.",
     "Изменить Camera XYZ, Euler Y и FOV, показать изменение вида; Reset camera. Переключить Perspective и вернуть. Демонстрация 25 секунд.",
     "Почему камера не заменяется переносом цилиндра? Тогда неверно изменялись бы отношения объекта к лампам, а положение наблюдателя для блика оставалось бы неправильным."),
    ("04:50–06:30", 100, "Ресурсы, дескрипторы и синхронизация", ["descriptors", "scene_gpu", "model_gpu", "upload", "growth", "fence", "draw"],
     "Set 0 относится ко всей сцене: UBO содержит view-projection, позицию камеры, ambient, directional и длины списков; bindings 1 и 2 — отдельные storage buffers для point и spot. Set 1 относится к модели: model, normal matrix и материал. Я действительно создаю два layouts, выделяю два sets, записываю buffer descriptors и привязываю оба перед draw. C++ структуры согласованы с std140/std430: используются поля по 16 байт, offsets проверяются static_assert. У runtime-массивов shader нет фиксированного максимума. Capacity растёт на CPU, count остаётся длиной списка. При удалении всех ламп сохраняется ненулевой buffer с count=0. UI меняет только CPU-данные; запись GPU buffers и обновление descriptors происходят после ожидания fence в prepare. Поэтому старый SSBO можно безопасно заменить и уничтожить.",
     "Показать pool, layouts и vkCmdBindDescriptorSets(...,0,2,sets,...). Добавить несколько point, показать count/capacity, Clear и Add снова. На действия оставить 30 секунд.",
     "Зачем flush? HOST_VISIBLE не гарантирует HOST_COHERENT; vmaFlushAllocation обеспечивает видимость записей в noncoherent памяти. Почему не писать в update? Fence ещё не ожидался, GPU мог читать старый буфер."),
    ("06:30–09:00", 150, "Подробно: fragment shader и Блинн–Фонг", ["sum", "ambient", "blinn", "upload", "material"],
     "Начну с main: сначала получаю единичную N и направление V от фрагмента к камере. Albedo берётся из материала; по умолчанию цилиндр однотонный. Ambient прибавляется один раз. Далее добавляется directional, затем в двух циклах суммируется каждый point и spot. Итог проходит displayColor. Теперь перейду к evaluateBlinnPhong. L всегда направлен от поверхности к свету. Скалярное произведение N и L даёт косинус угла: max(dot(N,L),0) определяет диффузный вклад. Если поверхность отвернута от лампы, функция возвращает ноль и блик не возникает. Для блика используется H — нормализованная сумма L и V. Это половинный вектор. Возведение max(dot(N,H),0) в степень shininess концентрирует блик вокруг нужного направления. Чем больше степень, тем уже блик. Цвет блика хранится отдельно от albedo и интенсивности источника. safeNormalize не допускает NaN при почти нулевых векторах. Все величины в мировом пространстве. CPU-параметры приходят через uploadScene из Scene, которую меняет drawControls; к модели относится specular.w=shininess. Значит я могу найти как формулу GPU, так и место её изменения на CPU.",
     "Временно оставить directional, выключив point/spot и ambient. Сравнить Diffuse contribution и Specular contribution; менять shininess от 8 до 128, затем цвет блика. Поводить камерой и показать перемещение блика. Вернуть Reset scene. На код и демонстрацию оставить около 60 секунд.",
     "Чем отличается от Phong? Здесь блик зависит от N·H; классический Phong использует отражённый вектор reflect и его угол с V. Shininess не является интенсивностью лампы."),
    ("09:00–10:00", 60, "Ambient и directional", ["ambient", "directional", "cpu"],
     "Ambient — приближение фонового освещения, он умножается на albedo и добавляется один раз независимо от количества ламп. Directional описывает лучи с общим направлением; позиции у такого источника нет. В CPU хранится направление движения лучей, поэтому в shader для направления к свету стоит минус. Его вклад не затухает с расстоянием. Цвет и intensity умножают сумму диффузного и зеркального вкладов. Отдельные переключатели в UI позволяют показать, что именно меняет каждый член формулы.",
     "Отключить point/spot, оставить только ambient; затем ambient выключить, directional включить и развернуть направление. На действия 20 секунд.",
     "Почему ambient нельзя добавлять внутри цикла point? Тогда фон непреднамеренно усиливался бы при добавлении ламп."),
    ("10:00–11:10", 70, "Точечные источники и 1/r²", ["point_ui", "point", "attenuation", "growth"],
     "У каждого point есть позиция, цвет, intensity и enabled. В shader delta равна позиции лампы минус мировая позиция фрагмента; из неё получаются L и квадрат расстояния. Вклад Блинн–Фонга умножается на 1/r². В знаменателе есть небольшое ограничение около нуля, чтобы точное совпадение позиций не давало бесконечность. При фиксированных направлениях и удвоении расстояния линейный вклад становится вчетверо меньше. Это проверено численно. От произвольного скриншота четверть яркости требовать нельзя: изменяются углы, а изображение проходит sRGB-кодирование и может насыщаться. Источники хранятся в настоящем unsized SSBO, не в массиве lights[16].",
     "Add point два раза, раскрыть записи и изменить цвета/позиции, удалить одну, затем Clear points и Add снова; 25 секунд.",
     "Бесконечно ли количество ламп? Нет: shader не зашивает лимит, но есть maxStorageBufferRange, память и стоимость расчёта каждого фрагмента."),
    ("11:10–12:30", 80, "Прожекторы и мягкие края", ["spot_ui", "packing", "spot", "soft"],
     "Прожектор добавляет к точечному свету направление и два полуугла конуса. UI задаёт градусы, CPU передаёт косинусы. Direction идёт от лампы, а L — к лампе, поэтому cosTheta=dot(-L,direction). Внутри внутреннего конуса коэффициент равен единице, снаружи внешнего — нулю. Между ними smoothstep плавно меняет свет. CosInner не меньше cosOuter, поэтому аргументы smoothstep идут именно в этом порядке. Если полууглы совпадают, вызывается жёсткий step, а не smoothstep с одинаковыми границами. Нулевое направление получает запасное вниз, чтобы и свет, и его визуализация оставались согласованными. Spot умножает прямой свет одновременно на cone factor и на 1/r², ambient при этом не затухает.",
     "Оставить один spot, менять direction и outer/inner. Свести inner к outer и показать резкую границу, затем вернуть разницу. Обнулить intensity; 30 секунд.",
     "Почему используются косинусы? Dot единичных векторов уже даёт косинус; не нужно вызывать дорогой acos для каждого фрагмента."),
    ("12:30–13:30", 60, "Unlit маркеры и конусы", ["markers", "cones", "unlit_vertex", "unlit_fragment", "pipeline"],
     "Кубы показывают позиции ламп и имеют их постоянный цвет. Отдельные unlit shaders не читают материал цилиндра и не вызывают Блинн–Фонга. У прожектора белая ось и внешнее кольцо, синее внутреннее кольцо. Геометрия строится из того же direction и углов, что используются для света. Радиус кольца равен длине показа, умноженной на тангенс полуугла. Базис защищён от направления вдоль опорного вектора. Линии имеют ширину один и проходят depth test, но не пишут depth. Это условная визуализация, а не тени и не физическая граница действия лампы.",
     "Выключить ambient, убедиться, что кубы сохраняют цвет. Снять Show light markers и Show spot cones: освещение цилиндра остаётся; 20 секунд.",
     "Почему скрытие куба не выключает свет? Это разные флаги: showMarkers управляет draw, enabled/intensity — вкладом в shader."),
    ("13:30–15:00", 90, "Проверки и завершение", ["smoke", "resize", "cleanup", "core_cleanup"],
     "Проект собран отдельно на MSVC x64; все четыре шейдера прошли spirv-val. Численные тесты проверяют замкнутость и winding цилиндра, нормали, обратную камеру, Vulkan depth и inverse-transpose, закон 1/r² и границы конуса. Реальный smoke на AMD Radeon проходит рост до 33 источников каждого типа, удаления, пустые списки, равные углы, нулевые направления и intensity, resize и reset. Вручную проверены UI, масштаб и три сворачивания с паузой после исправления гонки нулевого extent. Ошибок validation в итоговом запуске нет. Есть два общих сообщения loader о недостающих Steam JSON; они сохранены, поэтому я не называю весь лог чистым. NVIDIA и предельная нагрузка не проверялись. Все 15 файлов исходной ЛР1 сохранили свои хеши. При выходе программа ждёт завершения GPU и освобождает созданные ресурсы в обратном порядке.",
     "Показать docs/VERIFICATION.md и итог smoke-stdout.log, затем штатно закрыть приложение. На просмотр логов и ответ оставить 35 секунд.",
     "Что осталось проверить на другом компьютере? Совместимость GPU/драйвера, повторить smoke и интерактивные настройки; сообщения VUID нужно читать, а не отключать validation."),
]

assert sum(segment[1] for segment in SEGMENTS) == 900
out = ["# Защита: 15 минут", "", "Интервалы включают речь, открытие кода и демонстрацию, суммарно **15:00**. Это сценарий подготовки, а не гарантия вопросов или оценки. Сначала запустите приложение, затем держите CODE_MAP и fragment shader под рукой.", ""]
for interval, _, title, keys, speech, action, answer in SEGMENTS:
    out.extend([f"## {interval} — {title}", "", "**Что сказать.** «" + speech + "»", "", "**Что открыть.** " + "; ".join(ref(key) for key in keys) + ".", "", "**Действие и ожидаемый результат.** " + action, "", "**Уточняющий вопрос.** " + answer, ""])
(ROOT / "DEFENSE_15MIN.md").write_text("\n".join(out), encoding="utf-8")
print(f"Updated {len(ENTRIES)} code locations and {len(SEGMENTS)} defense segments (15:00)")
