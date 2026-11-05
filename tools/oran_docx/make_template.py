#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Make the template that build_docx.py fills from an O-RAN specification.

The O-RAN template ships as a full specification. This script keeps its layout:
the cover, the copyright page, the "Contents" heading, the Foreword and the Modal
verbs clause, the styles, the numbering, the headers and the footers. It replaces
the table of contents and the body with two markers, and ends the document with
one section, so a generated document needs no landscape or per-clause section
breaks.

The front matter is that of the libe3 specifications. The cover has the document
number and the title and no logo. The copyright page names Northeastern
University and the Apache License 2.0. The Foreword says who produced the
document, and the Modal verbs clause refers to the ETSI Drafting Rules. The
headers carry the document number and the footers the page number.

    make_template.py O-RAN.WG3.TS.E2SM-KPM-R005-v08.00.docx template.docx

Run it again only when the O-RAN template or the front matter changes. The
result is committed as template.docx next to this script.
"""
import re
import sys
import zipfile
from xml.sax.saxutils import escape

TOC_MARK = "@@TOC@@"
BODY_MARK = "@@BODY@@"

# One section for the whole document. The first page (the cover) has no header
# or footer, as in the O-RAN template; every other page has both.
SECT = (
    '<w:sectPr><w:headerReference w:type="default" r:id="rId10"/>'
    '<w:footerReference w:type="default" r:id="rId11"/>'
    '<w:footnotePr><w:numRestart w:val="eachSect"/></w:footnotePr>'
    '<w:pgSz w:w="11907" w:h="16840" w:code="9"/>'
    '<w:pgMar w:top="1138" w:right="850" w:bottom="403" w:left="850" '
    'w:header="680" w:footer="340" w:gutter="0"/>'
    '<w:cols w:space="720"/><w:formProt w:val="0"/><w:titlePg/>'
    '<w:docGrid w:linePitch="272"/></w:sectPr>'
)

OWNER = "Northeastern University"
# The paragraphs of the copyright page, in order.
COPYRIGHT = [
    f"Copyright © 2026 {OWNER}",
    'Licensed under the Apache License, Version 2.0 (the "License"); you may not use this '
    "document except in compliance with the License. You may obtain a copy of the License at "
    "http://www.apache.org/licenses/LICENSE-2.0. Unless required by applicable law or agreed to "
    'in writing, material distributed under the License is distributed on an "AS IS" BASIS, '
    "WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for "
    "the specific language governing permissions and limitations under the License.",
    "",
    f"{OWNER}, 360 Huntington Avenue, Boston, MA 02115, USA",
    "",
]
# The rule above the footer ends here; the page number is set flush with it.
FOOTER_TAB = 9639
# Paragraphs of the Foreword, by the text they start with. None removes the paragraph.
FRONT = {
    "This Technical Specification (TS) has been produced by ":
        f"This Technical Specification (TS) has been produced by {OWNER} in the context of a "
        "collaborative effort to create a set of specifications that promote dApps interoperability. "
        "The content of the present document is subject to continuing work and may change. "
        "It will be re-released with an identifying change of version date and an increase "
        "in version number as follows:",
    "The content of the present document is subject to continuing work within O-RAN": None,
}
# Run text of the Modal verbs clause that names O-RAN.
MODAL = {
    "O-RAN Drafting Rules": "ETSI Drafting Rules",
    "allowed in O-RAN deliverables": "allowed in the present document",
}
# The O-RAN logo (image1) and a figure of the body that the cut leaves behind (image2).
DROP_MEDIA = ("word/media/image1.jpeg", "word/media/image2.png")

PARA = r"<w:p[ >](?:(?!</w:p>).)*</w:p>"
RUN = r"<w:r[ >](?:(?!</w:r>).)*?</w:r>"
DRAWING_RUN = r"<w:r[ >](?:(?!</w:r>).)*?<w:drawing>.*?</w:drawing></w:r>"


def cut(document_xml: str) -> str:
    head_end = document_xml.index("<w:body>") + len("<w:body>")
    head, body = document_xml[:head_end], document_xml[head_end:]

    # The cover and copyright page end where the "Contents" heading starts.
    tt = body.index('w:val="TT"')
    contents_start = body.rfind("<w:p ", 0, tt)
    contents_end = body.index("</w:p>", tt) + len("</w:p>")

    # The TOC ends with a paragraph that holds the field end, then a page break.
    last_toc = body.rindex('w:val="TOC1"')
    toc_end = body.index("</w:p>", last_toc) + len("</w:p>")
    fld_end = body.index("</w:p>", toc_end) + len("</w:p>")  # field end paragraph
    brk_end = body.index("</w:p>", fld_end) + len("</w:p>")  # page break paragraph

    # Foreword and Modal verbs run up to the Scope heading.
    scope = next(
        m.start() for m in re.finditer(r"<w:p [^>]*>(?:(?!</w:p>).)*?w:val=\"Heading1\".*?</w:p>", body, re.S)
        if "Scope" in re.sub(r"<[^>]+>", "", m.group(0))
    )

    out = (
        head
        + body[:contents_end]
        + TOC_MARK
        + body[fld_end:brk_end]
        + body[brk_end:scope]
        + BODY_MARK
        + SECT
        + "</w:body></w:document>"
    )
    assert contents_start < contents_end
    return out


# --------------------------------------------------------------- front matter


def text(p):
    return "".join(re.findall(r"<w:t[^>]*>([^<]*)</w:t>", p))


def with_text(p, t):
    """The paragraph with its properties, the first run's format and the text t."""
    head = re.match(r"<w:p[^>]*>", p).group(0)
    ppr = re.search(r"<w:pPr>.*?</w:pPr>", p, re.S)
    rpr = re.search(r"<w:r[ >](?:(?!</w:r>).)*?(<w:rPr>.*?</w:rPr>)", p, re.S)
    runs = f'<w:r>{rpr.group(1) if rpr else ""}<w:t xml:space="preserve">{escape(t)}</w:t></w:r>' if t else ""
    return head + (ppr.group(0) if ppr else "") + runs + "</w:p>"


