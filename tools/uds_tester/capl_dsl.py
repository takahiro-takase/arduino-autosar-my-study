"""
CAPL 風 DSL (`.capl` スクリプト) の字句解析・構文解析・インタプリタ。

capl_api.py の Python API 層 (中間案) に対する「本格対応」案。設計合意時のメモ通り、
UDS 送受信の実体は capl_api.CaplContext をそのままランタイム層として再利用する
(パーサが構文木を解釈して最終的に呼ぶのが CaplContext のメソッドになる)。

対応する構文は最小構成:

    on start { ... }
    on timer <name> { ... }
    on message <id> { ... }

ブロック内は関数呼び出し文の並びのみ (if/while 等の制御構文、変数宣言はない)。
利用できる関数は BUILTIN_NAMES を参照 (README.md にも一覧表がある)。

`.py` の exec() ベースのスクリプト実行 (capl_api.run_script()) は本モジュールとは
独立に残っており、app.py がファイル拡張子で自動的にどちらを使うか切り替える。
"""
from __future__ import annotations

import re
import time
from dataclasses import dataclass, field
from typing import Optional, Union

import capl_api

ArgValue = Union[int, float, str]


class DslSyntaxError(Exception):
    """DSL のパースエラー。可能な限り行番号を含める。"""


# ---------------------------------------------------------------------------
# 字句解析
# ---------------------------------------------------------------------------

_TOKEN_SPEC = [
    ("COMMENT_LINE", r"//[^\n]*"),
    ("COMMENT_BLOCK", r"/\*.*?\*/"),
    ("WS", r"[ \t\r\n]+"),
    ("HEXNUM", r"0[xX][0-9a-fA-F]+"),
    ("FLOAT", r"\d+\.\d+"),
    ("INT", r"\d+"),
    ("STRING", r'"(?:[^"\\]|\\.)*"'),
    ("IDENT", r"[A-Za-z_][A-Za-z0-9_]*"),
    ("LBRACE", r"\{"),
    ("RBRACE", r"\}"),
    ("LPAREN", r"\("),
    ("RPAREN", r"\)"),
    ("COMMA", r","),
    ("SEMI", r";"),
]
_MASTER_RE = re.compile(
    "|".join(f"(?P<{name}>{pattern})" for name, pattern in _TOKEN_SPEC), re.DOTALL
)


@dataclass
class Token:
    kind: str
    value: str
    line: int


def tokenize(source: str) -> list[Token]:
    tokens: list[Token] = []
    pos = 0
    line = 1
    while pos < len(source):
        m = _MASTER_RE.match(source, pos)
        if not m:
            raise DslSyntaxError(f"{line}行目: 不正な文字 {source[pos]!r}")
        kind = m.lastgroup
        value = m.group()
        if kind in ("WS", "COMMENT_LINE", "COMMENT_BLOCK"):
            line += value.count("\n")
            pos = m.end()
            continue
        tokens.append(Token(kind, value, line))
        line += value.count("\n")
        pos = m.end()
    tokens.append(Token("EOF", "", line))
    return tokens


# ---------------------------------------------------------------------------
# 構文解析 (AST は関数呼び出し文の並びのみ)
# ---------------------------------------------------------------------------

@dataclass
class Call:
    name: str
    args: list  # list[ArgValue | Call]
    line: int


@dataclass
class Script:
    on_start: list  # list[list[Call]]  (複数の `on start` は順に実行)
    on_timer: dict  # name(str) -> list[Call]
    on_message: dict  # can_id(int) -> list[Call]


