# Тренировка

При устной тренировке задавать по одному вопросу и ждать ответа.
Проверять объяснение через код и действия, а не только определения.

| Вопрос | Краткий ответ |
| --- | --- |
| Что делает texture()? | Выбирает цвет изображения по UV с правилами sampler |
| Зачем seam duplicates? | Одна позиция должна иметь U=1 и U=0 на разных сторонах шва |
| Что хранит normal matrix? | Преобразование нормали; для T*R*S линейная часть R*S^-1 |
| Image, view, sampler, descriptor? | Ресурс, представление, правила выборки, привязка в shader |
| Почему staging нельзя удалить сразу? | GPU исполняет copy позже; сначала нужен fence |
| Что означает DEVICE_LOCAL на UMA? | Память оптимальна для устройства, но может быть общей с CPU |
| Чем dynamic UBO отличается от обычного? | Offset выбран в vkCmdBindDescriptorSets, один buffer обслуживает объекты |
| range и stride одинаковы? | Range=176 полезных байт; stride округлён до alignment GPU |
| Где sampling flag? | В 32-байтных fragment push constants, не в ModelGpu |
| Почему это не MSAA? | Несколько выборок текстуры по UV не являются сэмплами покрытия пикселя |
| Зачем separate effect pause? | Можно остановить узор, сохранив движение геометрии, и наоборот |
| Что изменяет specular mask? | Только specular-вклад; shininess остаётся степенью и шириной блика |
| Почему black fallback? | Чёрный убирает specular умножением и emissive нулевым сложением |
| Почему эмиссия видна в темноте? | Прибавляется после света без NdotL и intensity внешних источников |
| Почему specular UNORM? | Это линейные данные маски, а не цвет для sRGB декодирования |
| Можно ли менять descriptor активного кадра? | Здесь нет; материалы заранее созданы, CPU меняет индекс и bind |
| Зачем fence перед UBO write? | Предыдущий кадр может ещё читать те же байты |

## Упражнения «найди, предскажи, проверь»

1. В `Base demo` установите `UV repeat`=2 только первому. Предскажите число
   повторений и роль sampler repeat. Меняется через **UI**, пересборка не нужна.
   Код: [sampler](source/gpu_upload.cpp#L136), [fragment_main](shaders/lighting.glsl#L51).
2. Назначьте двум объектам разные `Material`, затем одинаковый. Найдите
   `materialSets` и объясните, почему VkBuffer не создаётся повторно.
   **UI**, без пересборки; [material_sets](source/renderer.cpp#L72), [dynamic_bind](source/renderer.cpp#L389).
3. В `Sampling demo` поставьте `Effect speed`=2, затем выключите
   `Play sampling effect`. Объясните, почему `Play animation` независим.
   **UI**, без пересборки; [effect_time](source/application.cpp#L18).
4. В копии shader временно замените sampleB на sampleA. Предскажите,
   какая часть узора исчезнет и почему задание требует разные UV.
   Правка **GLSL**, обычная сборка перекомпилирует shader. Верните код;
   [multi_sample](shaders/lighting.glsl#L61). Не оставляйте эту правку в финале.
5. Посмотрите `panels_specular.png`: мысленно замените всю карту белым,
   затем чёрным. Какие участки заблестят? Объясните по [specular_mask](shaders/lighting.glsl#L25).
   Для реального опыта меняется **PNG** в исходных assets, сборка копирует
   его, приложение надо перезапустить. После опыта восстановите PNG.
6. В `Emission demo` обнулите `Emissive strength`, затем верните 1.
   Почему без света поверхность исчезает? **UI**, [emissive_add](shaders/lighting.glsl#L77).
7. Нажмите `Restore lights` после эмиссии. Назовите слагаемые итогового цвета.
   **UI**, затем [fragment_main](shaders/lighting.glsl#L51). Найдите ambient, diffuse/specular и emissive.
8. По стартовому логу вычислите offsets двух записей ModelGpu. Затем
   предположите alignment=256 и объясните padding 80 байт после 176.
   Только чтение **C++/лога**; [ubo_allocate](source/renderer.cpp#L57), [dynamic_bind](source/renderer.cpp#L389).
9. В portable-копии assets переименуйте обязательную albedo; предскажите
   сообщение и код завершения. **Файл**, перезапуск, без компиляции.
   Не повреждайте исходный набор; автоматический пример в
   `tools/verify_runtime.py`. Код: [decode](source/image.cpp#L9).
10. В portable-копии удалите обе optional-карты Panels. Почему запуск
    продолжается, а удаление albedo его останавливает? **Файлы**, перезапуск.
    Покажите [fallback_bind](source/renderer.cpp#L71), [black_load](source/renderer.cpp#L325).
11. Найдите место, после которого staging можно освобождать. Предскажите,
    как раннее удаление проявилось бы на быстрой и медленной GPU.
    Только чтение **C++**, [upload_wait](source/gpu_upload.cpp#L24), [buffer_copy](source/gpu_upload.cpp#L81). Не вносите гонку.
12. Задайте `Scale XYZ`=(0.5,1.8,1.2). Объясните, почему умножение нормали
    на model matrix дало бы неверный свет. **UI** и [normal_matrix](source/scene.cpp#L70).

После экспериментов верните исходные файлы, пересоберите и снова выполните
CPU/spirv-val/smoke проверки. В поставляемой версии намеренных ошибок нет.
