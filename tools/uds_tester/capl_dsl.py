"""
CAPL 風 DSL (`.capl` スクリプト) の字句解析・構文解析・インタプリタ。

capl_api.py の Python API 層 (中間案) に対する「本格対応」案。設計合意時のメモ通り、
UDS 送受信の実体は capl_api.CaplContext をそのままランタイム層として再利用する
(パーサが構文木を解釈して最終的に呼ぶのが CaplContext のメソッドになる)。

対応する構文:

    variables { int x; float y = 1.5; }   // 省略可。1ファイルに1つまで、on start/
                                           // on timer/on message より前に書くこと
    on start { ... }
    on timer <name> { ... }
    on message <id> { ... }

ブロック内は以下の文が書ける:
    - 関数呼び出し文: send(...); / write(...); など
    - 代入文: x = x + 1;  （x は variables{} で宣言済みであること）
    - if (expr) { ... } [else if (expr) { ... }]* [else { ... }]
    - while (expr) { ... }

式は四則演算 (+ - * / %)・比較 (== != < > <= >=)・論理 (&& || !)・丸括弧・
関数呼び出し・変数参照・数値/文字列リテラルに対応する（if/while の演算だけを
サポートし、for 文や配列、構造体、ユーザー定義関数は対象外）。

利用できる関数は Interpreter._make_builtins() を参照 (README.md にも一覧表がある)。
未知の関数呼び出し・未宣言の変数参照は Interpreter 構築時 (実行前) に全ブロックを
走査して検出する (_validate() 参照)。on start の実行後に初めて呼ばれる on timer/
on message の中身でタイポがあっても、on start の副作用（セッション変更等）を実行
してしまった後になって気付く、という事態を避けるため。

setTimer(name, ms)/cancelTimer(name) の第1引数 (タイマー名) は特別扱いする。
CAPL の msTimer 変数と違い、本 DSL では variables{} での事前宣言を要求せず、
識別子の綴りそのものをタイマー名として使う（on timer <name> ブロックとの対応も
同じ綴りで取る）。real な変数参照 (int/float) と紛れないよう、Interpreter 側で
setTimer/cancelTimer の第1引数だけは評価も宣言チェックもしない。

`.py` の exec() ベースのスクリプト実行 (capl_api.run_script()) は本モジュールとは
独立に残っており、app.py がファイル拡張子で自動的にどちらを使うか切り替える。
"""
from __future__ import annotations

import re
import time
from dataclasses import dataclass
from typing import Optional, Union

import capl_api

ArgValue = Union[int, float, str]


class DslSyntaxError(Exception):
    """DSL のパースエラー・実行前検証エラー。可能な限り行番号を含める。"""


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
    # 2文字演算子は対応する1文字演算子より前に置くこと (先勝ちマッチのため)。
    ("EQ", r"=="),
    ("NEQ", r"!="),
    ("LE", r"<="),
    ("GE", r">="),
    ("AND", r"&&"),
    ("OR", r"\|\|"),
    ("ASSIGN", r"="),
    ("NOT", r"!"),
    ("LT", r"<"),
    ("GT", r">"),
    ("PLUS", r"\+"),
    ("MINUS", r"-"),
    ("STAR", r"\*"),
    ("SLASH", r"/"),
    ("PERCENT", r"%"),
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
# 構文解析
# ---------------------------------------------------------------------------

_VAR_TYPES = ("int", "float")
_COMPARISON_TOKENS = {"LT": "<", "GT": ">", "LE": "<=", "GE": ">="}
_ADDITIVE_TOKENS = {"PLUS": "+", "MINUS": "-"}
_MULTIPLICATIVE_TOKENS = {"STAR": "*", "SLASH": "/", "PERCENT": "%"}

# setTimer(name, ms)/cancelTimer(name) の第1引数 (タイマー名) は特別扱いする。パーサーが
# ここでその判断を一箇所に集約し、専用の TimerName ノードを作る (下記参照)。こうする
# ことで、インタプリタ側の _call()/_validate_expr() は「この関数の第1引数だけ特別扱い」
# という条件を各所で再判定する必要がなく、単に isinstance(node, TimerName) を見るだけで
# 済む (以前は同じ条件が _call() と _validate_expr() の2箇所にコピーされており、
# 片方だけ更新されるとズレる保守リスクがあった)。
_TIMER_NAME_FUNCS = ("setTimer", "cancelTimer")


