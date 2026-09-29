"""MISRA C:2012 静的解析（Cppcheck + misra アドオン）の実行スクリプト。

src/ 配下の製品コード（C のみ）を解析し、指摘一覧を report ファイルへ、
ルール別件数を標準出力へ出す。指摘があっても終了コードは 0（現状は
ベースライン把握用。Cppcheck 自体の実行失敗時のみ非 0）。

対象外:
  - test/ と stub/  : GoogleTest・フェイクで製品コードではないため。
  - src/Hal/        : Renesas BSP ヘッダ（レジスタ定義等）が無いと正しく
                      解析できないため（ヘッダ参照用に -I には含める）。
  - C++ ファイル    : MISRA C の対象外（main.cpp、src/Hal/*.cpp）。

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
SUPPRESSIONS = pathlib.Path(__file__).resolve().parent / "misra_suppressions.txt"

FINDING_RE = re.compile(r"^(?P<file>.+?):(?P<line>\d+):(?P<severity>\w+):(?P<id>[\w.\-]+):")


def relative(path):
    return path.relative_to(REPO_ROOT).as_posix()


def collect_sources():
    return sorted(relative(p) for p in SRC_DIR.rglob("*.c") if EXCLUDED_DIR not in p.parents)


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
    cmd += collect_sources()

    subprocess.run(cmd, cwd=REPO_ROOT, check=True)

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