class _Parser:
    def __init__(self, tokens: list[Token]):
        self._tokens = tokens
        self._pos = 0

    def _peek(self, offset: int = 0) -> Token:
        idx = min(self._pos + offset, len(self._tokens) - 1)
        return self._tokens[idx]

    def _advance(self) -> Token:
        tok = self._tokens[self._pos]
        self._pos += 1
        return tok

    def _expect(self, kind: str) -> Token:
        tok = self._peek()
        if tok.kind != kind:
            raise DslSyntaxError(
                f"{tok.line}行目: {kind} を期待しましたが '{tok.value}' でした"
            )
        return self._advance()

    def _expect_ident(self, value: str) -> Token:
        tok = self._peek()
        if tok.kind != "IDENT" or tok.value != value:
            raise DslSyntaxError(
                f"{tok.line}行目: キーワード '{value}' を期待しましたが '{tok.value}' でした"
            )
        return self._advance()

    def parse(self) -> Script:
        on_start: list = []
        on_timer: dict = {}
        on_message: dict = {}
        while self._peek().kind != "EOF":
            self._expect_ident("on")
            kind_tok = self._advance()
            if kind_tok.kind != "IDENT":
                raise DslSyntaxError(
                    f"{kind_tok.line}行目: on の後には start/timer/message が必要です"
                )
            if kind_tok.value == "start":
                on_start.append(self._parse_block())
            elif kind_tok.value == "timer":
                name_tok = self._expect("IDENT")
                on_timer[name_tok.value] = self._parse_block()
            elif kind_tok.value == "message":
                id_tok = self._advance()
                if id_tok.kind not in ("HEXNUM", "INT"):
                    raise DslSyntaxError(
                        f"{id_tok.line}行目: on message の後には CAN ID (数値) が必要です"
                    )
                can_id = int(id_tok.value, 0)
                on_message[can_id] = self._parse_block()
            else:
                raise DslSyntaxError(
                    f"{kind_tok.line}行目: 未対応のイベント種別 '{kind_tok.value}' "
                    "(start/timer/message のみ対応)"
                )
        return Script(on_start, on_timer, on_message)

    def _parse_block(self) -> list:
        self._expect("LBRACE")
        stmts = []
        while self._peek().kind != "RBRACE":
            stmts.append(self._parse_statement())
        self._expect("RBRACE")
        return stmts

    def _parse_statement(self) -> Call:
        call = self._parse_call()
        self._expect("SEMI")
        return call

    def _parse_call(self) -> Call:
        name_tok = self._expect("IDENT")
        self._expect("LPAREN")
        args = []
        if self._peek().kind != "RPAREN":
            args.append(self._parse_arg())
            while self._peek().kind == "COMMA":
                self._advance()
                args.append(self._parse_arg())
        self._expect("RPAREN")
        return Call(name_tok.value, args, name_tok.line)

    def _parse_arg(self):
        tok = self._peek()
        if tok.kind == "HEXNUM":
            self._advance()
            return int(tok.value, 16)
        if tok.kind == "INT":
            self._advance()
            return int(tok.value)
        if tok.kind == "FLOAT":
            self._advance()
            return float(tok.value)
        if tok.kind == "STRING":
            self._advance()
            return tok.value[1:-1].replace('\\"', '"').replace("\\\\", "\\")
        if tok.kind == "IDENT":
            # 次が '(' ならネストした関数呼び出し (例: log("byte0=", msgData(0)))。
            # そうでなければ setTimer(myTimer, 1000) の myTimer のような裸の識別子 (名前) として扱う。
            if self._peek(1).kind == "LPAREN":
                return self._parse_call()
            self._advance()
            return tok.value
        raise DslSyntaxError(f"{tok.line}行目: 引数として不正なトークン '{tok.value}'")


def parse(source: str) -> Script:
    return _Parser(tokenize(source)).parse()


# ---------------------------------------------------------------------------
# インタプリタ
# ---------------------------------------------------------------------------

