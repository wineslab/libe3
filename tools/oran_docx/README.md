# oran_docx

Builds the O-RAN Word edition of a specification from its Markdown folder. The
specifications live under `docs/` (`e2sm-dapp/`, `e3ap/`, `e3sm/spectrum/`,
`e3sm/l1-kpm/`). Each folder carries the Markdown chapters, a manifest
`docx.json` and the generated `O-RAN.WG3.TS.*.docx`.

```bash
tools/oran_docx/build_docx.py docs/e3ap
```

The script needs only Python 3.9 or later. For the page numbers of the table of
contents it also uses LibreOffice (`soffice`) and `pdftotext`. Without them the
table has no page numbers and Word fills them when you press F9 on it. Pass
`--no-pages` to skip the LibreOffice pass.

| File | What it is |
|---|---|
| `build_docx.py` | the generator |
| `template.docx` | the O-RAN template, cut down to the cover, the copyright page, the Foreword, the Modal verbs clause, the styles, the headers and the footers |
| `make_template.py` | how `template.docx` was cut from an O-RAN specification; run it again only when the O-RAN template changes |

## The manifest

```json
{
  "number": "O-RAN.WG3.TS.E3AP-R000-v00.01",
  "title":  "E3 Application Protocol (E3AP)",
  "cover":  ["E3 Application Protocol (E3AP)", ""],
  "parts": [
    {"clause": "1", "title": "Scope", "file": "README.md", "section": "Scope"},
    {"clause": "5", "title": "Identity and registration", "file": "01-identity-and-registration.md"},
    {"clause": "A", "title": "Discrepancies", "file": "06-discrepancies.md", "informative": true}
  ],
  "history": [["2026-11", "R000 v00.01", "First version", "Northeastern University"]]
}
```

* `number` is the document number. It goes on the cover, in the headers and in the
  document properties.
* `cover` holds the two title lines under "Near-Real-time RAN Intelligent Controller".
* A part is a whole Markdown file, or with `section` the text under one level 2
  heading of the file. Its `clause` gives its place and heading level: `5` is a
  level 1 heading, `8.2` a level 2 heading, `A` an annex.
* A part without `file` is a heading with no text.
* Headings inside a part are numbered under the part. A number at the start of a
  Markdown heading is dropped.

## What the Markdown can use

Headings, paragraphs, bullet and numbered lists (two levels), pipe tables, fenced
code blocks, block quotes (shown as notes), and the inline marks for code, bold,
italic and links. A link shows its text; an `http` link also shows its address.

A line that starts with `Table:` and stands directly above a table is the caption of
that table. In Markdown it reads as a plain line. In Word it becomes "Table 8-1:
..." above the table.

## Check the result

```bash
soffice --headless --convert-to pdf --outdir /tmp docs/e3ap/O-RAN.WG3.TS.E3AP-R000-v00.01.docx
```

Open the PDF and look at the cover, the contents, one table and one ASN.1 page.
