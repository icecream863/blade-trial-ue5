"""从公共头文件的中文职责注释更新 HTML 类速查表，避免文档和代码分别维护。"""
from pathlib import Path
import html
import re

ROOT = Path(__file__).resolve().parents[1]
COURSE = ROOT / "Docs/从0理解SoulCombatLab_完整课程.html"
PUBLIC = ROOT / "Source/SoulCombatLab/Public"
START = "<!-- CLASS_CATALOG_START -->"
END = "<!-- CLASS_CATALOG_END -->"

GROUPS = {
    "Characters": "角色与玩家输入",
    "Combat": "攻击执行与武器命中",
    "AbilitySystem": "GAS 技能、状态和属性",
    "Data": "招式与敌人配置",
    "Animation": "动画通知与编辑工具",
    "Demo": "游戏流程、菜单和区域",
    "AI": "敌人感知、行为树和 Boss 策略",
    "Targeting": "目标锁定与镜头",
    "UI": "玩家界面与血条",
    "Core": "生成与游戏规则入口",
    "Debug": "调试与性能采样",
    "Interfaces": "系统之间的接口",
}

# 只收录有定义的类型，排除前置声明；职责直接取声明前的连续 // 注释。
TYPE = re.compile(
    r"(?m)^(?P<comment>(?://[^\n]*\n)+)"
    r"(?:U(?:CLASS|STRUCT|INTERFACE)\([^\n]*\)\n)?"
    r"(?:class|struct) (?:SOULCOMBATLAB_API )?(?P<name>\w+)"
    r"(?=\s*(?::|final|\{))"
)


def build_catalog():
    groups = {key: [] for key in GROUPS}
    for header in sorted(PUBLIC.rglob("*.h")):
        source = header.read_text(encoding="utf-8-sig")
        group = header.relative_to(PUBLIC).parts[0]
        for match in TYPE.finditer(source):
            description = " ".join(line[2:].strip() for line in match["comment"].splitlines())
            link = "../" + header.relative_to(ROOT).as_posix()
            groups[group].append((match["name"], description, link))

    count = sum(len(rows) for rows in groups.values())
    lines = [f'<p id="class-count" class="subtle" aria-live="polite">共 {count} 个类型；点击模块展开，或输入类名和中文关键词。</p>']
    for group, rows in groups.items():
        if not rows:
            continue
        lines.extend([
            '<details class="catalog-group">',
            f'<summary>{GROUPS[group]} <span class="subtle">（{len(rows)}）</span></summary>',
            '<div class="table-wrap"><table><thead><tr><th>源码类型 / 头文件</th><th>中文职责</th></tr></thead><tbody>',
        ])
        for name, description, link in rows:
            lines.append(f'<tr class="class-row"><td><a href="{html.escape(link, quote=True)}"><code>{html.escape(name)}</code></a></td><td>{html.escape(description)}</td></tr>')
        lines.extend(['</tbody></table></div>', '</details>'])
    return "\n".join(lines), count


def main():
    source = COURSE.read_text(encoding="utf-8")
    if source.count(START) != 1 or source.count(END) != 1:
        raise RuntimeError("HTML 中必须保留唯一的类目录起止标记。")
    before, rest = source.split(START)
    _, after = rest.split(END)
    catalog, count = build_catalog()
    COURSE.write_bytes((before + START + "\n" + catalog + "\n" + END + after).replace("\r\n", "\n").encode("utf-8"))
    print(f"Updated class catalog: {count} types.")


if __name__ == "__main__":
    main()
