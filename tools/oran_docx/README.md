# oran_docx

Builds an O-RAN-like Word edition of a specification from its Markdown folder.
The document follows the layout of an O-RAN Technical Specification: the cover,
the copyright page, the contents, the Foreword, the Modal verbs clause, the
clause numbering, the styles, the headers and the footers. It is not an O-RAN
ALLIANCE document. It carries no O-RAN logo and no working group, and its
copyright belongs to Northeastern University under the Apache License 2.0.

The specifications live under `docs/` (`e2sm-dapp/`, `e3ap/`, `e3sm/spectrum/`,
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
| `template.docx` | the O-RAN template, cut down to the cover, the copyright page, the Foreword, the Modal verbs clause, the styles, the headers and the footers. It has no O-RAN logo, no working group and no rule across the cover; the copyright page and the Foreword name Northeastern University and the Apache License 2.0, and the footers carry only the page number |
| `make_template.py` | how `template.docx` was made from an O-RAN specification: it keeps the layout and writes the front matter of the libe3 specifications. Run it again only when the O-RAN template or the front matter changes |

## The manifest

```json
{
  "number": "O-RAN.XXX.TS.E3AP-R000-v00.01",
  "title":  "E3 Application Protocol (E3AP)",
  "cover":  ["E3 Application Protocol (E3AP)", ""],
  "markdown_only": ["06-discrepancies.md"],
  "parts": [
    {"clause": "1", "title": "Scope", "file": "README.md", "section": "Scope"},
    {"clause": "5", "title": "Identity and registration", "file": "01-identity-and-registration.md"},
    {"clause": "A", "title": "Example message exchange", "file": "07-examples.md", "informative": true}
  ],
  "history": [["2026-11", "R000 v00.01", "First version", "Northeastern University"]]
}
```

* `number` is the document number. It goes on the cover, in the headers and in the
  document properties.
* `cover` holds the two title lines of the cover.
* A part is a whole Markdown file, or with `section` the text under one level 2
  heading of the file. Its `clause` gives its place and heading level: `5` is a
  level 1 heading, `8.2` a level 2 heading, `A` an annex.
* A part without `file` is a heading with no text.
* Headings inside a part are numbered under the part. A number at the start of a
  Markdown heading is dropped.

## What the Markdown can use

Headings, paragraphs, bullet and numbered lists (two levels), pipe tables, fenced
code blocks, block quotes (shown as notes), and the inline marks for code, bold,
italic and links. A line that holds only HTML anchors, such as
`<a id="formats"></a>`, is left out, and its anchors name the heading below it.
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

## Links

The Word edition has no files to link to, so each link becomes a reference that a
reader of the document can follow:

| Link in the Markdown | In Word |
|---|---|
| `[04-encoding.md](04-encoding.md)`, to a file of a part | `clause 8` |
| `[encoding](04-encoding.md#json)`, with an anchor of a heading | `encoding (clause 8.3)` |
| `[06](06-discrepancies.md)`, `[section 2](#formats)` | `Annex A`, `clause 6.2` |
| `[E2SM-DAPP](../e2sm-dapp/README.md)`, to any other file | `E2SM-DAPP (https://github.com/wineslab/libe3/tree/main/docs/e2sm-dapp)` |
| `[../interop.md](../interop.md)` | `https://github.com/wineslab/libe3/blob/main/docs/interop.md` |
| `[RFC 9293](https://...)` | `RFC 9293 (https://...)` |

A label that only names the target (its path, its file name, its number such as
`06`, or a clause number of the Markdown edition) is replaced; any other label is
kept and the reference follows it in parentheses. An anchor is the one GitHub gives
the heading, or an `<a id>` above it. An anchor that matches no heading falls back
to the clause of the whole file and prints a note. The sentence "The index is
README.md." is left out, since the Word edition has its own contents.

## Check the result

```bash
soffice --headless --convert-to pdf --outdir /tmp docs/e3ap/O-RAN.XXX.TS.E3AP-R000-v00.01.docx
```

Open the PDF and look at the cover, the contents, one table and one ASN.1 page.
