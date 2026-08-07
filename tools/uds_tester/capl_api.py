"""
CAPL 風スクリプト API 層。

Vector CAPL に近い書き味 (send()/wait_response()/assert_positive() 等) の薄い
Python API。実体は uds_link.py の低レベル送受信関数のラッパーで、GUI (app.py)
の「スクリプト実行」から exec() されるスクリプトのグローバル名前空間に、
これらの関数をそのまま公開する。スクリプト自体は Python 構文だが、
これらの名前だけ使えば CAPL に近い書き味になる。

イベント機構は on_timer（時間駆動のポーリング）に加え、capl_dsl.py の
`on message`（受信フレームトリガ）が try_recv() を使う。wait_response() は
内部で uds_link.receive_uds_response() のブロッキング受信ループを使っており、
応答 ID 以外のフレームは本来そのループ内で読み捨てられてしまう。これだと
wait_response() が待っている間に届いた `on message` 対象のフレームが
ディスパッチされる前に失われるため、そうした「応答 ID 以外のフレーム」は
_unclaimed_frames に退避してから try_recv() 側で拾えるようにしてある。

try_recv() は自前で bus.recv() せず、app.py の _rx_monitor_worker がバスから
読み取ったフレームをファンアウトする message_queue を消費する側に回っている。
_rx_monitor_worker は Connect 中ずっと動き続ける GUI 側の RX モニタ表示用ワーカーで
あり、これと try_recv() が両方とも独自に bus.recv() すると、同じフレームを
奪い合ってどちらかが（あるいは両方が）取りこぼす競合状態になるため
（bus_lock は排他は保証するが、フレームの「どちらが受け取るか」までは
決めてくれない）。
"""
from __future__ import annotations

import collections
import queue
import threading
import time
from typing import Callable, Optional

import uds_link


class ScriptAbort(Exception):
    """assert_positive/assert_negative 失敗や応答タイムアウトでスクリプトを中断する。"""


class ScriptStopped(Exception):
    """GUI の「停止」ボタンで外部から停止要求された。"""


def _hex(data: bytes) -> str:
    return " ".join(f"{b:02X}" for b in data)


