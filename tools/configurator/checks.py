"""モジュールをまたぐ整合性の検査。

信号表（config/data/can_signals.json）と、PduR / CanIf の設定 json が、互いに食い違っていないかを調べる。
エラーは errors、直せば足りる注意は warnings に入れて返す。
"""


def _direction_ok(frame_dir, want):
    return frame_dir == want or frame_dir == "TX/RX"


def check_all(ctx):
    errors, warnings = [], []
    frames = ctx.frames
    canif = ctx.configs.get("CanIf")
    pdur = ctx.configs.get("PduR")

    if canif:
        # フレーム参照が実在し、向きが合っているか
        for key, want in (("txPdus", "TX"), ("rxPdus", "RX")):
            for p in canif[key]:
                if "frame" not in p:
                    continue
                f = frames.get(p["frame"])
                if f is None:
                    errors.append("CanIf %s: frame '%s' が信号表に存在しない" % (p["name"], p["frame"]))
                elif not _direction_ok(f["direction"], want):
                    errors.append("CanIf %s: frame '%s' の向き(%s)が %s PDU と合わない" % (p["name"], p["frame"], f["direction"], want))

        # CAN ID の重複（TX 同士、RX 同士）
        for key in ("txPdus", "rxPdus"):
            seen = {}
            for p in canif[key]:
                if "frame" in p and p["frame"] not in frames:
                    continue
                cid = frames[p["frame"]]["canId"] if "frame" in p else p["canId"]
                if cid in seen:
                    errors.append("CanIf %s: CAN ID %s が %s と重複している" % (p["name"], cid, seen[cid]))
                seen[cid] = p["name"]

        # 信号表のフレームが、CanIf に漏れなく設定されているか
        tx_frames = {p.get("frame") for p in canif["txPdus"]}
        rx_frames = {p.get("frame") for p in canif["rxPdus"]}
        for name, f in sorted(frames.items()):
            if f["direction"] in ("TX", "TX/RX") and name not in tx_frames:
                errors.append("信号表のフレーム '%s'(TX) に対応する CanIf TX PDU がない（sync_cfg.py で追加できる）" % name)
            if f["direction"] in ("RX", "TX/RX") and name not in rx_frames:
                errors.append("信号表のフレーム '%s'(RX) に対応する CanIf RX PDU がない（sync_cfg.py で追加できる）" % name)

    if canif and pdur:
        # PduR 設定が指す frame が実在するか
        for key in ("rxPaths", "txPaths"):
            for p in pdur[key]:
                if "frame" in p and p["frame"] not in frames:
                    errors.append("PduR %s: frame '%s' が信号表に存在しない" % (p["name"], p["frame"]))

        # TX: PduR の経路 ⇔ CanIf の TX PDU
        tx_pdus = canif["txPdus"]
        linked = set()
        for p in pdur["txPaths"]:
            idx = p["canIfTxPduId"]
            if idx >= len(tx_pdus):
                errors.append("PduR %s: canIfTxPduId=%d に対応する CanIf TX PDU がない" % (p["name"], idx))
                continue
            t = tx_pdus[idx]
            linked.add(idx)
            if t["txConfirm"] != "PDUR":
                errors.append("PduR %s: CanIf TX PDU '%s' の送信完了通知が PduR ではない" % (p["name"], t["name"]))
            if t["upperLayerTxPduId"] != p["srcPduId"]:
                errors.append(
                    "PduR %s: srcPduId=%d と、CanIf '%s' の upperLayerTxPduId=%d が一致しない"
                    % (p["name"], p["srcPduId"], t["name"], t["upperLayerTxPduId"])
                )
            if "frame" in p and "frame" in t and p["frame"] != t["frame"]:
                errors.append("PduR %s: frame '%s' が CanIf '%s' の frame '%s' と異なる" % (p["name"], p["frame"], t["name"], t["frame"]))
        for i, t in enumerate(tx_pdus):
            if t["txConfirm"] == "PDUR" and i not in linked:
                errors.append("CanIf TX PDU '%s'(TxPduId=%d) を指す PduR TX 経路がない" % (t["name"], i))

        # RX: CanIf の RX PDU ⇔ PduR の経路
        srcs = {p["srcPduId"]: p for p in pdur["rxPaths"]}
        for i, r in enumerate(canif["rxPdus"]):
            if r["rxIndication"] != "PDUR":
                continue
            p = srcs.get(r["upperLayerRxPduId"])
            if p is None:
                errors.append("CanIf RX PDU '%s': upperLayerRxPduId=%d に対応する PduR RX 経路がない" % (r["name"], r["upperLayerRxPduId"]))
            elif "frame" in p and "frame" in r and p["frame"] != r["frame"]:
                errors.append("PduR %s: frame '%s' が CanIf '%s' の frame '%s' と異なる" % (p["name"], p["frame"], r["name"], r["frame"]))
        rx_up = {r["upperLayerRxPduId"] for r in canif["rxPdus"] if r["rxIndication"] == "PDUR"}
        for p in pdur["rxPaths"]:
            if p["srcPduId"] not in rx_up:
                errors.append("PduR RX 経路 '%s'(srcPduId=%d) に対応する CanIf RX PDU がない" % (p["name"], p["srcPduId"]))

    com = ctx.configs.get("Com")
    if com:
        _check_com(ctx, com, errors, warnings)
        if pdur:
            _check_com_pdur(com, pdur, errors, ctx.configs.get("SecOC"))
    _check_e2exf(ctx, errors)
    _check_secoc(ctx, errors)
    _check_cryptostack(ctx, errors, warnings)
    _check_bswm(ctx, errors, warnings)

    return errors, warnings


