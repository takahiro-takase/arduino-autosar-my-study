# CAPL 風スクリプトのサンプル。
# 「スクリプト実行...」ボタンから選択すると、GUI と同じ bus / bus_lock を使って
# バックグラウンドスレッドで実行される。利用できる関数は capl_api.py を参照。

log("=== DefaultSession -> ExtendedSession -> SecurityAccess の確認 ===")

# 1. ExtendedSession へ遷移
send([0x10, 0x03])
resp = wait_response()
assert_positive(resp)
log("ExtendedSession OK")

# 2. SecurityAccess (seed->key 自動計算)
result = security_unlock()
log("SecurityAccess:", result)

# 3. DID 0x0101 (EngineSpeed) を読む
send([0x22, 0x01, 0x01])
resp = wait_response()
assert_positive(resp)
log("EngineSpeed 読み出し成功:", resp.describe())

# 4. 1秒毎に TesterPresent を送る on_timer の例 (5秒間だけ待受)


@ctx.on_timer(1.0)
def _keep_alive(ctx):
    ctx.send([0x3E, 0x00])
    ctx.wait_response(timeout=1.0)


log("TesterPresent を1秒毎に送信しつつ5秒待機します")
wait(5.0)

log("=== 完了 ===")
