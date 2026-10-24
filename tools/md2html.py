# -*- coding: utf-8 -*-
"""Minimal Markdown -> standalone HTML for printing the lab report to PDF.

Supports exactly the subset used by 实验报告.md / 博客.md:
headings, paragraphs, fenced code, tables, ordered/unordered lists,
blockquotes, horizontal rules, images, **bold**, *italic*, `inline code`.
"""
import re
import sys
import html as html_mod

CSS = """
@page { size: A4; margin: 16mm 15mm 16mm 15mm; }
* { box-sizing: border-box; }
body { font-family: "Microsoft YaHei", "PingFang SC", "Segoe UI", sans-serif;
       font-size: 10.4pt; line-height: 1.62; color: #111; margin: 0; }
h1 { font-size: 19pt; text-align: center; margin: 0 0 14px; letter-spacing: 1px; }
h2 { font-size: 13.5pt; margin: 20px 0 8px; padding-bottom: 3px;
     border-bottom: 2px solid #333; page-break-after: avoid; }
h3 { font-size: 11.4pt; margin: 14px 0 6px; color: #1a3d6d; page-break-after: avoid; }
p { margin: 6px 0; text-align: justify; }
table { border-collapse: collapse; width: 100%; margin: 8px 0; font-size: 9.2pt;
        page-break-inside: avoid; }
th, td { border: 1px solid #888; padding: 3px 6px; vertical-align: top; }
th { background: #eceff3; font-weight: 600; }
code { font-family: Consolas, "Courier New", monospace; font-size: 9.2pt;
       background: #f2f2f2; padding: 0 3px; border-radius: 2px; }
pre { background: #f7f7f8; border: 1px solid #dcdcdc; border-left: 3px solid #8aa;
      padding: 7px 9px; margin: 8px 0; overflow-wrap: anywhere;
      page-break-inside: avoid; }
pre code { background: none; padding: 0; font-size: 8.8pt; line-height: 1.5; }
img { max-width: 100%; display: block; margin: 10px auto 4px; }
p > img { page-break-inside: avoid; }
.fig { break-inside: avoid; page-break-inside: avoid; margin: 10px 0 2px; }
.cap { text-align: center; margin: 2px 0 10px; }
em { color: #444; font-style: normal; font-size: 9.3pt; }
blockquote { border-left: 4px solid #c0c8d0; margin: 8px 0; padding: 4px 10px;
             background: #fafbfc; color: #333; }
ul, ol { margin: 6px 0 6px 22px; padding: 0; }
li { margin: 2px 0; }
hr { border: none; border-top: 1px solid #ccc; margin: 14px 0; }
"""

INLINE_CODE = re.compile(r"`([^`]+)`")
IMG = re.compile(r"!\[([^\]]*)\]\(([^)]+)\)")


def inline(text):
    """Escape HTML then apply inline markdown (code spans protected first)."""
    stash = []

    def keep_code(m):
        stash.append(f"<code>{html_mod.escape(m.group(1))}</code>")
        return f"\x00{len(stash) - 1}\x00"

    text = INLINE_CODE.sub(keep_code, text)
    text = html_mod.escape(text, quote=False)
    text = IMG.sub(lambda m: f'<img src="{m.group(2)}" alt="{m.group(1)}">', text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<!\*)\*([^*\n]+)\*(?!\*)", r"<em>\1</em>", text)
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', text)

    def restore(m):
        return stash[int(m.group(1))]

    return re.sub(r"\x00(\d+)\x00", restore, text)


def convert(md_text: str) -> str:
    lines = md_text.splitlines()
    out = []
    i = 0
    in_list = None

    def close_list():
        nonlocal in_list
        if in_list:
            out.append(f"</{in_list}>")
            in_list = None

    while i < len(lines):
        line = lines[i]

        # fenced code
        if line.startswith("```"):
            close_list()
            i += 1
            buf = []
            while i < len(lines) and not lines[i].startswith("```"):
                buf.append(lines[i])
                i += 1
            i += 1
            code = html_mod.escape("\n".join(buf))
            out.append(f"<pre><code>{code}</code></pre>")
            continue

        # table
        if line.startswith("|") and i + 1 < len(lines) and re.match(r"^\|[\s:|-]+\|$", lines[i + 1]):
            close_list()
            header = [c.strip() for c in line.strip().strip("|").split("|")]
            i += 2
            rows = []
            while i < len(lines) and lines[i].startswith("|"):
                rows.append([c.strip() for c in lines[i].strip().strip("|").split("|")])
                i += 1
            out.append("<table><thead><tr>")
            out.extend(f"<th>{inline(c)}</th>" for c in header)
            out.append("</tr></thead><tbody>")
            for r in rows:
                out.append("<tr>")
                out.extend(f"<td>{inline(c)}</td>" for c in r)
                out.append("</tr>")
            out.append("</tbody></table>")
            continue

        if not line.strip():
            close_list()
            i += 1
            continue

        if line.startswith("---") and line.strip() == "---":
            close_list()
            out.append("<hr>")
            i += 1
            continue

        m = re.match(r"^(#{1,4})\s+(.*)$", line)
        if m:
            close_list()
            lvl = len(m.group(1))
            out.append(f"<h{lvl}>{inline(m.group(2))}</h{lvl}>")
            i += 1
            continue

        if line.startswith(">"):
            close_list()
            buf = []
            while i < len(lines) and lines[i].startswith(">"):
                buf.append(lines[i].lstrip("> ").rstrip())
                i += 1
            out.append(f"<blockquote>{inline(' '.join(buf))}</blockquote>")
            continue

        m = re.match(r"^(\s*)([-*])\s+(.*)$", line)
        if m:
            if in_list != "ul":
                close_list()
                out.append("<ul>")
                in_list = "ul"
            out.append(f"<li>{inline(m.group(3))}</li>")
            i += 1
            continue

        m = re.match(r"^(\s*)\d+\.\s+(.*)$", line)
        if m:
            if in_list != "ol":
                close_list()
                out.append("<ol>")
                in_list = "ol"
            out.append(f"<li>{inline(m.group(2))}</li>")
            i += 1
            continue

        # paragraph (may span several lines)
        close_list()
        buf = [line.strip()]
        i += 1
        while i < len(lines) and lines[i].strip() and not re.match(
                r"^(#{1,4}\s|\||```|>|\s*[-*]\s|\s*\d+\.\s|---$)", lines[i]):
            buf.append(lines[i].strip())
            i += 1
        text = " ".join(buf)
        # a paragraph that is only an image: keep it on one page together with its caption
        if text.startswith("![") and text.rstrip().endswith(")"):
            out.append(f'<p class="fig">{inline(text)}</p>')
        # a paragraph that is only an italic caption, e.g. "*图 3-1 ...*"
        elif re.match(r"^\*[^*].*\*$", text):
            out.append(f'<p class="cap">{inline(text)}</p>')
        else:
            out.append(f"<p>{inline(text)}</p>")

    close_list()
    return "\n".join(out)


def build(md_path: str, html_path: str, title: str):
    md = open(md_path, encoding="utf-8").read()
    body = convert(md)
    html = f"""<!DOCTYPE html>
<html lang="zh-CN"><head><meta charset="utf-8">
<title>{html_mod.escape(title)}</title>
<style>{CSS}</style></head>
<body>
{body}
</body></html>"""
    with open(html_path, "w", encoding="utf-8") as f:
        f.write(html)
    print("html ->", html_path, len(html), "bytes")


if __name__ == "__main__":
    build(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else "实验报告")
