"""Build the combined course report from verified content and real screenshots."""
from pathlib import Path
import json,os
from PIL import Image as PILImage
from xml.sax.saxutils import escape
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER,TA_JUSTIFY,TA_LEFT
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.platypus import BaseDocTemplate,PageTemplate,Frame,Paragraph,Spacer,PageBreak,NextPageTemplate,Image,KeepTogether,Flowable
from reportlab.platypus.tableofcontents import TableOfContents

ROOT=Path(__file__).resolve().parents[2]
DATA=json.loads((ROOT/'reports/report_content.json').read_text(encoding='utf-8'))
OUTPUT=ROOT/'output/pdf/Салихов_Р.Р._М8О-309БВ-24_ЛР1-4.pdf'
OUTPUT.parent.mkdir(parents=True,exist_ok=True)
FONT=Path(os.environ.get('WINDIR','C:/Windows'))/'Fonts'
for name,file in [('TNR','times.ttf'),('TNR-Bold','timesbd.ttf'),('TNR-Italic','timesi.ttf'),('TNR-BoldItalic','timesbi.ttf')]:
    pdfmetrics.registerFont(TTFont(name,str(FONT/file)))
pdfmetrics.registerFontFamily('TNR',normal='TNR',bold='TNR-Bold',italic='TNR-Italic',boldItalic='TNR-BoldItalic')
W,H=A4;LEFT=30*mm;RIGHT=15*mm;TOP=BOTTOM=20*mm;TW=W-LEFT-RIGHT;TH=H-TOP-BOTTOM
S={
    'body':ParagraphStyle('Body',fontName='TNR',fontSize=14,leading=21,alignment=TA_JUSTIFY,firstLineIndent=12.5*mm,spaceAfter=6,allowWidows=0,allowOrphans=0),
    'lab':ParagraphStyle('Lab',fontName='TNR-Bold',fontSize=16,leading=21,alignment=TA_CENTER,spaceAfter=16,keepWithNext=True),
    'section':ParagraphStyle('Section',fontName='TNR-Bold',fontSize=14,leading=21,spaceBefore=12,spaceAfter=8,keepWithNext=True),
    'sub':ParagraphStyle('Sub',fontName='TNR-Bold',fontSize=14,leading=21,spaceBefore=10,spaceAfter=6,keepWithNext=True),
    'caption':ParagraphStyle('Caption',fontName='TNR',fontSize=12,leading=16,alignment=TA_CENTER,spaceBefore=5,spaceAfter=10),
    'formula':ParagraphStyle('Formula',fontName='TNR',fontSize=13,leading=20,alignment=TA_CENTER,spaceBefore=5,spaceAfter=10),
    'source':ParagraphStyle('Source',fontName='TNR',fontSize=12,leading=17,spaceAfter=10,alignment=TA_LEFT),
    'center':ParagraphStyle('Center',fontName='TNR',fontSize=14,leading=21,alignment=TA_CENTER),
}

def p(text,style='body'):return Paragraph(text,S[style])

class Cover(Flowable):
    def __init__(self):super().__init__();self.width=TW;self.height=TH-1
    def draw(self):
        c=self.canv
        def center(text,y,font=14,bold=False):
            style=ParagraphStyle('CV',fontName='TNR-Bold' if bold else 'TNR',fontSize=font,leading=font*1.4,alignment=TA_CENTER)
            para=Paragraph(text,style);_,hh=para.wrap(TW,self.height);para.drawOn(c,0,y-hh);return y-hh
        y=self.height
        y=center('МОСКОВСКИЙ АВИАЦИОННЫЙ ИНСТИТУТ',y,14,True)
        y=center('(НАЦИОНАЛЬНЫЙ ИССЛЕДОВАТЕЛЬСКИЙ УНИВЕРСИТЕТ)',y-3,12)
        y=center('Институт № 8 «Компьютерные науки и прикладная математика»',y-15,12)
        y=center('Кафедра 806 «Вычислительная математика и программирование»',y-5,12)
        y=center('ОТЧЕТ ПО ЛАБОРАТОРНЫМ РАБОТАМ № 1-4',self.height-225,16,True)
        y=center('по курсу «Компьютерная графика»',y-12,14)
        y=center('Основы 3D-графики<br/>Камера и освещение<br/>Текстурирование<br/>Наложение теней',y-20,14,True)
        center('Вариант 8',y-12,14)
        style=ParagraphStyle('Names',fontName='TNR',fontSize=14,leading=24,alignment=TA_LEFT)
        names=f'Выполнил: {DATA["author"]}<br/>Группа: {DATA["group"]}<br/>Преподаватель: {DATA["teacher"]}'
        para=Paragraph(names,style);_,hh=para.wrap(TW-56*mm,150);para.drawOn(c,56*mm,135)
        center(f'Москва, {DATA["year"]}',27,14)

def body_page(canvas,doc):
    canvas.saveState();canvas.setFillColor(colors.black);canvas.setFont('TNR',12)
    canvas.drawCentredString(W/2,12*mm,str(doc.page));canvas.restoreState()