@dataclass
class Call:
    name: str
    args: list  # list[Expr]
    line: int


@dataclass
class Var:
    """変数参照式 (int/float、variables{} での宣言が必要)。"""
    name: str
    line: int


@dataclass
class TimerName:
    """setTimer(name, ms)/cancelTimer(name) の第1引数専用のノード。CAPL の msTimer 型
    変数と違い、識別子の綴りそのものがタイマー名になる (on timer <name> ブロックとの
    対応もこの綴りで取る)。int/float 変数のように宣言して値を持つものではないため、
    Var とは別の型にして混同を防ぐ。"""
    name: str
    line: int


@dataclass
class BinOp:
    op: str  # "+" "-" "*" "/" "%" "==" "!=" "<" ">" "<=" ">=" "&&" "||"
    left: object  # Expr
    right: object  # Expr
    line: int


@dataclass
class UnaryOp:
    op: str  # "-" "!"
    operand: object  # Expr
    line: int


@dataclass
class VarDecl:
    type_name: str  # "int" | "float"
    name: str
    init: object  # Expr | None
    line: int


@dataclass
class Assign:
    name: str
    expr: object  # Expr
    line: int


@dataclass
class If:
    cond: object  # Expr
    then_block: list  # list[Stmt]
    else_block: Optional[list]  # else if は [If(...)] という1要素のリストで表現する
    line: int


@dataclass
class While:
    cond: object  # Expr
    body: list  # list[Stmt]
    line: int


