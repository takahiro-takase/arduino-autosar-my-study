"""Rte（Rte.h / Rte.c）の信号アクセス関数の生成。

信号表の信号に対して、ASW 向けの `Rte_Write_<フレーム名>_<信号名>`（TX）と
`Rte_Read_<フレーム名>_<信号名>`（RX）を生成する。中身は Com_SendSignal / Com_ReceiveSignal への委譲だけ。
仕様では `<p>` はポートの名前、`<o>` はデータ要素の名前で、どちらも SW-C の設計者が決める名前（SWS_RTE）。
ここでは、フレーム名をポート名、信号名をデータ要素名として自動で作り、
作った名前が合わないものだけ Rte.json の names で上書きする。

生成するのは、次のどれにも当てはまらない信号だけ:
  - 信号グループの I-PDU の信号（確定コミットが要るため、手書きの Rte 関数で扱う）
  - ゲートウェイの入出力の信号（SWC を介さず、Com の中で転送する）
  - Com 以外の src/ のコードから、すでに使われている信号（手書きの Rte 関数や E2EMon など）
  - Rte.json の exclude に書いた信号
"""

import pathlib
import re

from . import symbols
from .com import frame_field, frame_of

REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]
SCAN_SKIP_DIRS = ("src/Bsw/Com/",)
REGION = "rte-signal-api"
BEGIN_RE = re.compile(r"^[ \t]*/\* @@GEN-BEGIN " + REGION + r"\b.*\*/[ \t]*$")
END_RE = re.compile(r"^[ \t]*/\* @@GEN-END " + REGION + r"[ \t]*\*/[ \t]*$")
IDENT_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")


def validate(cfg):
    errors = []
    signals = [n["signal"] for n in cfg["names"]]
    if len(set(signals)) != len(signals):
        errors.append("names: signal が重複している")
    return errors


# ----------------------------------------------------------------------
def ident(text):
    """信号表の名前を、C の識別子の一部にする（空白や括弧などで区切り、各語の先頭を大文字にして連結）。"""
    parts = re.findall(r"[A-Za-z0-9]+", text)
    return "".join(p[0].upper() + p[1:] for p in parts)


def ctype_of(bits):
    if bits <= 8:
        return "uint8"
    if bits <= 16:
        return "uint16"
    if bits <= 32:
        return "uint32"
    return None


def strip_region(text):
    """生成領域の中を取り除く（生成した関数が、自分自身を「使われている」と判定しないため）。"""
    out, skipping = [], False
    for line in text.replace("\r\n", "\n").split("\n"):
        if BEGIN_RE.match(line):
            skipping = True
        elif END_RE.match(line):
            skipping = False
        elif not skipping:
            out.append(line)
    return "\n".join(out)


def scan_sources(ctx):
    """Com 以外の src/ のコードを 1 つの文字列にする（生成領域は除く）。ctx に覚えておく。"""
    cached = getattr(ctx, "rte_scan", None)
    if cached is not None:
        return cached
    root = pathlib.Path(getattr(ctx, "repo_root", None) or REPO_ROOT)
    chunks = []
    for path in sorted((root / "src").rglob("*")):
        if path.suffix not in (".c", ".h"):
            continue
        rel = path.relative_to(root).as_posix()
        if rel.startswith(SCAN_SKIP_DIRS):
            continue
        chunks.append(strip_region(path.read_bytes().decode("utf-8", errors="replace")))
    ctx.rte_scan = "\n".join(chunks)
    return ctx.rte_scan


