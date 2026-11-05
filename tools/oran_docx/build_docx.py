#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Build the O-RAN Word edition of a specification from its Markdown folder.

    build_docx.py docs/e3ap
    build_docx.py docs/e3ap -o /tmp/e3ap.docx --no-pages

The folder holds the Markdown chapters and one manifest, docx.json:

    {
      "number": "O-RAN.XXX.TS.E3AP-R000-v00.01",
      "title":  "E3 Application Protocol (E3AP)",
      "cover":  ["E3 Application Protocol (E3AP)", ""],
      "parts": [
        {"clause": "1", "title": "Scope", "file": "README.md", "section": "Scope"},
        {"clause": "5", "title": "Identity", "file": "01-identity.md"},
        {"clause": "A", "title": "Notes", "file": "06-notes.md", "informative": true}
      ],
      "history": [["2026-11", "R000 v00.01", "First version", "Northeastern University"]]
    }

A part is a whole Markdown file, or with "section" the text under one level 2
heading of the file. The clause number gives the part its place and its heading
level (one dot per level). A part without "file" is a bare heading. Headings
inside a part are numbered under the part. A leading number in a Markdown
heading is dropped. A paragraph that starts with "Table:" and sits directly
above a table becomes the table caption. An image on a line of its own,
![caption](figures/flow.png), becomes a figure with that caption. Fenced
blocks become a box of monospaced lines.

A link to a file of the same specification becomes its clause number
("clause 8", "Annex A", or with an anchor "clause 8.2"); every other link
becomes an absolute GitHub URL. The sentence "The index is README.md." is
left out.