def _is_com_field(f):
    """信号表のフィールドのうち、Com の信号になりうるもの（update-bit と、E2E / SecOC の保護用は除く）。"""
    return not f["name"].startswith("(") and not f["type"].startswith(("e2e_", "secoc_"))


def _check_com(ctx, com, errors, warnings):
    frames = ctx.frames
    excluded = {e["frame"] for e in com.get("excludedFrames", [])}
    for e in com.get("excludedFrames", []):
        if e["frame"] not in frames:
            errors.append("Com excludedFrames: フレーム '%s' が信号表に存在しない" % e["frame"])

    # I-PDU が参照するフレーム
    for key, want in (("rxIpdus", "RX"), ("txIpdus", "TX")):
        for p in com[key]:
            f = frames.get(p["frame"])
            if f is None:
                errors.append("Com %s: frame '%s' が信号表に存在しない" % (p["name"], p["frame"]))
            elif not _direction_ok(f["direction"], want):
                errors.append("Com %s: frame '%s' の向き(%s)が %s I-PDU と合わない" % (p["name"], p["frame"], f["direction"], want))
            elif "dlc" in p and p["dlc"] == f["dlc"]:
                warnings.append("Com %s: dlc は信号表と同じ値なので指定不要" % p["name"])

    # 信号表のフレームが Com に漏れなく設定されているか
    rx_frames = {p["frame"] for p in com["rxIpdus"]}
    tx_frames = {p["frame"] for p in com["txIpdus"]}
    for name, f in sorted(frames.items()):
        if name in excluded:
            continue
        if f["direction"] in ("TX", "TX/RX") and name not in tx_frames:
            errors.append("信号表のフレーム '%s'(TX) に対応する Com TX I-PDU がない（sync_cfg.py で追加できる）" % name)
        if f["direction"] in ("RX", "TX/RX") and name not in rx_frames:
            errors.append("信号表のフレーム '%s'(RX) に対応する Com RX I-PDU がない（sync_cfg.py で追加できる）" % name)

    # シグナル: フィールドの実在、ビットの重なり・範囲、フィールドの漏れ
    ipdus = {("RX", p["name"]): p for p in com["rxIpdus"]}
    ipdus.update({("TX", p["name"]): p for p in com["txIpdus"]})
    used = {}
    ranges = {}
    for s in com["signals"]:
        p = ipdus.get((s["direction"], s["ipdu"]))
        f = frames.get(p["frame"]) if p else None
        if f is None:
            continue
        fld = next((x for x in f["fields"] if x["name"] == s["field"]), None)
        if fld is None:
            errors.append("Com %s: フィールド '%s' がフレーム '%s' に存在しない" % (s["name"], s["field"], f["name"]))
            continue
        used.setdefault((s["direction"], s["ipdu"]), set()).add(s["field"])
        pos = s.get("bitPosition", fld["bitPosition"])
        size = s.get("bitSize", fld["bitSize"])
        if "bitPosition" in s or "bitSize" in s:
            warnings.append("Com %s: 信号表と異なるビット位置・長さを個別に指定している" % s["name"])
        dlc = p.get("dlc", f["dlc"])
        if pos + size > dlc * 8:
            errors.append("Com %s: ビット範囲 [%d, %d) が I-PDU '%s' の DLC(%d byte) を超える" % (s["name"], pos, pos + size, p["name"], dlc))
        for (a, b2, other) in ranges.setdefault((s["direction"], s["ipdu"]), []):
            if pos < b2 and a < pos + size:
                errors.append("Com %s: ビット範囲が '%s' と重なる" % (s["name"], other))
        ranges[(s["direction"], s["ipdu"])].append((pos, pos + size, s["name"]))
    for (direction, name), p in ipdus.items():
        f = frames.get(p["frame"])
        if f is None:
            continue
        have = used.get((direction, name), set()) | set(p.get("unusedFields", []))
        for fld in f["fields"]:
            if _is_com_field(fld) and fld["name"] not in have:
                errors.append(
                    "信号表のフィールド '%s.%s' に対応する Com のシグナルがない（sync_cfg.py で追加するか、意図して使わないなら I-PDU の unusedFields に書く）"
                    % (f["name"], fld["name"])
                )
        for u in p.get("unusedFields", []):
            if not any(x["name"] == u for x in f["fields"]):
                errors.append("Com %s: unusedFields の '%s' がフレーム '%s' に存在しない" % (name, u, f["name"]))

    # 定数: 信号表から引く値の存在
    for c in com["constants"]:
        if "fromFrame" in c:
            fr = frames.get(c["fromFrame"]["frame"])
            if fr is None or fr.get(c["fromFrame"]["key"]) is None:
                errors.append("Com 定数 %s: 信号表のフレーム '%s' に %s がない" % (c["macro"], c["fromFrame"]["frame"], c["fromFrame"]["key"]))

    # update-bit: 信号表に位置があるのに、I-PDU が別の値を指定していないか
    for (direction, name), p in ipdus.items():
        f = frames.get(p["frame"])
        ub = next((x for x in f["fields"] if x["name"] == "(update-bit)"), None) if f else None
        if ub and "updateBitPosition" in p and p["updateBitPosition"] != ub["bitPosition"]:
            errors.append("Com %s: updateBitPosition=%d が信号表の (update-bit) の位置 %d と一致しない" % (name, p["updateBitPosition"], ub["bitPosition"]))


