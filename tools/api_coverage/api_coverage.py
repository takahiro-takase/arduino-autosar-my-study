"""AUTOSAR API の実装数を数える。

autosar_api_list.json（SWS から抽出した API 名の一覧）と、各モジュールのソース
（src/Bsw/<Module>/）を突き合わせて、次の 3 つに分類する。

  実装      : 関数の定義（本体）がソースにある
  未実装    : 小見出しの下に `/* 未実装 */` がある（定義が無い）
  目印なし  : 上記のどちらでもない（ソースに載っていない。= 追加が必要）

使い方:
    python tools/api_coverage/api_coverage.py                      # 一覧を表示
    python tools/api_coverage/api_coverage.py --check-readme       # README の表と一致するか検証
    python tools/api_coverage/api_coverage.py --update-readme      # README の表の数字を更新
    python tools/api_coverage/api_coverage.py --insert-stubs       # 目印なし API へ `/* 未実装 */` を追加

「目印なし」が 1 つでもあると終了コード 1 を返す。
"""

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LIST_PATH = Path(__file__).resolve().parent / "autosar_api_list.json"
README = ROOT / "README.md"
MARK = "/* 未実装 */"

BANNER = re.compile(r"^/\* -{20,}\n \* (\w+)\n \* -{20,} \*/\n(.*?)(?=\n/\* -{20,}\n|\n/\* ={20,}|\Z)", re.S | re.M)


def read_source(rel):
    return (ROOT / rel).read_bytes().decode("utf-8").replace("\r\n", "\n")


def has_definition(text, name):
    """列 0 から始まる `... name(...)` の直後に `{` が続く（= 定義であり、宣言ではない）か。"""
    pat = re.compile(r"^[A-Za-z_][\w \t\*]*?\b" + re.escape(name) + r"\s*\(", re.M)
    for m in pat.finditer(text):
        i = m.end()
        depth = 1
        while i < len(text) and depth:
            c = text[i]
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            i += 1
        j = i
        while j < len(text) and text[j] in " \t\n":
            j += 1
        if j < len(text) and text[j] == "{":
            return True
    return False


def classify(module, info):
    texts = [read_source(s) for s in info["sources"]]
    marked = set()
    for t in texts:
        for m in BANNER.finditer(t):
            if m.group(2).strip().startswith(MARK):
                marked.add(m.group(1))
    impl, unimpl, missing = [], [], []
    for api in info["apis"]:
        name = api["name"]
        if any(has_definition(t, name) for t in texts):
            impl.append(name)
        elif name in marked:
            unimpl.append(name)
        else:
            missing.append(name)
    return impl, unimpl, missing


def load():
    return json.loads(LIST_PATH.read_text(encoding="utf-8"))


def stub_block(names):
    parts = []
    for n in names:
        parts.append(
            "/* ----------------------------------------------------------------------\n"
            f" * {n}\n"
            " * ---------------------------------------------------------------------- */\n\n"
            f"{MARK}\n"
        )
    return "\n".join(parts) + "\n"


def insert_stubs(rel, names):
    p = ROOT / rel
    raw = p.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in raw else "\n"
    t = raw.replace("\r\n", "\n")
    m = re.search(r"\n(/\* ={20,}\n \* (?:Internal [Ff]unctions|Test Functions)\n)", t)
    block = stub_block(names)
    if m:
        pos = m.start() + 1
        t = t[:pos] + block + t[pos:]
    else:
        t = t.rstrip("\n") + "\n\n" + block.rstrip("\n") + "\n"
    p.write_bytes(t.replace("\n", nl).encode("utf-8"))


def table_row_pattern(module):
    return re.compile(r"^(\|[^|]*\|\s*" + re.escape(module) + r"\s*\|[^|]*\|)([^|]*)(\|.*)$")


def readme_cell(cell, impl_n, total):
    label = f"API実装: {impl_n} / {total}"
    if re.search(r"(?:API)?実装:? \d+ / \d+", cell):
        return re.sub(r"(?:API)?実装:? \d+ / \d+", label, cell)
    if "主要機能実装" in cell:
        return cell.replace("主要機能実装", label, 1)
    if "パススルー" in cell:
        return cell.replace("パススルー", label + "<br>パススルー", 1)
    return cell


def process_readme(results, update):
    text = README.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in text else "\n"
    lines = text.replace("\r\n", "\n").split("\n")
    bad = []
    for module, (impl, unimpl, missing) in results.items():
        total = len(impl) + len(unimpl) + len(missing)
        pat = table_row_pattern(module)
        for i, ln in enumerate(lines):
            m = pat.match(ln)
            if not m:
                continue
            new_cell = readme_cell(m.group(2), len(impl), total)
            if new_cell != m.group(2):
                bad.append(module)
                if update:
                    lines[i] = m.group(1) + new_cell + m.group(3)
            break
        else:
            bad.append(module + "(README に行が無い)")
    if update:
        README.write_bytes("\n".join(lines).replace("\n", nl).encode("utf-8"))
    return bad


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check-readme", action="store_true")
    ap.add_argument("--update-readme", action="store_true")
    ap.add_argument("--insert-stubs", action="store_true")
    args = ap.parse_args()

    data = load()
    results = {m: classify(m, info) for m, info in data["modules"].items()}

    if args.insert_stubs:
        for m, (impl, unimpl, missing) in results.items():
            if missing:
                insert_stubs(data["modules"][m]["sources"][0], missing)
                print(f"{m}: {len(missing)} 件の未実装目印を追加")
        results = {m: classify(m, info) for m, info in data["modules"].items()}

    total_missing = 0
    print(f"{'Module':8s} {'全体':>4s} {'実装':>4s} {'未実装':>5s} {'目印なし':>6s}")
    for m, (impl, unimpl, missing) in results.items():
        t = len(impl) + len(unimpl) + len(missing)
        total_missing += len(missing)
        print(f"{m:8s} {t:4d} {len(impl):4d} {len(unimpl):5d} {len(missing):6d}")
        if missing:
            print("   目印なし:", ", ".join(missing))
    for m, why in data["excluded"].items():
        print(f"{m:8s} 対象外: {why}")

    rc = 1 if total_missing else 0
    if args.check_readme or args.update_readme:
        bad = process_readme(results, update=args.update_readme)
        if bad:
            print(("README を更新: " if args.update_readme else "README の数字が一致しない: ") + ", ".join(bad))
            if args.check_readme:
                rc = 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