The styles, headers, footers and cover come from template.docx, which
make_template.py cuts from an O-RAN specification and gives the copyright of
Northeastern University. The page numbers of the table of contents come from a LibreOffice layout, so Word may move them by a
page: press F9 on the table of contents in Word to refresh them.
"""
import argparse
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile
from xml.sax.saxutils import escape

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(os.path.dirname(HERE))
# Links to files outside the specification point here, on the main branch.
REPO_URL = "https://github.com/wineslab/libe3"
TOC_MARK = "@@TOC@@"
BODY_MARK = "@@BODY@@"
TEMPLATE_NUMBER = "O-RAN.WG3.TS.E2SM-KPM-R005-v08.00"
TABLE_WIDTH = 9631
MONO = '<w:rFonts w:ascii="Courier New" w:hAnsi="Courier New" w:cs="Courier New"/>'
CODE_BOX = (
    "<w:pBdr>"
    + "".join(f'<w:{side} w:val="single" w:sz="4" w:space="4" w:color="A6A6A6"/>' for side in ("top", "left", "bottom", "right"))
    + '</w:pBdr><w:shd w:val="clear" w:color="auto" w:fill="F2F2F2"/>'
    '<w:spacing w:after="0" w:line="240" w:lineRule="auto"/><w:ind w:left="113" w:right="113"/>'
)

# ---------------------------------------------------------------- inline text


def run(text, bold=False, italic=False, code=False, size=None, font=None):
    """One <w:r> element."""
    rpr = ""
    if code:
        rpr += MONO + '<w:sz w:val="18"/><w:szCs w:val="18"/>'
    elif font:
        rpr += font
    if bold:
        rpr += "<w:b/>"
    if italic:
        rpr += "<w:i/>"
    if size and not code:
        rpr += f'<w:sz w:val="{size}"/><w:szCs w:val="{size}"/>'
    rpr = f"<w:rPr>{rpr}</w:rPr>" if rpr else ""
    parts = text.split("\t")
    inner = []
    for i, part in enumerate(parts):
        if i:
            inner.append("<w:tab/>")
        if part:
            inner.append(f'<w:t xml:space="preserve">{escape(part)}</w:t>')
    return f"<w:r>{rpr}{''.join(inner)}</w:r>"


INLINE = re.compile(
    r"(`+)(?P<code>.+?)\1"
    r"|\*\*(?P<bold>.+?)\*\*"
    r"|(?<![\w*])\*(?P<ital>[^*\s][^*]*?)\*(?![\w*])"
    r"|\[(?P<ltext>[^\]]+)\]\((?P<lurl>[^)\s]+)\)"
)


def inline_runs(text, **base):
    """Markdown inline text to runs. Links keep their text; only an external URL is spelled out."""
    out = []
    pos = 0
    for m in INLINE.finditer(text):
        if m.start() > pos:
            out.append(run(unescape_md(text[pos:m.start()]), **base))
        if m.group("code") is not None:
            out.append(run(m.group("code").strip(), **{**base, "code": True}))
        elif m.group("bold") is not None:
            out.append(run(unescape_md(m.group("bold")), **{**base, "bold": True}))
        elif m.group("ital") is not None:
            out.append(run(unescape_md(m.group("ital")), **{**base, "italic": True}))
        else:
            label, url = m.group("ltext"), m.group("lurl")
            if url.startswith("http") or LINKS is None:
                out.append("".join(inline_runs(label, **base)))
                if url.startswith("http"):
                    out.append(run(f" ({url})", **base))
            else:
                out.append(LINKS.runs(label, url, base))
        pos = m.end()
    if pos < len(text):
        out.append(run(unescape_md(text[pos:]), **base))
    return out


def unescape_md(s):
    return re.sub(r"\\([\\`*_{}\[\]()#+\-.!|<>])", r"\1", s)


def plain(text):
    """Text without Markdown marks, for the table of contents and for widths."""
    t = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text)
    t = re.sub(r"[`*]", "", t)
    return unescape_md(t)


# ---------------------------------------------------------------------- links


def slug(heading):
    """The anchor that GitHub gives a heading."""
    return re.sub(r"[^\w\- ]", "", plain(heading).strip().lower()).replace(" ", "-")


class Links:
    """Links of the Markdown edition, made into references that a reader of the Word edition can follow.

    A link to a file of the specification becomes the clause that holds its target. The
    clause numbers are known only when every part is rendered, so a link leaves a marker
    in the text, and resolve() puts the number in. Any other link becomes a GitHub URL.
    """

    def __init__(self, folder, parts):
        self.folder = os.path.abspath(folder)
        self.files = {os.path.join(self.folder, p["file"]) for p in parts if "file" in p}
        self.whole = {}    # file -> clause of the part that is the whole file
        self.anchors = {}  # (file, anchor) -> clause
        self.seen = {}     # (file, slug) -> times seen, for GitHub's -1, -2 suffixes
        self.refs = []     # (file, anchor, url to fall back to)
        self.current = None

    def enter(self, part, clause):
        self.current = os.path.join(self.folder, part["file"])
        if part.get("section"):
            self.add(self.current, slug(part["section"]), clause)
        else:
            self.whole[self.current] = clause

    def add(self, path, anchor, clause):
        n = self.seen.get((path, anchor), 0)
        self.seen[(path, anchor)] = n + 1
        self.anchors.setdefault((path, anchor if n == 0 else f"{anchor}-{n}"), clause)

    def heading(self, text, ids, clause):
        self.add(self.current, slug(text), clause)
        for i in ids:
            self.anchors.setdefault((self.current, i), clause)

    def url(self, path, anchor):
        rel = os.path.relpath(path, REPO_ROOT).replace(os.sep, "/")
        if os.path.basename(path) == "README.md" and not anchor:
            return f"{REPO_URL}/tree/main/{os.path.dirname(rel)}"
        return f"{REPO_URL}/blob/main/{rel}" + (f"#{anchor}" if anchor else "")

    def runs(self, label, url, base):
        path, _, anchor = url.partition("#")
        target = os.path.normpath(os.path.join(os.path.dirname(self.current), path)) if path else self.current
        text = plain(label).strip()
        # A label that only names the target: the path, the file name, its number
        # ("06" for 06-discrepancies.md) or a clause number of the Markdown edition.
        bare = (
            text in (url, path, os.path.basename(path))
            or (re.fullmatch(r"\d\d", text) and os.path.basename(path).startswith(text + "-"))
            or re.fullmatch(r"(?i)(clause|section|annex)\s+[\w.]+", text)
        )
        if target in self.files:
            self.refs.append((target, anchor, self.url(target, anchor)))
            ref = f"@@REF{len(self.refs) - 1}@@"
        else:
            ref = self.url(target, anchor)
        if bare:
            return run(ref, **base)
        return "".join(inline_runs(label, **base)) + run(f" ({ref})", **base)

    def resolve(self, xml):
        def number(m):
            target, anchor, fallback = self.refs[int(m.group(1))]
            clause = self.anchors.get((target, anchor))
            if clause is None:
                if anchor:
                    print(f"note: no heading for #{anchor} in {os.path.relpath(target, REPO_ROOT)}", file=sys.stderr)
                clause = self.whole.get(target)
            if clause is None:
                return fallback
            return f"Annex {clause}" if clause[0].isalpha() and "." not in clause else f"clause {clause}"
        return re.sub(r"@@REF(\d+)@@", number, xml)


LINKS = None  # the Links of the specification being built


# ------------------------------------------------------------- markdown blocks


def text_weight(text):
    """Width of a table cell: a monospaced span counts a third more than plain text."""
    code = sum(len(m.group("code")) for m in INLINE.finditer(text) if m.group("code") is not None)
    return len(plain(text)) + code // 3


def longest_word(text):
    """Twips that the longest unbreakable word of a cell needs, with room for the cell margins."""
    best = 0
    for m in INLINE.finditer(text):
        if m.group("code") is not None:
            best = max(best, len(m.group("code")) * 112)
    for word in plain(text).split():
        best = max(best, len(word) * 100)
    return best + 220


# A line that holds only HTML anchors, such as <a id="x"></a>, is a link target
# for the Markdown edition. The line is dropped and its ids go to the next heading.
ANCHOR = re.compile(r'^\s*(<a\s+(?:id|name)="[^"]*"\s*>\s*</a>\s*)+$')
ANCHOR_ID = re.compile(r'(?:id|name)="([^"]*)"')
# An image on a line of its own is a figure. Its alt text is the caption.
IMAGE = re.compile(r'^\s*!\[(?P<alt>[^\]]*)\]\((?P<src>[^)\s]+)(?:\s+"[^"]*")?\)\s*$')


def parse_blocks(lines):
    """Split Markdown lines into blocks: heading, para, list, table, code, quote."""
    blocks = []
    ids = []  # anchor ids waiting for the next heading
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        if not line.strip():
            i += 1
            continue
        if line.lstrip().startswith("<!--"):
            while i < n and "-->" not in lines[i]:
                i += 1
            i += 1
            continue
        if ANCHOR.match(line):
            ids += ANCHOR_ID.findall(line)
            i += 1
            continue
        m = IMAGE.match(line)
        if m:
            blocks.append(("image", m.group("alt"), m.group("src")))
            i += 1
            continue
        m = re.match(r"^(\s*)(`{3,}|~{3,})\s*([\w+-]*)\s*$", line)
        if m:
            fence, lang = m.group(2), m.group(3)
            i += 1
            body = []
            while i < n and not lines[i].lstrip().startswith(fence):
                body.append(lines[i].rstrip("\n"))
                i += 1
            i += 1
            blocks.append(("code", lang, body))
            continue
        m = re.match(r"^(#{1,6})\s+(.*?)\s*#*\s*$", line)
        if m:
            blocks.append(("heading", len(m.group(1)), m.group(2), ids))
            ids = []
            i += 1
            continue
        if re.match(r"^\s*(-{3,}|\*{3,}|_{3,})\s*$", line):
            i += 1
            continue
        if line.lstrip().startswith("|") and i + 1 < n and re.match(r"^\s*\|?\s*:?-{2,}", lines[i + 1]):
            rows = []
            while i < n and lines[i].lstrip().startswith("|"):
                rows.append(split_row(lines[i]))
                i += 1
            blocks.append(("table", rows[0], rows[2:]))
            continue
        if line.lstrip().startswith(">"):
            quote = []
            while i < n and lines[i].lstrip().startswith(">"):
                quote.append(re.sub(r"^\s*>\s?", "", lines[i]))
                i += 1
            blocks.append(("quote", " ".join(q.strip() for q in quote)))
            continue
        m = re.match(r"^(\s*)([-*+]|\d+[.)])\s+(.*)$", line)
        if m:
            items = []
            while i < n:
                mm = re.match(r"^(\s*)([-*+]|\d+[.)])\s+(.*)$", lines[i])
                if mm:
                    items.append([len(mm.group(1)) // 2, bool(mm.group(2)[0].isdigit()), mm.group(3)])
                    i += 1
                elif lines[i].strip() and lines[i].startswith("  ") and items:
                    items[-1][2] += " " + lines[i].strip()
                    i += 1
                else:
                    break
            blocks.append(("list", items))
            continue
        para = [line.strip()]
        i += 1
        while i < n and lines[i].strip() and not ANCHOR.match(lines[i]) and not IMAGE.match(lines[i]) and not re.match(
            r"^(\s*([-*+]|\d+[.)])\s|#{1,6}\s|\s*`{3,}|\s*\||\s*>|\s*<!--)", lines[i]
        ):
            para.append(lines[i].strip())
            i += 1
        blocks.append(("para", " ".join(para)))
    return blocks


def split_row(line):
    s = line.strip().strip("|")
    cells = re.split(r"(?<!\\)\|", s)
    return [c.strip().replace("\\|", "|") for c in cells]


def select(blocks, section):
    """Blocks of one file for a part: all but the title, or the body of one level 2 heading."""
    if section is None:
        if blocks and blocks[0][0] == "heading" and blocks[0][1] == 1:
            return blocks[1:], 1
        return blocks, 1
    out = []
    taking = False
    for b in blocks:
        if b[0] == "heading" and b[1] == 2:
            if taking:
                break
            taking = plain(b[2]).strip() == section
            continue
        if taking:
            out.append(b)
    if not out and not taking:
        raise SystemExit(f"section '{section}' not found")
    return out, 2


# --------------------------------------------------------------------- output


EMU_PER_INCH = 914400
MAX_FIGURE_EMU = (TABLE_WIDTH * 635, 8 * EMU_PER_INCH)  # text width, and 8 inches of height
DRAWINGML = "http://schemas.openxmlformats.org/drawingml/2006"
IMAGE_REL = "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image"


def image_size(path):
    """Extension, and width and height in EMU at the resolution the file states (96 dpi if none)."""
    if not os.path.isfile(path):
        raise SystemExit(f"{path}: figure not found")
    with open(path, "rb") as f:
        data = f.read()
    dpi = 96.0
    if data[:8] == b"\x89PNG\r\n\x1a\n":
        ext = "png"
        w, h = struct.unpack(">II", data[16:24])
        i = 8
        while i + 8 <= len(data):
            length, kind = struct.unpack(">I4s", data[i:i + 8])
            if kind == b"pHYs":
                ppu, _, unit = struct.unpack(">IIB", data[i + 8:i + 17])
                if unit == 1 and ppu:
                    dpi = ppu * 0.0254
            if kind in (b"IDAT", b"IEND"):
                break
            i += 12 + length
    elif data[:2] == b"\xff\xd8":
        ext = "jpeg"
        w = h = 0
        i = 2
        while i + 4 <= len(data) and data[i] == 0xFF:
            marker = data[i + 1]
            if marker == 0xD8 or 0xD0 <= marker <= 0xD7:
                i += 2
                continue
            length = struct.unpack(">H", data[i + 2:i + 4])[0]
            if marker == 0xE0 and data[i + 4:i + 9] == b"JFIF\0":
                units, density = data[i + 11], struct.unpack(">H", data[i + 12:i + 14])[0]
                if density and units in (1, 2):
                    dpi = density if units == 1 else density * 2.54
            if 0xC0 <= marker <= 0xCF and marker not in (0xC4, 0xC8, 0xCC):
                h, w = struct.unpack(">HH", data[i + 5:i + 9])
                break
            i += 2 + length
        if not w or not h:
            raise SystemExit(f"{path}: no frame size in the JPEG file")
    else:
        raise SystemExit(f"{path}: a figure must be a PNG or JPEG file")
    cx, cy = w / dpi * EMU_PER_INCH, h / dpi * EMU_PER_INCH
    scale = min(1.0, MAX_FIGURE_EMU[0] / cx, MAX_FIGURE_EMU[1] / cy)
    return data, ext, int(cx * scale), int(cy * scale)


class Doc:
    def __init__(self):
        self.xml = []
        self.headings = []  # (depth, number, title, bookmark)
        self.bm = 0
        self.table_no = {}
        self.figure_no = {}
        self.media = []  # (part name under word/, bytes, relationship id)

    def para(self, runs, style=None, ppr=""):
        st = f'<w:pStyle w:val="{style}"/>' if style else ""
        p = f"<w:pPr>{st}{ppr}</w:pPr>" if (st or ppr) else ""
        self.xml.append(f"<w:p>{p}{''.join(runs)}</w:p>")

    def heading(self, depth, number, title, annex_label=None):
        self.bm += 1
        name = f"_Toc9{self.bm:07d}"
        bid = 1000 + self.bm
        d = min(depth, 5)
        if annex_label:
            body = run(annex_label) + run("\t" + title)
        else:
            body = run(number) + run("\t" + title)
        self.xml.append(
            f'<w:p><w:pPr><w:pStyle w:val="Heading{d}"/></w:pPr>'
            f'<w:bookmarkStart w:id="{bid}" w:name="{name}"/>{body}<w:bookmarkEnd w:id="{bid}"/></w:p>'
        )
        self.headings.append((depth, annex_label or number, title, name))

    def code(self, lines):
        """PL paragraphs with one border and shading, which Word and LibreOffice join into one box."""
        for ln in lines:
            r = f'<w:r><w:t xml:space="preserve">{escape(ln.expandtabs(4))}</w:t></w:r>' if ln.strip() else ""
            self.para([r] if r else [], style="PL", ppr=CODE_BOX)
        self.para([], ppr='<w:spacing w:after="0"/>')

    def table(self, header, rows, caption, top):
        cols = len(header)
        weights = []
        floors = []
        for c in range(cols):
            cells = [header[c]] + [r[c] if c < len(r) else "" for r in rows]
            weights.append(max(8, min(60, max(text_weight(x) for x in cells))))
            floors.append(min(2600, max(longest_word(x) for x in cells)))
        total_floor = sum(floors)
        if total_floor > TABLE_WIDTH:
            floors = [f * TABLE_WIDTH // total_floor for f in floors]
            total_floor = sum(floors)
        spare = TABLE_WIDTH - total_floor
        wsum = sum(weights)
        widths = [f + spare * w // wsum for f, w in zip(floors, weights)]
        widths[-1] += TABLE_WIDTH - sum(widths)
        if caption:
            self.table_no[top] = self.table_no.get(top, 0) + 1
            cap = f"Table {top}-{self.table_no[top]}: {caption}"
            self.para(
                [run(cap, bold=True)],
                ppr='<w:keepNext/><w:spacing w:before="120" w:after="120" w:line="240" w:lineRule="auto"/><w:jc w:val="center"/>',
            )
        grid = "".join(f'<w:gridCol w:w="{w}"/>' for w in widths)
        out = [
            f'<w:tbl><w:tblPr><w:tblStyle w:val="TableGrid"/><w:tblW w:w="{TABLE_WIDTH}" w:type="dxa"/>'
            '<w:tblLayout w:type="fixed"/>'
            '<w:tblLook w:val="04A0" w:firstRow="1" w:lastRow="0" w:firstColumn="1" w:lastColumn="0" w:noHBand="0" w:noVBand="1"/>'
            f"</w:tblPr><w:tblGrid>{grid}</w:tblGrid>"
        ]
        arial = '<w:rFonts w:ascii="Arial" w:hAnsi="Arial" w:cs="Arial"/>'
        for ri, row in enumerate([header] + rows):
            row = (row + [""] * cols)[:cols]
            tr = "<w:tr>" + ('<w:trPr><w:cantSplit/><w:tblHeader/></w:trPr>' if ri == 0 else "<w:trPr><w:cantSplit/></w:trPr>")
            for ci, cell in enumerate(row):
                if ri == 0:
                    runs = inline_runs(cell, bold=True, font=arial, size=20)
                    ppr = '<w:keepNext/><w:spacing w:before="60" w:after="60" w:line="240" w:lineRule="auto"/><w:jc w:val="center"/>'
                    shade = '<w:shd w:val="clear" w:color="auto" w:fill="D9D9D9"/>'
                else:
                    runs = inline_runs(cell, font=arial, size=18)
                    ppr = '<w:spacing w:before="20" w:after="20" w:line="240" w:lineRule="auto"/>'
                    shade = ""
                tr += (
                    f'<w:tc><w:tcPr><w:tcW w:w="{widths[ci]}" w:type="dxa"/>{shade}</w:tcPr>'
                    f"<w:p><w:pPr>{ppr}</w:pPr>{''.join(runs)}</w:p></w:tc>"
                )
            out.append(tr + "</w:tr>")
        out.append("</w:tbl>")
        self.xml.append("".join(out))
        self.para([], ppr='<w:spacing w:after="120"/>')

    def figure(self, path, caption, top):
        data, ext, cx, cy = image_size(path)
        n = len(self.media) + 1
        rid, part, pid = f"rIdFig{n}", f"media/figure{n}.{ext}", 1000 + n
        self.media.append((part, data, rid))
        descr = escape(caption, {'"': "&quot;"})
        drawing = (
            f'<w:r><w:drawing><wp:inline distT="0" distB="0" distL="0" distR="0">'
            f'<wp:extent cx="{cx}" cy="{cy}"/><wp:effectExtent l="0" t="0" r="0" b="0"/>'
            f'<wp:docPr id="{pid}" name="Figure {n}" descr="{descr}"/>'
            f'<wp:cNvGraphicFramePr><a:graphicFrameLocks xmlns:a="{DRAWINGML}/main" noChangeAspect="1"/></wp:cNvGraphicFramePr>'
            f'<a:graphic xmlns:a="{DRAWINGML}/main"><a:graphicData uri="{DRAWINGML}/picture">'
            f'<pic:pic xmlns:pic="{DRAWINGML}/picture">'
            f'<pic:nvPicPr><pic:cNvPr id="{pid}" name="figure{n}.{ext}"/>'
            f'<pic:cNvPicPr><a:picLocks noChangeAspect="1"/></pic:cNvPicPr></pic:nvPicPr>'
            f'<pic:blipFill><a:blip r:embed="{rid}"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill>'
            f'<pic:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="{cx}" cy="{cy}"/></a:xfrm>'
            f'<a:prstGeom prst="rect"><a:avLst/></a:prstGeom></pic:spPr>'
            f"</pic:pic></a:graphicData></a:graphic></wp:inline></w:drawing></w:r>"
        )
        self.para([drawing], style="TH")
        self.figure_no[top] = self.figure_no.get(top, 0) + 1
        self.para([run(f"Figure {top}-{self.figure_no[top]}: {caption}")], style="TF")

    def lst(self, items):
        counters = {}
        for level, ordered, text in items:
            level = min(level, 2)
            style = ["B10", "B20", "B30"][level]
            counters = {k: v for k, v in counters.items() if k <= level}
            if ordered:
                counters[level] = counters.get(level, 0) + 1
                mark = f"{counters[level]})"
            else:
                mark = "-"
            self.para([run(mark + "\t")] + inline_runs(text), style=style)

    def note(self, text):
        ppr = '<w:keepLines/><w:spacing w:after="180" w:line="240" w:lineRule="auto"/><w:ind w:left="1135" w:hanging="851"/>'
        m = re.match(r"^(NOTE\s*\d*:?)\s*(.*)$", text)
        if m:
            self.para([run(m.group(1) + "\t")] + inline_runs(m.group(2)), ppr=ppr)
        else:
            self.para(inline_runs(text), ppr=ppr)


def render_part(doc, part, base):
    clause = part["clause"]
    depth = clause.count(".") + 1
    title = part["title"]
    if clause[0].isalpha() and depth == 1:
        kind = "informative" if part.get("informative", True) else "normative"
        doc.heading(1, clause, title, annex_label=f"Annex {clause} ({kind}):")
    else:
        doc.heading(depth, clause, title)
    top = clause.split(".")[0]
    if "file" not in part:
        return
    LINKS.enter(part, clause)
    with open(os.path.join(base, part["file"]), encoding="utf8") as f:
        blocks = parse_blocks(f.read().split("\n"))
    blocks, top_level = select(blocks, part.get("section"))
    path = [clause]  # clause number of the heading at each level below the part
    counts = {}
    caption = None
    for i, b in enumerate(blocks):
        kind = b[0]
        if kind == "heading":
            rel = max(1, b[1] - top_level)
            text = re.sub(r"^\s*(\d+(\.\d+)*\.?|[A-Z]\.\d+(\.\d+)*)\s+", "", plain(b[2])).strip()
            del path[rel:]
            parent = path[-1]
            counts[parent] = counts.get(parent, 0) + 1
            number = f"{parent}.{counts[parent]}"
            path.append(number)
            doc.heading(depth + rel, number, text)
            LINKS.heading(b[2], b[3], number)
        elif kind == "para":
            if b[1].startswith("Table:") and i + 1 < len(blocks) and blocks[i + 1][0] == "table":
                caption = b[1][len("Table:"):].strip()
            else:
                # The Word edition has its own contents, so it needs no pointer to the index.
                text = re.sub(r"\s*The index is \[README\.md\]\([^)]*\)\.", "", b[1]).strip()
                if text:
                    doc.para(inline_runs(text))
        elif kind == "list":
            doc.lst(b[1])
        elif kind == "table":
            doc.table(b[1], b[2], plain(caption) if caption else None, top)
            caption = None
        elif kind == "image":
            doc.figure(os.path.join(base, b[2]), plain(b[1]), top)
        elif kind == "code" and b[1] == "mermaid":
            doc.para([run("[A diagram is in the Markdown edition of this clause.]", italic=True)])
        elif kind == "code":
            doc.code(b[2])
        elif kind == "quote":
            doc.note(plain(b[1]))


# ------------------------------------------------------------------------ toc


def toc_xml(doc, pages, extra):
    entries = [(1, "", t, n) for (t, n) in extra] + [
        (d, num, t, bm) for (d, num, t, bm) in doc.headings if d <= 3
    ]
    out = []
    for idx, (d, num, title, bm) in enumerate(entries):
        pg = pages.get(bm, "")
        start = ""
        if idx == 0:
            start = (
                '<w:r><w:fldChar w:fldCharType="begin"/></w:r>'
                '<w:r><w:instrText xml:space="preserve"> TOC \\o "1-3" \\h \\z \\u </w:instrText></w:r>'
                '<w:r><w:fldChar w:fldCharType="separate"/></w:r>'
            )
        label = (num + "\t" if num and not num.startswith("Annex") else "") + title
        if num.startswith("Annex"):
            label = num + " " + title
        tabs = {1: 480, 2: 960, 3: 1560}.get(d, 480)
        ppr = (
            f'<w:pStyle w:val="TOC{d}"/><w:tabs><w:tab w:val="left" w:pos="{tabs}"/>'
            f'<w:tab w:val="right" w:leader="dot" w:pos="9631"/></w:tabs>'
        )
        body = run(label) + run("\t")
        body += (
            '<w:r><w:fldChar w:fldCharType="begin"/></w:r>'
            f'<w:r><w:instrText xml:space="preserve"> PAGEREF {bm} \\h </w:instrText></w:r>'
            '<w:r><w:fldChar w:fldCharType="separate"/></w:r>'
            f"{run(str(pg) if pg != '' else '')}"
            '<w:r><w:fldChar w:fldCharType="end"/></w:r>'
        )
        out.append(f"<w:p><w:pPr>{ppr}</w:pPr>{start}{body}</w:p>")
    out.append('<w:p><w:r><w:fldChar w:fldCharType="end"/></w:r></w:p>')
    return "".join(out)


# ---------------------------------------------------------------- front matter


def set_cover(xml, number, cover):
    za = re.search(r"<w:p [^>]*>(?:(?!</w:p>).)*?w:val=\"ZA\".*?</w:p>", xml, re.S)
    p = za.group(0)
    sep = p.index('w:fldCharType="separate"')
    head = p[: p.rfind("<w:r", 0, sep)]
    newp = (
        head
        + '<w:r><w:fldChar w:fldCharType="separate"/></w:r>'
        + run(number)
        + '<w:r><w:fldChar w:fldCharType="end"/></w:r>'
        + "</w:p>"
    )
    xml = xml.replace(p, newp, 1)
    zts = list(re.finditer(r"<w:p [^>]*>(?:(?!</w:p>).)*?w:val=\"ZT\".*?</w:p>", xml, re.S))
    assert len(zts) == 3, "template cover changed"
    for m, text in zip(zts[1:], cover):
        p = m.group(0)
        ppr_end = p.index("</w:pPr>") + len("</w:pPr>")
        xml = xml.replace(p, p[:ppr_end] + (run(text) if text else "") + "</w:p>", 1)
    return xml


def history_table(doc, rows):
    doc.heading(1, "", "Change history", annex_label="Annex (informative):")
    doc.table(["Date", "Revision", "Description", "Author"], rows, None, "H")


# ----------------------------------------------------------------- pagination


def find_soffice():
    for c in ("soffice", "/Applications/LibreOffice.app/Contents/MacOS/soffice", "libreoffice"):
        p = shutil.which(c) or (c if os.path.exists(c) else None)
        if p:
            return p
    return None


def pages_from_pdf(docx_path, headings, extra):
    soffice = find_soffice()
    pdftotext = shutil.which("pdftotext")
    if not soffice or not pdftotext:
        print("note: soffice or pdftotext missing, the table of contents has no page numbers", file=sys.stderr)
        return {}
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(
            [soffice, f"-env:UserInstallation=file://{tmp}/profile", "--headless", "--convert-to", "pdf", "--outdir", tmp, docx_path],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=300,
        )
        pdf = os.path.join(tmp, os.path.splitext(os.path.basename(docx_path))[0] + ".pdf")
        txt = subprocess.run([pdftotext, "-layout", pdf, "-"], check=True, capture_output=True, text=True).stdout
    pages = txt.split("\f")
    result = {}
    cursor = 0
    # The Foreword heading is the first line that is only that word, after the contents.
    for pi, pg in enumerate(pages):
        if re.search(r"^\s*Foreword\s*$", pg, re.M):
            cursor = pi
            break
    for t, bm in extra:
        for pi in range(cursor, len(pages)):
            if re.search(rf"^\s*{re.escape(t)}\s*$", pages[pi], re.M):
                result[bm] = pi + 1
                cursor = pi
                break
    for d, num, title, bm in headings:
        if d > 3:
            continue
        if num.startswith("Annex"):
            pat = rf"^\s*{re.escape(num)}"
        else:
            pat = rf"^\s*{re.escape(num)}\s+{re.escape(title[:30])}"
        for pi in range(cursor, len(pages)):
            if re.search(pat, pages[pi], re.M):
                result[bm] = pi + 1
                cursor = pi
                break
    return result


# ------------------------------------------------------------------------ main


def build(folder, out, with_pages=True):
    with open(os.path.join(folder, "docx.json"), encoding="utf8") as f:
        man = json.load(f)
    global LINKS
    LINKS = Links(folder, man["parts"])
    doc = Doc()
    for part in man["parts"]:
        render_part(doc, part, folder)
    if man.get("history"):
        history_table(doc, man["history"])
    doc.xml = [LINKS.resolve(x) for x in doc.xml]

    extra = [("Foreword", "_Toc223965221"), ("Modal verbs terminology", "_Toc223965222")]
    tpl = zipfile.ZipFile(os.path.join(HERE, "template.docx"))

    def write(path, pages):
        with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as zout:
            for item in tpl.infolist():
                data = tpl.read(item.filename)
                name = item.filename
                if name == "word/document.xml":
                    x = data.decode("utf8")
                    x = set_cover(x, man["number"], man["cover"])
                    x = x.replace(TOC_MARK, toc_xml(doc, pages, extra)).replace(BODY_MARK, "".join(doc.xml))
                    data = x.encode("utf8")
                elif name == "word/_rels/document.xml.rels":
                    rels = "".join(
                        f'<Relationship Id="{rid}" Type="{IMAGE_REL}" Target="{part}"/>' for part, _, rid in doc.media
                    )
                    data = data.decode("utf8").replace("</Relationships>", rels + "</Relationships>").encode("utf8")
                elif name == "[Content_Types].xml":
                    x = data.decode("utf8")
                    for ext in sorted({p.rsplit(".", 1)[1] for p, _, _ in doc.media}):
                        if f'Extension="{ext}"' not in x:
                            x = x.replace("<Override ", f'<Default Extension="{ext}" ContentType="image/{ext}"/><Override ', 1)
                    data = x.encode("utf8")
                elif name in ("word/header1.xml", "word/header2.xml"):
                    data = data.decode("utf8").replace(TEMPLATE_NUMBER, man["number"]).encode("utf8")
                elif name == "docProps/app.xml":
                    data = re.sub(r"(<TitlesOfParts>.*?<vt:lpstr>).*?(</vt:lpstr>)", lambda m: m.group(1) + escape(man["number"]) + m.group(2), data.decode("utf8"), flags=re.S).encode("utf8")
                elif name == "docProps/core.xml":
                    data = re.sub(r"<dc:title>.*?</dc:title>", f"<dc:title>{escape(man['number'])}</dc:title>", data.decode("utf8")).encode("utf8")
                elif name == "docProps/custom.xml":
                    x = data.decode("utf8")
                    x = re.sub(r'(name="Document number"><vt:lpwstr>).*?(</vt:lpwstr>)', lambda m: m.group(1) + escape(man["number"]) + m.group(2), x)
                    x = re.sub(r'(name="TITLE"><vt:lpwstr>).*?(</vt:lpwstr>)', lambda m: m.group(1) + escape(man["title"]) + m.group(2), x)
                    data = x.encode("utf8")
                zout.writestr(item.filename, data)
            for part, blob, _ in doc.media:
                zout.writestr("word/" + part, blob)

    write(out, {})
    if with_pages:
        pages = pages_from_pdf(out, doc.headings, extra)
        write(out, pages)
    print(f"{out}: {len(doc.headings)} headings")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folder")
    ap.add_argument("-o", "--output")
    ap.add_argument("--no-pages", action="store_true", help="skip the LibreOffice pass for page numbers")
    a = ap.parse_args()
    with open(os.path.join(a.folder, "docx.json"), encoding="utf8") as f:
        number = json.load(f)["number"]
    out = a.output or os.path.join(a.folder, number + ".docx")
    build(a.folder, out, not a.no_pages)


if __name__ == "__main__":
    main()
