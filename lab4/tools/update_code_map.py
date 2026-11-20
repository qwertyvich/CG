"""Regenerate exact references from the final source. Run after code changes."""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[1]
# key, reviewer request, file, unique search string, explanation
entries=[
('feature-query','Проверка фактической поддержки','source/graphics_internal.cpp','VkPhysicalDeviceFeatures2 features','Query features2/pNext и аудит device API/расширения для всех GPU.'),
('feature-enable','Включение Dynamic Rendering','source/graphics_internal.cpp','.set_required_features_13(required)','vk-bootstrap запрашивает dynamicRendering=true при vkCreateDevice; выбран core 1.3.'),
('entry-points','Core команды','source/graphics_internal.cpp','context.beginRendering=reinterpret_cast','vkGetDeviceProcAddr загружает и проверяет core entry points. KHR fallback не реализован.'),
('depth-format','Выбор depth-формата','source/shadows.cpp','constexpr auto needed=','Формат должен поддерживать optimal attachment и sampled image; дополнительно image-format query.'),
('depth-image','Создание depth image/view','source/shadows.cpp','VkImageCreateInfo image','Две независимые DEVICE_LOCAL карты, DEPTH_ATTACHMENT|SAMPLED, depth-aspect view.'),
('compare-sampler','Comparison sampler LESS','source/shadows.cpp','.compareEnable=VK_TRUE','Отдельный sampler: compareEnable, LESS, NEAREST, белый border.'),
('shadow-descriptors','Shadow descriptor bindings','source/renderer.cpp','writeDescriptor(0,3,','Set 0: binding 3 ShadowGpu; binding 4/5 — sun/spot combined image samplers.'),
('shadow-samplers','sampler2DShadow в GLSL','shaders/shadows.glsl','uniform sampler2DShadow sunShadowSampler','Самpler ресурсы совпадают с CPU binding 4 и 5.'),
('depth-pipeline','Depth-only pipeline','source/shadows.cpp','VkPipelineRenderingCreateInfo rendering','Ноль цветовых attachments, depth format; ниже stageCount=1, renderPass=null.'),
('begin-rendering','Dynamic Rendering begin/end','source/shadows.cpp','c.beginRendering(cmd,&rendering)','Загруженный vkCmdBeginRendering; затем bind/draw и endRendering; без shadow framebuffer.'),
('shadow-push','Light VP для depth-only vertex','source/shadows.cpp','vkCmdPushConstants(cmd,layout,VK_SHADER_STAGE_VERTEX_BIT','Отдельный shadow layout, vertex push 64 bytes; sampling push имеет другой layout/stage.'),
('dynamic-draw','Shadow/main draws с dynamic offsets','source/renderer.cpp','const uint32_t dynamicOffset=','Одна модельная запись UBO выбирается для каждого объекта и каждого прохода.'),
('mesh-draw','Диапазоны цилиндра/пола','source/renderer.cpp','vkCmdDrawIndexed(cmd,mesh.indexCount','indexCount/firstIndex/vertexOffset берутся по MeshKind, пол не рисуется как 600 индексов.'),
('mesh-ranges','Сборка общей геометрии','source/renderer.cpp','meshRanges[1]=','Floor имеет отдельный firstIndex и vertexOffset в общем device-local buffer.'),
('barriers','Image barriers','source/shadows.cpp','void transition(','UNDEFINED отдельно; EARLY/LATE depth writes ↔ fragment shader read. Descriptor layout совпадает.'),
('lookat','LookAt и устойчивый базис','source/scene.cpp','Mat4 lookAt(','Вид вдоль -Z; альтернативный up около вертикали и fallback нулевого направления.'),
('light-matrices','World light matrices','source/scene.cpp','ShadowGpu buildShadows(','Солнце и spot имеют собственные VP; observer camera не участвует.'),
('ortho','Ортографическое покрытие солнца','source/scene.cpp','g.sunViewProjection=multiply','Ortho half extent в world units; eye=target-direction*25; near/far 0.1/60.'),
('spot-fov','Перспектива spot: FOV=2*outer','source/scene.cpp','const float fullFov=2*','Выбранный enabled spot с валидным outer использует perspective * LookAt; near/far 0.1/50.'),
('bias','Динамический raster depth bias','source/shadows.cpp','vkCmdSetDepthBias(','Constant/slope из UI, clamp=0, положительный bias при обычной LESS depth.'),
('bias-ui','Управление bias/coverage','source/controls.cpp','ImGui::SliderFloat("Shadow half extent"','Обязательные настройки UI: половина покрытия и два depth bias.'),
('light-transform','Преобразование depth vertex','shaders/shadow.vert','gl_Position=','lightViewProjection * model * position, один vertex shader.'),
('perspective-divide','Clip → NDC → UV','shaders/shadows.glsl','vec3 ndc=clip.xyz/clip.w','w>0; XY scale/bias; Vulkan Z уже 0..1; Y не переворачивается повторно.'),
('bounds','Выход за карту','shaders/shadows.glsl','if(any(lessThan(uv','UV/depth bounds проверяются до sampling; вне карты visibility=1.'),
('comparison','Сравнение глубины','shaders/shadows.glsl','return texture(map,vec3(uv,ndc.z))','Comparison sampler возвращает visibility: LESS успех = свет виден.'),
('sun-visibility','Visibility только для солнца','shaders/lighting.glsl','result+=sunVisibility*','Shadow ограничивает directional diffuse+specular, не общий результат.'),
('spot-visibility','Visibility только для выбранного spot','shaders/lighting.glsl','float visibility=shadows.flags.y','GPU packed index вычислен по stable ID; остальные spot/point независимы.'),
('emissive','Независимые ambient/emissive','shaders/lighting.glsl','result+=texture(emissiveSampler','Emissive прибавляется после света, ambient — до него без visibility.'),
('floor','Пол: два треугольника','source/geometry.cpp','Mesh generateFloor()','4 вершины, 6 индексов, нормаль +Y, UV; настоящий receiver.'),
('floor-model','Третья модель','source/scene.cpp','ModelState defaultModel(','Пол y=-1.16, под цилиндрами y_min=-1.15, без общей анимации.'),
('stable-selection','Выбор spot по ID','source/scene.cpp','int selectedShadowSpot(','Каждый раз поиск актуального индекса; не хранится ссылка в vector.'),
('reconcile','Удаление/clear/re-add/reset spot','source/scene.cpp','void reconcileShadowSpot(','Первый оставшийся либо -1; новый spot выбирается при отсутствии прежнего.'),
('remove-spot','UI erase выбранного spot','source/controls.cpp','if(remove) s.spots.erase','Erase после последнего обращения к ссылке; reconcile выполняется в конце UI.'),
('inactive-map','Валидная неактивная карта','source/shadows.cpp','if(!active && map.initialized)','Первый кадр даже неактивной карты: clear+layout; позже pass пропускается.'),
('pass-order','Порядок проходов кадра','source/renderer.cpp','shadows::record(cmd,i,','Sun затем spot до vkCmdBeginRenderPass основного окна; карты каждый активный кадр.'),
('fence','Ожидание GPU до upload','source/graphics_internal.cpp','vkWaitForFences(','В prepare ожидается fence одного кадра; затем renderer пишет общие UBO/SSBO.'),
('lit-variants','Обычный и sampling shader','source/renderer.cpp','if(!shadowPass) vkCmdBindPipeline','Выбор base/sampling pipeline; оба включают общий lighting.glsl с shadow logic.'),
('cleanup','Освобождение ресурсов','source/renderer.cpp','shadows::shutdown();','Renderer ждёт GPU; уничтожает shadow pipeline/layout/maps, descriptors, buffers и textures.'),
('cmake','Сборка новых shaders','CMakeLists.txt','compile_shader(shadow.vert)','Автоматическая GLSL compilation + tracking shadows.glsl, свой target cg-lab4.'),
]
refs={}
lines=['# Карта кода ЛР4','',
       'Строки вычислены из текущих исходников скриптом `python lab4/tools/update_code_map.py`. После правок запустите его повторно. Поисковые строки позволяют найти место и при смещении номеров.','',
       '| Что показать | Файл / точная строка | Функция или поисковая строка | Что сказать |',
       '|---|---|---|---|']
for key,title,path,needle,explanation in entries:
    source=(root/path).read_text(encoding='utf-8').splitlines()
    matches=[i+1 for i,s in enumerate(source) if needle in s]
    assert matches,(key,path,needle)
    line=matches[0]
    ref=f'[{path}:{line}]({path}#L{line})'
    refs[key]=ref
    escaped_needle=needle.replace('|',r'\|')
    escaped_explanation=explanation.replace('|',r'\|')
    lines.append(f'| {title} | {ref} | `{escaped_needle}` | {escaped_explanation} |')
(root/'CODE_MAP.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
for name in ['DEMO_CHECKLIST.md','DEFENSE_15MIN.md','DEFENSE_PRACTICE.md']:
    p=root/name
    if not p.exists():continue
    s=p.read_text(encoding='utf-8')
    s=re.sub(r'\{\{code:([\w-]+)\}\}',lambda m:f'<!--code:{m[1]}-->{refs[m[1]]}<!--/code-->',s)
    s=re.sub(r'<!--code:([\w-]+)-->.*?<!--/code-->',lambda m:f'<!--code:{m[1]}-->{refs[m[1]]}<!--/code-->',s)
    p.write_text(s,encoding='utf-8')
print(f'Updated {len(refs)} exact source references and defense documents')
