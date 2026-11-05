#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Cut an O-RAN specification down to the template that build_docx.py fills.

The O-RAN template ships as a full specification. This script keeps what every
specification has: the cover, the copyright page, the "Contents" heading, the
Foreword and the Modal verbs clause, the styles, the numbering, the headers
and the footers. It replaces the table of contents and the body with two
markers, and ends the document with one section, so a generated document needs
no landscape or per-clause section breaks. The cover and the Foreword name the
working group of the E3 specifications, DEFAULT_GROUP of build_docx.py, instead
of O-RAN Work Group 3.

    make_template.py O-RAN.WG3.TS.E2SM-KPM-R005-v08.00.docx template.docx

Run it again only when the O-RAN template changes. The result is committed as
template.docx next to this script.
"""
import re
import sys
import zipfile

from build_docx import DEFAULT_GROUP, set_group

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


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    src, dst = sys.argv[1], sys.argv[2]
    with zipfile.ZipFile(src) as zin, zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as zout:
        for item in zin.infolist():
            data = zin.read(item.filename)
            if item.filename == "word/document.xml":
                data = set_group(cut(data.decode("utf8")), DEFAULT_GROUP).encode("utf8")
            zout.writestr(item.filename, data)
    return 0


if __name__ == "__main__":
    sys.exit(main())
