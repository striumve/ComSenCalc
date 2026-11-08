# -*- coding: utf-8 -*-
"""把 INTERVIEW.md 转成排版良好的 Word 文档（标题层级 / 表格 / 代码块 / 列表 / 引用）。"""
import re
import sys

from docx import Document
from docx.shared import Pt, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

SRC = r'D:\coding\stm32\ComSenCalc\INTERVIEW.md'
OUT = r'D:\coding\stm32\ComSenCalc\INTERVIEW.docx'

BODY_FONT = '微软雅黑'
MONO_FONT = 'Consolas'
INK = RGBColor(0x1F, 0x2A, 0x37)
BODY = RGBColor(0x26, 0x2E, 0x38)
ACCENT = RGBColor(0x1F, 0x6F, 0xB2)
MUTED = RGBColor(0x5A, 0x66, 0x74)
CODECLR = RGBColor(0x1B, 0x3A, 0x52)
QUOTE = RGBColor(0x44, 0x4F, 0x5C)
HDRFILL = 'DCE6F1'
CODEFILL = 'F2F5F8'
QUOTEFILL = 'F7F9FB'
CONTENT_W = Cm(16.0)          # A4 减去左右各 2cm 边距


# --------------------------------------------------------------------- 基础工具
def set_run(run, font=BODY_FONT, size=10.5, bold=None, italic=None, color=BODY,
            mono=False):
    if mono:
        font = MONO_FONT
    run.font.size = Pt(size)
    if bold is not None:
        run.font.bold = bold
    if italic is not None:
        run.font.italic = italic
    if color is not None:
        run.font.color.rgb = color
    run.font.name = font
    rPr = run._element.get_or_add_rPr()
    rFonts = rPr.find(qn('w:rFonts'))
    if rFonts is None:
        rFonts = OxmlElement('w:rFonts')
        rPr.insert(0, rFonts)
    rFonts.set(qn('w:ascii'), font)
    rFonts.set(qn('w:hAnsi'), font)
    rFonts.set(qn('w:eastAsia'), BODY_FONT if mono else font)


def shade(paragraph_or_cell, fill):
    el = paragraph_or_cell._p if hasattr(paragraph_or_cell, '_p') else paragraph_or_cell._tc
    pr = el.get_or_add_pPr() if hasattr(el, 'get_or_add_pPr') else el.get_or_add_tcPr()
    shd = OxmlElement('w:shd')
    shd.set(qn('w:val'), 'clear')
    shd.set(qn('w:color'), 'auto')
    shd.set(qn('w:fill'), fill)
    pr.append(shd)


def left_border(paragraph, color='B9C7D5', size=18):
    pPr = paragraph._p.get_or_add_pPr()
    bdr = OxmlElement('w:pBdr')
    left = OxmlElement('w:left')
    left.set(qn('w:val'), 'single')
    left.set(qn('w:sz'), str(size))
    left.set(qn('w:space'), '6')
    left.set(qn('w:color'), color)
    bdr.append(left)
    pPr.append(bdr)


def bottom_border(paragraph, color='1F6FB2', size=8):
    pPr = paragraph._p.get_or_add_pPr()
    bdr = OxmlElement('w:pBdr')
    bot = OxmlElement('w:bottom')
    bot.set(qn('w:val'), 'single')
    bot.set(qn('w:sz'), str(size))
    bot.set(qn('w:space'), '2')
    bot.set(qn('w:color'), color)
    bdr.append(bot)
    pPr.append(bdr)


def spacing(paragraph, before=0, after=0, line=None):
    pf = paragraph.paragraph_format
    pf.space_before = Pt(before)
    pf.space_after = Pt(after)
    if line is not None:
        pf.line_spacing = line


# --------------------------------------------------------------------- 行内解析
INLINE = re.compile(r'(\*\*.+?\*\*|`[^`]+`)')


def add_inline(paragraph, text, size=10.5, color=BODY, base_bold=False):
    """支持 **加粗** 与 `代码` 的嵌套（例如 **Shift + `.`**）。"""
    buf = []
    bold = bool(base_bold)
    code = False

    def flush():
        if not buf:
            return
        s = ''.join(buf)
        del buf[:]
        r = paragraph.add_run(s)
        if code:
            set_run(r, size=size - 0.5, bold=bold, color=CODECLR, mono=True)
        else:
            set_run(r, size=size, bold=bold, color=color)

    i, n = 0, len(text)
    while i < n:
        if text.startswith('**', i):
            flush()
            bold = not bold
            i += 2
            continue
        if text[i] == '`':
            flush()
            code = not code
            i += 1
            continue
        if text[i] == '\\' and i + 1 < n and text[i + 1] in '|`*_':
            buf.append(text[i + 1])
            i += 2
            continue
        buf.append(text[i])
        i += 1
    flush()


