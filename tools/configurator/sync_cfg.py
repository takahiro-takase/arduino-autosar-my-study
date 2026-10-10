"""信号表（config/data/can_signals.json）に追加されたフレームの設定を、CanIf / Com / PduR の設定 json へ追加する。

使い方:
    python tools/configurator/sync_cfg.py             # 不足している設定のひな形を追加する
    python tools/configurator/sync_cfg.py --dry-run   # 追加内容を表示するだけ（書き込まない）

追加するのは「参照」と既定の経路だけで、CAN ID や DLC は設定 json にコピーしない
（生成時に信号表から引く）。人が決める値（経路の宛先モジュールなど）は既定値で入れるので、
必要なら追加後に設定 json を直し、gen_cfg.py で生成する。

削除や名前変更は自動では扱わない。信号表に無いフレームを参照している設定は、報告だけする。
"""

import argparse
import json
import pathlib
import re
import sys

TOOL_DIR = pathlib.Path(__file__).resolve().parent
REPO_ROOT = TOOL_DIR.parents[1]
DEFAULT_SIGNALS = REPO_ROOT / "config" / "data" / "can_signals.json"


def read_json(path):
    return json.loads(pathlib.Path(path).read_text(encoding="utf-8"))


def write_json(path, data):
    """設定 json の書式（インデント 2、日本語はそのまま、CRLF）に統一して書き出す。"""
    text = json.dumps(data, ensure_ascii=False, indent=2) + "\n"
    pathlib.Path(path).write_bytes(text.replace("\n", "\r\n").encode("utf-8"))


def short_name(frame_name):
    """'SecureCommand (ImmobilizerCmd)' → 'SecureCommand_ImmobilizerCmd'。"""
    return re.sub(r"_+", "_", re.sub(r"[^A-Za-z0-9]", "_", frame_name)).strip("_")


def snake_upper(name):
    """'BrakePressure' → 'BRAKE_PRESSURE'、'RunLamp (mirror)' → 'RUN_LAMP_MIRROR'。"""
    s = re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_")
    s = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", s)
    return s.upper()


def is_com_field(f):
    """信号表のフィールドのうち、Com の信号になりうるもの。"""
    return not f["name"].startswith("(") and not f["type"].startswith(("e2e_", "secoc_"))