def targets(cfg, ctx):
    """生成する信号アクセス関数の一覧（Com の信号の並び順）。"""
    com = ctx.configs["Com"]
    ipdus = {p["name"]: p for p in com["rxIpdus"] + com["txIpdus"]}
    gateway = {g[k] for g in com["gateway"] for k in ("src", "dest")}
    overrides = {n["signal"]: n for n in cfg["names"]}
    known = {s["name"] for s in com["signals"]}
    unknown = [n for n in list(overrides) + list(cfg["exclude"]) if n not in known]
    if unknown:
        raise ValueError("Rte.json: Com.json の signals に無い信号名が指定されている: %s" % ", ".join(unknown))
    # 手書きのコードがすでに使っている信号のシンボリック名（ソース全体を 1 回だけ走査して集める）
    used = set(re.findall(symbols.COM_SIGNAL + r"\w+", scan_sources(ctx)))
    result, seen = [], {}
    for s in com["signals"]:
        symbol = symbols.COM_SIGNAL + s["name"]  # Com_SendSignal() / Com_ReceiveSignal() に渡す ID のシンボリック名
        ipdu = ipdus[s["ipdu"]]
        if ipdu.get("isSignalGroup") or s["ipdu"] in gateway or s["name"] in cfg["exclude"] or symbol in used:
            continue
        frame = frame_of(ipdu, ctx)
        field = frame_field(frame, s["field"])
        ctype = ctype_of(field["bitSize"])
        if ctype is None:
            raise ValueError("%s: ビット長 %d は 32 bit を超えるため、Rte の関数を生成できない" % (symbol, field["bitSize"]))
        ov = overrides.get(s["name"], {})
        port = ov.get("port") or ident(frame["name"])
        element = ov.get("element") or ident(s["field"])
        for label, value in (("ポート名", port), ("データ要素名", element)):
            if not IDENT_RE.match(value):
                raise ValueError("%s: %s '%s' が C の識別子にならない（Rte.json の names で上書きしてください）" % (symbol, label, value))
        kind = "Write" if s["direction"] == "TX" else "Read"
        name = "Rte_%s_%s_%s" % (kind, port, element)
        if name in seen:
            raise ValueError("%s: 関数名 %s が %s と重複している（Rte.json の names で上書きしてください）" % (symbol, name, seen[name]))
        seen[name] = symbol
        result.append({"name": name, "kind": kind, "symbol": symbol, "ctype": ctype, "bits": field["bitSize"],
                       "port": port, "element": element, "frame": frame["name"], "canId": frame["canId"],
                       "field": s["field"]})
    # 同じ名前の宣言・定義が手書きのコードにあれば重複（呼び出しだけなら問題ない）
    decl = r"(?:Std_ReturnType|Rte_IStatusType|uint8|void)[ \t]+%s[ \t]*\("
    names_in_src = [t["name"] for t in result if re.search(decl % t["name"], scan_sources(ctx))]
    if names_in_src:
        raise ValueError("手書きの関数と名前が重複している: %s（Rte.json の names で上書きしてください）" % ", ".join(names_in_src))
    return result


# ----------------------------------------------------------------------
def signature(t):
    if t["kind"] == "Write":
        return "Std_ReturnType %s(%s value)" % (t["name"], t["ctype"])
    return "Std_ReturnType %s(%s* data)" % (t["name"], t["ctype"])


def render_header(cfg, ctx):
    items = targets(cfg, ctx)
    if not items:
        return ""
    out = ["/* 信号表から生成した信号アクセス（手書きの Rte 関数が無い信号のみ）。", " * 名前は Rte_Write/Rte_Read_<フレーム名>_<信号名>。合わない名前は Rte.json の names で上書きする */"]
    for t in items:
        out.append("/* %s (CAN %s) %s */" % (t["frame"], t["canId"], "送信" if t["kind"] == "Write" else "受信"))
        out.append(signature(t) + ";")
    return "\n".join(out)


def render_source(cfg, ctx):
    items = targets(cfg, ctx)
    blocks = []
    for t in items:
        if t["kind"] == "Write":
            brief = "%s 提供ポートへ %s シグナルを書き込む。" % (t["port"], t["element"])
            detail = ["信号表（%s, CAN %s）から生成した送信ポート。" % (t["frame"], t["canId"]),
                      "Com_SendSignal(%s) へ委譲するだけ。" % t["symbol"]]
            param = " * \\param[in]  value  %s の値（%d bit）。" % (t["element"], t["bits"])
            retval = [" * \\retval  E_OK      COM の実 TX バッファへ正常にパックした。",
                      " * \\retval  E_NOT_OK  COM 未初期化またはシグナル ID が見つからない。"]
            body = "    return Com_SendSignal(%s, &value);" % t["symbol"]
        else:
            brief = "%s 要求ポートから %s シグナルを読み出す。" % (t["port"], t["element"])
            detail = ["信号表（%s, CAN %s）から生成した受信ポート。" % (t["frame"], t["canId"]),
                      "Com_ReceiveSignal(%s) へ委譲するだけ。" % t["symbol"]]
            param = " * \\param[out] data  %s の値の格納先（%d bit）。" % (t["element"], t["bits"])
            retval = [" * \\retval  E_OK      COM の RX バッファから正常に読み出した。",
                      " * \\retval  E_NOT_OK  COM 未初期化、シグナル ID が見つからない、または data が NULL。"]
            body = "    return Com_ReceiveSignal(%s, data);" % t["symbol"]
        doc = ["/**", " * \\brief   " + brief, " *", " * \\details " + detail[0]]
        doc += [" *          " + ln for ln in detail[1:]]
        doc += [" *", param, " *"] + retval
        doc += [" *", " * \\pre        Com_Init() が正常に完了していること。", " *",
                " * \\note       AUTOSAR 標準外の API（本プロジェクト独自拡張。信号表からの生成）。",
                " * \\Reentrancy     {Reentrant}", " * \\Synchronicity  {Synchronous}", " */"]
        blocks.append("\n".join(doc + [signature(t), "{", body, "}"]))
    return "\n\n".join(blocks)


SPEC = {
    "config": "config/data/Rte.json",
    "schema": "config/schema/Rte.schema.json",
    "validate": validate,
    "targets": [
        ("src/Rte/Rte.h", REGION, render_header, None),
        ("src/Rte/Rte.c", REGION, render_source, None),
    ],
}
