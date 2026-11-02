#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Build the O-RAN Word edition of a specification from its Markdown folder.

    build_docx.py docs/e3ap
    build_docx.py docs/e3ap -o /tmp/e3ap.docx --no-pages

The folder holds the Markdown chapters and one manifest, docx.json:

    {
      "number": "O-RAN.WG3.TS.E3AP-R000-v00.01",
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
above a table becomes the table caption. Fenced blocks become monospaced lines.

The styles, headers, footers and cover come from template.docx, which
make_template.py cuts from an O-RAN specification. The page numbers of the
table of contents come from a LibreOffice layout, so Word may move them by a
page: press F9 on the table of contents in Word to refresh them.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from xml.sax.saxutils import escape

HERE = os.path.dirname(os.path.abspath(__file__))
TOC_MARK = "@@TOC@@"
BODY_MARK = "@@BODY@@"
TEMPLATE_NUMBER = "O-RAN.WG3.TS.E2SM-KPM-R005-v08.00"
TABLE_WIDTH = 9631
MONO = '<w:rFonts w:ascii="Courier New" w:hAnsi="Courier New" w:cs="Courier New"/>'

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
            out.append("".join(inline_runs(label, **base)))
            if url.startswith("http"):
                out.append(run(f" ({url})", **base))
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


def parse_blocks(lines):
    """Split Markdown lines into blocks: heading, para, list, table, code, quote."""
    blocks = []
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
            blocks.append(("heading", len(m.group(1)), m.group(2)))
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
        while i < n and lines[i].strip() and not re.match(
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


class Doc:
    def __init__(self):
        self.xml = []
        self.headings = []  # (depth, number, title, bookmark)
        self.bm = 0
        self.table_no = {}

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
        for ln in lines:
            ppr = '<w:spacing w:after="0" w:line="240" w:lineRule="auto"/>'
            if ln.strip():
                r = (
                    '<w:r><w:rPr><w:rFonts w:ascii="Courier New" w:hAnsi="Courier New" w:cs="Courier New"/>'
                    '<w:sz w:val="16"/><w:szCs w:val="16"/></w:rPr>'
                    f'<w:t xml:space="preserve">{escape(ln.expandtabs(4))}</w:t></w:r>'
                )
            else:
                r = ""
            self.para([r] if r else [], ppr=ppr)
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
        elif kind == "para":
            if b[1].startswith("Table:") and i + 1 < len(blocks) and blocks[i + 1][0] == "table":
                caption = b[1][len("Table:"):].strip()
            else:
                doc.para(inline_runs(b[1]))
        elif kind == "list":
            doc.lst(b[1])
        elif kind == "table":
            doc.table(b[1], b[2], plain(caption) if caption else None, top)
            caption = None
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
    drawing = re.search(r"<w:r[ >](?:(?!</w:r>).)*?<w:drawing>.*?</w:drawing></w:r>", p, re.S).group(0)
    sep = p.index('w:fldCharType="separate"')
    head = p[: p.rfind("<w:r", 0, sep)]
    newp = (
        head
        + '<w:r><w:fldChar w:fldCharType="separate"/></w:r>'
        + run(number)
        + '<w:r><w:fldChar w:fldCharType="end"/></w:r>'
        + drawing
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
    doc = Doc()
    for part in man["parts"]:
        render_part(doc, part, folder)
    if man.get("history"):
        history_table(doc, man["history"])

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
                elif name in ("word/header1.xml", "word/header2.xml"):
                    data = data.decode("utf8").replace(TEMPLATE_NUMBER, man["number"]).encode("utf8")
                elif name == "docProps/core.xml":
                    data = re.sub(r"<dc:title>.*?</dc:title>", f"<dc:title>{escape(man['number'])}</dc:title>", data.decode("utf8")).encode("utf8")
                elif name == "docProps/custom.xml":
                    x = data.decode("utf8")
                    x = re.sub(r'(name="Document number"><vt:lpwstr>).*?(</vt:lpwstr>)', lambda m: m.group(1) + escape(man["number"]) + m.group(2), x)
                    x = re.sub(r'(name="TITLE"><vt:lpwstr>).*?(</vt:lpwstr>)', lambda m: m.group(1) + escape(man["title"]) + m.group(2), x)
                    data = x.encode("utf8")
                zout.writestr(item.filename, data)

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