def latex_to_text(s):
    s = s.strip()
    if s.startswith('$$') and s.endswith('$$'):
        s = s[2:-2]
    s = s.replace(r'\approx', ' ≈ ').replace(r'\cdots', ' … ')
    s = s.replace(r'\cdot', ' · ')
    s = re.sub(r'\\left|\\right|\\Big|\\big|\\!|\\,|\\;', '', s)
    s = re.sub(r'\\int_\{([^}]*)\}\^\{([^}]*)\}', r'∫[\1 → \2]', s)
    s = re.sub(r'\\int_([A-Za-z0-9])\^([A-Za-z0-9])', r'∫[\1 → \2]', s)
    s = re.sub(r'\\frac\{([^{}]*)\}\{([^{}]*)\}', r'(\1) / (\2)', s)
    s = re.sub(r'_\{([^{}]*)\}', r'(\1)', s)
    s = re.sub(r'_([A-Za-z0-9])', r'\1', s)
    s = re.sub(r'\\[A-Za-z]+', '', s)
    s = s.replace('{', '').replace('}', '')
    s = s.replace('(h) / (3)', 'h/3')
    s = s.replace('h/3(', 'h/3 · (').replace('h/3[', 'h/3 · [')
    s = re.sub(r'\s+', ' ', s).strip()
    return s


# --------------------------------------------------------------------- 文档骨架
doc = Document()
st = doc.styles['Normal']
st.font.name = BODY_FONT
st.font.size = Pt(10.5)
st.element.rPr.rFonts.set(qn('w:eastAsia'), BODY_FONT)
st.paragraph_format.space_after = Pt(4)
st.paragraph_format.line_spacing = 1.25

sec = doc.sections[0]
sec.page_width = Cm(21.0)
sec.page_height = Cm(29.7)
sec.left_margin = Cm(2.0)
sec.right_margin = Cm(2.0)
sec.top_margin = Cm(2.0)
sec.bottom_margin = Cm(2.0)

# 页脚页码
fp = sec.footer.paragraphs[0]
fp.alignment = WD_ALIGN_PARAGRAPH.CENTER
fld = OxmlElement('w:fldSimple')
fld.set(qn('w:instr'), 'PAGE')
r = OxmlElement('w:r')
t = OxmlElement('w:t')
t.text = '1'
r.append(t)
fld.append(r)
fp._p.append(fld)
for run in fp.runs:
    set_run(run, size=9, color=MUTED)


# --------------------------------------------------------------------- 解析
raw = open(SRC, encoding='utf-8').read().replace('\r\n', '\n')
lines = raw.split('\n')

def is_table_sep(s):
    return bool(re.match(r'^\|[\s:|-]+\|$', s.strip()))

def cells_of(s):
    """按 | 切分单元格，但不动代码区（`...`）里的竖线，并还原 \\| 转义。"""
    s = s.strip()
    if s.startswith('|'):
        s = s[1:]
    if s.endswith('|'):
        s = s[:-1]
    out, buf, code = [], [], False
    i = 0
    while i < len(s):
        ch = s[i]
        if ch == '\\' and i + 1 < len(s) and s[i + 1] in '|`*_':
            buf.append(s[i + 1])
            i += 2
            continue
        if ch == '`':
            code = not code
            buf.append(ch)
            i += 1
            continue
        if ch == '|' and not code:
            out.append(''.join(buf).strip())
            buf = []
            i += 1
            continue
        buf.append(ch)
        i += 1
    out.append(''.join(buf).strip())
    return out

def w_units(s):
    n = 0.0
    for ch in s:
        n += 2.0 if ord(ch) > 0x2E80 else 1.0
    return n

def add_table(rows):
    ncols = max(len(r) for r in rows)
    rows = [r + [''] * (ncols - len(r)) for r in rows]
    tbl = doc.add_table(rows=len(rows), cols=ncols)
    tbl.style = 'Table Grid'
    tbl.alignment = WD_TABLE_ALIGNMENT.LEFT
    tbl.autofit = False

    # 按内容宽度分配列宽
    weights = []
    for c in range(ncols):
        w = max(w_units(r[c]) for r in rows)
        weights.append(max(w, 6.0))
    total = sum(weights)
    widths = [CONTENT_W * (w / total) for w in weights]

    for ri, row in enumerate(rows):
        tr = tbl.rows[ri]._tr
        trPr = tr.get_or_add_trPr()
        cant = OxmlElement('w:cantSplit')
        trPr.append(cant)
        for ci, val in enumerate(row):
            cell = tbl.cell(ri, ci)
            cell.width = widths[ci]
            p = cell.paragraphs[0]
            p.paragraph_format.space_before = Pt(1.5)
            p.paragraph_format.space_after = Pt(1.5)
            p.paragraph_format.line_spacing = 1.15
            if ri == 0:
                add_inline(p, val, size=9.5, color=INK, base_bold=True)
                shade(cell, HDRFILL)
            else:
                add_inline(p, val, size=9.5)
    return tbl

