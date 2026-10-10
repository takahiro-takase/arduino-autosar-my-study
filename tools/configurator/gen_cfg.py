"""json の設定データから Cfg ソースの該当部分を生成する（コンフィグレータ）。

使い方:
    python tools/configurator/gen_cfg.py            # 生成して書き換える
    python tools/configurator/gen_cfg.py --check    # 書き換えず、差分があれば終了コード 1
    python tools/configurator/gen_cfg.py PduR       # モジュールを指定

入力:
    config/data/can_signals.json              ビットアサイン表（フレーム、CAN ID、DLC、信号）
    config/data/*.json                 モジュールごとの設定（信号表のフレームは名前で参照する）
    config/schema/*.schema.json        設定 json のスキーマ

仕組み:
    C ソース（テンプレート）には、生成領域の目印コメントを置く。
        /* @@GEN-BEGIN <領域ID> ... */
        /* @@GEN-END <領域ID> */
    ツールは目印の間だけを json から生成した内容で置き換え、目印の外（ファイル冒頭の説明、
    インクルード、帯見出しなど）には触れない。領域の中は手編集しない（次の生成で上書きされる）。
    生成の前に、スキーマ検証と、信号表・各モジュール設定の整合性検査を行う。
"""

import argparse
import json
import pathlib
import re
import sys

import jsonschema

import checks
from modules import bswm, canif, com, cryptostack, e2exf, pdur, secoc

TOOL_DIR = pathlib.Path(__file__).resolve().parent
REPO_ROOT = TOOL_DIR.parents[1]
CONFIG_DATA_DIR = REPO_ROOT / "config" / "data"
CONFIG_SCHEMA_DIR = REPO_ROOT / "config" / "schema"
DEFAULT_SIGNALS = REPO_ROOT / "config" / "data" / "can_signals.json"

BEGIN_RE = re.compile(r"^[ \t]*/\* @@GEN-BEGIN (?P<id>[\w\-]+)\b.*\*/[ \t]*$")
END_RE = re.compile(r"^[ \t]*/\* @@GEN-END (?P<id>[\w\-]+)[ \t]*\*/[ \t]*$")

MODULES = {
    "BswM": bswm.SPEC,
    "CanIf": canif.SPEC,
    "Com": com.SPEC,
    "CryptoStack": cryptostack.SPEC,
    "E2EXf": e2exf.SPEC,
    "PduR": pdur.SPEC,
    "SecOC": secoc.SPEC,
}


class Context:
    """生成に使う入力一式。frames は信号表のフレーム（名前で引く）、configs は各モジュールの設定。"""

    def __init__(self, signals_path, config_dir):
        self.signals_path = pathlib.Path(signals_path)
        self.config_dir = pathlib.Path(config_dir)
        data = json.loads(self.signals_path.read_text(encoding="utf-8"))
        self.frames = {f["name"]: f for f in data["frames"]}
        self.byte_order = data["byteOrder"]
        self.configs = {}
        self.config_paths = {}


# ----------------------------------------------------------------------
# 目印コメントによる差し込み
# ----------------------------------------------------------------------
def splice(text, region_id, body):
    """text の中の領域 region_id を body（改行 \\n の文字列）で置き換える。"""
    nl = "\r\n" if "\r\n" in text else "\n"
    lines = text.replace("\r\n", "\n").split("\n")
    begin = end = None
    for i, line in enumerate(lines):
        m = BEGIN_RE.match(line)
        if m and m.group("id") == region_id:
            if begin is not None:
                raise ValueError("領域 %s の BEGIN が複数ある" % region_id)
            begin = i
        m = END_RE.match(line)
        if m and m.group("id") == region_id:
            end = i
    if begin is None or end is None or end < begin:
        raise ValueError("領域 %s の BEGIN/END が見つからない（または順序が逆）" % region_id)
    new = lines[: begin + 1] + body.split("\n") + lines[end:]
    return nl.join(new)


def begin_marker(region_id, source):
    return "/* @@GEN-BEGIN %s  生成元: %s  （自動生成: 手編集禁止） */" % (region_id, source)


