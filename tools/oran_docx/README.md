# oran_docx

Builds the O-RAN Word edition of a specification from its Markdown folder. The
specifications live under `docs/` (`e2sm-dapp/`, `e3ap/`, `e3sm/spectrum/`,
`e3sm/l1-kpm/`). Each folder carries the Markdown chapters, a manifest
`docx.json` and the generated `O-RAN.XXX.TS.*.docx`.

```bash
tools/oran_docx/build_docx.py docs/e3ap
```

The script needs only Python 3.9 or later. For the page numbers of the table of
contents it also uses LibreOffice (`soffice`) and `pdftotext`. Without them the
table has no page numbers and Word fills them when you press F9 on it. Pass
`--no-pages` to skip the LibreOffice pass.

To commit a Word edition, build it in the container, so that the page numbers are
always there and the same on every host:

```bash
docker build -t libe3-oran-docx tools/oran_docx
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD":/src libe3-oran-docx docs/e3ap
```

| File | What it is |
|---|---|
| `build_docx.py` | the generator |
| `Dockerfile` | Ubuntu 24.04 with Python 3, LibreOffice, `pdftotext` and the Liberation fonts |
| `template.docx` | the O-RAN template, cut down to the cover, the copyright page, the Foreword, the Modal verbs clause, the styles, the headers and the footers. The cover and the Foreword name O-RAN Work Group X, Real-Time Distributed Applications (dApps) |
| `make_template.py` | how `template.docx` was cut from an O-RAN specification and given the working group of the E3 specifications; run it again only when the O-RAN template changes |

## The manifest

```json
{
  "number": "O-RAN.XXX.TS.E3AP-R000-v00.01",
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
* `cover` holds the two title lines under the working group.
* `group` is optional. The template names O-RAN Work Group X, Real-Time Distributed
  Applications (dApps), where `X` holds the place of the number until the naming is
  agreed. `group` names another working group: `cover` gives its two lines on the
  cover and `foreword` its short name in the Foreword. E2SM-DAPP is an E2 service
  model and sets `{"cover": ["O-RAN Work Group 3", "Near-Real-time RAN Intelligent
  Controller"], "foreword": "WG3"}`.
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
A line that holds only HTML anchors, such as `<a id="formats"></a>`, is left out.
A fenced code block is set in the PL style of the template, in one box with a
light gray background and a border.

A line that starts with `Table:` and stands directly above a table is the caption of
that table. In Markdown it reads as a plain line. In Word it becomes "Table 8-1:
..." above the table.

An image on a line of its own is a figure:

```markdown
![Message flow of E3SM-L1-KPM over E3AP](figures/message-flow.png)
```

The path is relative to the Markdown file. In Word the image is centered, at the
size that its resolution gives and at most the width of the text, with the caption
"Figure 5-1: Message flow of E3SM-L1-KPM over E3AP" below it. Figures are numbered in
each clause, as tables are, so the text can refer to "Figure 5-1". PNG and JPEG
files are accepted. A missing file stops the build.

## Check the result

```bash
soffice --headless --convert-to pdf --outdir /tmp docs/e3ap/O-RAN.XXX.TS.E3AP-R000-v00.01.docx
```

Open the PDF and look at the cover, the contents, one table and one ASN.1 page.