i = 0
first_h1_done = False
while i < len(lines):
    line = lines[i]
    stripped = line.strip()

    # 代码块
    if stripped.startswith('```'):
        i += 1
        block = []
        while i < len(lines) and not lines[i].strip().startswith('```'):
            block.append(lines[i])
            i += 1
        i += 1
        for code in block:
            p = doc.add_paragraph()
            spacing(p, before=0, after=0, line=1.0)
            p.paragraph_format.left_indent = Cm(0.35)
            p.paragraph_format.right_indent = Cm(0.1)
            shade(p, CODEFILL)
            r = p.add_run(code if code else ' ')
            set_run(r, size=8.0, color=CODECLR, mono=True)
        tail = doc.add_paragraph()
        spacing(tail, before=0, after=6, line=1.0)
        continue

    # 表格
    if stripped.startswith('|') and i + 1 < len(lines) and is_table_sep(lines[i + 1]):
        rows = [cells_of(stripped)]
        i += 2
        while i < len(lines) and lines[i].strip().startswith('|'):
            rows.append(cells_of(lines[i]))
            i += 1
        add_table(rows)
        gap = doc.add_paragraph()
        spacing(gap, before=0, after=6, line=1.0)
        continue

    # 空行
    if not stripped:
        i += 1
        continue

    # 分隔线
    if re.match(r'^(-{3,}|\*{3,}|_{3,})$', stripped):
        p = doc.add_paragraph()
        spacing(p, before=2, after=8, line=1.0)
        bottom_border(p, 'D5DDE5', 6)
        i += 1
        continue

    # 标题
    m = re.match(r'^(#{1,6})\s+(.*)$', stripped)
    if m:
        level = len(m.group(1))
        text = m.group(2).strip()
        if level == 1 and not first_h1_done:
            first_h1_done = True
            p = doc.add_paragraph()
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
            spacing(p, before=0, after=14)
            add_inline(p, text, size=20, color=INK, base_bold=True)
            for r in p.runs:
                r.font.bold = True
            bottom_border(p, '1F6FB2', 12)
            i += 1
            continue
        sizes = {1: 16.5, 2: 13.5, 3: 11.8, 4: 11.0, 5: 10.5, 6: 10.5}
        p = doc.add_paragraph()
        if level == 1:
            spacing(p, before=14, after=7)
            bottom_border(p, 'C9D6E4', 6)
        elif level == 2:
            spacing(p, before=12, after=5)
        else:
            spacing(p, before=9, after=4)
        add_inline(p, text, size=sizes[level],
                   color=INK if level <= 3 else BODY, base_bold=True)
        for r in p.runs:
            r.font.bold = True
        i += 1
        continue

    # 引用
    if stripped.startswith('>'):
        block = []
        while i < len(lines) and lines[i].strip().startswith('>'):
            block.append(lines[i].strip()[1:].strip())
            i += 1
        for idx, q in enumerate(block):
            if not q:
                continue
            p = doc.add_paragraph()
            spacing(p, before=2 if idx == 0 else 0, after=2 if idx == len(block) - 1 else 0,
                    line=1.25)
            p.paragraph_format.left_indent = Cm(0.5)
            p.paragraph_format.right_indent = Cm(0.2)
            left_border(p)
            shade(p, QUOTEFILL)
            add_inline(p, q, size=10.5, color=QUOTE)
        continue

    # 公式
    if stripped.startswith('$$'):
        p = doc.add_paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        spacing(p, before=5, after=5)
        r = p.add_run(latex_to_text(stripped))
        set_run(r, size=11, italic=True, color=INK)
        i += 1
        continue

    # 列表
    m = re.match(r'^(\s*)[-*+]\s+(.*)$', line)
    if m:
        indent = len(m.group(1))
        level = min(indent // 2, 2)
        style = 'List Bullet' if level == 0 else ('List Bullet 2' if level == 1 else 'List Bullet 3')
        p = doc.add_paragraph(style=style)
        spacing(p, before=0, after=3, line=1.25)
        add_inline(p, m.group(2), size=10.5)
        i += 1
        continue

    m = re.match(r'^(\s*)(\d+)\.\s+(.*)$', line)
    if m:
        indent = len(m.group(1))
        p = doc.add_paragraph()
        spacing(p, before=0, after=3, line=1.25)
        p.paragraph_format.left_indent = Cm(0.75 + 0.5 * min(indent // 2, 3))
        p.paragraph_format.first_line_indent = Cm(-0.5)
        r = p.add_run(m.group(2) + '. ')
        set_run(r, size=10.5)
        add_inline(p, m.group(3), size=10.5)
        i += 1
        continue

    # 普通段落
    p = doc.add_paragraph()
    spacing(p, before=0, after=5, line=1.3)
    add_inline(p, stripped, size=10.5)
    i += 1

doc.save(OUT)
sys.stdout.write('saved %s | paragraphs=%d tables=%d\n'
                 % (OUT, len(doc.paragraphs), len(doc.tables)))