def front_matter(x):
    found = []

    # The cover logo is a drawing in the document number paragraph (ZA).
    za = re.search(r"<w:p [^>]*>(?:(?!</w:p>).)*?w:val=\"ZA\".*?</w:p>", x, re.S).group(0)
    x = x.replace(za, re.sub(DRAWING_RUN, "", za, flags=re.S), 1)

    # The rule across the middle of the cover is a border of the table that
    # holds "Technical Specification" (ZB).
    zb = x.index('w:val="ZB"')
    start, end = x.rfind("<w:tbl>", 0, zb), x.index("</w:tbl>", zb)
    x = x[:start] + re.sub(r'(<w:(?:top|bottom|insideH)) w:val="single"[^>]*/>', r'\1 w:val="nil"/>', x[start:end]) + x[end:]

    # The copyright page: one paragraph per line of COPYRIGHT, from "Copyright ©" on.
    first = x.rfind("<w:p ", 0, x.index("Copyright ©"))
    paras = list(re.finditer(PARA, x[first:], re.S))[: len(COPYRIGHT)]
    x = x[:first] + "".join(with_text(m.group(0), t) for m, t in zip(paras, COPYRIGHT)) + x[first + paras[-1].end():]

    def fix(m):
        p = m.group(0)
        t = text(p)
        # The working group, the first of the three title paragraphs (ZT) of the
        # cover. The paragraph stays, empty, so the titles keep their place.
        if 'w:val="ZT"' in p and t.startswith("O-RAN Work Group"):
            found.append("group")
            return with_text(p, "")
        for start, new in FRONT.items():
            if t.startswith(start):
                found.append(start)
                return with_text(p, new) if new is not None else ""
        for old, new in MODAL.items():
            if old in t:
                found.append(old)
                p = p.replace(old, new)
        return p

    x = re.sub(PARA, fix, x, flags=re.S)
    assert len(found) == 1 + len(FRONT) + len(MODAL), "template cover or Foreword changed"
    return x


def footer(x):
    """The page number alone, at a right tab where the rule above it ends."""
    def fix(m):
        p = m.group(0)
        if "O-RAN ALLIANCE" not in text(p):
            return p
        head = re.match(r"<w:p[^>]*>", p).group(0)
        ppr = re.search(r"<w:pPr>.*?</w:pPr>", p, re.S).group(0)
        ppr = re.sub(r"<w:tabs>.*?</w:tabs>", "", ppr)
        ppr = ppr.replace("<w:jc ", f'<w:tabs><w:tab w:val="right" w:pos="{FOOTER_TAB}"/></w:tabs><w:jc ', 1)
        runs = re.findall(RUN, p, re.S)
        field = runs[next(i for i, r in enumerate(runs) if 'w:fldCharType="begin"' in r):]
        return head + ppr + "<w:r><w:tab/></w:r>" + "".join(field) + "</w:p>"
    return re.sub(PARA, fix, x, flags=re.S)


def drop_rels(x):
    targets = "|".join(re.escape(m.split("word/", 1)[1]) for m in DROP_MEDIA)
    return re.sub(rf'<Relationship [^>]*Target="(?:{targets})"/>', "", x)


def template_part(name, data):
    """One part of the template, made from the same part of the O-RAN specification."""
    if name in DROP_MEDIA:
        return None
    if name == "word/document.xml":
        return front_matter(cut(data.decode("utf8"))).encode("utf8")
    if name in ("word/header1.xml", "word/header2.xml"):
        return re.sub(DRAWING_RUN, "", data.decode("utf8"), flags=re.S).encode("utf8")
    if name in ("word/footer1.xml", "word/footer2.xml"):
        return footer(data.decode("utf8")).encode("utf8")
    if name.startswith("word/_rels/"):
        return drop_rels(data.decode("utf8")).encode("utf8")
    return data


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    src, dst = sys.argv[1], sys.argv[2]
    with zipfile.ZipFile(src) as zin, zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as zout:
        for item in zin.infolist():
            data = template_part(item.filename, zin.read(item.filename))
            if data is not None:
                zout.writestr(item, data)
    return 0


if __name__ == "__main__":
    sys.exit(main())
