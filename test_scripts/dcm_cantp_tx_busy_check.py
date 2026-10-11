# Dcm_ComIndication() の CanTp TXビジー中無視チェック（[SWS_Dcm_00557]）を
# 実機で確認するスクリプト。
#
# 背景: Dcm_ComIndication() は以前、CanTp の送信状態を一切確認せずに新規要求を
# ディスパッチしていた。前回応答（特にマルチフレーム）の送信が完了していない
# 状態で新規要求が届くと、CanTp_Transmit() は自身のビジー判定で新しい応答を
# 黙って破棄する一方、Dcm は要求の副作用（セッション遷移・SecurityAccess試行
# 回数の加算等）を完全に確定させてしまっていた。修正後は CanTp_IsTxBusy() が
# TRUE の間、新規要求を一切ディスパッチせず無視する。
#
# 検証方針: 実際の CF フレーム間隔（STmin=0/ECUの1ms周期タスク次第で数msしか
# 無くスクリプトから正確に割り込むのは困難）を狙う代わりに、ISO 15765-2の
# N_Bs（FF送信後、FCを受信するまでの待機、本プロジェクトは5000ms）を利用する。
# FF を受信した直後は CanTp_Tx.state が CANTP_TX_WAIT_FC（=ビジー）のまま
# FC 送信を故意に遅らせることで、好きなだけ長い「ビジー窓」を確保できる。
#
# 手順:
#   1. extendedSession へ遷移（SecurityAccess はここでは Unlock しない）。
#   2. VIN (DID 0xF190, 17バイト) を要求し、マルチフレーム応答をトリガーする。
#      FF を受信したら、あえて FC をすぐには送らない（CanTp は WAIT_FC=ビジー
#      のまま最大 N_Bs=5000ms 待ってくれる）。
#   3. ビジー中に衝突要求 [0x10, 0x01]（defaultSessionへの切替）を送る。
#      修正が効いていれば、この要求は一切ディスパッチされず応答も来ない。
#   4. FC を送って VIN 応答の残りを正常に回収できることを確認する
#      （ビジー中の割り込みが本来の応答自体を壊していないことの確認）。
#   5. SecurityAccess requestSeed [0x27, 0x01] を送る。
#        - 正応答（seed取得）      -> extendedSession のまま = 修正が機能している
#        - NRC 0x7F (serviceNotSupportedInActiveSession) -> defaultSessionへ
#          切り替わってしまった = 衝突要求が処理されてしまった（バグ再現）
#
# 注意: ステップ2〜3は生の CAN フレームを直接組み立てて送るため、
# send()/wait_response() は使わず send_can()/ctx.try_recv() を使う
# （try_recv() は RX モニタ用ワーカーとの競合を避けるための正規ルート、
# capl_api.py 冒頭のコメント参照）。

import time

REQUEST_ID = 0x7E0
RESPONSE_ID = 0x7E8


def _hex(data) -> str:
    return " ".join(f"{b:02X}" for b in data)


def _recv_response_frame(timeout_s: float):
    """RESPONSE_ID 宛のフレームだけを待つ（RXモニタ用の他IDフレームは読み捨てる）。"""
    deadline = time.monotonic() + timeout_s
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return None
        msg = ctx.try_recv(timeout=min(0.1, remaining))
        if msg is not None and msg.arbitration_id == RESPONSE_ID and msg.data:
            return msg


log("=== [SWS_Dcm_00557] CanTp TXビジー中の新規要求無視チェック ===")

# 1. extendedSession へ遷移
send([0x10, 0x03])
resp = wait_response()
assert_positive(resp)
log("extendedSession OK")

# 2. VIN 読み出し要求 (SF: [0x03, 0x22, 0xF1, 0x90]) を送り、
#    17バイトのVINによりマルチフレーム応答（FF+CF）をトリガーする。
log("VIN(0xF190)要求送信 (マルチフレーム応答をトリガー)")
send_can(REQUEST_ID, [0x03, 0x22, 0xF1, 0x90, 0x00, 0x00, 0x00, 0x00])

ff = _recv_response_frame(2.0)
if ff is None:
    log("NG: FFフレームを受信できませんでした（応答が短すぎた/未受信）")
elif (ff.data[0] >> 4) != 0x1:
    log(f"NG: 想定外のフレームを受信しました: {_hex(ff.data)}")
else:
    expected_len = ((ff.data[0] & 0x0F) << 8) | ff.data[1]
    payload = bytearray(ff.data[2:8])
    log(f"FF受信 (全長={expected_len}): {_hex(ff.data)}  "
        f"-> CanTp は WAIT_FC(ビジー)のはず。まだFCを送らない。")

    # 3. ビジー中に衝突要求（defaultSessionへの切替）を送る
    log("ビジー中に衝突要求 [0x10, 0x01] (defaultSessionへ切替) を送信")
    send_can(REQUEST_ID, [0x02, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00])

    stray = _recv_response_frame(0.5)
    if stray is not None:
        log(f"警告: 衝突要求に対する応答らしきフレームを検出: {_hex(stray.data)} "
            f"（本来は無応答のはず。手動でも原因を確認してください）")
    else:
        log("衝突要求への応答なし（期待通り: ディスパッチされていない）")

    # 4. FC を送って VIN 応答の残りを回収する
    log("FC送信 (BS=0/STmin=0)、残りのCFフレームを回収")
    send_can(REQUEST_ID, [0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])

    cf_deadline = time.monotonic() + 2.0
    while len(payload) < expected_len and time.monotonic() < cf_deadline:
        msg = ctx.try_recv(timeout=0.1)
        if msg is not None and msg.arbitration_id == RESPONSE_ID and msg.data \
                and (msg.data[0] >> 4) == 0x2:
            payload.extend(msg.data[1:8])

    if len(payload) < expected_len:
        log(f"NG: VIN応答を最後まで回収できませんでした ({len(payload)}/{expected_len} バイト)")
    else:
        full = bytes(payload[:expected_len])
        vin_ascii = full[3:].decode("ascii", errors="replace")
        log(f"VIN応答受信完了: SID=0x{full[0]:02X} DID=0x{full[1]:02X}{full[2]:02X} VIN={vin_ascii}")

        # 5. SecurityAccess requestSeed でセッション状態を判定する
        log("=== 判定: SecurityAccess requestSeed でセッション状態を確認 ===")
        result = security_unlock()
        log(result)
        if "アンロック成功" in result or "既にアンロック済み" in result:
            log("OK: extendedSessionが維持されていた -> 修正が正しく機能している")
        elif "NRC=0x7F" in result or "serviceNotSupportedInActiveSession" in result:
            log("NG: defaultSessionへ切り替わっていた -> 衝突要求が処理されてしまった（バグ再現）")
        else:
            log(f"判定不能な応答: {result}（手動で内容を確認してください）")

log("=== 完了 ===")
