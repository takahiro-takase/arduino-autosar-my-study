"""MISRA C:2012 静的解析（Cppcheck + misra アドオン）の実行スクリプト。

src/ 配下の製品コード（C のみ）を解析し、指摘一覧を report ファイルへ、
ルール別件数を標準出力へ出す。指摘があっても終了コードは 0（現状は
ベースライン把握用。Cppcheck 自体の実行失敗時のみ非 0）。

対象外:
  - test/ と stub/  : GoogleTest・フェイクで製品コードではないため。
  - src/Hal/        : Renesas BSP ヘッダ（レジスタ定義等）が無いと正しく
                      解析できないため（ヘッダ参照用に -I には含める）。
  - C++ ファイル    : MISRA C の対象外（main.cpp、src/Hal/*.cpp）。
  - MISRA 練習用のブロック（src/Asw/App_Test.c 内の `#ifdef MISRA_PRACTICE_ENABLE` ... `#endif`）:
                      src/Asw/App_Test.h の `#define MISRA_PRACTICE_ENABLE` が有効（コメント
                      アウトされていない）ときだけ解析する。MISRA アドオンの一部の規則
                      （15.6 / 16.3 / 2.5 など）は #if で除かれたコードも見るため、無効のときは、
                      そのブロックを空行に置き換えた一時コピー（行番号は変えない）を解析し、
                      レポート上のファイル名は元のパスへ戻す（App_Test.c の本物の部分は
                      通常どおり解析される）。

逸脱（deviation）とするルールは misra_suppressions.txt に理由付きで記載する。

通常は CMake のカスタムターゲット経由で実行する:
    cmake --build --preset native-chain --target misra_check
"""

import argparse
import collections
import os
import pathlib
import re
import subprocess
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_DIR = REPO_ROOT / "src"
EXCLUDED_DIR = SRC_DIR / "Hal"
PRACTICE_CFG = SRC_DIR / "Asw" / "App_Test.h"
PRACTICE_MARK = "MISRA_PRACTICE_ENABLE"
PRACTICE_ON_RE = re.compile(r"^\s*#\s*define\s+MISRA_PRACTICE_ENABLE(?!\w)", re.MULTILINE)
SUPPRESSIONS = pathlib.Path(__file__).resolve().parent / "misra_suppressions.txt"

FINDING_RE = re.compile(r"^(?P<file>.+?):(?P<line>\d+):(?P<severity>\w+):(?P<id>[\w.\-]+):")


def relative(path):
    return path.relative_to(REPO_ROOT).as_posix()


def practice_enabled():
    """練習用ダミーアプリを解析対象にするか（Cfg ヘッダの define がコメントアウトされていない）。"""
    if not PRACTICE_CFG.exists():
        return False
    return PRACTICE_ON_RE.search(PRACTICE_CFG.read_text(encoding="utf-8", errors="replace")) is not None


PRACTICE_BLOCK_START_RE = re.compile(
    r"^\s*#\s*(?:ifdef\s+MISRA_PRACTICE_ENABLE|if\s+defined\s*\(?\s*MISRA_PRACTICE_ENABLE\s*\)?)\s*(?:/\*.*\*/)?\s*$")


def strip_practice_blocks(text):
    """練習用ブロックを空行に置き換える（行番号は変えない）。

    対応する書き方:
      `#ifdef MISRA_PRACTICE_ENABLE` / `#if defined(MISRA_PRACTICE_ENABLE)` ... `#endif`
      途中に `#else` があれば、`#else` 以降（スイッチが無効のときに有効になる側）は残す。
    対応しない書き方: `#elif`（見つけたらエラーで止める）、`#ifndef`（無効のときに有効になる側なので、除かない）。
    """
    out = []
    depth = 0
    in_else = False
    for line in text.split("\n"):
        if depth == 0:
            if PRACTICE_BLOCK_START_RE.match(line):
                depth = 1
                in_else = False
                out.append("")
            else:
                out.append(line)
            continue
        is_if = re.match(r"^\s*#\s*(if|ifdef|ifndef)\b", line)
        is_endif = re.match(r"^\s*#\s*endif\b", line)
        is_else = re.match(r"^\s*#\s*else\b", line)
        is_elif = re.match(r"^\s*#\s*elif\b", line)
        if is_if:
            depth += 1
            out.append(line if in_else else "")
        elif is_endif:
            depth -= 1
            if depth == 0:
                in_else = False
                out.append("")
            else:
                out.append(line if in_else else "")
        elif depth == 1 and is_elif:
            sys.exit("練習用ブロックの中の #elif には対応していません。#else か、別のブロックに分けてください。")
        elif depth == 1 and is_else:
            in_else = True
            out.append("")
        else:
            out.append(line if in_else else "")
    return "\n".join(out)