def _check_com_pdur(com, pdur, errors, secoc=None):
    """Com ⇔ PduR: 参照する I-PDU の実在と、PduR の経路の宛先。"""
    rx = {p["name"] for p in com["rxIpdus"]}
    tx = {p["name"] for p in com["txIpdus"]}
    for path in pdur["rxPaths"]:
        for d in path["dests"]:
            if "ipdu" in d and d["ipdu"] not in rx:
                errors.append("PduR %s: Com に RX I-PDU '%s' が存在しない" % (path["name"], d["ipdu"]))
    for path in pdur["txPaths"]:
        if "confIpdu" in path and path["confIpdu"] not in tx:
            errors.append("PduR %s: Com に TX I-PDU '%s' が存在しない" % (path["name"], path["confIpdu"]))
    # Com の I-PDU に、PduR の経路がある（PduR を経由しないものは pduRId を明示して区別する）
    dests = {d.get("ipdu") for p in pdur["rxPaths"] for d in p["dests"]}
    confs = {p.get("confIpdu") for p in pdur["txPaths"]}
    via_secoc = {s["comIpdu"] for s in (secoc or {}).get("rxPdus", [])}
    for p in com["rxIpdus"]:
        if "pduRId" not in p and p["name"] not in dests and p["name"] not in via_secoc:
            errors.append("Com RX I-PDU '%s': PduR の経路も SecOC の経路もなく、pduRId も未指定" % p["name"])
    for p in com["txIpdus"]:
        if "pduRId" not in p and p["name"] not in confs:
            errors.append("Com TX I-PDU '%s': PduR の経路がなく、pduRId も未指定" % p["name"])