class ReportDoc(BaseDocTemplate):
    def __init__(self,path):
        super().__init__(str(path),pagesize=A4,leftMargin=LEFT,rightMargin=RIGHT,topMargin=TOP,bottomMargin=BOTTOM,
                         title='Отчеты по лабораторным работам 1-4 по компьютерной графике',author=DATA['author'],
                         subject=f'{DATA["group"]}; вариант 8',pageCompression=1,allowSplitting=1)
        frame=Frame(LEFT,BOTTOM,TW,TH,leftPadding=0,rightPadding=0,topPadding=0,bottomPadding=0,id='text')
        self.addPageTemplates([PageTemplate(id='cover',frames=[frame]),PageTemplate(id='body',frames=[frame],onPage=body_page)])
    def afterFlowable(self,flowable):
        if hasattr(flowable,'toc_title'):
            key=flowable.toc_key;self.canv.bookmarkPage(key)
            self.notify('TOCEntry',(flowable.toc_level,flowable.toc_title,self.page,key))
            self.canv.addOutlineEntry(flowable.toc_title.replace('<br/>',' '),key,level=flowable.toc_level,closed=False)

story=[]
toc=TableOfContents();toc.levelStyles=[
    ParagraphStyle('TOC0',fontName='TNR-Bold',fontSize=12,leading=17,spaceBefore=7,textColor=colors.black),
    ParagraphStyle('TOC1',fontName='TNR',fontSize=12,leading=16,leftIndent=8*mm,firstLineIndent=0,textColor=colors.black)]

def heading(text,key,level=1):
    q=p(text,'lab' if level==0 else 'section');q.toc_title=text;q.toc_key=key;q.toc_level=level;story.append(q)

story.append(Cover());story.extend([NextPageTemplate('body'),PageBreak()])
story.append(p('СОДЕРЖАНИЕ','lab'));story.append(toc);story.append(PageBreak())
for lab in DATA['labs']:
    n=lab['number']
    if n>1:story.append(PageBreak())
    heading(f'Лабораторная работа № {n}<br/>{lab["title"]}',f'lab-{n}',0)
    heading('1 Условие',f'lab-{n}-condition')
    for i,text in enumerate(lab['condition']):story.append(p(text+(f' [{n}]' if i==0 else '')))
    heading('2 Метод решения',f'lab-{n}-method')
    for i,section in enumerate(lab['method'],1):
        story.append(p(f'2.{i} {section["title"]}','sub'))
        for text in section['paragraphs']:story.append(p(text))
        if section.get('formula'):story.append(p(section['formula'],'formula'))
    results_start=len(story)
    heading('3 Результаты',f'lab-{n}-results')
    intro=p(lab['results_intro']);intro.keepWithNext=True;story.append(intro)
    for i,fig in enumerate(lab['figures']):
        path=ROOT/'reports/screenshots'/fig['file']
        with PILImage.open(path) as raster:iw,ih=raster.size
        image=Image(str(path),width=TW,height=TW*ih/iw)
        image.hAlign='CENTER'
        block=[Spacer(1,5),image,p(fig['caption'],'caption'),p(lab['results'][i])]
        if i==0:
            block=story[results_start:]+block
            del story[results_start:]
        story.append(KeepTogether(block))
    heading('4 Выводы',f'lab-{n}-conclusion')
    story.append(p(lab['conclusion']))

story.append(PageBreak());heading('СПИСОК ИСПОЛЬЗОВАННЫХ ИСТОЧНИКОВ','references',0)
sources=[
    'Лабораторная работа № 1. Основы 3D-графики. Учебное задание курса «Компьютерная графика». Файл CG Lab #1.pdf. 2 с.',
    'Лабораторная работа № 2. Камера и освещение. Учебное задание курса «Компьютерная графика». Файл Lab #2 Camera and Lighting.pdf. 3 с.',
    'Лабораторная работа № 3. Текстурирование. Учебное задание курса «Компьютерная графика». Файл Lab #3 Texture mapping.pdf. 3 с.',
    'Лабораторная работа № 4. Наложение теней. Учебное задание курса «Компьютерная графика». Файл Lab #4 Shadow mapping.pdf. 2 с.',
    'Салихов Р. Р. Исходный код лабораторных работ по компьютерной графике. Репозиторий CG, ветка lab4. URL: <link href="https://github.com/qwertyvich/CG/tree/lab4" color="black">https://github.com/qwertyvich/CG/tree/lab4</link> (дата обращения: 20.11.2026).',
    'Khronos Group. Vulkan Specification. URL: <link href="https://docs.vulkan.org/spec/latest/" color="black">https://docs.vulkan.org/spec/latest/</link> (дата обращения: 20.11.2026).',
    'ГОСТ 7.32-2017. Система стандартов по информации, библиотечному и издательскому делу. Отчет о научно-исследовательской работе. Структура и правила оформления. Москва: Стандартинформ, 2018.',
    'Образец отчета по лабораторной работе курса «Компьютерная графика». Файл 3XX_Фамилия_ЛабX.docx. Предоставленный шаблон оформления.'
]
for i,text in enumerate(sources,1):story.append(p(f'{i}. {text}','source'))
ReportDoc(OUTPUT).multiBuild(story)
print(OUTPUT)