class CaplContext:
    """スクリプト内から使う CAPL 風関数群。1回のスクリプト実行につき1つ生成する。"""

    def __init__(self, get_bus: Callable[[], Optional[object]], bus_lock: threading.Lock,
                 log_func: Callable[[str], None], stop_event: threading.Event,
                 message_queue: Optional["queue.Queue"] = None):
        self._get_bus = get_bus
        self._bus_lock = bus_lock
        self._log = log_func
        self._stop_event = stop_event
        self._message_queue = message_queue
        self._timers: list[list] = []  # [interval_s, func, next_fire] （in-place 更新するため list）
        self._last_response: Optional[uds_link.UdsResponse] = None
        # wait_response()/security_unlock() の受信ループが読み捨てた「応答 ID 以外」の
        # フレームを一時退避するバッファ。try_recv() が優先的にここから取り出す。
        # maxlen は on message 側が長時間ポーリングを止めても無限に溜め込まないための保険。
        self._unclaimed_frames: "collections.deque" = collections.deque(maxlen=32)

    @property
    def bus(self):
        """現在接続中の python-can Bus。app.py の他ワーカー (_periodic_can_worker 等)
        と同じく、呼ぶたびに GUI 側の self.bus を読み直すことで切断/再接続に追従する
        (コンストラクタで受け取った bus をずっと使い続けると、スクリプト実行中に
        切断→再接続された場合に古い/無効な bus へ送信し続けてしまう)。
        未接続の場合は ScriptStopped を送出する。"""
        bus = self._get_bus()
        if bus is None:
            raise ScriptStopped("接続が切断されました")
        return bus

    # ---- ログ ----
    def log(self, *args) -> None:
        self._log(" ".join(str(a) for a in args))

    # ---- 送信 ----
    def send(self, payload, request_id: int = uds_link.REQUEST_ID) -> None:
        """UDS 要求を送信する。payload は PCI 抜きの生バイト列 (list[int] 可)。
        7 バイト以内なら SF、超える場合は自動的に FF+CF (マルチフレーム) で送る
        (7 バイト境界の判定自体は uds_link.send_multiframe_request() 側が持っているため、
        ここでは重複させずにそのまま委譲する)。"""
        self._check_stop()
        data = bytes(payload)
        with self._bus_lock:
            uds_link.send_multiframe_request(self.bus, data, request_id)
        self._log("TX " + _hex(data))

    def send_can(self, can_id: int, data) -> None:
        """任意 CAN ID への生フレーム送信 (UDS 応答待ちなし)。"""
        self._check_stop()
        data = bytes(data)
        with self._bus_lock:
            uds_link.send_can_frame(self.bus, can_id, data)
        self._log(f"TX ID=0x{can_id:03X} " + _hex(data))

    # ---- 受信 ----
    def wait_response(self, timeout: float = 2.0) -> uds_link.UdsResponse:
        """UDS 応答を待って返す。タイムアウト時は ScriptAbort を送出する。
        待っている間に届いた応答 ID 以外のフレームは _unclaimed_frames に退避し、
        try_recv() (on message ディスパッチ) が後で拾えるようにする。"""
        self._check_stop()
        with self._bus_lock:
            try:
                resp = uds_link.receive_uds_response(
                    self.bus, timeout=timeout, on_other_frame=self._unclaimed_frames.append
                )
            except uds_link.UdsTimeoutError as exc:
                self._log(str(exc))
                raise ScriptAbort(str(exc)) from exc
        self._last_response = resp
        self._log("RX " + resp.describe())
        return resp

    def try_recv(self, timeout: float = 0.05):
        """1フレームを試験的に受信する (capl_dsl.py の `on message` ディスパッチ用)。
        フレームが届かなかった場合のみ None を返す。

        優先順位:
        1. wait_response() 等が退避しておいたフレーム (_unclaimed_frames)
        2. message_queue (app.py の _rx_monitor_worker がファンアウトしたフレーム)

        message_queue が渡されている場合、ここでは自前で bus.recv() しない。
        _rx_monitor_worker と両方が独自に bus.recv() すると、同じフレームを
        奪い合ってどちらかが取りこぼす競合状態になるため（モジュール冒頭の
        docstring 参照）。message_queue が渡されていない場合 (直接 CaplContext を
        使う場合等) のみ、フォールバックとして自前で bus.recv() する。

        bus.recv() 自体は python-can の仕様上「タイムアウトなら None を返す」
        （receive_uds_response() 等が None を timeout の意味で扱っているのと同じ）。
        以前はここで例外まで一律 None に握りつぶしていたため、CAN アダプタが
        切断された等の実エラーが send()/wait_response() と違ってログにも出ず、
        デッドなバスに対して無限ポーリングし続けてしまっていた。例外は素通しし、
        呼び出し元 (Interpreter → _script_worker の except Exception) で
        他の失敗と同様に扱われるようにする。"""
        self._check_stop()
        if self._unclaimed_frames:
            return self._unclaimed_frames.popleft()
        if self._message_queue is not None:
            try:
                return self._message_queue.get(timeout=timeout)
            except queue.Empty:
                return None
        with self._bus_lock:
            bus = self.bus
            return bus.recv(timeout=timeout)

    # ---- アサーション ----
    def assert_positive(self, resp: Optional[uds_link.UdsResponse] = None) -> uds_link.UdsResponse:
        resp = resp if resp is not None else self._last_response
        if resp is None or resp.is_negative:
            msg = f"assert_positive 失敗: {resp.describe() if resp else '(no response)'}"
            self._log(msg)
            raise ScriptAbort(msg)
        return resp

    def assert_negative(self, resp: Optional[uds_link.UdsResponse] = None,
                         nrc: Optional[int] = None) -> uds_link.UdsResponse:
        resp = resp if resp is not None else self._last_response
        if resp is None or not resp.is_negative:
            msg = f"assert_negative 失敗: {resp.describe() if resp else '(no response)'}"
            self._log(msg)
            raise ScriptAbort(msg)
        if nrc is not None and resp.nrc != nrc:
            # resp.nrc は NRC バイトを欠いた不正な2バイト応答 (7F <SID> のみ) の場合 None に
            # なりうる。素直に f"{resp.nrc:02X}" とすると None を :02X でフォーマットしようと
            # して TypeError になり、意図した ScriptAbort ではなく未処理例外で落ちてしまう。
            actual = f"0x{resp.nrc:02X}" if resp.nrc is not None else "(none)"
            msg = f"assert_negative 失敗: NRC={actual} (期待値 0x{nrc:02X})"
            self._log(msg)
            raise ScriptAbort(msg)
        return resp

    # ---- SecurityAccess ----
    def security_unlock(self, key_mask: int = uds_link.SECURITY_KEY_MASK,
                         timeout: float = 2.0) -> str:
        """SecurityAccess の seed→key 自動計算を実行する。内部の requestSeed/sendKey
        応答待ちがタイムアウトした場合も、wait_response() と同様に ScriptAbort に
        変換する (uds_link.security_access_auto() は UdsTimeoutError を捕捉せず
        そのまま送出するため、ここで変換しないと _script_worker 側の汎用 except に
        落ちて「エラー:」表示になり、他のタイムアウトの「中断:」表示と一貫しなくなる)。"""
        self._check_stop()
        with self._bus_lock:
            try:
                result = uds_link.security_access_auto(
                    self.bus, key_mask, timeout, on_other_frame=self._unclaimed_frames.append
                )
            except uds_link.UdsTimeoutError as exc:
                self._log(str(exc))
                raise ScriptAbort(str(exc)) from exc
        self._log(result)
        return result

    # ---- 待機・イベント ----
    def wait(self, seconds: float) -> None:
        """seconds 秒待つ。その間 on_timer で登録済みのイベントもポーリングし続ける
        (単純な time.sleep() だとタイマーイベントが発火しなくなるため)。
        スクリプトの最後で待受状態にする用途にも使う（CAPL の event loop 相当）。"""
        deadline = time.monotonic() + seconds
        poll_interval = 0.05
        while True:
            self._check_stop()
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return
            self._poll_timers()
            time.sleep(min(poll_interval, remaining))

    def on_timer(self, interval_s: float):
        """`@ctx.on_timer(1.0)` で interval_s 秒毎に呼ばれる関数を登録するデコレータ。
        登録した関数は wait() の実行中にのみポーリングされる
        (CAPL の `on timer` と同様、明示的な待受中でないと発火しない)。"""
        def decorator(func):
            self._timers.append([interval_s, func, time.monotonic() + interval_s])
            return func
        return decorator

    def _poll_timers(self) -> None:
        now = time.monotonic()
        for timer in self._timers:
            interval_s, func, next_fire = timer
            if now >= next_fire:
                timer[2] = now + interval_s
                func(self)

    def _check_stop(self) -> None:
        if self._stop_event.is_set():
            raise ScriptStopped("スクリプトが停止されました")