class Interpreter:
    """パース済み Script を実行する。UDS 送受信は capl_api.CaplContext に委譲する。"""

    def __init__(self, script: Script, ctx: capl_api.CaplContext, stop_event):
        self._script = script
        self._ctx = ctx
        self._stop_event = stop_event
        self._armed_timers: dict[str, float] = {}  # name -> 発火時刻 (time.monotonic())
        self._last_message: Optional[object] = None  # can.Message (on message ハンドラ内でのみ有効)
        self._builtins = self._make_builtins()

    def _make_builtins(self) -> dict:
        ctx = self._ctx
        return {
            "send": lambda args: ctx.send([self._to_byte(a) for a in args]),
            "send_can": lambda args: ctx.send_can(
                self._to_int(args[0]), [self._to_byte(a) for a in args[1:]]
            ),
            "wait_response": lambda args: ctx.wait_response(
                float(args[0]) if args else 2.0
            ),
            "assert_positive": lambda args: ctx.assert_positive(),
            "assert_negative": lambda args: ctx.assert_negative(
                nrc=self._to_int(args[0]) if args else None
            ),
            "security_unlock": lambda args: ctx.security_unlock(),
            "wait": lambda args: ctx.wait(float(args[0])),
            "log": lambda args: ctx.log(*args),
            "write": lambda args: ctx.log(*args),  # CAPL の write() 相当のエイリアス
            "setTimer": lambda args: self._set_timer(str(args[0]), float(args[1])),
            "cancelTimer": lambda args: self._armed_timers.pop(str(args[0]), None),
            "msgData": lambda args: self._msg_data(self._to_int(args[0])),
            "msgId": lambda args: (
                self._last_message.arbitration_id if self._last_message else 0
            ),
            "msgDlc": lambda args: (
                len(self._last_message.data) if self._last_message else 0
            ),
        }

    @staticmethod
    def _to_int(value: ArgValue) -> int:
        return int(value)

    @staticmethod
    def _to_byte(value: ArgValue) -> int:
        return int(value) & 0xFF

    def _msg_data(self, index: int) -> int:
        if self._last_message is None or index >= len(self._last_message.data):
            return 0
        return self._last_message.data[index]

    def _set_timer(self, name: str, ms: float) -> None:
        self._armed_timers[name] = time.monotonic() + ms / 1000.0

    # ---- 式/文の評価 ----
    def _eval(self, node):
        if isinstance(node, Call):
            return self._call(node)
        return node  # リテラル (int/float/str) はそのまま

    def _call(self, call: Call):
        fn = self._builtins.get(call.name)
        if fn is None:
            raise DslSyntaxError(
                f"{call.line}行目: 未知の関数 '{call.name}' "
                "(send/send_can/wait_response/assert_positive/assert_negative/"
                "security_unlock/wait/log/write/setTimer/cancelTimer/msgData/msgId/msgDlc のみ対応)"
            )
        args = [self._eval(a) for a in call.args]
        return fn(args)

    def _run_block(self, stmts: list) -> None:
        for stmt in stmts:
            self._call(stmt)

    # ---- 実行本体 ----
    def run(self) -> None:
        """on start を順に実行したのち、on timer/on message が1つでも定義されていれば
        stop_event がセットされるまでポーリングし続ける (CAPL のプログラムが測定終了まで
        動き続けるのと同じ考え方)。何も定義されていなければ on start だけで完了する。"""
        for stmts in self._script.on_start:
            self._run_block(stmts)

        if not self._script.on_timer and not self._script.on_message:
            return

        while True:
            if self._stop_event.is_set():
                raise capl_api.ScriptStopped("スクリプトが停止されました")
            self._poll_timers()
            msg = self._ctx.try_recv(0.05)
            if msg is not None:
                self._last_message = msg
                handler = self._script.on_message.get(msg.arbitration_id)
                if handler is not None:
                    self._run_block(handler)
            else:
                time.sleep(0.02)

    def _poll_timers(self) -> None:
        now = time.monotonic()
        due = [name for name, deadline in self._armed_timers.items() if now >= deadline]
        for name in due:
            # 単発 (one-shot)。CAPL の msTimer と同じく、繰り返すにはハンドラ内で
            # 再度 setTimer() を呼ぶ必要がある。
            del self._armed_timers[name]
            handler = self._script.on_timer.get(name)
            if handler is not None:
                self._run_block(handler)


def run_dsl_script(source: str, script_path: str, get_bus, bus_lock,
                    log_func, stop_event) -> None:
    """`.capl` スクリプトを実行する。capl_api.run_script() の DSL 版で、app.py からは
    ファイル拡張子で自動的にどちらを呼ぶか切り替えられる (同じ引数の並びにしてある)。"""
    script = parse(source)  # DslSyntaxError は呼び出し側 (_script_worker) の except で表示される
    ctx = capl_api.CaplContext(get_bus, bus_lock, log_func, stop_event)
    Interpreter(script, ctx, stop_event).run()