def sync(signals, canif, pdur, com):
    """不足分を canif / pdur / com に追加し、(追加の説明, 参照切れの報告, 要確認の事項) を返す。"""
    added, orphans, todo = [], [], []
    frames = {f["name"]: f for f in signals["frames"]}
    excluded = {e["frame"] for e in com.get("excludedFrames", [])}

    def canif_pdu(key, frame):
        return next((p for p in canif[key] if p.get("frame") == frame), None)

    for f in signals["frames"]:
        name, sn, can_id = f["name"], short_name(f["name"]), f["canId"]
        for direction, d_in in (("TX", ("TX", "TX/RX")), ("RX", ("RX", "TX/RX"))):
            if f["direction"] not in d_in:
                continue
            suffix = "_" + direction.title()
            ckey = "txPdus" if direction == "TX" else "rxPdus"
            comkey = "txIpdus" if direction == "TX" else "rxIpdus"
            pkey = "txPaths" if direction == "TX" else "rxPaths"
            made = []

            # --- CanIf ---
            cp = canif_pdu(ckey, name)
            if cp is None:
                if direction == "TX":
                    up = max([p["srcPduId"] for p in pdur["txPaths"]] + [p["upperLayerTxPduId"] for p in canif[ckey]] + [-1]) + 1
                    cp = {"name": sn + suffix, "frame": name, "title": "%s フレーム" % name,
                          "upperLayerTxPduId": up, "hth": 0, "txConfirm": "PDUR"}
                else:
                    up = max([p["srcPduId"] for p in pdur["rxPaths"]] + [p["upperLayerRxPduId"] for p in canif[ckey]] + [-1]) + 1
                    cp = {"name": sn + suffix, "frame": name, "title": "%s フレーム" % name,
                          "upperLayerRxPduId": up, "hrh": 0, "rxIndication": "PDUR"}
                canif[ckey].append(cp)
                made.append("CanIf %sPduId=%d" % (direction.title(), len(canif[ckey]) - 1))

            # --- Com ---
            comname = None
            if name not in excluded:
                ip = next((p for p in com[comkey] if p["frame"] == name), None)
                if ip is None:
                    ip = {"name": sn + suffix, "frame": name, "title": "%s フレーム" % name}
                    if direction == "RX" and f.get("rxTimeoutMs") is not None:
                        macro = "COM_TIMEOUT_%s_MS" % snake_upper(sn)
                        com["constants"].append(
                            {"macro": macro, "fromFrame": {"frame": name, "key": "rxTimeoutMs"},
                             "doc": ["%s (CAN %s) 受信タイムアウト [ms]（信号表の rxTimeoutMs から引く）" % (name, can_id)]}
                        )
                        ip["firstTimeoutMs"] = macro
                        ip["timeoutMs"] = macro
                    com[comkey].append(ip)
                    made.append("Com %s I-PDU %s" % (direction, ip["name"]))
                    todo.append("%s: Com の振る舞い（送信モード、タイムアウト、I-PDU グループ、コールバックなど）は既定値。必要なら Com.json を直す" % name)
                comname = ip["name"]
                have = {s["field"] for s in com["signals"] if s["ipdu"] == comname and s["direction"] == direction}
                for fld in f["fields"]:
                    if not is_com_field(fld) or fld["name"] in have or fld["name"] in ip.get("unusedFields", []):
                        continue
                    base = "%s_%s" % (short_name(name), short_name(fld["name"]))
                    s = {
                        "name": "%s_%s" % (base, direction.title()),
                        "title": "%s  %s %dbit  CAN %s bit%d" % (fld["name"], direction, fld["bitSize"], can_id, fld["bitPosition"]),
                        "macro": "COM_SIGNAL_%s_%s" % (snake_upper(sn), snake_upper(fld["name"])),
                        "direction": direction,
                        "ipdu": comname,
                        "field": fld["name"],
                        "idDoc": ["%s: %s (%d bit, CAN ID %s, bit %d)" % (direction, fld["name"], fld["bitSize"], can_id, fld["bitPosition"])],
                    }
                    if direction == "TX":
                        s["filterAlgorithm"] = "COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD"
                        s["mask"] = (1 << fld["bitSize"]) - 1
                    com["signals"].append(s)
                    made.append("Com シグナル %s" % s["macro"])

            # --- PduR（Com の I-PDU があり、CanIf が PduR 宛ての場合） ---
            if comname and cp.get("txConfirm", cp.get("rxIndication")) == "PDUR":
                cidx0 = canif[ckey].index(cp)
                if direction == "TX":
                    routed = any(p.get("confIpdu") == comname or p["canIfTxPduId"] == cidx0 for p in pdur["txPaths"])
                else:
                    routed = any(p["srcPduId"] == cp["upperLayerRxPduId"] for p in pdur["rxPaths"])
                if not routed:
                    cidx = canif[ckey].index(cp)
                    if direction == "TX":
                        src = cp["upperLayerTxPduId"]
                        pdur[pkey].append(
                            {"name": sn + suffix, "frame": name,
                             "summary": "COM (SrcPduId=%d) → CanIf TxPduId=%d (CAN {canId}, %s)" % (src, cidx, name),
                             "srcPduId": src, "canIfTxPduId": cidx, "confIpdu": comname, "confModule": "COM"}
                        )
                    else:
                        src = cp["upperLayerRxPduId"]
                        pdur[pkey].append(
                            {"name": sn + suffix, "frame": name,
                             "tableSummary": "CAN {canId} → COM (%s)" % name,
                             "summary": "CanIf RxPduId=%d (CAN {canId}) → COM/%s" % (cidx, name),
                             "srcPduId": src, "destCountNote": "COM のみ",
                             "dests": [{"module": "COM", "ipdu": comname}]}
                        )
                    made.append("PduR %s 経路 SrcPduId=%d" % (direction, src))

            if made:
                added.append("%s %s (CAN %s, DLC %d): %s" % (direction, name, can_id, f["dlc"], " / ".join(made)))
                if any(x["type"].startswith(("e2e_", "secoc_")) for x in f["fields"]):
                    todo.append("%s: E2E / SecOC 用のフィールドがある。E2EXf.json / SecOC.json に手で追加する"
                                "（Dem イベント、Csm のジョブ、E2EXf.c / Rte の呼び出し関数も必要）。追加するまで gen_cfg.py は整合性エラーで止まる" % name)
        if f["direction"] == "TX/RX" and any(a.startswith(("TX %s " % name, "RX %s " % name)) for a in added):
            todo.append("%s は TX/RX 両方向のフレーム。経路の既定は PduR→Com にしてある。CanNm 等へ直接渡すなら CanIf の txConfirm / rxIndication を直す" % name)

    refs = [("CanIf.txPdus", canif["txPdus"]), ("CanIf.rxPdus", canif["rxPdus"]), ("PduR.rxPaths", pdur["rxPaths"]),
            ("PduR.txPaths", pdur["txPaths"]), ("Com.rxIpdus", com["rxIpdus"]), ("Com.txIpdus", com["txIpdus"])]
    for key, cfg in refs:
        for p in cfg:
            if "frame" in p and p["frame"] not in frames:
                orphans.append("%s '%s' が参照するフレーム '%s' が信号表に無い（削除・名前変更は手で直す）" % (key, p["name"], p["frame"]))
    return added, orphans, todo


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dry-run", action="store_true", help="追加内容を表示するだけで、書き込まない")
    ap.add_argument("--signals", default=str(DEFAULT_SIGNALS), help="信号表（既定: config/data/can_signals.json）")
    ap.add_argument("--config-dir", default=str(REPO_ROOT / "config" / "data"), help="モジュール設定の置き場所")
    args = ap.parse_args()

    cdir = pathlib.Path(args.config_dir)
    signals = read_json(args.signals)
    canif = read_json(cdir / "CanIf.json")
    pdur = read_json(cdir / "PduR.json")
    com = read_json(cdir / "Com.json")

    added, orphans, todo = sync(signals, canif, pdur, com)

    for a in added:
        print("追加: " + a)
    for o in orphans:
        print("警告: " + o)
    for t in todo:
        print("要確認: " + t)
    if not added:
        print("追加するものはない（信号表のフレームはすべて設定済み）")
    elif args.dry_run:
        print("--dry-run のため書き込まない")
    else:
        write_json(cdir / "CanIf.json", canif)
        write_json(cdir / "PduR.json", pdur)
        write_json(cdir / "Com.json", com)
        print("設定 json を更新した。gen_cfg.py で Cfg ソースを生成する")
    sys.exit(1 if orphans else 0)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as e:
        # 信号表や設定 json の読み込み失敗など。終了コード 1（警告あり）と区別する
        print("エラー: %s: %s" % (type(e).__name__, e))
        sys.exit(2)