@dataclass
class Script:
    variables: list  # list[VarDecl]  (`variables { ... }` ブロック。無ければ空)
    on_start: list  # list[list[Stmt]]  (複数の `on start` は順に実行)
    on_timer: dict  # name(str) -> list[Stmt]
    on_message: dict  # can_id(int) -> list[Stmt]


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

    def _at_ident(self, value: str) -> bool:
        tok = self._peek()
        return tok.kind == "IDENT" and tok.value == value

    # ---- トップレベル ----
    def parse(self) -> Script:
        variables: list = []
        var_block_seen = False
        on_block_seen = False
        on_start: list = []
        on_timer: dict = {}
        on_message: dict = {}
        while self._peek().kind != "EOF":
            if self._at_ident("variables"):
                tok = self._peek()
                if var_block_seen:
                    raise DslSyntaxError(
                        f"{tok.line}行目: variables ブロックは1ファイルに1つまでです"
                    )
                if on_block_seen:
                    raise DslSyntaxError(
                        f"{tok.line}行目: variables ブロックは on start/on timer/"
                        "on message より前に書いてください"
                    )
                var_block_seen = True
                self._advance()
                variables.extend(self._parse_var_block())
                continue

            on_block_seen = True
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
        return Script(variables, on_start, on_timer, on_message)

    def _parse_var_block(self) -> list:
        self._expect("LBRACE")
        decls = []
        while self._peek().kind != "RBRACE":
            decls.append(self._parse_var_decl())
        self._expect("RBRACE")
        return decls

    def _parse_var_decl(self) -> VarDecl:
        type_tok = self._peek()
        if type_tok.kind != "IDENT" or type_tok.value not in _VAR_TYPES:
            raise DslSyntaxError(
                f"{type_tok.line}行目: 変数の型 (int/float) を期待しましたが "
                f"'{type_tok.value}' でした"
            )
        self._advance()
        name_tok = self._expect("IDENT")
        init = None
        if self._peek().kind == "ASSIGN":
            self._advance()
            init = self._parse_expr()
        self._expect("SEMI")
        return VarDecl(type_tok.value, name_tok.value, init, name_tok.line)

    # ---- 文 ----
    def _parse_block(self) -> list:
        self._expect("LBRACE")
        stmts = []
        while self._peek().kind != "RBRACE":
            stmts.append(self._parse_statement())
        self._expect("RBRACE")
        return stmts

    def _parse_statement(self):
        if self._at_ident("if"):
            return self._parse_if()
        if self._at_ident("while"):
            return self._parse_while()
        tok = self._peek()
        if tok.kind == "IDENT" and self._peek(1).kind == "ASSIGN":
            return self._parse_assignment()
        if tok.kind == "IDENT" and self._peek(1).kind == "LPAREN":
            call = self._parse_call()
            self._expect("SEMI")
            return call
        raise DslSyntaxError(f"{tok.line}行目: 文として不正なトークン '{tok.value}'")

    def _parse_if(self) -> If:
        if_tok = self._expect_ident("if")
        self._expect("LPAREN")
        cond = self._parse_expr()
        self._expect("RPAREN")
        then_block = self._parse_block()
        else_block = None
        if self._at_ident("else"):
            self._advance()
            if self._at_ident("if"):
                else_block = [self._parse_if()]
            else:
                else_block = self._parse_block()
        return If(cond, then_block, else_block, if_tok.line)

    def _parse_while(self) -> While:
        while_tok = self._expect_ident("while")
        self._expect("LPAREN")
        cond = self._parse_expr()
        self._expect("RPAREN")
        body = self._parse_block()
        return While(cond, body, while_tok.line)

    def _parse_assignment(self) -> Assign:
        name_tok = self._expect("IDENT")
        self._expect("ASSIGN")
        expr = self._parse_expr()
        self._expect("SEMI")
        return Assign(name_tok.value, expr, name_tok.line)

    def _parse_call(self) -> Call:
        name_tok = self._expect("IDENT")
        self._expect("LPAREN")
        args = []
        if self._peek().kind != "RPAREN":
            args.append(self._parse_call_arg(name_tok.value, is_first=True))
            while self._peek().kind == "COMMA":
                self._advance()
                args.append(self._parse_call_arg(name_tok.value, is_first=False))
        self._expect("RPAREN")
        return Call(name_tok.value, args, name_tok.line)

    def _parse_call_arg(self, func_name: str, is_first: bool):
        """通常は式として引数を読むが、setTimer/cancelTimer の第1引数が裸の識別子
        (関数呼び出しではない) の場合だけ TimerName ノードにする。この特別扱いを
        パーサーのここ1箇所に閉じ込めることで、インタプリタ側は isinstance 判定だけで
        済み、「setTimer/cancelTimer かつ第1引数かどうか」を各所で再判定しなくてよい。"""
        if (
            is_first
            and func_name in _TIMER_NAME_FUNCS
            and self._peek().kind == "IDENT"
            and self._peek(1).kind != "LPAREN"
        ):
            tok = self._advance()
            return TimerName(tok.value, tok.line)
        return self._parse_expr()

    # ---- 式 (優先順位: || > && > ==,!= > <,>,<=,>= > +,- > *,/,% > 単項 > 一次) ----
    def _parse_expr(self):
        return self._parse_or()

    def _parse_or(self):
        left = self._parse_and()
        while self._peek().kind == "OR":
            op_tok = self._advance()
            right = self._parse_and()
            left = BinOp("||", left, right, op_tok.line)
        return left

    def _parse_and(self):
        left = self._parse_equality()
        while self._peek().kind == "AND":
            op_tok = self._advance()
            right = self._parse_equality()
            left = BinOp("&&", left, right, op_tok.line)
        return left

    def _parse_equality(self):
        left = self._parse_comparison()
        while self._peek().kind in ("EQ", "NEQ"):
            op_tok = self._advance()
            op = "==" if op_tok.kind == "EQ" else "!="
            right = self._parse_comparison()
            left = BinOp(op, left, right, op_tok.line)
        return left

    def _parse_comparison(self):
        left = self._parse_additive()
        while self._peek().kind in _COMPARISON_TOKENS:
            op_tok = self._advance()
            right = self._parse_additive()
            left = BinOp(_COMPARISON_TOKENS[op_tok.kind], left, right, op_tok.line)
        return left

    def _parse_additive(self):
        left = self._parse_multiplicative()
        while self._peek().kind in _ADDITIVE_TOKENS:
            op_tok = self._advance()
            right = self._parse_multiplicative()
            left = BinOp(_ADDITIVE_TOKENS[op_tok.kind], left, right, op_tok.line)
        return left

    def _parse_multiplicative(self):
        left = self._parse_unary()
        while self._peek().kind in _MULTIPLICATIVE_TOKENS:
            op_tok = self._advance()
            right = self._parse_unary()
            left = BinOp(_MULTIPLICATIVE_TOKENS[op_tok.kind], left, right, op_tok.line)
        return left

    def _parse_unary(self):
        tok = self._peek()
        if tok.kind == "MINUS":
            self._advance()
            return UnaryOp("-", self._parse_unary(), tok.line)
        if tok.kind == "NOT":
            self._advance()
            return UnaryOp("!", self._parse_unary(), tok.line)
        return self._parse_primary()

    def _parse_primary(self):
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
        if tok.kind == "LPAREN":
            self._advance()
            expr = self._parse_expr()
            self._expect("RPAREN")
            return expr
        if tok.kind == "IDENT":
            if self._peek(1).kind == "LPAREN":
                return self._parse_call()
            self._advance()
            return Var(tok.value, tok.line)
        raise DslSyntaxError(f"{tok.line}行目: 式として不正なトークン '{tok.value}'")


