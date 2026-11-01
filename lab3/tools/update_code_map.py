"""Regenerate exact source lines and refresh links in the defense materials."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
# key, file, unique source fragment, explanation
entries=[
 ('decode','source/image.cpp','ImagePixels decodeImage','Декодирование файла, проверка размеров и один переворот строк'),
 ('vertex','source/scene.hpp','struct Vertex {','Position, normal, UV; цвет оставлен для помощников, offsets проверены'),
 ('model_layout','source/scene.hpp','struct alignas(16) ModelGpu','Запись std140 на 176 байт без sampling flag'),
 ('cylinder','source/geometry.cpp','Mesh generateCylinder','50 сегментов, дублированный шов, U по часовой стрелке и отдельные крышки'),
 ('normal_matrix','source/scene.cpp','mg.normalMatrix=','Обратная транспонированная R*S^-1 при неравномерном масштабе'),
 ('geometry_upload','source/gpu_upload.cpp','bool uploadGeometry','Staging и финальный буфер с обязательным DEVICE_LOCAL'),
 ('buffer_copy','source/gpu_upload.cpp','vkCmdCopyBuffer(cmd','Копирование и барьер transfer write -> vertex/index read'),
 ('upload_wait','source/gpu_upload.cpp','vkWaitForFences','Ожидание до освобождения staging и upload command buffer'),
 ('image_barrier','source/gpu_upload.cpp','VkImageMemoryBarrier barrier','Переходы UNDEFINED -> TRANSFER_DST -> SHADER_READ'),
 ('image_copy','source/gpu_upload.cpp','vkCmdCopyBufferToImage','Копирование RGBA из staging в изображение'),
 ('image_view','source/gpu_upload.cpp','VkImageViewCreateInfo vi','Представление цветного изображения, один mip'),
 ('sampler','source/gpu_upload.cpp','VkSamplerCreateInfo si','Linear/repeat, без anisotropy, LOD=0'),
 ('ubo_allocate','source/renderer.cpp','const auto alignment=','Alignment, stride, range, лимиты и один общий model buffer'),
 ('ubo_descriptor','source/renderer.cpp','VkDescriptorBufferInfo bufferInfo{modelBuffer.handle,0,sizeof(ModelGpu)}','Range dynamic descriptor равен размеру одной записи'),
 ('ubo_write','source/renderer.cpp','i*modelStride)) return false','Запись полезных байт в каждый выровненный слот после frame fence'),
 ('dynamic_bind','source/renderer.cpp','litLayout,0,2,bound,1,&dynamicOffset','pDynamicOffsets выбирает запись модели перед draw'),
 ('material_sets','source/renderer.cpp','for(size_t material=0;material<3;++material)','Один set на материал, общий model VkBuffer'),
 ('material_combo','source/controls.cpp','ImGui::Combo("Material"','Независимый выбор материала выбранного объекта'),
 ('sampling_push','source/renderer.cpp','SamplingPush push','32 байта fragment push constants: флаг, время, сила'),
 ('effect_time','source/application.cpp','if(scene.effectPlaying)','Накопление dt*speed независимо от анимации геометрии'),
 ('fragment_main','shaders/lighting.glsl','void main()','Albedo -> эффект -> Блинн-Фонг -> emissive -> вывод'),
 ('multi_sample','shaders/lighting.glsl','vec3 sampleA=texture','Две texture() одного albedo sampler по uvA и uvB'),
 ('sampling_mix','shaders/lighting.glsl','vec3 modulation=','Смешивание и модуляция от времени, регулировка силы'),
 ('specular_mask','shaders/lighting.glsl','object.specular.rgb*specularMask','Маска умножает только specular; shininess отдельно'),
 ('emissive_add','shaders/lighting.glsl','result+=texture(emissiveSampler','Эмиссия прибавляется после света на исходных UV'),
 ('black_load','source/renderer.cpp','const char* files[]={"black.png"','Black загружается общим путём; отсутствие optional-карт разрешено'),
 ('fallback_bind','source/renderer.cpp','const size_t maps[3][3]','Отсутствующие specular/emissive привязаны к общему black'),
 ('prepare_fence','source/graphics_internal.cpp','if(graphicsFailed || vkWaitForFences','Один кадр в работе: ожидание перед CPU write и заменой ресурсов'),
 ('cleanup','source/renderer.cpp','void shutdown()','Device idle, освобождение pipelines, descriptors, textures, buffers и upload pool'),
 ('cmake_target','CMakeLists.txt','add_executable(${PROJECT_NAME}','Самостоятельная цель, исходники decoder и GPU upload'),
 ('cmake_shaders','CMakeLists.txt','macro(compile_shader','glslc пишет SPIR-V только в build-каталог'),
]
rows=['# Карта кода ЛР3','',
 'Номера вычислены из итоговых файлов. Обновление: `python tools/update_code_map.py`.',
 'Ссылки относительные; после публикации GitHub открывает соответствующую строку.',
 '', '| Что показать | Файл и строка | Поисковая строка | Объяснение |',
 '| --- | --- | --- | --- |']
links={}
for key,file,needle,explanation in entries:
    lines=(root/file).read_text(encoding='utf-8').splitlines()
    hits=[i+1 for i,line in enumerate(lines) if needle in line]
    assert len(hits)==1,(key,hits)
    line=hits[0]
    links[key]=f'[{key}]({file}#L{line})'
    rows.append(f'| {key} | [{file}:{line}]({file}#L{line}) | `{needle}` | {explanation} |')
(root/'CODE_MAP.md').write_text('\n'.join(rows)+'\n',encoding='utf-8')
for name in ['DEMO_CHECKLIST.md','DEFENSE_15MIN.md','DEFENSE_PRACTICE.md','REPORT_DRAFT.md']:
    path=root/name
    if not path.exists():continue
    text=path.read_text(encoding='utf-8')
    for key,link in links.items():
        text=text.replace('[['+key+']]',link)
        text=re.sub(r'\['+re.escape(key)+r'\]\([^)]*\)',lambda _:link,text)
    assert not re.search(r'\[\[\w+\]\]',text),name
    path.write_text(text,encoding='utf-8')
print(f'Updated {len(entries)} exact code locations and defense links')