def run_script(source: str, script_path: str, get_bus: Callable[[], Optional[object]],
               bus_lock: threading.Lock, log_func: Callable[[str], None],
               stop_event: threading.Event,
               message_queue: Optional["queue.Queue"] = None) -> None:
    """スクリプトファイルの内容 (source) を exec() する。

    get_bus は現在接続中の python-can Bus (未接続なら None) を返す呼び出し可能
    オブジェクト。固定の bus インスタンスではなく取得用の callable を渡すのは、
    スクリプト実行中に GUI 側で切断/再接続されても常に最新の bus を使わせるため
    (app.py の他ワーカーが self.bus を毎回読み直すのと同じ理由)。

    message_queue は capl_dsl.run_dsl_script() と引数の並びを揃えるためのもので、
    Python 版スクリプトでは `ctx.try_recv()` を明示的に呼ばない限り使われない。

    グローバル名前空間に CaplContext の主要メソッドを直接ぶら下げるため、
    スクリプト側は `ctx.send(...)` ではなく `send(...)` とだけ書ける
    （`on_timer` はデコレータ登録時に `ctx` 自体を束縛したいことが多いため、
    こちらは `ctx.on_timer(...)` の形のみ提供する）。
    """
    ctx = CaplContext(get_bus, bus_lock, log_func, stop_event, message_queue)
    script_globals = {
        "__name__": "__capl_script__",
        "__file__": script_path,
        "ctx": ctx,
        "send": ctx.send,
        "send_can": ctx.send_can,
        "wait_response": ctx.wait_response,
        "assert_positive": ctx.assert_positive,
        "assert_negative": ctx.assert_negative,
        "security_unlock": ctx.security_unlock,
        "wait": ctx.wait,
        "log": ctx.log,
        "NRC_NAMES": uds_link.NRC_NAMES,
        "DID_NAMES": uds_link.DID_NAMES,
        "DTC_NAMES": uds_link.DTC_NAMES,
    }
    code = compile(source, script_path, "exec")
    exec(code, script_globals)