# ----------------------------------------------------------------------
def load_module(ctx, name):
    """モジュールの設定を読み込み、スキーマと個別の整合性を検査する。エラーの一覧を返す。"""
    spec = MODULES[name]
    path = ctx.config_dir / pathlib.Path(spec["config"]).name
    cfg = json.loads(path.read_text(encoding="utf-8"))
    schema = json.loads((CONFIG_SCHEMA_DIR / pathlib.Path(spec["schema"]).name).read_text(encoding="utf-8"))
    errors = [
        "%s: %s" % ("/".join(str(x) for x in e.absolute_path) or "(root)", e.message)
        for e in sorted(jsonschema.Draft202012Validator(schema).iter_errors(cfg), key=lambda e: list(e.absolute_path))
    ]
    if not errors:
        errors = spec["validate"](cfg)
    ctx.configs[name] = cfg
    ctx.config_paths[name] = path
    return errors


def render_module(ctx, name, repo_root, check):
    """モジュールの生成領域を更新する。戻り値: 0 = 変更なし、1 = 変更あり（--check では不一致）。"""
    spec = MODULES[name]
    cfg = ctx.configs[name]
    try:
        source = ctx.config_paths[name].relative_to(repo_root).as_posix()
    except ValueError:
        source = ctx.config_paths[name].as_posix()
    status = 0
    for rel, region, render, include_check in spec["targets"]:
        path = repo_root / rel
        text = path.read_bytes().decode("utf-8")
        new = splice(text, region, render(cfg, ctx))
        nl = "\r\n" if "\r\n" in new else "\n"
        lines = new.replace("\r\n", "\n").split("\n")
        for i, line in enumerate(lines):
            m = BEGIN_RE.match(line)
            if m and m.group("id") == region:
                lines[i] = begin_marker(region, source)
        new = nl.join(lines)
        if include_check:
            for msg in include_check(cfg, new):
                print("%s: 警告: %s" % (rel, msg))
        if new == text:
            print("%s [%s]: 変更なし" % (rel, region))
        elif check:
            print("%s [%s]: 生成結果と一致しない（--check）" % (rel, region))
            status = 1
        else:
            path.write_bytes(new.encode("utf-8"))
            print("%s [%s]: 更新した" % (rel, region))
            status = 1
    return status


def run(names, check, signals, config_dir, repo_root):
    ctx = Context(signals, config_dir)
    bad = False
    # 整合性検査は、対象に関わらず全モジュールの設定を読んで行う
    for name in sorted(MODULES):
        errors = load_module(ctx, name)
        if errors:
            bad = True
            print("%s: 設定エラー" % name)
            for e in errors:
                print("  - " + e)
    if bad:
        return 2
    errors, warnings = checks.check_all(ctx)
    for w in warnings:
        print("警告: " + w)
    if errors:
        print("整合性エラー:")
        for e in errors:
            print("  - " + e)
        return 2
    status = 0
    for name in names:
        try:
            r = render_module(ctx, name, repo_root, check)
        except ValueError as e:
            print("%s: 生成エラー: %s" % (name, e))
            return 2
        if check:
            status = status or r
    return status


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("modules", nargs="*", default=sorted(MODULES), help="対象モジュール（既定: すべて）")
    ap.add_argument("--check", action="store_true", help="書き換えず、生成結果との差分を検査する")
    ap.add_argument("--signals", default=str(DEFAULT_SIGNALS), help="信号表（既定: config/data/can_signals.json）")
    ap.add_argument("--config-dir", default=str(CONFIG_DATA_DIR), help="モジュール設定の置き場所")
    ap.add_argument("--repo-root", default=str(REPO_ROOT), help="生成先ソースのあるリポジトリのルート")
    args = ap.parse_args()
    unknown = [m for m in args.modules if m not in MODULES]
    if unknown:
        ap.error("未対応のモジュール: " + ", ".join(unknown))
    sys.exit(run(args.modules, args.check, args.signals, args.config_dir, pathlib.Path(args.repo_root)))


if __name__ == "__main__":
    main()
