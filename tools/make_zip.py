# -*- coding: utf-8 -*-
"""Zip the project for submission (forward slashes, UTF-8 names, no build outputs).

usage: python tools/make_zip.py <project_dir> <out.zip>
"""
import os
import sys
import zipfile

EXCLUDE_DIRS = {"_check", "_stage", "Objects", "Listings", ".git", "__pycache__", ".vs", ".vscode"}
EXCLUDE_EXT = {".log", ".dep", ".map", ".htm", ".uvguix"}
EXCLUDE_FILES = {"_report.html"}


def main(src: str, out: str):
    src = os.path.abspath(src)
    out = os.path.abspath(out)
    n = 0
    total = 0
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for dirpath, dirnames, filenames in os.walk(src):
            dirnames[:] = sorted(d for d in dirnames if d not in EXCLUDE_DIRS)
            for fn in sorted(filenames):
                if fn in EXCLUDE_FILES or os.path.splitext(fn)[1].lower() in EXCLUDE_EXT:
                    continue
                full = os.path.join(dirpath, fn)
                if os.path.abspath(full) == out:
                    continue
                if fn.startswith(".uvguix"):
                    continue
                arc = os.path.relpath(full, src).replace("\\", "/")
                zf.write(full, arc)
                n += 1
                total += os.path.getsize(full)
    print(f"{n} files, {total / 1024:.0f} KB -> {out} ({os.path.getsize(out) / 1024:.0f} KB)")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
