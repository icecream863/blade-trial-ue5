"""把分章讲义和当前源码节选写入完整 HTML，保留类目录与原有证据。"""
from pathlib import Path
import html
import json
import re

ROOT = Path(__file__).resolve().parents[1]
COURSE = ROOT / "Docs/从0理解SoulCombatLab_完整课程.html"
LESSONS = ROOT / "Docs/CourseLessons.json"


def build():
    data = json.loads(LESSONS.read_text(encoding="utf-8"))
    page = COURSE.read_text(encoding="utf-8")
    for lesson in data["lessons"]:
        content = lesson["html"]
        for key, snippet in data["snippets"].items():
            marker = "{{SNIPPET:" + key + "}}"
            if marker not in content:
                continue
            path = ROOT / snippet["path"]
            source = path.read_text(encoding="utf-8-sig")
            anchor = snippet["anchor"]
            if source.count(anchor) != 1:
                raise ValueError(f"源码节选锚点必须唯一: {key}")
            start = source.index(anchor)
            code = "\n".join(source[start:].splitlines()[:snippet["lines"]])
            link = "../" + snippet["path"]
            content = content.replace(marker, '<pre><code>' + html.escape(code) + '</code></pre>'
                + '<p class="source">当前源码节选：<a href="' + link + '">' + path.name + '</a></p>')
        if "{{SNIPPET:" in content:
            raise ValueError("讲义中存在未解析的源码节选")
        section_id = lesson["id"]
        pattern = re.compile(r'(<section class="chapter" id="' + re.escape(section_id)
            + r'">)(.*?)(</section>)', re.S)
        match = pattern.search(page)
        if not match:
            raise ValueError("缺少章节: " + section_id)
        body = match[2]
        marker = '<!-- LESSON_START -->'
        replacement = marker + '\n<div class="lesson-main">' + content + '</div>\n<!-- LESSON_END -->'
        if marker in body:
            body = re.sub(r'<!-- LESSON_START -->.*?<!-- LESSON_END -->', lambda _: replacement, body, flags=re.S)
        else:
            heading_end = body.index('</h2>') + len('</h2>')
            heading, notes = body[:heading_end], body[heading_end:]
            body = heading + '\n' + replacement + '\n<details class="reference-notes"><summary>展开：源码细节与额外证据</summary>' + notes + '</details>\n'
        page = page[:match.start()] + match[1] + body + match[3] + page[match.end():]
    COURSE.write_text(page, encoding="utf-8")
    print(f"Built {len(data['lessons'])} lessons from source-backed material.")


if __name__ == "__main__":
    build()