def _crc_counter(frame):
    crc = next((f for f in frame["fields"] if f["type"] == "e2e_crc"), None)
    cnt = next((f for f in frame["fields"] if f["type"] == "e2e_counter"), None)
    return crc, cnt


def _check_e2exf(ctx, errors):
    """信号表の E2E 用フィールド ⇔ E2EXf の設定 ⇔ Com のコールバック。"""
    cfg = ctx.configs.get("E2EXf")
    if cfg is None:
        return
    frames = ctx.frames
    com = ctx.configs.get("Com")
    covered = {}
    ids = {}
    for p in cfg["instances"]:
        f = frames.get(p["frame"])
        if f is None:
            errors.append("E2EXf %s: frame '%s' が信号表に存在しない" % (p["name"], p["frame"]))
            continue
        covered[(p["frame"], p["direction"])] = p
        if not _direction_ok(f["direction"], p["direction"]):
            errors.append("E2EXf %s: frame '%s' の向き(%s)が %s と合わない" % (p["name"], p["frame"], f["direction"], p["direction"]))
        crc, cnt = _crc_counter(f)
        if crc is None or cnt is None:
            errors.append("E2EXf %s: フレーム '%s' に e2e_crc / e2e_counter のフィールドがない" % (p["name"], f["name"]))
            continue
        if crc["bitSize"] != 16 or cnt["bitSize"] != 8 or cnt["bitPosition"] != crc["bitPosition"] + 16:
            errors.append("E2EXf %s: Profile05 のレイアウト（CRC16 の直後に Counter8）になっていない" % p["name"])
        data_id = p["dataId"] if "dataId" in p else int(f["canId"], 16)
        if data_id in ids:
            errors.append("E2EXf %s: DataID 0x%04X が %s と重複している" % (p["name"], data_id, ids[data_id]))
        ids[data_id] = p["name"]
        if com:
            key = "rxIpdus" if p["direction"] == "RX" else "txIpdus"
            ip = next((x for x in com[key] if x["frame"] == p["frame"]), None)
            need = "rxIndicationCbk" if p["direction"] == "RX" else "txTransformCbk"
            if ip is None:
                errors.append("E2EXf %s: フレーム '%s' に対応する Com の I-PDU がない" % (p["name"], p["frame"]))
            elif need not in ip:
                errors.append("E2EXf %s: Com の I-PDU '%s' に %s（E2E の呼び出し元）が設定されていない" % (p["name"], ip["name"], need))
    # E2E 用フィールドを持つフレームに、設定が漏れていないか
    for name, f in sorted(frames.items()):
        crc, cnt = _crc_counter(f)
        if crc is None and cnt is None:
            continue
        for d in ("TX", "RX"):
            if _direction_ok(f["direction"], d) and f["direction"] != ("RX" if d == "TX" else "TX") and (name, d) not in covered:
                errors.append(
                    "信号表のフレーム '%s'(%s) は E2E 用のフィールドを持つが、E2EXf の設定がない"
                    "（E2EXf.json に手で追加する。Dem イベントと、E2EXf.c / Rte の呼び出し関数も必要）" % (name, d)
                )