def collect_sources(stripped_dir):
    """解析する C ファイルを返す。練習用ブロックを含むファイルは、無効のとき一時コピーに置き換える。

    戻り値: (cppcheck へ渡すパスの一覧, {一時コピーのパス: 元の相対パス})
    """
    enabled = practice_enabled()
    sources = []
    remap = {}
    for p in sorted(SRC_DIR.rglob("*.c")):
        if EXCLUDED_DIR in p.parents:
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        if enabled or PRACTICE_MARK not in text:
            sources.append(relative(p))
            continue
        copy = stripped_dir / relative(p)
        copy.parent.mkdir(parents=True, exist_ok=True)
        copy.write_text(strip_practice_blocks(text.replace("\r\n", "\n")), encoding="utf-8", newline="\n")
        sources.append(str(copy))
        remap[str(copy)] = relative(p)
    return sources, remap


def collect_include_dirs():
    return sorted({relative(p.parent) for p in SRC_DIR.rglob("*.h")})


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--cppcheck", required=True, help="cppcheck 実行ファイルのパス")
    parser.add_argument("--output-dir", required=True, help="レポートの出力先ディレクトリ")
    args = parser.parse_args()

    output_dir = pathlib.Path(args.output_dir)
    build_dir = output_dir / "cppcheck-build"
    build_dir.mkdir(parents=True, exist_ok=True)
    report = output_dir / "misra_report.txt"

    cmd = [
        args.cppcheck,
        "--addon=misra",
        f"--addon-python={sys.executable}",
        "--std=c99",
        "--platform=unix32",       # Renesas RA4M1（Cortex-M4、int/long/ポインタ 32bit）
        "--language=c",
        "--max-configs=1",         # 本番ビルド同様 -D 無しの1構成のみ（*_UNIT_TEST 等を除外）
        "--enable=warning,style,portability",  # MISRA アドオンの指摘は severity=style
        "--suppress=missingIncludeSystem",
        f"--suppressions-list={SUPPRESSIONS}",
        "--inline-suppr",
        # 未指定だとアドオン用の中間ファイル（*.dump）が src/ 内に残ることがある
        f"--cppcheck-build-dir={build_dir}",
        f"-j{os.cpu_count() or 1}",
        "--quiet",
        "--template={file}:{line}:{severity}:{id}:{message}",
        f"--output-file={report}",
    ]
    cmd += [f"-I{d}" for d in collect_include_dirs()]
    sources, remap = collect_sources((output_dir / "stripped").resolve())
    cmd += sources
    if remap:
        # ファイルパス付きの抑制エントリ（例: misra-c2012-11.8:src/Asw/App_Test.c:NN）は、解析するのが
        # 一時コピーのため、そのままでは一致しない。パスを一時コピーのものへ書き換えた抑制リストを使う
        adjusted = SUPPRESSIONS.read_text(encoding="utf-8")
        for copy, original in remap.items():
            adjusted = adjusted.replace(original, copy.replace("\\", "/"))
        adjusted_path = output_dir / "misra_suppressions_adjusted.txt"
        adjusted_path.write_text(adjusted, encoding="utf-8", newline="\n")
        cmd = [f"--suppressions-list={adjusted_path}" if x == f"--suppressions-list={SUPPRESSIONS}" else x for x in cmd]

    subprocess.run(cmd, cwd=REPO_ROOT, check=True)

    if remap:
        text = report.read_text(encoding="utf-8", errors="replace")
        for copy, original in remap.items():
            text = text.replace(copy, original).replace(copy.replace("\\", "/"), original)
        report.write_text(text, encoding="utf-8", newline="\n")

    counts = collections.Counter()
    for line in report.read_text(encoding="utf-8", errors="replace").splitlines():
        m = FINDING_RE.match(line)
        if m:
            counts[m.group("id")] += 1

    print(f"MISRA/Cppcheck findings: {sum(counts.values())} (report: {report})")
    for rule_id, count in sorted(counts.items(), key=lambda kv: (-kv[1], kv[0])):
        print(f"  {count:5d}  {rule_id}")


if __name__ == "__main__":
    main()