def parse(source: str) -> Script:
    return _Parser(tokenize(source)).parse()


# ---------------------------------------------------------------------------
# インタプリタ
# ---------------------------------------------------------------------------


def _iter_child_exprs(node):
    """BinOp/UnaryOp の子ノード (部分式) を列挙する。Call/Var/TimerName/リテラルは
    子を持たないので空を返す。_reject_calls()/_validate_expr() の再帰下降で
    「BinOp なら left/right、UnaryOp なら operand を辿る」という同じ形の走査を
    2箇所に個別に書かないよう、ここに1本化する。"""
    if isinstance(node, BinOp):
        return (node.left, node.right)
    if isinstance(node, UnaryOp):
        return (node.operand,)
    return ()


@dataclass
class _Variable:
    """宣言済み変数の実行時状態。型 (代入時の型変換に使う) と現在値を1つにまとめて
    持つ (別々の dict で持つと書き込み箇所ごとに2つの dict を同期させる必要がある)。"""
    type_name: str  # "int" | "float"
    value: object


class Interpreter:
    """パース済み Script を実行する。UDS 送受信は capl_api.CaplContext に委譲する。"""

    def __init__(self, script: Script, ctx: capl_api.CaplContext, stop_event):
        self._script = script
        self._ctx = ctx
        self._stop_event = stop_event
        self._armed_timers: dict[str, float] = {}  # name -> 発火時刻 (time.monotonic())
        self._last_message: Optional[object] = None  # can.Message (on message ハンドラ内でのみ有効)
        self._vars: dict[str, _Variable] = {}
        self._builtins = self._make_builtins()
        # variables{} の宣言を先に検証・評価してから、on start/on timer/on message を
        # 検証する (代入・変数参照が「宣言済みかどうか」を検証する際に self._vars が要る
        # ため)。_init_variables() 内で、初期値式に関数呼び出しが含まれていないことを
        # 確認してから評価するので (variables{} の初期値式は定数式のみ許可、実際の CAPL の
        # variables{} も定数式のみ)、ここでの評価に副作用はない。関数呼び出しを許してしまうと、
        # on start/on timer/on message 側の検証が完了する前に (例えば send() のような)
        # 副作用のある呼び出しが実行されてしまい、"検証完了前は何も実行しない" という
        # _validate() の前提が崩れてしまう。
        self._init_variables()
        # on start だけでなく on timer/on message の中身も含め、実行前に全ブロックを
        # 検証する。実行時の遅延チェックだけだと、on timer/on message 内のタイポ・
        # 未宣言変数は実際にそのイベントが発火するまで検出されない。そうすると on start
        # の副作用のある処理 (セッション変更、SecurityAccess のアンロック等) を実行し
        # 終えた後になってようやく判明する、という事態になりかねないため。
        self._validate()

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
            # ctx.wait() ではなく self.wait() (Interpreter 自身の待機ループ) を使う。
            # ctx.wait() は Python 版 (@ctx.on_timer) の _timers しか見ておらず、DSL 側の
            # _armed_timers(setTimer)/on_message ディスパッチとは無関係なため、そちらに
            # 委譲すると wait() 中は setTimer タイマーも on message も止まってしまう。
            "wait": lambda args: self.wait(float(args[0])),
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

    @staticmethod
    def _truthy(value) -> bool:
        return bool(value)

    def _msg_data(self, index: int) -> int:
        if self._last_message is None or index >= len(self._last_message.data):
            return 0
        return self._last_message.data[index]

    def _set_timer(self, name: str, ms: float) -> None:
        self._armed_timers[name] = time.monotonic() + ms / 1000.0

    # ---- variables{} の初期値式の検証・初期化 ----
    def _reject_calls(self, node) -> None:
        """variables{} の初期値式に関数呼び出しが含まれていないか再帰的に確認する。
        変数参照 (Var) は副作用がないので許可する (前方の宣言を参照できる)。"""
        if isinstance(node, Call):
            raise DslSyntaxError(
                f"{node.line}行目: variables の初期値に関数呼び出し '{node.name}(...)' は"
                "使えません (定数式のみ)"
            )
        for child in _iter_child_exprs(node):
            self._reject_calls(child)

    def _init_variables(self) -> None:
        """宣言順に初期値式を検証・評価して self._vars に格納する。後続の宣言の初期値式が
        先に宣言した変数を参照することもできる（宣言済みかどうかは self._vars への
        追加順でそのまま決まる）。"""
        for decl in self._script.variables:
            if decl.name in self._vars:
                raise DslSyntaxError(f"{decl.line}行目: 変数 '{decl.name}' は既に宣言されています")
            if decl.init is not None:
                self._reject_calls(decl.init)
                value = self._eval(decl.init)
            else:
                value = 0 if decl.type_name == "int" else 0.0
            self._vars[decl.name] = _Variable(decl.type_name, self._coerce(value, decl.type_name))

    @staticmethod
    def _coerce(value, type_name: str):
        """代入・初期化のたびに宣言型 (int/float) への変換を行う。C/CAPL の代入と同様、
        float を int 変数に代入すると 0 方向へ切り捨てられる (Python の int() は
        int(2.9)==2, int(-2.9)==-2 と C の (int) キャストと同じ丸め方向なのでそのまま使える)。
        これをやらないと `int half; half = 10 / 4;` が 2 ではなく 2.5 のまま half に
        入ってしまい、int 変数のつもりで書いた比較・分岐が期待通りに動かなくなる。"""
        if type_name == "int":
            return int(value)
        if type_name == "float":
            return float(value)
        return value

    # ---- 式の評価 ----
    def _eval(self, node):
        if isinstance(node, Call):
            return self._call(node)
        if isinstance(node, Var):
            if node.name not in self._vars:
                raise DslSyntaxError(f"{node.line}行目: 未宣言の変数 '{node.name}'")
            return self._vars[node.name].value
        if isinstance(node, TimerName):
            return node.name  # 識別子の綴りそのものがタイマー名
        if isinstance(node, BinOp):
            return self._eval_binop(node)
        if isinstance(node, UnaryOp):
            return self._eval_unaryop(node)
        return node  # リテラル (int/float/str)

    @staticmethod
    def _int_div(left: int, right: int) -> int:
        """C/CAPL の int 同士の割り算と同じ、0 方向への切り捨て除算。
        Python の `//` は負数で床方向 (負の無限大方向) に丸めるため単純に流用できない
        (例: -7 // 2 は Python では -4 だが C/CAPL では -3)。right==0 の場合は Python の
        ZeroDivisionError がそのまま送出される (他の実行時エラーと同様、特別なガードは
        入れず素通しする方針)。"""
        q = abs(left) // abs(right)
        return -q if (left < 0) != (right < 0) else q

    @staticmethod
    def _int_mod(left: int, right: int) -> int:
        """C/CAPL の int 同士の剰余と同じ、剰余の符号を被除数 (left) 側に揃える
        (Python の `%` は除数側の符号に揃うため、例えば -7 % 2 は Python では 1 だが
        C/CAPL では -1)。"""
        r = abs(left) % abs(right)
        return -r if left < 0 else r

    def _eval_binop(self, node: BinOp):
        if node.op == "&&":
            if not self._truthy(self._eval(node.left)):
                return 0
            return 1 if self._truthy(self._eval(node.right)) else 0
        if node.op == "||":
            if self._truthy(self._eval(node.left)):
                return 1
            return 1 if self._truthy(self._eval(node.right)) else 0

        left = self._eval(node.left)
        right = self._eval(node.right)
        if node.op == "+":
            return left + right
        if node.op == "-":
            return left - right
        if node.op == "*":
            return left * right
        if node.op == "/":
            try:
                if isinstance(left, int) and isinstance(right, int):
                    return self._int_div(left, right)
                return left / right
            except ZeroDivisionError as exc:
                # 生の ZeroDivisionError を素通しすると行番号のない未構造化な
                # 「エラー: division by zero」表示になり、他のランタイムエラー
                # (assert_positive 失敗・wait_response タイムアウト等が ScriptAbort に
                # 変換されて「中断:」表示になるの) と非対称になってしまう。
                raise capl_api.ScriptAbort(f"{node.line}行目: ゼロ除算です") from exc
        if node.op == "%":
            try:
                if isinstance(left, int) and isinstance(right, int):
                    return self._int_mod(left, right)
                return left % right
            except ZeroDivisionError as exc:
                raise capl_api.ScriptAbort(f"{node.line}行目: ゼロ除算です") from exc
        if node.op == "==":
            return 1 if left == right else 0
        if node.op == "!=":
            return 1 if left != right else 0
        if node.op == "<":
            return 1 if left < right else 0
        if node.op == ">":
            return 1 if left > right else 0
        if node.op == "<=":
            return 1 if left <= right else 0
        if node.op == ">=":
            return 1 if left >= right else 0
        raise DslSyntaxError(f"{node.line}行目: 未対応の演算子 '{node.op}'")

    def _eval_unaryop(self, node: UnaryOp):
        value = self._eval(node.operand)
        if node.op == "-":
            return -value
        if node.op == "!":
            return 0 if self._truthy(value) else 1
        raise DslSyntaxError(f"{node.line}行目: 未対応の単項演算子 '{node.op}'")

    def _call(self, call: Call):
        fn = self._builtins.get(call.name)
        if fn is None:
            raise self._unknown_function_error(call)
        # setTimer/cancelTimer の第1引数 (TimerName) も _eval() が name をそのまま
        # 返すので、ここで関数名による特別扱いは不要 (パーサーの _parse_call_arg() 参照)。
        args = [self._eval(a) for a in call.args]
        return fn(args)

    def _unknown_function_error(self, call: Call) -> DslSyntaxError:
        known = "/".join(sorted(self._builtins))
        return DslSyntaxError(f"{call.line}行目: 未知の関数 '{call.name}' ({known} のみ対応)")

    # ---- 文の実行 ----
    def _exec_stmt(self, stmt) -> None:
        if isinstance(stmt, Call):
            self._call(stmt)
        elif isinstance(stmt, Assign):
            var = self._vars[stmt.name]
            var.value = self._coerce(self._eval(stmt.expr), var.type_name)
        elif isinstance(stmt, If):
            if self._truthy(self._eval(stmt.cond)):
                self._run_block(stmt.then_block)
            elif stmt.else_block is not None:
                self._run_block(stmt.else_block)
        elif isinstance(stmt, While):
            while self._truthy(self._eval(stmt.cond)):
                if self._stop_event.is_set():
                    raise capl_api.ScriptStopped("スクリプトが停止されました")
                self._run_block(stmt.body)
        else:
            raise DslSyntaxError(f"未対応の文です: {stmt!r}")

    def _run_block(self, stmts: list) -> None:
        for stmt in stmts:
            self._exec_stmt(stmt)

    # ---- 実行前の静的検証 ----
    def _validate(self) -> None:
        """self._builtins/self._vars (実行時の状態そのもの、別リストとして二重管理
        しない) を情報源として、on start/on timer/on message の全ブロックを走査し、
        未知の関数呼び出し・未宣言の変数参照があれば実行前に DslSyntaxError を送出する。"""
        for stmts in self._script.on_start:
            self._validate_block(stmts)
        for stmts in self._script.on_timer.values():
            self._validate_block(stmts)
        for stmts in self._script.on_message.values():
            self._validate_block(stmts)

    def _validate_block(self, stmts: list) -> None:
        for stmt in stmts:
            self._validate_stmt(stmt)

    def _validate_stmt(self, stmt) -> None:
        if isinstance(stmt, Call):
            self._validate_expr(stmt)
        elif isinstance(stmt, Assign):
            if stmt.name not in self._vars:
                raise DslSyntaxError(
                    f"{stmt.line}行目: 未宣言の変数 '{stmt.name}' への代入です "
                    "(variables { int/float ...; } で宣言してください)"
                )
            self._validate_expr(stmt.expr)
        elif isinstance(stmt, If):
            self._validate_expr(stmt.cond)
            self._validate_block(stmt.then_block)
            if stmt.else_block is not None:
                self._validate_block(stmt.else_block)
        elif isinstance(stmt, While):
            self._validate_expr(stmt.cond)
            self._validate_block(stmt.body)
        else:
            raise DslSyntaxError(f"未対応の文です: {stmt!r}")

    def _validate_expr(self, node) -> None:
        if isinstance(node, Call):
            if node.name not in self._builtins:
                raise self._unknown_function_error(node)
            for arg in node.args:
                self._validate_expr(arg)
            return
        if isinstance(node, Var):
            if node.name not in self._vars:
                raise DslSyntaxError(
                    f"{node.line}行目: 未宣言の変数 '{node.name}' "
                    "(variables { int/float ...; } で宣言してください)"
                )
            return
        if isinstance(node, TimerName):
            return  # タイマー名は識別子そのものなので、宣言済み変数チェックの対象外
        for child in _iter_child_exprs(node):
            self._validate_expr(child)
        # リテラル (int/float/str) は子を持たないため何もしない

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
            self._poll_once(0.05)

    def wait(self, seconds: float) -> None:
        """DSL の `wait(seconds)` 文の実体。run() のイベントループと同じ _poll_once() を
        使うことで、待機中も setTimer タイマーの発火・on message ディスパッチが
        止まらないようにする (capl_api.CaplContext.wait() は Python 版専用の _timers
        しか見ないため、ここでは委譲しない)。"""
        deadline = time.monotonic() + seconds
        while True:
            if self._stop_event.is_set():
                raise capl_api.ScriptStopped("スクリプトが停止されました")
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return
            self._poll_once(min(0.05, remaining))

    def _poll_once(self, timeout: float) -> None:
        """タイマーを1回分チェックし、フレームを1件だけ試験受信して on message に
        ディスパッチする。run() と wait() の両方から共通で使う。"""
        self._poll_timers()
        msg = self._ctx.try_recv(timeout)
        if msg is not None:
            self._last_message = msg
            handler = self._script.on_message.get(msg.arbitration_id)
            if handler is not None:
                self._run_block(handler)

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
                    log_func, stop_event, message_queue=None) -> None:
    """`.capl` スクリプトを実行する。capl_api.run_script() の DSL 版で、app.py からは
    ファイル拡張子で自動的にどちらを呼ぶか切り替えられる (同じ引数の並びにしてある)。

    message_queue は app.py の _rx_monitor_worker がファンアウトする共有キュー。
    on message ディスパッチ (Interpreter.run() 内の ctx.try_recv()) はこのキューを
    消費する側に回ることで、_rx_monitor_worker と bus.recv() を奪い合わないようにする
    (capl_api.py モジュール冒頭の docstring 参照)。"""
    script = parse(source)  # DslSyntaxError は呼び出し側 (_script_worker) の except で表示される
    ctx = capl_api.CaplContext(get_bus, bus_lock, log_func, stop_event, message_queue)
    Interpreter(script, ctx, stop_event).run()