def _check_secoc(ctx, errors):
    """信号表の SecOC 用フィールド ⇔ SecOC の設定 ⇔ Com / PduR / CanIf。"""
    cfg = ctx.configs.get("SecOC")
    if cfg is None:
        return
    frames = ctx.frames
    com, pdur, canif = ctx.configs.get("Com"), ctx.configs.get("PduR"), ctx.configs.get("CanIf")
    covered = set()
    for p in cfg["rxPdus"]:
        f = frames.get(p["frame"])
        if f is None:
            errors.append("SecOC %s: frame '%s' が信号表に存在しない" % (p["name"], p["frame"]))
            continue
        covered.add(p["frame"])
        if f["direction"] != "RX":
            errors.append("SecOC %s: フレーム '%s' の向きが RX ではない（TX の Secured I-PDU は未対応）" % (p["name"], f["name"]))
        for t in ("secoc_freshness", "secoc_mac"):
            if not any(x["type"] == t for x in f["fields"]):
                errors.append("SecOC %s: フレーム '%s' に %s のフィールドがない" % (p["name"], f["name"], t))
        if com:
            ip = next((x for x in com["rxIpdus"] if x["name"] == p["comIpdu"]), None)
            if ip is None:
                errors.append("SecOC %s: Com に RX I-PDU '%s' が存在しない" % (p["name"], p["comIpdu"]))
            elif ip["frame"] != p["frame"]:
                errors.append("SecOC %s: Com の I-PDU '%s' が別のフレーム '%s' を指している" % (p["name"], ip["name"], ip["frame"]))
            elif "dlc" in ip:
                errors.append("Com %s: dlc は SecOC の Authentic Payload 長から引くため指定しない" % ip["name"])
        if pdur and canif:
            path = next((r for r in pdur["rxPaths"] for d in r["dests"] if d.get("secocPdu") == p["name"]), None)
            if path is None:
                errors.append("SecOC %s: PduR に、この PDU を宛先にする RX 経路がない" % p["name"])
            else:
                cp = next((r for r in canif["rxPdus"] if r["rxIndication"] == "PDUR" and r["upperLayerRxPduId"] == path["srcPduId"]), None)
                if cp is None or cp.get("frame") != p["frame"]:
                    errors.append("SecOC %s: PduR の経路 '%s' につながる CanIf の RX PDU が、フレーム '%s' を指していない" % (p["name"], path["name"], p["frame"]))
    for name, f in sorted(frames.items()):
        if any(x["type"].startswith("secoc_") for x in f["fields"]) and name not in covered:
            errors.append("信号表のフレーム '%s' は SecOC 用のフィールドを持つが、SecOC の設定がない（SecOC.json に手で追加する。Csm のジョブも必要）" % name)


def _check_cryptostack(ctx, errors, warnings):
    """SecOC のジョブが Csm のジョブとして存在し、ジョブが使う鍵が存在するか。未使用の鍵・ジョブは警告。"""
    cs = ctx.configs.get("CryptoStack")
    if cs is None:
        return
    jobs = {j["macro"]: j for j in cs["jobs"]}
    used_jobs = set()
    sec = ctx.configs.get("SecOC")
    if sec:
        for p in sec["rxPdus"]:
            job = jobs.get(p["csmJobId"])
            if job is None:
                errors.append("SecOC %s: csmJobId '%s' が Csm のジョブとして存在しない" % (p["name"], p["csmJobId"]))
                continue
            used_jobs.add(job["macro"])
            if job["service"] != "MACVERIFY":
                errors.append("SecOC %s: RX の Secured I-PDU に使うジョブ '%s' が MACVERIFY ではない" % (p["name"], job["macro"]))
    for j in cs["jobs"]:
        if sec and j["macro"] not in used_jobs:
            warnings.append("Csm のジョブ '%s' を使う SecOC の PDU がない" % j["macro"])
    used_keys = {j["key"] for j in cs["jobs"]}
    for k in cs["keys"]:
        if k["name"] not in used_keys:
            warnings.append("鍵 '%s' を使うジョブがない" % k["macro"])


def _check_bswm(ctx, errors, warnings):
    """BswM のルールが指す I-PDU グループの実在と、グループごとの起動・停止ルールの有無。"""
    bswm, com = ctx.configs.get("BswM"), ctx.configs.get("Com")
    if bswm is None or com is None:
        return
    groups = [g["macro"] for g in com.get("ipduGroups", [])]
    starts, stops = set(), set()
    for i, r in enumerate(bswm["rules"]):
        g = r.get("ipduGroup")
        if g is None:
            continue
        if g not in groups:
            errors.append("BswM rules[%d]: ipduGroup '%s' が Com の ipduGroups に存在しない" % (i, g))
        (starts if r["action"] == "PDU_GROUP_START" else stops).add(g)
    members = {}
    for p in com["rxIpdus"] + com["txIpdus"]:
        g = p.get("ipduGroupId")
        if g is not None and g != "COM_IPDU_GROUP_NONE":
            members.setdefault(g, []).append(p["name"])
    for g in groups:
        ips = members.get(g, [])
        if not ips:
            warnings.append("I-PDU グループ '%s' に属する I-PDU がない" % g)
            continue
        if g not in starts:
            errors.append(
                "I-PDU グループ '%s'（%s）を起動する BswM のルールがない（グループは既定で停止状態のため、送受信されない）"
                % (g, ", ".join(ips))
            )
        if g not in stops:
            warnings.append("I-PDU グループ '%s' を停止する BswM のルールがない" % g)
