"""
CAPL 風 DSL (`.capl` スクリプト) の字句解析・構文解析・インタプリタ。

capl_api.py の Python API 層 (中間案) に対する「本格対応」案。設計合意時のメモ通り、
UDS 送受信の実体は capl_api.CaplContext をそのままランタイム層として再利用する
(パーサが構文木を解釈して最終的に呼ぶのが CaplContext のメソッドになる)。

対応する構文:

    variables { int x; float y = 1.5; byte data[8]; }   // 省略可。1ファイルに1つまで、
                                           // on start/on timer/on message より前に書くこと
    on start { ... }
    on timer <name> { ... }
    on message <id> { ... }

ブロック内は以下の文が書ける:
    - 関数呼び出し文: send(...); / write(...); など
    - 代入文: x = x + 1; / x += 1; / x++; / x--;  （x は variables{} で宣言済みで
      あること。複合代入 (+= -= *= /= %=) とインクリメント/デクリメント (++/--、
      後置のみ) はパーサーが x = x <op> 1 の代入文に脱糖する。式の中では使えず、
      文として、または for の初期化/更新句としてのみ使える）
    - 配列要素への代入文: data[i] = expr;  （data は variables{} で byte 配列として
      宣言済みであること）
    - if (expr) { ... } [else if (expr) { ... }]* [else { ... }]
    - while (expr) { ... }
    - for (x = 0; x < 10; x++) { ... }  （初期化/条件/更新はいずれも省略可）
    - break; / continue;  （break は while/for/switch の中、continue は while/for
      の中でのみ使える。continue は switch を素通りして外側の while/for に効く）
    - switch (expr) { case N: ... break; case M: ... default: ... }  （C/CAPL と
      同じフォールスルー動作。case の値は整数定数のみ）
    - return; / return expr;  （ユーザー定義関数の中でのみ使える。下記参照）

式は四則演算 (+ - * / %)・比較 (== != < > <= >=)・論理 (&& || !)・丸括弧・
関数呼び出し・変数参照・配列要素参照 (data[i])・数値/文字列リテラル・
this.byte(n)/this.id/this.dlc (on message ハンドラ内限定、下記参照) に対応する
（構造体は対象外）。配列を添字なしでそのまま参照すると (例: send(data)) 全要素の
コピーを返す。send()/send_can() はこれを検出して配列の全バイトをペイロードに
展開するので、CAPL の byte 配列をメッセージのペイロードとして丸ごと送る書き方に
近い形で使える (_flatten_bytes() 参照)。

ユーザー定義関数:

    int add(int a, int b) { return a + b; }
    void logRetry(int n) { ... }   // void 関数は return; か何も return せず終了

トップレベル (variables{}/on ... と同じ階層) にいくつでも書ける。型は戻り値・
仮引数とも int/float のみ (void は戻り値にのみ使え、配列は戻り値にも仮引数にも
できない)。定義順に関係なく呼び出せる (前方参照・相互再帰も可。全関数の名前を
実行前に一括登録してから各ブロック・各関数本体を検証する)。void 関数の戻り値を
式の中で使おうとする (例: `x = voidFunc();`) のは実行前検証でエラーになる。
int/float 関数が return を一度も実行せずに本体の最後まで到達した場合は (これは
静的には検出しない。分岐を網羅する制御フロー解析はしていないため) 実行時に
capl_api.ScriptAbort で中断する。

関数はローカル変数を持てない (仮引数以外の識別子は常にグローバルの variables{}
を指す)。ループカウンタ等の作業用変数が要る場合は専用のグローバル変数を使うか
仮引数を使い回す。**同じグローバル変数をループカウンタに使う関数同士が互いを
呼び出す (直接・間接の再帰を含む) と、内側の呼び出しが外側のループカウンタを
上書きしてしまう** ため、そのような関数には別々の名前のグローバル変数を割り当てる
こと (このミニ言語がブロックスコープのローカル変数を持たないことによる制約)。
仮引数はその関数の中でだけ同名のグローバル変数をシャドーイングし、呼び出しから
戻ると元の値に復元される (再帰呼び出しでも Python の呼び出しスタックを使って
正しく退避・復元される、_call_user_function() 参照)。

利用できる関数は Interpreter._make_builtins() を参照 (README.md にも一覧表がある)。
write(fmt, ...)/log(fmt, ...) は、第1引数が '%' を含む文字列かつ他に引数がある場合、
CAPL の write() と同様 printf 風の書式文字列 (%d/%f/%s/%x/%X/%%) として扱う。それ以外
(引数1つだけ、または '%' を含まない) は従来通りスペース区切りで連結する。

this.byte(n)/this.id/this.dlc は on message ハンドラ内限定で、直近に受信したフレームの
byte[n]/CAN ID/データ長を返す。既存の msgData(n)/msgId()/msgDlc() 関数と同じ内容を返す
プロパティ風の書き方で、実際の CAPL の on message ハンドラでの書き方に近い
(msgData 等の関数は互換性のため残してあり、on message ハンドラ外でも呼べる)。

未知の関数呼び出し・未宣言の変数参照・on message ハンドラ外での this 使用は、
Interpreter 構築時 (実行前) に全ブロックを走査して検出する (_validate() 参照)。
on start の実行後に初めて呼ばれる on timer/on message の中身でタイポがあっても、
on start の副作用（セッション変更等）を実行してしまった後になって気付く、という
事態を避けるため。

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
from contextlib import contextmanager
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
    # 複合代入・インクリメント/デクリメントは対応する1文字演算子より前に置くこと
    # (先勝ちマッチのため。例えば PLUSEQ を PLUS より後ろに置くと "+=" が
    # PLUS,ASSIGN の2トークンに割れてしまう)。
    ("PLUSEQ", r"\+="),
    ("MINUSEQ", r"-="),
    ("STAREQ", r"\*="),
    ("SLASHEQ", r"/="),
    ("PERCENTEQ", r"%="),
    ("INC", r"\+\+"),
    ("DEC", r"--"),
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
    ("LBRACKET", r"\["),
    ("RBRACKET", r"\]"),
    ("LPAREN", r"\("),
    ("RPAREN", r"\)"),
    ("COMMA", r","),
    ("SEMI", r";"),
    ("COLON", r":"),  # switch の case/default ラベル用
    ("DOT", r"\."),  # this.byte(n) 等のメンバアクセス用 (FLOAT の \d+\.\d+ より後ろなので数値は奪われない)
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
_ARRAY_TYPES = ("byte",)  # 配列として宣言できる型 (byte[固定長] のみ対応)
_FUNC_RETURN_TYPES = ("void",) + _VAR_TYPES  # 関数の戻り値の型 (void/int/float。配列は返せない)
_COMPARISON_TOKENS = {"LT": "<", "GT": ">", "LE": "<=", "GE": ">="}
_ADDITIVE_TOKENS = {"PLUS": "+", "MINUS": "-"}
_MULTIPLICATIVE_TOKENS = {"STAR": "*", "SLASH": "/", "PERCENT": "%"}
# 複合代入 (x += expr 等) はパーサーが `x = x <op> expr` の Assign/BinOp に脱糖する
# (下記 _parse_assignment_expr 参照)。インタプリタ・検証系に新しいノード種別を
# 増やさずに済み、代入の型変換ロジックも Assign 実行の1箇所のままで済む。
_COMPOUND_ASSIGN_TOKENS = {
    "PLUSEQ": "+", "MINUSEQ": "-", "STAREQ": "*", "SLASHEQ": "/", "PERCENTEQ": "%",
}

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
class ThisAccess:
    """this.byte(n)/this.id/this.dlc の式ノード。member は "byte"/"id"/"dlc"、
    args は byte(n) の n のような引数 (id/dlc は空リスト)。on message ハンドラの
    外で使うと _validate_expr() が実行前に弾く (実際の CAPL でも this は message
    ハンドラの外では使えない)。"""
    member: str
    args: list  # list[Expr]
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
class For:
    """for (init; cond; update) { body }。init/cond/update はいずれも省略可
    (C の for(;;) と同様)。init/update は Assign 文のみ (i++ 等の増分演算子はない
    ので x = x + 1 の形で書く)。"""
    init: Optional[object]  # Assign | None
    cond: object  # Expr | None (省略時は常に真とみなす)
    update: Optional[object]  # Assign | None
    body: list  # list[Stmt]
    line: int


@dataclass
class ArrayDecl:
    """`byte name[size];` / `byte name[size] = {v0, v1, ...};` の配列宣言。
    size は初期化リストがあればその長さを、無ければ明示された添字の値を使う
    (両方指定時は初期化リストが size を超えていないことをパース時に確認する)。"""
    name: str
    size: int
    init: Optional[list]  # list[Expr] | None (定数式のみ、VarDecl.init と同じ制約)
    line: int


@dataclass
class Index:
    """配列要素参照式 `name[expr]` (式としても代入先 (IndexAssign) としても使う)。"""
    name: str
    index: object  # Expr
    line: int


@dataclass
class IndexAssign:
    """配列要素への代入文 `name[expr] = expr;`。"""
    name: str
    index: object  # Expr
    expr: object  # Expr
    line: int


@dataclass
class Break:
    line: int


@dataclass
class Continue:
    line: int


@dataclass
class CaseLabel:
    """switch 文本体中の `case N:` / `default:` ラベル (実行時は単なる目印で、
    文としては何もしない)。value is None が default を表す (_parse_case_value() は
    整数定数以外を受け付けないので、None は default 専用の番人として使える。
    別に is_default フラグを持つと value と食い違いうる導出可能な状態になるため
    持たない)。"""
    value: Optional[int]
    line: int


@dataclass
class Switch:
    """switch (expr) { case N: ... break; default: ... } 文。body は case/default
    ラベルと文が原文の並び順のままフラットに入ったリスト (CaseLabel | Stmt)。
    C/CAPL と同じフォールスルー動作 (break が無ければ次の case に流れ込む) を、
    「一致したラベルの位置から break まで順に実行する」という単純な走査で
    再現できるようにするための表現 (case ごとにネストした木にすると
    フォールスルーの表現が逆に複雑になる)。case_index/default_index は
    case 値 -> body 内の位置、を事前に持たせたもの (case 値はパース時点で重複禁止
    済みなので一意)。switch は on message ハンドラの中で毎回実行されうるため、
    実行のたびに body を線形走査して一致するラベルを探すのは避け、パース時に
    1回だけ作った辞書を実行時は O(1) で引くだけにする。"""
    expr: object  # Expr
    body: list  # list[CaseLabel | Stmt]
    case_index: dict  # int(case値) -> body 内の CaseLabel の位置
    default_index: Optional[int]  # default ラベルの body 内位置 (無ければ None)
    line: int


@dataclass
class Param:
    """ユーザー定義関数の仮引数。型は int/float のみ (byte 配列は仮引数にできない。
    配列は variables{} のグローバル宣言のみが唯一の置き場所、という既存の制約と
    揃えてある)。"""
    type_name: str  # "int" | "float"
    name: str
    line: int


@dataclass
class FuncDecl:
    """`void name(int a, float b) { ... }` のようなユーザー定義関数の宣言。
    return_type は "void"/"int"/"float"。関数はトップレベル (variables{}/on ... と
    同じ階層) にいくつでも書け、on start/on timer/on message や他の関数から
    呼び出せる (前方参照・相互再帰も可、_functions は実行前に一括登録するため)。
    ローカル変数宣言には対応しない (仮引数のみがその関数内で使えるローカルな名前で、
    それ以外の識別子は常にグローバルの variables{} を指す。関数内で作業用の変数が
    要る場合はグローバル変数を使うか、仮引数をワークエリアとして使う)。"""
    return_type: str  # "void" | "int" | "float"
    name: str
    params: list  # list[Param]
    body: list  # list[Stmt]
    line: int


@dataclass
class Return:
    """`return;` (void 関数用) / `return expr;` (int/float 関数用)。関数の外
    (on start/on timer/on message) では使えない (_validate_stmt() 参照)。"""
    expr: object  # Expr | None
    line: int


@dataclass
class Script:
    variables: list  # list[VarDecl | ArrayDecl]  (`variables { ... }` ブロック。無ければ空)
    on_start: list  # list[list[Stmt]]  (複数の `on start` は順に実行)
    on_timer: dict  # name(str) -> list[Stmt]
    on_message: dict  # can_id(int) -> list[Stmt]
    functions: list  # list[FuncDecl]  (トップレベルのユーザー定義関数。無ければ空)


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
        functions: list = []
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
                        "on message・関数定義より前に書いてください"
                    )
                var_block_seen = True
                self._advance()
                variables.extend(self._parse_var_block())
                continue

            if self._peek().kind == "IDENT" and self._peek().value in _FUNC_RETURN_TYPES:
                # 関数の呼び出し元は on start 等だけでなく他の関数からもあるため、宣言順に
                # 制約は課さない (前方参照・相互再帰も可。_register_functions() が実行前に
                # 全関数を一括登録してから各ブロックを検証するため)。variables{} との
                # 前後関係だけは on ブロックと同様の制約を課す (on_block_seen を流用)。
                on_block_seen = True
                functions.append(self._parse_func_decl())
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
        return Script(variables, on_start, on_timer, on_message, functions)

    def _parse_func_decl(self) -> FuncDecl:
        type_tok = self._advance()  # void/int/float (呼び出し元で確認済み)
        name_tok = self._expect("IDENT")
        self._expect("LPAREN")
        params = self._parse_arg_list(lambda i: self._parse_param())
        self._expect("RPAREN")
        body = self._parse_block()
        return FuncDecl(type_tok.value, name_tok.value, params, body, type_tok.line)

    def _parse_param(self) -> Param:
        type_tok = self._peek()
        if type_tok.kind != "IDENT" or type_tok.value not in _VAR_TYPES:
            raise DslSyntaxError(
                f"{type_tok.line}行目: 引数の型 (int/float) を期待しましたが "
                f"'{type_tok.value}' でした"
            )
        self._advance()
        name_tok = self._expect("IDENT")
        return Param(type_tok.value, name_tok.value, name_tok.line)

    def _parse_var_block(self) -> list:
        self._expect("LBRACE")
        decls = []
        while self._peek().kind != "RBRACE":
            decls.append(self._parse_var_decl())
        self._expect("RBRACE")
        return decls

    def _parse_var_decl(self):
        type_tok = self._peek()
        if type_tok.kind != "IDENT" or type_tok.value not in (_VAR_TYPES + _ARRAY_TYPES):
            raise DslSyntaxError(
                f"{type_tok.line}行目: 変数の型 (int/float/byte) を期待しましたが "
                f"'{type_tok.value}' でした"
            )
        self._advance()
        name_tok = self._expect("IDENT")
        if self._peek().kind == "LBRACKET":
            return self._parse_array_decl(type_tok, name_tok)
        if type_tok.value in _ARRAY_TYPES:
            raise DslSyntaxError(
                f"{type_tok.line}行目: '{type_tok.value}' は配列専用の型です "
                f"('{type_tok.value} {name_tok.value}[size];' の形式で宣言してください。"
                "スカラー変数は int/float のみ対応)"
            )
        init = None
        if self._peek().kind == "ASSIGN":
            self._advance()
            init = self._parse_expr()
        self._expect("SEMI")
        return VarDecl(type_tok.value, name_tok.value, init, name_tok.line)

    def _parse_array_decl(self, type_tok: Token, name_tok: Token) -> ArrayDecl:
        """`byte name[size];` / `byte name[size] = {v0, v1, ...};` / `byte name[] = {...};`
        (size 省略時は初期化リストの長さを使う)。int/float は配列として宣言できない
        (実運用上、配列を使うのは送受信ペイロード = byte 列を組み立てる用途に限られるため)。"""
        if type_tok.value not in _ARRAY_TYPES:
            raise DslSyntaxError(f"{type_tok.line}行目: 配列は byte 型のみ対応しています")
        self._expect("LBRACKET")
        size = None
        if self._peek().kind != "RBRACKET":
            size_tok = self._peek()
            if size_tok.kind not in ("INT", "HEXNUM"):
                raise DslSyntaxError(f"{size_tok.line}行目: 配列サイズは整数定数である必要があります")
            self._advance()
            size = int(size_tok.value, 0)
        self._expect("RBRACKET")
        init = None
        if self._peek().kind == "ASSIGN":
            self._advance()
            self._expect("LBRACE")
            init = self._parse_arg_list(lambda i: self._parse_expr(), terminator="RBRACE")
            self._expect("RBRACE")
        self._expect("SEMI")
        if size is None and init is None:
            raise DslSyntaxError(
                f"{name_tok.line}行目: 配列 '{name_tok.value}' はサイズ指定か初期化リストの"
                "どちらかが必要です"
            )
        if init is not None and size is not None and len(init) > size:
            raise DslSyntaxError(
                f"{name_tok.line}行目: 配列 '{name_tok.value}' の初期化子の個数 ({len(init)}) が"
                f"宣言サイズ ({size}) を超えています"
            )
        if size is None:
            size = len(init)
        return ArrayDecl(name_tok.value, size, init, name_tok.line)

    # ---- 文 ----
    def _parse_block(self) -> list:
        self._expect("LBRACE")
        stmts = []
        while self._peek().kind != "RBRACE":
            stmts.append(self._parse_statement())
        self._expect("RBRACE")
        return stmts

    # 代入文の先頭 (IDENT の次) に来うるトークン。単純代入 (=) に加え、複合代入・
    # インクリメント/デクリメントも _parse_assignment_expr() 側で1箇所にまとめて処理する。
    _ASSIGN_START_TOKENS = ("ASSIGN", "INC", "DEC") + tuple(_COMPOUND_ASSIGN_TOKENS)

    def _parse_statement(self):
        if self._at_ident("if"):
            return self._parse_if()
        if self._at_ident("while"):
            return self._parse_while()
        if self._at_ident("for"):
            return self._parse_for()
        if self._at_ident("switch"):
            return self._parse_switch()
        if self._at_ident("break"):
            tok = self._advance()
            self._expect("SEMI")
            return Break(tok.line)
        if self._at_ident("continue"):
            tok = self._advance()
            self._expect("SEMI")
            return Continue(tok.line)
        if self._at_ident("return"):
            tok = self._advance()
            expr = None
            if self._peek().kind != "SEMI":
                expr = self._parse_expr()
            self._expect("SEMI")
            return Return(expr, tok.line)
        tok = self._peek()
        if tok.kind == "IDENT" and self._peek(1).kind == "LBRACKET":
            return self._parse_index_assignment()
        if tok.kind == "IDENT" and self._peek(1).kind in self._ASSIGN_START_TOKENS:
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

    def _parse_for(self) -> For:
        for_tok = self._expect_ident("for")
        self._expect("LPAREN")
        init = None
        if self._peek().kind != "SEMI":
            init = self._parse_assignment_expr()
        self._expect("SEMI")
        cond = None
        if self._peek().kind != "SEMI":
            cond = self._parse_expr()
        self._expect("SEMI")
        update = None
        if self._peek().kind != "RPAREN":
            update = self._parse_assignment_expr()
        self._expect("RPAREN")
        body = self._parse_block()
        return For(init, cond, update, body, for_tok.line)

    def _parse_assignment_expr(self) -> Assign:
        """`x = expr` / `x += expr` (等の複合代入) / `x++` / `x--` の代入式本体
        (末尾のセミコロンは呼び出し側の責任)。文としての代入 (_parse_assignment、
        セミコロンを伴う) と、for 文の初期化/更新句 (セミコロンを伴わない、
        `for (i = 0; i < 10; i++)` のような CAPL 風の書き方をここで受けられるように
        する) の両方から使う。複合代入・インクリメント/デクリメントは全て
        `x = x <op> expr` の Assign(BinOp(...)) に脱糖して返す。実行系・検証系に
        新しいノード種別を増やさずに済み、代入の型変換ロジック (int への切り捨て等)
        も Assign 実行の1箇所のままで使い回せるため。"""
        name_tok = self._expect("IDENT")
        op_tok = self._peek()
        if op_tok.kind == "INC":
            self._advance()
            return Assign(
                name_tok.value,
                BinOp("+", Var(name_tok.value, name_tok.line), 1, op_tok.line),
                name_tok.line,
            )
        if op_tok.kind == "DEC":
            self._advance()
            return Assign(
                name_tok.value,
                BinOp("-", Var(name_tok.value, name_tok.line), 1, op_tok.line),
                name_tok.line,
            )
        if op_tok.kind in _COMPOUND_ASSIGN_TOKENS:
            self._advance()
            rhs = self._parse_expr()
            op = _COMPOUND_ASSIGN_TOKENS[op_tok.kind]
            return Assign(
                name_tok.value,
                BinOp(op, Var(name_tok.value, name_tok.line), rhs, op_tok.line),
                name_tok.line,
            )
        self._expect("ASSIGN")
        expr = self._parse_expr()
        return Assign(name_tok.value, expr, name_tok.line)

    def _parse_assignment(self) -> Assign:
        assign = self._parse_assignment_expr()
        self._expect("SEMI")
        return assign

    def _parse_index_assignment(self) -> IndexAssign:
        name_tok, index = self._parse_name_index()
        self._expect("ASSIGN")
        expr = self._parse_expr()
        self._expect("SEMI")
        return IndexAssign(name_tok.value, index, expr, name_tok.line)

    def _parse_switch(self) -> Switch:
        switch_tok = self._expect_ident("switch")
        self._expect("LPAREN")
        expr = self._parse_expr()
        self._expect("RPAREN")
        self._expect("LBRACE")
        body = []
        case_index: dict = {}
        default_index = None
        while self._peek().kind != "RBRACE":
            if self._at_ident("case"):
                case_tok = self._advance()
                value = self._parse_case_value()
                self._expect("COLON")
                if value in case_index:
                    raise DslSyntaxError(
                        f"{case_tok.line}行目: case {value} が重複しています"
                    )
                case_index[value] = len(body)
                body.append(CaseLabel(value, case_tok.line))
            elif self._at_ident("default"):
                default_tok = self._advance()
                self._expect("COLON")
                if default_index is not None:
                    raise DslSyntaxError(f"{default_tok.line}行目: default が重複しています")
                default_index = len(body)
                body.append(CaseLabel(None, default_tok.line))
            else:
                body.append(self._parse_statement())
        self._expect("RBRACE")
        return Switch(expr, body, case_index, default_index, switch_tok.line)

    def _parse_case_value(self) -> int:
        """case ラベルの値は整数定数のみ (負数も `-1` のように単項マイナスまで許容)。
        実際の CAPL の switch も整数定数式のみを case に取れるため、変数参照や
        式は非対応にしている (実行時に何度も評価するのではなく、パース時点で
        重複チェックできるようにする意図もある)。HEXNUM/INT トークンから int への
        変換自体は _parse_primary() の同じ変換を再利用し (ここで再度書かない)、
        戻り値が int かどうかで整数リテラル以外 (float/文字列/変数参照等) を弾く。"""
        neg = False
        if self._peek().kind == "MINUS":
            self._advance()
            neg = True
        tok = self._peek()
        value = self._parse_primary()
        if not isinstance(value, int):
            raise DslSyntaxError(f"{tok.line}行目: case の後には整数定数が必要です")
        return -value if neg else value

    def _parse_arg_list(self, parse_arg, terminator: str = "RPAREN") -> list:
        """開き括弧は呼び出し側で読了済みの前提で、terminator (デフォルトは関数呼び出しの
        RPAREN) までのカンマ区切り列を読む。parse_arg(index) は index 番目 (0始まり) の
        要素の読み方 (_parse_call/_parse_this_access の引数列、_parse_array_decl の
        `{v0, v1, ...}` 初期化子のいずれも同じループ形なので、要素の読み方と終端トークンを
        呼び出し側に委ねてここに1本化する)。"""
        args = []
        if self._peek().kind != terminator:
            args.append(parse_arg(0))
            i = 1
            while self._peek().kind == "COMMA":
                self._advance()
                args.append(parse_arg(i))
                i += 1
        return args

    def _parse_call(self) -> Call:
        name_tok = self._expect("IDENT")
        self._expect("LPAREN")
        args = self._parse_arg_list(
            lambda i: self._parse_call_arg(name_tok.value, is_first=(i == 0))
        )
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
            if tok.value == "this":
                return self._parse_this_access()
            if self._peek(1).kind == "LPAREN":
                return self._parse_call()
            if self._peek(1).kind == "LBRACKET":
                return self._parse_index()
            self._advance()
            return Var(tok.value, tok.line)
        raise DslSyntaxError(f"{tok.line}行目: 式として不正なトークン '{tok.value}'")

    def _parse_index(self) -> Index:
        name_tok, index = self._parse_name_index()
        return Index(name_tok.value, index, name_tok.line)

    def _parse_name_index(self):
        """`name[expr]` の共通部分 (配列名と添字式) を読む。名前の識別子トークンと
        添字式を返す。式としての配列要素参照 (_parse_index) と、配列要素への代入文
        (_parse_index_assignment) の両方から使う。"""
        name_tok = self._expect("IDENT")
        self._expect("LBRACKET")
        index = self._parse_expr()
        self._expect("RBRACKET")
        return name_tok, index

    def _parse_this_access(self) -> ThisAccess:
        this_tok = self._advance()  # "this"
        self._expect("DOT")
        member_tok = self._expect("IDENT")
        args = []
        if self._peek().kind == "LPAREN":
            self._advance()
            args = self._parse_arg_list(lambda i: self._parse_expr())
            self._expect("RPAREN")
        return ThisAccess(member_tok.value, args, this_tok.line)


def parse(source: str) -> Script:
    return _Parser(tokenize(source)).parse()


# ---------------------------------------------------------------------------
# インタプリタ
# ---------------------------------------------------------------------------


def _iter_child_exprs(node):
    """BinOp/UnaryOp/ThisAccess/Index の子ノード (部分式) を列挙する。Call/Var/
    TimerName/リテラルは子を持たないので空を返す (Call は _validate_expr() 側で
    個別に引数を辿るため、ここには含めない)。_reject_calls()/_validate_expr() の
    再帰下降で同じ形の走査を2箇所に個別に書かないよう、ここに1本化する。"""
    if isinstance(node, BinOp):
        return (node.left, node.right)
    if isinstance(node, UnaryOp):
        return (node.operand,)
    if isinstance(node, ThisAccess):
        return tuple(node.args)
    if isinstance(node, Index):
        return (node.index,)
    return ()


class _BreakSignal(Exception):
    """break; の実行時信号。最も内側の while/for/switch がこれを捕捉して抜ける
    (DslSyntaxError/ScriptAbort と違い、エラーではなく単なる制御フローなので
    別の例外クラスにしてある)。"""


class _ContinueSignal(Exception):
    """continue; の実行時信号。最も内側の while/for がこれを捕捉して次の周回に
    進む (switch は捕捉せずそのまま外側の while/for まで通過させる。C/CAPL と
    同じく switch は continue に対しては透過的なため)。"""


class _ReturnSignal(Exception):
    """return; / return expr; の実行時信号。関数本体の実行 (Interpreter._call_user_function())
    がこれを捕捉して呼び出し元へ戻り値を渡す。while/for/switch はこれを捕捉しない
    (_BreakSignal/_ContinueSignal と違って個別の except 節を追加していない) ので、
    ネストしたループ・switch の中から return しても素通りしてそのまま関数の外まで
    伝播する (C/CAPL の return と同じ、ループ途中からの即時関数脱出)。"""
    def __init__(self, value):
        super().__init__()
        self.value = value


@dataclass
class _Variable:
    """宣言済み変数 (スカラーも配列も) の実行時状態。type_name は "int"/"float" なら
    スカラー、_ARRAY_TYPES に含まれる ("byte") なら配列を表し、value はそれぞれ
    スカラー値 / list[int] を持つ。スカラーと配列を別々の dict (self._vars/
    self._arrays) で持つと、「この名前は宣言済みか」「スカラーか配列か」を毎回
    2つの dict のメンバーシップを OR して求める羽目になり、判定漏れの温床になる
    (実際、配列サポート追加時にこのパターンが複数箇所で発生した)。type_name が
    スカラー/配列のどちらかを表す唯一の情報源になるようにして、1つの dict に統一する。"""
    type_name: str  # "int" | "float" | "byte" (byte なら配列)
    value: object  # int | float | list[int]

    @property
    def is_array(self) -> bool:
        return self.type_name in _ARRAY_TYPES


class Interpreter:
    """パース済み Script を実行する。UDS 送受信は capl_api.CaplContext に委譲する。"""

    def __init__(self, script: Script, ctx: capl_api.CaplContext, stop_event):
        self._script = script
        self._ctx = ctx
        self._stop_event = stop_event
        self._armed_timers: dict[str, float] = {}  # name -> 発火時刻 (time.monotonic())
        self._last_message: Optional[object] = None  # can.Message (on message ハンドラ内でのみ有効)
        self._vars: dict[str, _Variable] = {}  # スカラー・配列とも (_Variable.is_array 参照)
        self._builtins = self._make_builtins()
        self._this_members = self._make_this_members()
        self._register_functions()
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

    # 戻り値を式の中で使わせない (require_value=True の文脈で拒否する) 組み込み関数名。
    # 実装が Python の None を返しうるもの (send()/send_can() 等は常に None、
    # cancelTimer() はタイマーが未アームだと dict.pop() の既定値 None) が対象。
    # ここに載せずに素通しすると、ユーザー定義 void 関数と全く同じ理由で
    # _coerce(None, 'int'/'float') が未処理の TypeError になる
    # (例: `x = cancelTimer(未アームのタイマー);`)。
    _VOID_BUILTINS = frozenset({"send", "send_can", "wait", "log", "write", "setTimer", "cancelTimer"})

    def _make_builtins(self) -> dict:
        """関数名 -> (最小引数数, 実装) の辞書。最小引数数を実装と同じ場所で宣言する
        ことで、_validate_expr() の事前チェックと実際の実装が食い違わないようにする
        (別のテーブルで二重管理すると、実装だけ引数を増やして片方の更新を忘れる、
        という保守リスクになるため)。可変長引数を取る send()/send_can() のデータ
        バイト部分等、"最低いくつ必要か" 以上のことは表現しない。戻り値が式の中で
        使えるかどうかは実装から独立して _VOID_BUILTINS で管理する (関数名の集合を
        見るだけで済み、各エントリのタプルの形を増やさずに済むため)。"""
        ctx = self._ctx
        return {
            "send": (0, lambda args: ctx.send(self._flatten_bytes(args))),
            "send_can": (1, lambda args: ctx.send_can(
                self._to_int(args[0]), self._flatten_bytes(args[1:])
            )),
            "wait_response": (0, lambda args: ctx.wait_response(
                float(args[0]) if args else 2.0
            )),
            "assert_positive": (0, lambda args: ctx.assert_positive()),
            "assert_negative": (0, lambda args: ctx.assert_negative(
                nrc=self._to_int(args[0]) if args else None
            )),
            "security_unlock": (0, lambda args: ctx.security_unlock()),
            # ctx.wait() ではなく self.wait() (Interpreter 自身の待機ループ) を使う。
            # ctx.wait() は Python 版 (@ctx.on_timer) の _timers しか見ておらず、DSL 側の
            # _armed_timers(setTimer)/on_message ディスパッチとは無関係なため、そちらに
            # 委譲すると wait() 中は setTimer タイマーも on message も止まってしまう。
            "wait": (1, lambda args: self.wait(float(args[0]))),
            "log": (0, lambda args: ctx.log(self._format_write_args(args))),
            "write": (0, lambda args: ctx.log(self._format_write_args(args))),  # CAPL の write() 相当のエイリアス
            "setTimer": (2, lambda args: self._set_timer(str(args[0]), float(args[1]))),
            "cancelTimer": (1, lambda args: self._armed_timers.pop(str(args[0]), None)),
            "msgData": (1, lambda args: self._msg_data(self._to_int(args[0]))),
            "msgId": (0, lambda args: self._msg_id()),
            "msgDlc": (0, lambda args: self._msg_dlc()),
            # 直近の wait_response()/security_unlock() で受信した UDS 応答を読む関数群。
            # msgData()/msgId() 等 (on message で受信した生 CAN フレーム用) とは別物で、
            # 応答フレーム自体は wait_response() の受信ループが直接消費するため
            # _last_message には流れてこない (README「on message は UDS 応答以外の
            # 監視向き」参照)。switch(respNrc()) のような分岐に使う。
            "respSid": (0, lambda args: self._resp_sid()),
            "respNrc": (0, lambda args: self._resp_nrc()),
            "respByte": (1, lambda args: self._resp_byte(self._to_int(args[0]))),
            "respIsNegative": (0, lambda args: self._resp_is_negative()),
        }

    def _make_this_members(self) -> dict:
        """this.<member> の (必要な引数の個数, 実装) の辞書。_make_builtins() と同じ
        理由で、アリティ専用の別テーブルを持たない (別テーブルだと実装だけ引数を
        変えて片方の更新を忘れる、という保守リスクになるため)。id/dlc は
        msgId()/msgDlc() ビルトインと同じ _msg_id()/_msg_dlc() を呼ぶ (「直近メッセージ
        が無い場合」のデフォルト値ロジックを2箇所に複製しない)。"""
        return {
            "byte": (1, lambda args: self._msg_data(self._to_int(args[0]))),
            "id": (0, lambda args: self._msg_id()),
            "dlc": (0, lambda args: self._msg_dlc()),
        }

    def _register_functions(self) -> None:
        """トップレベルのユーザー定義関数を名前で引けるようにする。on start 等の
        検証・実行が始まる前に全関数を一括登録しておくことで、定義順に関係なく
        呼び出せる (前方参照・相互再帰も可)。組み込み関数と同名の定義・関数名の
        重複・仮引数名の重複はここで実行前に弾く。仮引数名の重複を許すと、
        _bound_params() が同じ名前を2回束縛しようとして「1個目の仮引数が束縛した
        直後の値」を「シャドーイング前の元の値」として誤って退避してしまい、
        関数呼び出し後 (検証時点でも!) にグローバル変数を恒久的に壊してしまうため
        (どちらにせよ同名の仮引数が複数あると値が一意に決まらず意味を持たない)。"""
        self._functions: dict[str, FuncDecl] = {}
        for func in self._script.functions:
            if func.name in self._builtins:
                raise DslSyntaxError(
                    f"{func.line}行目: 関数 '{func.name}' は組み込み関数と同名のため"
                    "定義できません"
                )
            if func.name in self._functions:
                raise DslSyntaxError(f"{func.line}行目: 関数 '{func.name}' は既に定義されています")
            seen_params: set = set()
            for param in func.params:
                if param.name in seen_params:
                    raise DslSyntaxError(
                        f"{param.line}行目: 関数 '{func.name}' の仮引数 '{param.name}' が"
                        "重複しています"
                    )
                seen_params.add(param.name)
            self._functions[func.name] = func

    @staticmethod
    def _to_int(value: ArgValue) -> int:
        return int(value)

    @staticmethod
    def _to_byte(value: ArgValue) -> int:
        return int(value) & 0xFF

    def _flatten_bytes(self, args) -> list:
        """send()/send_can() のデータバイト引数を1つの int リストに組み立てる。
        通常の引数はそのまま1バイトとして扱うが、`send(data)` のように配列変数を
        裸で渡した場合 (Var 評価が list を返す、_eval() 参照) はその全要素を
        展開する。これにより CAPL の byte 配列を message のペイロードとして
        そのまま送信する書き方に近い形で、`send(0x10, 0x03)` のような個別バイト
        列挙と `send(data)` のような配列渡しの両方を同じ send()/send_can() で
        書ける。"""
        result = []
        for a in args:
            if isinstance(a, list):
                result.extend(self._to_byte(x) for x in a)
            else:
                result.append(self._to_byte(a))
        return result

    @staticmethod
    def _truthy(value) -> bool:
        return bool(value)

    def _msg_data(self, index: int) -> int:
        if self._last_message is None or index >= len(self._last_message.data):
            return 0
        return self._last_message.data[index]

    def _msg_id(self) -> int:
        return self._last_message.arbitration_id if self._last_message else 0

    def _msg_dlc(self) -> int:
        return len(self._last_message.data) if self._last_message else 0

    def _resp_byte(self, index: int) -> int:
        resp = self._ctx.last_response
        if resp is None or index >= len(resp.raw):
            return 0
        return resp.raw[index]

    def _resp_sid(self) -> int:
        resp = self._ctx.last_response
        return resp.raw[0] if resp is not None and resp.raw else 0

    def _resp_nrc(self) -> int:
        resp = self._ctx.last_response
        if resp is None:
            return 0
        nrc = resp.nrc
        return nrc if nrc is not None else 0

    def _resp_is_negative(self) -> int:
        resp = self._ctx.last_response
        return 1 if (resp is not None and resp.is_negative) else 0

    @staticmethod
    def _format_write_args(args) -> str:
        """write()/log() の引数を1つの文字列に組み立てる。第1引数が文字列で '%' を
        含み、かつ他に引数がある場合は CAPL の write() と同様 printf 風の書式文字列
        (%d/%f/%s/%x/%X/%% 等) として扱い、Python の % 演算子で残りの引数を埋め込む。
        それ以外 (引数が1つだけ、または '%' を含まない) は従来通りスペース区切りで
        連結する。引数が1つだけの場合に printf 扱いしないのは、`write("50% done")`
        のような '%' を含む単なるテキストが (書式指定漏れとして) 例外にならないよう
        にするため。書式と引数の数が合わない場合は ScriptAbort にする
        (他のランタイムエラーと同様、行番号は付かないが assert_positive() の失敗
        メッセージ等も同様に付いていない)。"""
        if len(args) >= 2 and isinstance(args[0], str) and "%" in args[0]:
            fmt, rest = args[0], tuple(args[1:])
            try:
                return fmt % rest
            except (TypeError, ValueError) as exc:
                raise capl_api.ScriptAbort(
                    f"write()/log() の書式文字列が不正です: {exc}"
                ) from exc
        return " ".join(str(a) for a in args)

    def _set_timer(self, name: str, ms: float) -> None:
        self._armed_timers[name] = time.monotonic() + ms / 1000.0

    # ---- variables{} の初期値式の検証・初期化 ----
    def _reject_calls(self, node) -> None:
        """variables{} の初期値式に関数呼び出し・this 使用が含まれていないか再帰的に
        確認する。変数参照 (Var) は副作用がないので許可する (前方の宣言を参照できる)。

        this も on message ハンドラの外なので使えない (variables{} の初期値式は
        Interpreter 構築中、on message が一度も発火する前に評価されるため
        self._last_message は常に None で、this.id 等は黙って既定値 0 を返して
        しまう。on start/on timer で this を使うと _validate() が弾くのと同じ
        扱いにしないと、ここだけ「エラーにならず意味のない既定値で通る」という
        抜け道になってしまう)。"""
        if isinstance(node, Call):
            raise DslSyntaxError(
                f"{node.line}行目: variables の初期値に関数呼び出し '{node.name}(...)' は"
                "使えません (定数式のみ)"
            )
        if isinstance(node, ThisAccess):
            raise DslSyntaxError(
                f"{node.line}行目: variables の初期値で this は使えません "
                "(on message ブロック内でのみ使用可能)"
            )
        if isinstance(node, Var):
            var = self._vars.get(node.name)
            if var is not None and var.is_array:
                # 配列を丸ごと渡せるのは send()/send_can() 等の関数呼び出しの引数としてのみ
                # (_validate_expr() 参照)。variables の初期値式では Call 自体が上で拒否されて
                # いるため、配列全体を参照できる正当なケースが存在しない。ここで弾かないと
                # `int total = data;` のように list が int の初期値に流れ込み、_coerce() の
                # int(list) が未処理の TypeError になってしまう (send() 等の副作用が実行された
                # 後に落ちることもあり、実行前検証で防ぎたい失敗パターンそのものになる)。
                raise DslSyntaxError(
                    f"{node.line}行目: variables の初期値で配列 '{node.name}' 全体は参照できません "
                    f"(要素を使うには '{node.name}[0]' のように添字を指定してください)"
                )
        for child in _iter_child_exprs(node):
            self._reject_calls(child)

    def _init_variables(self) -> None:
        """宣言順に初期値式を検証・評価して self._vars に格納する。後続の宣言の初期値式が
        先に宣言した変数を参照することもできる（宣言済みかどうかは self._vars への
        追加順でそのまま決まる）。スカラー変数 (VarDecl) も配列 (ArrayDecl) も同じ
        self._vars に入れる (_Variable.is_array 参照) ので、名前空間は自然に共有され、
        同名の scalar と配列を両方宣言しようとすると通常の重複宣言チェックに引っかかる。"""
        for decl in self._script.variables:
            if decl.name in self._vars:
                raise DslSyntaxError(f"{decl.line}行目: 変数 '{decl.name}' は既に宣言されています")
            if isinstance(decl, ArrayDecl):
                if decl.init is not None:
                    for item in decl.init:
                        self._reject_calls(item)
                    values = [self._to_byte(self._eval(item)) for item in decl.init]
                else:
                    values = []
                values.extend([0] * (decl.size - len(values)))
                self._vars[decl.name] = _Variable("byte", values)
                continue
            if decl.init is not None:
                self._reject_calls(decl.init)
                value = self._eval(decl.init)
            else:
                value = self._default_value(decl.type_name)
            self._vars[decl.name] = _Variable(decl.type_name, self._coerce(value, decl.type_name))

    @staticmethod
    def _default_value(type_name: str):
        """初期化式を省略したスカラー変数、および _validate_function() が仮引数を
        検証用に一時束縛する際のプレースホルダ値に使う、型ごとの既定値 (int なら 0、
        float なら 0.0)。両者で同じ int/float 判定を別々に書かないよう1箇所にまとめる。"""
        return 0 if type_name == "int" else 0.0

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
            var = self._vars.get(node.name)
            if var is None:
                raise DslSyntaxError(f"{node.line}行目: 未宣言の変数 '{node.name}'")
            if var.is_array:
                # 配列名を添字なしで参照した場合、現在の全要素をコピーして返す
                # (send(data) のように配列を丸ごと渡す用途、_flatten_bytes() 参照)。
                # コピーを返すのは、呼び出し側が変更しても配列本体に影響しないように
                # するため。
                return list(var.value)
            return var.value
        if isinstance(node, Index):
            return self._eval_index(node)
        if isinstance(node, TimerName):
            return node.name  # 識別子の綴りそのものがタイマー名
        if isinstance(node, ThisAccess):
            return self._eval_this_access(node)
        if isinstance(node, BinOp):
            return self._eval_binop(node)
        if isinstance(node, UnaryOp):
            return self._eval_unaryop(node)
        return node  # リテラル (int/float/str)

    def _resolve_array_index(self, name: str, index_node, line: int):
        """配列要素の読み取り (_eval_index) と書き込み (IndexAssign の実行) の両方が使う
        共通ヘルパー。宣言済みの配列本体 (list) と、範囲チェック済みの添字を返す
        (範囲外は capl_api.ScriptAbort、ゼロ除算等と同じ「スクリプト中断」扱い)。
        読み書きで同じチェックを別々に手書きすると、将来どちらか片方だけ修正されて
        挙動が食い違う保守リスクになるため、ここに1本化する。"""
        var = self._vars.get(name)
        if var is None or not var.is_array:
            raise DslSyntaxError(f"{line}行目: 未宣言の配列 '{name}'")
        idx = self._to_int(self._eval(index_node))
        if not (0 <= idx < len(var.value)):
            raise capl_api.ScriptAbort(
                f"{line}行目: 配列 '{name}' の添字 {idx} が範囲外です (サイズ {len(var.value)})"
            )
        return var.value, idx

    def _eval_index(self, node: Index) -> int:
        arr, idx = self._resolve_array_index(node.name, node.index, node.line)
        return arr[idx]

    def _eval_this_access(self, node: ThisAccess):
        # member・引数個数とも _validate_expr() が実行前に検証済みなので、通常は
        # ここに来ないが保険として残しておく (_call() の entry is None チェックと
        # 同じ考え方)。
        entry = self._this_members.get(node.member)
        if entry is None:
            raise self._unknown_this_member_error(node)
        _arity, fn = entry
        args = [self._eval(a) for a in node.args]
        return fn(args)

    def _unknown_this_member_error(self, node: ThisAccess) -> DslSyntaxError:
        known = "/".join(sorted(self._this_members))
        return DslSyntaxError(f"{node.line}行目: 未知の this.{node.member} ({known} のみ対応)")

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
        entry = self._builtins.get(call.name)
        if entry is not None:
            _min_args, fn = entry
            # setTimer/cancelTimer の第1引数 (TimerName) も _eval() が name をそのまま
            # 返すので、ここで関数名による特別扱いは不要 (パーサーの _parse_call_arg() 参照)。
            args = [self._eval(a) for a in call.args]
            return fn(args)
        func = self._functions.get(call.name)
        if func is not None:
            arg_values = [self._eval(a) for a in call.args]
            return self._call_user_function(func, arg_values)
        raise self._unknown_function_error(call)

    @contextmanager
    def _bound_params(self, params: list, values: list):
        """仮引数を self._vars に一時的に束縛するコンテキストマネージャ。with を抜ける際
        (正常終了・return・例外いずれでも) に、シャドーイングした元の値へ確実に戻す。
        _call_user_function() (実行時、values は実引数を型変換済みのもの) と
        _validate_function() (検証時、values は型のプレースホルダ) の両方から使う
        共通ロジック (退避・復元をそれぞれ別々に実装すると、修正が片方だけに適用されて
        食い違う保守リスクになるため統一する。with にすることで、呼び出し側が
        finally での復元を書き忘れる心配もなくなる)。

        同じ名前が複数回渡された場合 (通常は _register_functions() が仮引数名の
        重複を実行前に拒否するので起きないはずだが、万一のための保険として) は、
        最初の出現でのみ元の値を退避する。ここを `saved[param.name] = ...` のように
        無条件に上書きしてしまうと、2回目の退避が「1回目の束縛で書き換わった後の
        値」を「シャドーイング前の元の値」として誤って記録してしまい、関数呼び出し
        (や検証) の後にグローバル変数が恒久的に壊れてしまう。"""
        saved: dict = {}
        for param, value in zip(params, values):
            if param.name not in saved:
                saved[param.name] = self._vars.get(param.name)
            self._vars[param.name] = _Variable(param.type_name, value)
        try:
            yield
        finally:
            for name, old in saved.items():
                if old is None:
                    self._vars.pop(name, None)
                else:
                    self._vars[name] = old

    def _call_user_function(self, func: FuncDecl, arg_values: list):
        """ユーザー定義関数を呼び出す。仮引数を self._vars に一時的に束縛して本体を
        実行し、呼び出し前の値に戻す (関数呼び出しは再帰・ネストしうるため、Python の
        呼び出しスタック自体を使って退避・復元する。再帰呼び出しでも with (try/finally)
        が LIFO で対応するので、各フレームの仮引数の値は正しく独立する)。仮引数名が
        たまたまグローバル変数と同じ場合はその関数の中でだけシャドーイングされる。
        戻り値は _ReturnSignal で受け取る。void 関数は None を返す。int/float 関数が
        return 文を1度も実行せずに本体の最後まで到達した場合は (return 漏れは静的には
        検出しない方針、下記 _validate_stmt の Return 検証コメント参照)、
        capl_api.ScriptAbort で中断する (漏れたまま無意味な既定値で処理が進むよりは、
        他のランタイムエラーと同様にその場で中断させる方が安全)。"""
        bound_values = [
            self._coerce(value, param.type_name) for param, value in zip(func.params, arg_values)
        ]
        with self._bound_params(func.params, bound_values):
            try:
                self._run_block(func.body)
                return_value = None
            except _ReturnSignal as sig:
                return_value = sig.value
        if func.return_type == "void":
            return None
        if return_value is None:
            raise capl_api.ScriptAbort(
                f"関数 '{func.name}' が値を return せずに終了しました"
            )
        return self._coerce(return_value, func.return_type)

    def _unknown_function_error(self, call: Call) -> DslSyntaxError:
        known = "/".join(sorted(list(self._builtins) + list(self._functions)))
        return DslSyntaxError(f"{call.line}行目: 未知の関数 '{call.name}' ({known} のみ対応)")

    # ---- 文の実行 ----
    def _exec_stmt(self, stmt) -> None:
        if isinstance(stmt, Call):
            self._call(stmt)
        elif isinstance(stmt, Assign):
            var = self._vars[stmt.name]
            var.value = self._coerce(self._eval(stmt.expr), var.type_name)
        elif isinstance(stmt, IndexAssign):
            arr, idx = self._resolve_array_index(stmt.name, stmt.index, stmt.line)
            arr[idx] = self._to_byte(self._eval(stmt.expr))
        elif isinstance(stmt, If):
            if self._truthy(self._eval(stmt.cond)):
                self._run_block(stmt.then_block)
            elif stmt.else_block is not None:
                self._run_block(stmt.else_block)
        elif isinstance(stmt, While):
            while self._truthy(self._eval(stmt.cond)):
                if self._stop_event.is_set():
                    raise capl_api.ScriptStopped("スクリプトが停止されました")
                try:
                    self._run_block(stmt.body)
                except _BreakSignal:
                    break
                except _ContinueSignal:
                    continue
        elif isinstance(stmt, For):
            if stmt.init is not None:
                self._exec_stmt(stmt.init)
            while stmt.cond is None or self._truthy(self._eval(stmt.cond)):
                if self._stop_event.is_set():
                    raise capl_api.ScriptStopped("スクリプトが停止されました")
                try:
                    self._run_block(stmt.body)
                except _BreakSignal:
                    break
                except _ContinueSignal:
                    pass  # continue は for の更新句 (stmt.update) を実行してから次周回へ
                if stmt.update is not None:
                    self._exec_stmt(stmt.update)
        elif isinstance(stmt, Switch):
            self._exec_switch(stmt)
        elif isinstance(stmt, Break):
            raise _BreakSignal()
        elif isinstance(stmt, Continue):
            raise _ContinueSignal()
        elif isinstance(stmt, Return):
            raise _ReturnSignal(self._eval(stmt.expr) if stmt.expr is not None else None)
        else:
            raise DslSyntaxError(f"未対応の文です: {stmt!r}")

    def _exec_switch(self, stmt: Switch) -> None:
        """switch (expr) { case N: ...; break; default: ... } を実行する。C/CAPL と
        同じフォールスルー動作: 値が一致した case ラベル (無ければ default、それも
        無ければ何もしない) の位置から、break に当たるかブロック末尾に達するまで
        後続の文を順に実行する (途中の case/default ラベル自体は単なる目印なので
        読み飛ばす)。一致するラベルの検索は stmt.case_index/default_index (パース時に
        1回だけ構築済み) を引くだけなので、on message ハンドラのように switch が
        何度も実行される場面でも body を毎回線形走査しない。break は _BreakSignal で
        抜けるが、continue (_ContinueSignal) はここでは捕捉せず外側の while/for まで
        そのまま通過させる。"""
        value = self._eval(stmt.expr)
        start = stmt.case_index.get(value, stmt.default_index)
        if start is None:
            return
        try:
            for i in range(start, len(stmt.body)):
                entry = stmt.body[i]
                if isinstance(entry, CaseLabel):
                    continue
                self._exec_stmt(entry)
        except _BreakSignal:
            pass

    def _run_block(self, stmts: list) -> None:
        for stmt in stmts:
            self._exec_stmt(stmt)

    # ---- 実行前の静的検証 ----
    def _validate(self) -> None:
        """self._builtins/self._vars/self._functions (実行時の状態そのもの、別リストとして
        二重管理しない) を情報源として、on start/on timer/on message・全関数本体の
        ブロックを走査し、未知の関数呼び出し・未宣言の変数参照・on message ハンドラ外での
        this 使用があれば実行前に DslSyntaxError を送出する。self._in_message_handler は
        on message ブロック内かどうかを表し、this.byte(n) 等が使えるかの判定に使う
        (_validate() は構築時に1回しか呼ばれない非再入の走査なので、全ての検証
        メソッドにパラメータとして引き回す代わりにインスタンス属性で持たせる)。"""
        self._in_message_handler = False
        for stmts in self._script.on_start:
            self._validate_block(stmts, in_loop=False, in_switch=False, return_type=None)
        for stmts in self._script.on_timer.values():
            self._validate_block(stmts, in_loop=False, in_switch=False, return_type=None)
        self._in_message_handler = True
        for stmts in self._script.on_message.values():
            self._validate_block(stmts, in_loop=False, in_switch=False, return_type=None)
        self._in_message_handler = False
        for func in self._functions.values():
            self._validate_function(func)

    def _validate_function(self, func: FuncDecl) -> None:
        """関数本体を検証する。仮引数を self._vars に一時的に束縛してから
        (シャドーイングされる可能性のある同名グローバルは _bound_params() で
        退避・復元する。_call_user_function() の実行時の退避・復元と共通のロジック)
        通常のブロック検証に委譲し、return_type=func.return_type を渡すことで本体中の
        return 文の型チェックが効くようにする。this は関数の中では使えない
        (on message ハンドラ内で直接使う場合のみを想定した機能で、そこから呼ばれる
        関数の中まで自動的に有効になるわけではない。呼び出し元の文脈を追跡していないため)。"""
        placeholders = [self._default_value(param.type_name) for param in func.params]
        with self._bound_params(func.params, placeholders):
            self._validate_block(func.body, in_loop=False, in_switch=False, return_type=func.return_type)

    def _validate_block(self, stmts: list, in_loop: bool, in_switch: bool, return_type) -> None:
        for stmt in stmts:
            self._validate_stmt(stmt, in_loop, in_switch, return_type)

    def _validate_stmt(self, stmt, in_loop: bool, in_switch: bool, return_type) -> None:
        """in_loop/in_switch は break/continue が使える文脈にいるかを表す
        (break は while/for/switch の中、continue は while/for の中でのみ有効。
        break は最も内側の while/for/switch に、continue は switch を素通りして
        最も内側の while/for に効くという C/CAPL のスコープ規則を、While/For/Switch
        に入るたびにこの2フラグをどう更新して子ブロックへ渡すかで表現している。
        return_type は現在いる関数の戻り値の型 ("void"/"int"/"float")、on start/
        on timer/on message の直下では None (return 文自体が使えない文脈)。
        While/For/Switch のネストでは変わらないのでそのまま子ブロックへ引き継ぐ
        (関数本体に入るときだけ _validate_function() が新しい値を渡す)。
        _validate() は構築時に1回しか呼ばれない非再入の走査なので、他の検証と違い
        インスタンス属性ではなく引数で持ち回る (ループ/switch/関数のネストで値が
        変わるため、インスタンス属性だと再帰から戻った時に元の文脈に戻す処理が要る)。"""
        if isinstance(stmt, Call):
            self._validate_expr(stmt, require_value=False)
        elif isinstance(stmt, Assign):
            var = self._vars.get(stmt.name)
            if var is not None and var.is_array:
                raise DslSyntaxError(
                    f"{stmt.line}行目: 配列 '{stmt.name}' へ直接代入することはできません "
                    f"(要素ごとに '{stmt.name}[i] = ...;' の形で代入してください)"
                )
            if var is None:
                raise DslSyntaxError(
                    f"{stmt.line}行目: 未宣言の変数 '{stmt.name}' への代入です "
                    "(variables { int/float ...; } で宣言してください)"
                )
            self._validate_expr(stmt.expr)
        elif isinstance(stmt, IndexAssign):
            var = self._vars.get(stmt.name)
            if var is None or not var.is_array:
                raise DslSyntaxError(
                    f"{stmt.line}行目: 未宣言の配列 '{stmt.name}' への代入です "
                    f"(variables {{ byte {stmt.name}[size]; }} で宣言してください)"
                )
            self._validate_expr(stmt.index)
            self._validate_expr(stmt.expr)
        elif isinstance(stmt, If):
            self._validate_expr(stmt.cond)
            self._validate_block(stmt.then_block, in_loop, in_switch, return_type)
            if stmt.else_block is not None:
                self._validate_block(stmt.else_block, in_loop, in_switch, return_type)
        elif isinstance(stmt, While):
            self._validate_expr(stmt.cond)
            self._validate_block(stmt.body, in_loop=True, in_switch=False, return_type=return_type)
        elif isinstance(stmt, For):
            if stmt.init is not None:
                self._validate_stmt(stmt.init, in_loop, in_switch, return_type)
            if stmt.cond is not None:
                self._validate_expr(stmt.cond)
            self._validate_block(stmt.body, in_loop=True, in_switch=False, return_type=return_type)
            if stmt.update is not None:
                self._validate_stmt(stmt.update, in_loop, in_switch, return_type)
        elif isinstance(stmt, Switch):
            self._validate_expr(stmt.expr)
            # continue は switch を素通りして外側の while/for に効くため in_loop は
            # そのまま引き継ぎ、break は switch 自身が受け止めるので in_switch=True。
            for entry in stmt.body:
                if isinstance(entry, CaseLabel):
                    continue
                self._validate_stmt(entry, in_loop, True, return_type)
        elif isinstance(stmt, Return):
            if return_type is None:
                raise DslSyntaxError(f"{stmt.line}行目: return は関数の中でのみ使えます")
            if return_type == "void":
                if stmt.expr is not None:
                    raise DslSyntaxError(
                        f"{stmt.line}行目: void 関数の return に値を付けることはできません"
                    )
            else:
                if stmt.expr is None:
                    raise DslSyntaxError(
                        f"{stmt.line}行目: '{return_type}' を返す関数の return には"
                        "値が必要です"
                    )
                self._validate_expr(stmt.expr)
        elif isinstance(stmt, Break):
            if not (in_loop or in_switch):
                raise DslSyntaxError(
                    f"{stmt.line}行目: break は while/for/switch の中でのみ使えます"
                )
        elif isinstance(stmt, Continue):
            if not in_loop:
                raise DslSyntaxError(
                    f"{stmt.line}行目: continue は while/for の中でのみ使えます"
                )
        else:
            raise DslSyntaxError(f"未対応の文です: {stmt!r}")

    def _validate_expr(self, node, require_value: bool = True) -> None:
        """require_value=False は「この Call は文として呼ばれていて、戻り値を捨てる」
        ことを表す (_validate_stmt() の Call 分岐からのみ渡される)。それ以外の全ての
        呼び出し元 (デフォルト True) は戻り値を実際に使う文脈 (代入の右辺・条件式・
        他の呼び出しの引数等) なので、void 関数をそこで呼ぶのは実行前検証で弾く
        (void 関数は _call_user_function() が None を返すため、そのまま数値扱いされると
        _coerce() の int(None) が未処理の TypeError になってしまう。配列の裸参照を
        値文脈で弾いたのと同じ理由・同じパターン)。"""
        if isinstance(node, Call):
            entry = self._builtins.get(node.name)
            if entry is not None:
                min_args, _fn = entry
                if len(node.args) < min_args:
                    raise DslSyntaxError(
                        f"{node.line}行目: '{node.name}' には少なくとも{min_args}個の引数が"
                        f"必要です ({len(node.args)}個指定されています)"
                    )
                if require_value and node.name in self._VOID_BUILTINS:
                    raise DslSyntaxError(
                        f"{node.line}行目: '{node.name}' の戻り値は式の中では使えません"
                    )
                for arg in node.args:
                    # 配列を丸ごと関数に渡す (send(data) 等) のは、直接の引数としてだけ許可する
                    # (_flatten_bytes() が list をそのまま展開できるのはここだけ)。
                    # data + 1 や total = data のように配列がネストした式・代入に紛れ込むと
                    # list に対して算術・比較・型変換が走って TypeError や意味のない False
                    # 判定になってしまうため、直接引数以外は下の Var の通常チェックで弾く。
                    if isinstance(arg, Var):
                        arg_var = self._vars.get(arg.name)
                        if arg_var is not None and arg_var.is_array:
                            continue
                    self._validate_expr(arg)
                return
            func = self._functions.get(node.name)
            if func is not None:
                if len(node.args) != len(func.params):
                    raise DslSyntaxError(
                        f"{node.line}行目: 関数 '{node.name}' には{len(func.params)}個の"
                        f"引数が必要です ({len(node.args)}個指定されています)"
                    )
                if require_value and func.return_type == "void":
                    raise DslSyntaxError(
                        f"{node.line}行目: void 関数 '{node.name}' の戻り値は"
                        "式の中では使えません"
                    )
                for arg in node.args:
                    self._validate_expr(arg)
                return
            raise self._unknown_function_error(node)
        if isinstance(node, Var):
            var = self._vars.get(node.name)
            if var is None:
                raise DslSyntaxError(
                    f"{node.line}行目: 未宣言の変数 '{node.name}' "
                    "(variables { int/float/byte ...; } で宣言してください)"
                )
            if var.is_array:
                raise DslSyntaxError(
                    f"{node.line}行目: 配列 '{node.name}' は単体の式としては使えません "
                    f"(要素参照は '{node.name}[i]'、配列全体を渡すのは send(...) 等の"
                    "関数呼び出しの直接の引数としてのみ使えます)"
                )
            return
        if isinstance(node, Index):
            var = self._vars.get(node.name)
            if var is None or not var.is_array:
                raise DslSyntaxError(
                    f"{node.line}行目: 未宣言の配列 '{node.name}' "
                    f"(variables {{ byte {node.name}[size]; }} で宣言してください)"
                )
            self._validate_expr(node.index)
            return
        if isinstance(node, TimerName):
            return  # タイマー名は識別子そのものなので、宣言済み変数チェックの対象外
        if isinstance(node, ThisAccess):
            if not self._in_message_handler:
                raise DslSyntaxError(
                    f"{node.line}行目: this は on message ブロック内でのみ使えます"
                )
            entry = self._this_members.get(node.member)
            if entry is None:
                raise self._unknown_this_member_error(node)
            expected, _fn = entry
            if len(node.args) != expected:
                raise DslSyntaxError(
                    f"{node.line}行目: this.{node.member} には{expected}個の引数が"
                    f"必要です ({len(node.args)}個指定されています)"
                )
            # 子ノード (引数式) の走査は下の共通ループにフォールスルーさせる
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
