"""
CAPL 風 DSL (`.capl` スクリプト) の字句解析・構文解析・インタプリタ。

capl_api.py の Python API 層 (中間案) に対する「本格対応」案。設計合意時のメモ通り、
UDS 送受信の実体は capl_api.CaplContext をそのままランタイム層として再利用する
(パーサが構文木を解釈して最終的に呼ぶのが CaplContext のメソッドになる)。

対応する構文:

    variables { int x; float y = 1.5; byte data[8]; message 0x123 msg; }
        // 省略可。1ファイルに1つまで、on start/on timer/on message より前に書くこと
    on start { ... }
    on timer <name> { ... }
    on message <id> { ... }

ブロック内は以下の文が書ける:
    - 関数呼び出し文: send(...); / write(...); など
    - 代入文: x = x + 1; / x += 1; / x++; / x--;  （x は variables{} で宣言済みで
      あること。複合代入 (+= -= *= /= %=) とインクリメント/デクリメント (++/--、
      後置のみ) はパーサーが x = x <op> 1 の代入文に脱糖する。式の中では使えず、
      文として、または for の初期化/更新句としてのみ使える）
    - 配列要素への代入文: data[i] = expr;  （data は variables{} で配列として
      宣言済みであること。要素型は byte/int/float のいずれか）
    - message 変数のフィールドへの代入文: msg.dlc = expr; / msg.byte(n) = expr;
      （msg.id への代入は不可、読み取り専用。下記参照）
    - if (expr) { ... } [else if (expr) { ... }]* [else { ... }]
    - while (expr) { ... }
    - for (x = 0; x < 10; x++) { ... }  （初期化/条件/更新はいずれも省略可）
    - break; / continue;  （break は while/for/switch の中、continue は while/for
      の中でのみ使える。continue は switch を素通りして外側の while/for に効く）
    - switch (expr) { case N: ... break; case M: ... default: ... }  （C/CAPL と
      同じフォールスルー動作。case の値は整数定数のみ）
    - return; / return expr;  （ユーザー定義関数の中でのみ使える。下記参照）
    - ローカル宣言: int x; / int x = expr; / byte data[n]; / message <id> m;
      （ユーザー定義関数の**直接の本体でのみ** 書ける。if/while/for/switch の中には
      書けない (パース時にエラー)。variables{} と同じ書き方だが `variables { }` では
      包まない。下記「ユーザー定義関数」参照）

式は四則演算 (+ - * / %)・比較 (== != < > <= >=)・論理 (&& || !)・
ビット演算 (& | ^ ~ << >>、C/CAPL と同じ優先順位。複合代入 &=/|=/^=/<<=/>>= は
非対応、`x = x & y;` のように書く)・丸括弧・関数呼び出し・変数参照・
配列要素参照 (data[i])・message 変数のフィールド読み取り
(msg.dlc/msg.byte(n)/msg.id)・数値/文字列リテラル・this.byte(n)/this.id/this.dlc
(on message ハンドラ内限定、下記参照) に対応する（構造体は対象外）。配列を
添字なしでそのまま参照すると (例: send(data)) 全要素のコピーを返す。send()/
send_can() はこれを検出して配列の全バイトをペイロードに展開するので、CAPL の
byte 配列をメッセージのペイロードとして丸ごと送る書き方に近い形で使える
(_flatten_bytes() 参照)。同様に message 変数を添字なしでそのまま参照できるのは
output(...) の直接の引数としてのみで、output(msg) が msg.dlc バイト分を生の CAN
フレームとして送る (_output_message() 参照。UDS 応答待ちの無い send_can() と同じ
「投げっぱなし」送信)。

message 変数 (`message <id> name;`):

    variables { message 0x123 msg; }
    on start {
        msg.byte(0) = 0x10;
        msg.dlc = 1;
        output(msg);
    }

実際の CAPL の message 型に近い書き味で、CAN フレームを組み立てて送る用途に使う
(byte 配列 + send_can() よりも「1つの送信メッセージ」という単位がはっきりする
書き方)。can_id は宣言時に固定 (後から書き換える機能は対象外)。dlc は既定 8、
data (8バイト、全て0) を持つ。フィールドは msg.dlc (0-8、範囲外は実行時に
スクリプト中断)・msg.byte(n) (0-7、範囲外は実行時にスクリプト中断)・msg.id
(読み取り専用) の3つのみ。variables{} のグローバル宣言、またはユーザー定義関数の
直接の本体でのローカル宣言として使える (byte 配列と同じ制約: if/while/for/switch
の中には書けない、理由も同じ。関数内でローカル宣言した場合は呼び出しごとに独立した
新しい message になる)。

ユーザー定義関数:

    int add(int a, int b) { return a + b; }
    void logRetry(int n) { ... }   // void 関数は return; か何も return せず終了

トップレベル (variables{}/on ... と同じ階層) にいくつでも書ける。型は戻り値・
仮引数とも int/float/byte のみ (void は戻り値にのみ使える。配列は戻り値にも
仮引数にもできない、byte スカラーは可)。定義順に関係なく呼び出せる (前方参照・
相互再帰も可。全関数の名前を実行前に一括登録してから各ブロック・各関数本体を
検証する)。void 関数の戻り値を式の中で使おうとする (例: `x = voidFunc();`) のは
実行前検証でエラーになる。int/float/byte を返す関数が return を一度も実行せずに
本体の最後まで到達した場合は (これは静的には検出しない。分岐を網羅する制御
フロー解析はしていないため) 実行時に capl_api.ScriptAbort で中断する。

関数本体では、仮引数に加えて `int x;`/`byte data[n];` のようなローカル変数宣言も
文として (variables{} で包まずに直接) 書ける。ローカル変数は**関数スコープ**
(ブロックスコープではない): 宣言した位置から関数の終わりまでどこからでも参照できる。
ただし**宣言できるのは関数の直接の本体だけ**で、if/while/for/switch の中には
書けない (パース時にエラーになる)。これは意図的な制約: `_validate_stmt()` は
if/while/for/switch の全分岐を無条件に (実際にその回の呼び出しで実行されるか
どうかに関わらず) 辿って検証するのに対し、`_exec_stmt()` はその回の呼び出しで
実際にその分岐が実行された場合にしかローカル変数を束縛しない。もし分岐の中での
宣言を許すと、宣言している分岐が実行されない呼び出しに限って、検証は通るのに
実行時にクラッシュする (未宣言の変数エラー、最悪 KeyError) という、
"副作用が起きる前に全て検証する" という設計の目的を無効化する失敗パターンに
なってしまう。関数の直接の本体は (前方に return 等の早期脱出が無い限り) 常に
同じ順序で無条件に実行されるので、宣言を常にそこに限定すればこの食い違いは
構造的に起こり得ない。ループ内で毎回リセットしたい作業用変数は、ループの外
(関数の直接の本体) で宣言してからループの中で代入する。仮引数・ローカル変数とも、
その関数の中でだけ同名のグローバル変数をシャドーイングし、呼び出しから戻ると
元の値に復元される (再帰呼び出しでも Python の呼び出しスタックを使って各呼び出し
フレームが独立して正しく退避・復元される、Interpreter._bound_params()/
_declare_local() 参照)。仮引数・ローカル変数とも on start/on timer/on message や
variables{} の中では書けない (関数の中限定)。

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
    # ビットシフトも同じ理由で LT/GT より前に置く (先に置かないと "<<" が LT,LT の
    # 2トークンに割れてしまう)。
    ("LSHIFT", r"<<"),
    ("RSHIFT", r">>"),
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
    # ビット単項/2項演算子。AMP/PIPE は AND(&&)/OR(||) より後ろ (& が && の後半を
    # 食わないように、&& を先に判定させる)。CARET/TILDE は他と衝突しないので
    # 順序は問わない。
    ("AMP", r"&"),
    ("PIPE", r"\|"),
    ("CARET", r"\^"),
    ("TILDE", r"~"),
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

_VAR_TYPES = ("int", "float", "byte")  # スカラー変数・仮引数・戻り値に使える型
# byte はスカラーでも配列 (byte[固定長]) でも使えるので、両者の型キーワード集合は
# 完全に一致する (int/float/byte のどれも両方の書き方ができる)。以前は byte が
# 配列専用だったため別々の定数だったが、byte をスカラーにも開放したことで
# 区別する意味が無くなった。_ARRAY_ELEMENT_TYPES という名前は既存コードとの
# 互換のため残す。
_ARRAY_ELEMENT_TYPES = _VAR_TYPES  # 配列の要素型として使える型 (int[]/float[]/byte[])
_DECL_TYPES = _ARRAY_ELEMENT_TYPES  # 変数宣言 (variables{}・ローカル共通) の先頭に来る型キーワード
# 宣言文の先頭に来うるキーワード全体 (int/float/byte の VarDecl/ArrayDecl に加え、
# message <id> name; の MessageDecl も同じ位置に書けるため)。_parse_statement() が
# ローカル宣言文の受理判定に使う (_parse_decl_stmt() 自体は _at_ident("message") で
# 自己判定するので、こちらは「関数の直接の本体でだけ許可する」判定専用)。
_DECL_START_KEYWORDS = _DECL_TYPES + ("message",)
_FUNC_RETURN_TYPES = ("void",) + _VAR_TYPES  # 関数の戻り値の型 (void/int/float/byte。配列は返せない)
_COMPARISON_TOKENS = {"LT": "<", "GT": ">", "LE": "<=", "GE": ">="}
_SHIFT_TOKENS = {"LSHIFT": "<<", "RSHIFT": ">>"}
_ADDITIVE_TOKENS = {"PLUS": "+", "MINUS": "-"}
_MULTIPLICATIVE_TOKENS = {"STAR": "*", "SLASH": "/", "PERCENT": "%"}
# Interpreter._eval_binop() が使う、演算子文字列 -> 実装関数の辞書 (& | ^ は
# 「int に変換してから Python の演算子を適用する」以外の差が無いので、if を
# 3つ並べて演算子だけ変える代わりに1つの辞書引きにまとめる)。
_BITWISE_BINOPS = {
    "&": lambda a, b: a & b,
    "|": lambda a, b: a | b,
    "^": lambda a, b: a ^ b,
}
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
    """変数参照式 (int/float/byte、variables{} での宣言が必要)。"""
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
    op: str  # "+" "-" "*" "/" "%" "==" "!=" "<" ">" "<=" ">=" "&&" "||" "&" "|" "^" "<<" ">>"
    left: object  # Expr
    right: object  # Expr
    line: int


@dataclass
class UnaryOp:
    op: str  # "-" "!" "~"
    operand: object  # Expr
    line: int


@dataclass
class VarDecl:
    type_name: str  # "int" | "float" | "byte"
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
    """`byte name[size];` / `int name[size] = {v0, v1, ...};` / `float name[n];` の
    配列宣言。type_name は要素型 ("byte"|"int"|"float")。size は初期化リストが
    あればその長さを、無ければ明示された添字の値を使う (両方指定時は初期化リストが
    size を超えていないことをパース時に確認する)。"""
    type_name: str  # "byte" | "int" | "float" (配列の要素型)
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
class MessageDecl:
    """`message <id> name;` の宣言。実際の CAPL の message 型に近い書き味で、
    CAN フレームを組み立てて output() で送る用途に使う (byte 配列 + send()/
    send_can() よりも「1つの送信メッセージ」という単位がはっきりする書き方)。
    can_id は宣言時に固定 (CAPL 同様、後から書き換える機能は対象外)。dlc は
    既定 8、data (8バイト、全て0) と合わせて Interpreter 側で _MessageValue として
    実体化する (_make_message_value() 参照)。variables{} のグローバル宣言、または
    関数の直接の本体でのローカル宣言として使える (ArrayDecl と同じ制約: if/while/
    for/switch の中には書けない、_parse_nested_block() 参照。理由も同じ)。"""
    can_id: int
    name: str
    line: int


@dataclass
class MemberAccess:
    """`name.dlc` / `name.byte(n)` / `name.id` の読み取り式。message 変数専用
    (this.byte(n) 等とは別物: this は on message ハンドラの「直近受信フレーム」用、
    こちらは名前付きの message 変数のフィールドを読む)。member は "dlc"/"byte"/"id"、
    args は byte(n) の n (dlc/id は空リスト)。"""
    name: str
    member: str
    args: list  # list[Expr]
    line: int


@dataclass
class MemberAssign:
    """`name.dlc = expr;` / `name.byte(n) = expr;` の代入文。member は "dlc"/"byte"
    のみ (id は読み取り専用、_validate_stmt() が弾く)。"""
    name: str
    member: str
    args: list  # list[Expr]
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
    """ユーザー定義関数の仮引数。型は int/float/byte のスカラーのみ (配列は仮引数
    にできない。配列は variables{} のグローバル宣言か、関数の直接の本体での
    ローカル宣言のみが置き場所、という制約と揃えてある)。byte 仮引数は _coerce()
    により呼び出しのたびに 0-255 にマスクされる (byte スカラー変数と同じ)。"""
    type_name: str  # "int" | "float" | "byte"
    name: str
    line: int


@dataclass
class FuncDecl:
    """`void name(int a, float b) { ... }` のようなユーザー定義関数の宣言。
    return_type は "void"/"int"/"float"/"byte"。関数はトップレベル (variables{}/on ... と
    同じ階層) にいくつでも書け、on start/on timer/on message や他の関数から
    呼び出せる (前方参照・相互再帰も可、_functions は実行前に一括登録するため)。
    仮引数に加えて、body の直接の要素として `int x;`/`byte data[n];` のようなローカル
    変数宣言 (VarDecl/ArrayDecl) も混ぜて書ける。ローカル変数はブロックスコープでは
    なく関数スコープ (宣言した位置から関数の終わりまでどこからでも参照できる) だが、
    宣言できるのは body の直接の要素としてのみで、if/while/for/switch の中には
    書けない (パーサーが _parse_nested_block() でこれを強制する)。理由: 検証
    (_validate_stmt()) は条件分岐の全枝を無条件に辿るが、実行 (_exec_stmt()) は
    実際に実行された枝でしかローカル変数を束縛しないため、分岐の中だけで宣言を
    許すと検証は通るのに実行時にクラッシュしうる。body が常に無条件・同じ順序で
    実行されることを保証することで、この食い違いを構造的に防ぐ。仮引数・ローカル
    変数とも on start/on timer/on message や variables{} の中では書けない
    (関数の中限定)。"""
    return_type: str  # "void" | "int" | "float" | "byte"
    name: str
    params: list  # list[Param]
    body: list  # list[Stmt]  (VarDecl/ArrayDecl が body の直接の要素として混在しうる。
                 # if/while/for/switch の内側には混在しない、_parse_nested_block() 参照)
    line: int


@dataclass
class Return:
    """`return;` (void 関数用) / `return expr;` (int/float/byte を返す関数用)。
    関数の外 (on start/on timer/on message) では使えない (_validate_stmt() 参照)。"""
    expr: object  # Expr | None
    line: int


@dataclass
class Script:
    variables: list  # list[VarDecl | ArrayDecl | MessageDecl]  (`variables { ... }` ブロック。無ければ空)
    on_start: list  # list[list[Stmt]]  (複数の `on start` は順に実行)
    on_timer: dict  # name(str) -> list[Stmt]
    on_message: dict  # can_id(int) -> list[Stmt]
    functions: list  # list[FuncDecl]  (トップレベルのユーザー定義関数。無ければ空)


class _Parser:
    def __init__(self, tokens: list[Token]):
        self._tokens = tokens
        self._pos = 0
        # ローカル変数宣言文 (`int x;` 等) が今どこまで書けるかを表す3値の状態。
        # None: 関数の外 (variables{}/on start 等)。ローカル宣言は使えず、下の
        #       _parse_statement() では「文として不正なトークン」の通常のエラーになる。
        # True: 関数の「直接の」本体。ローカル宣言を文として受け付ける。
        # False: 関数の中だが if/while/for/switch でネストした本体
        #        (_suspend_local_decls() 参照)。「関数の中だがネストが深すぎる」という
        #        より具体的なエラーを出すために、「そもそも関数の外」(None) と区別する。
        #
        # ローカル宣言を関数の直接の本体だけに限定するのは意図的な制約: 条件分岐の中
        # だけで宣言を許すと、全分岐を無条件に辿る検証と、その回に実際に実行された
        # 分岐でしか束縛しない実行との間で食い違いが起き、検証を通っても実行時に
        # 未束縛エラーになりうる (_suspend_local_decls() のコメント参照)。関数はネストして
        # 定義できない文法 (トップレベルにしか書けない) なので、スタックではなく
        # 単純な1つの状態で足りる。
        self._local_decl_scope: Optional[bool] = None

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
        type_tok = self._advance()  # void/int/float/byte (呼び出し元で確認済み)
        name_tok = self._expect("IDENT")
        self._expect("LPAREN")
        params = self._parse_arg_list(lambda i: self._parse_param())
        self._expect("RPAREN")
        # 本体の解析中だけ _local_decl_scope=True にして、int x; のようなローカル変数
        # 宣言文を _parse_statement() が受け付けられるようにする (関数はネストして
        # 定義できないので、単純に立てて finally で戻すだけでよい。ネストした
        # if/while/for/switch の中は _suspend_local_decls() が一時的に False にする)。
        self._local_decl_scope = True
        try:
            body = self._parse_block()
        finally:
            self._local_decl_scope = None
        return FuncDecl(type_tok.value, name_tok.value, params, body, type_tok.line)

    def _parse_param(self) -> Param:
        type_tok = self._peek()
        if type_tok.kind != "IDENT" or type_tok.value not in _VAR_TYPES:
            raise DslSyntaxError(
                f"{type_tok.line}行目: 引数の型 (int/float/byte) を期待しましたが "
                f"'{type_tok.value}' でした"
            )
        self._advance()
        name_tok = self._expect("IDENT")
        return Param(type_tok.value, name_tok.value, name_tok.line)

    def _parse_var_block(self) -> list:
        self._expect("LBRACE")
        decls = []
        while self._peek().kind != "RBRACE":
            decls.append(self._parse_decl_stmt())
        self._expect("RBRACE")
        return decls

    def _parse_decl_stmt(self):
        """変数宣言 (VarDecl/ArrayDecl、_parse_var_decl() 参照) と message 宣言
        (MessageDecl、_parse_message_decl() 参照) をまとめて解釈する共通エントリ
        ポイント。variables{} ブロック (_parse_var_block()) と、関数の直接の本体
        でのローカル宣言 (_parse_statement()) の両方から使う。"""
        if self._at_ident("message"):
            return self._parse_message_decl()
        return self._parse_var_decl()

    def _parse_message_decl(self) -> MessageDecl:
        msg_tok = self._expect_ident("message")
        id_tok = self._advance()
        if id_tok.kind not in ("HEXNUM", "INT"):
            raise DslSyntaxError(f"{id_tok.line}行目: message の後には CAN ID (数値) が必要です")
        can_id = int(id_tok.value, 0)
        name_tok = self._expect("IDENT")
        self._expect("SEMI")
        return MessageDecl(can_id, name_tok.value, msg_tok.line)

    def _parse_var_decl(self):
        type_tok = self._peek()
        if type_tok.kind != "IDENT" or type_tok.value not in _DECL_TYPES:
            raise DslSyntaxError(
                f"{type_tok.line}行目: 変数の型 (int/float/byte) を期待しましたが "
                f"'{type_tok.value}' でした"
            )
        self._advance()
        name_tok = self._expect("IDENT")
        if self._peek().kind == "LBRACKET":
            return self._parse_array_decl(type_tok, name_tok)
        init = None
        if self._peek().kind == "ASSIGN":
            self._advance()
            init = self._parse_expr()
        self._expect("SEMI")
        return VarDecl(type_tok.value, name_tok.value, init, name_tok.line)

    def _parse_array_decl(self, type_tok: Token, name_tok: Token) -> ArrayDecl:
        """`byte name[size];` / `int name[size] = {v0, v1, ...};` / `float name[] = {...};`
        (size 省略時は初期化リストの長さを使う)。要素型は byte/int/float いずれも
        使える (_ARRAY_ELEMENT_TYPES 参照。呼び出し元の _parse_var_decl() が既に
        type_tok.value in _DECL_TYPES であることを確認済みなので、ここで型を
        再チェックする必要はない)。"""
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
        return ArrayDecl(type_tok.value, name_tok.value, size, init, name_tok.line)

    # ---- 文 ----
    def _parse_block(self) -> list:
        self._expect("LBRACE")
        stmts = []
        while self._peek().kind != "RBRACE":
            stmts.append(self._parse_statement())
        self._expect("RBRACE")
        return stmts

    @contextmanager
    def _suspend_local_decls(self):
        """if/while/for/switch の本体のように、関数の直接の本体ではないブロックを解析する
        間だけ、ローカル変数宣言文 (int x; 等) を一時的に禁止する (_local_decl_scope 参照)。
        理由: _validate_stmt() は if/while/for/switch の全分岐を無条件に (実際にその回の
        呼び出しで実行されるかどうかに関わらず) 辿って VarDecl/ArrayDecl を self._vars に
        登録するのに対し、_exec_stmt() はその回の呼び出しで実際にその分岐が実行された
        場合にしか束縛しない。条件分岐の中でだけ宣言されたローカル変数を分岐の外や
        後続で参照すると、検証 (全分岐を無条件に辿るので登録済みに見える) は通るのに
        実行時に「未宣言の変数」(読み取り) や KeyError (代入) でクラッシュしうる ──
        まさに _validate() が実行前に防ごうとしている失敗パターンそのものになって
        しまう。ローカル宣言を関数の直接の本体 (常に無条件に実行される) だけに限定
        すれば、検証も実行も同じ本体リストを同じ順序で辿るだけなので、この食い違いは
        構造的に起こり得ない。

        現在の状態 (外側が「関数の外」(None) でも「関数の直接の本体」(True) でも)
        をどちらも一律 False (ネスト中) に変え、with を抜けたら元に戻す (else if の
        連鎖等、任意の深さのネストでも安全に動く)。_parse_nested_block()/_parse_switch()
        の両方から使う (switch は CaseLabel の混在があり _parse_block() をそのまま
        使えないため、_parse_nested_block() 経由にできず個別に with で包む)。"""
        outer = self._local_decl_scope
        self._local_decl_scope = False
        try:
            yield
        finally:
            self._local_decl_scope = outer

    def _parse_nested_block(self) -> list:
        """if/while/for の本体のように、関数の直接の本体ではないブロックを解析する
        (ローカル変数宣言文を一時的に禁止する理由は _suspend_local_decls() 参照)。"""
        with self._suspend_local_decls():
            return self._parse_block()

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
        # 関数の直接の本体の中でだけ、int x; / byte data[n]; / message <id> m; のような
        # ローカル宣言を文として書ける (_parse_decl_stmt() は variables{} ブロックの
        # 中でも使う共通のパーサーで、VarDecl/ArrayDecl/MessageDecl を返して末尾の
        # セミコロンまで読み切る)。
        #
        # "message" は int/float/byte と違って、宣言の先頭以外の位置では普通の識別子
        # (message 型変数の名前) としても現れうる (int/float/byte は型キーワードとして
        # 常に予約されているため変数名になり得ないが、message にはその制約が無い)。
        # そのため `message.dlc = 1;` (message という名前の変数へのメンバー代入) や
        # `message[0] = 1;` を、`message <id> name;` の宣言と混同しないよう、
        # 「次のトークンが CAN ID (HEXNUM/INT) かどうか」で判別する (実際の宣言文法上、
        # message の直後には必ず CAN ID が来るため)。int/float/byte はこの追加判定の
        # 対象外 (常に宣言の先頭として扱う、従来通りの挙動)。
        decl_tok = self._peek()
        looks_like_message_decl = decl_tok.value != "message" or self._peek(1).kind in ("HEXNUM", "INT")
        if decl_tok.kind == "IDENT" and decl_tok.value in _DECL_START_KEYWORDS and looks_like_message_decl:
            if self._local_decl_scope is True:
                return self._parse_decl_stmt()
            if self._local_decl_scope is False:
                # 関数の中ではあるが if/while/for/switch でネストしすぎている
                # (_suspend_local_decls() 参照)。生の「文として不正なトークン」より
                # 具体的な理由を示す。self._local_decl_scope is None (そもそも関数の外)
                # のときはこの分岐に入らず、下の通常の「文として不正なトークン」に落ちる。
                tok = self._peek()
                raise DslSyntaxError(
                    f"{tok.line}行目: ローカル変数宣言は関数の直接の本体でのみ書けます "
                    "(if/while/for/switch の中には書けません。関数の先頭付近で"
                    "宣言してください)"
                )
        tok = self._peek()
        if tok.kind == "IDENT" and self._peek(1).kind == "LBRACKET":
            return self._parse_index_assignment()
        if tok.kind == "IDENT" and self._peek(1).kind == "DOT":
            return self._parse_member_assignment()
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
        then_block = self._parse_nested_block()
        else_block = None
        if self._at_ident("else"):
            self._advance()
            if self._at_ident("if"):
                else_block = [self._parse_if()]
            else:
                else_block = self._parse_nested_block()
        return If(cond, then_block, else_block, if_tok.line)

    def _parse_while(self) -> While:
        while_tok = self._expect_ident("while")
        self._expect("LPAREN")
        cond = self._parse_expr()
        self._expect("RPAREN")
        body = self._parse_nested_block()
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
        body = self._parse_nested_block()
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
        # switch 本体も if/while/for の本体と同じ「関数の直接の本体ではないブロック」
        # なので、ローカル変数宣言文は許可しない (_suspend_local_decls() 参照。switch は
        # CaseLabel の混在があり _parse_block() をそのまま使えないため、
        # _parse_nested_block() を経由できず個別に with で包む)。
        with self._suspend_local_decls():
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

    # ---- 式 (優先順位: || > && > | > ^ > & > ==,!= > <,>,<=,>= > <<,>> > +,- >
    #        *,/,% > 単項 > 一次。C/CAPL と同じ優先順位) ----
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
        left = self._parse_bitor()
        while self._peek().kind == "AND":
            op_tok = self._advance()
            right = self._parse_bitor()
            left = BinOp("&&", left, right, op_tok.line)
        return left

    def _parse_bitor(self):
        left = self._parse_bitxor()
        while self._peek().kind == "PIPE":
            op_tok = self._advance()
            right = self._parse_bitxor()
            left = BinOp("|", left, right, op_tok.line)
        return left

    def _parse_bitxor(self):
        left = self._parse_bitand()
        while self._peek().kind == "CARET":
            op_tok = self._advance()
            right = self._parse_bitand()
            left = BinOp("^", left, right, op_tok.line)
        return left

    def _parse_bitand(self):
        left = self._parse_equality()
        while self._peek().kind == "AMP":
            op_tok = self._advance()
            right = self._parse_equality()
            left = BinOp("&", left, right, op_tok.line)
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
        left = self._parse_shift()
        while self._peek().kind in _COMPARISON_TOKENS:
            op_tok = self._advance()
            right = self._parse_shift()
            left = BinOp(_COMPARISON_TOKENS[op_tok.kind], left, right, op_tok.line)
        return left

    def _parse_shift(self):
        left = self._parse_additive()
        while self._peek().kind in _SHIFT_TOKENS:
            op_tok = self._advance()
            right = self._parse_additive()
            left = BinOp(_SHIFT_TOKENS[op_tok.kind], left, right, op_tok.line)
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
        if tok.kind == "TILDE":
            self._advance()
            return UnaryOp("~", self._parse_unary(), tok.line)
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
            if self._peek(1).kind == "DOT":
                return self._parse_member_access()
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

    def _parse_member_access(self) -> MemberAccess:
        """`name.dlc` / `name.byte(n)` / `name.id` (message 変数のフィールド読み取り)。
        `this.xxx` (_parse_this_access()) とは別物: this は識別子そのものが特別な
        キーワードだが、こちらは通常の識別子 (message 変数名) に対する `.member` な
        ので、name も読む。式としての読み取り (_parse_primary()) と、代入文の
        左辺 (_parse_member_assignment()) の両方から使う。"""
        name_tok = self._expect("IDENT")
        self._expect("DOT")
        member_tok = self._expect("IDENT")
        args = []
        if self._peek().kind == "LPAREN":
            self._advance()
            args = self._parse_arg_list(lambda i: self._parse_expr())
            self._expect("RPAREN")
        return MemberAccess(name_tok.value, member_tok.value, args, name_tok.line)

    def _parse_member_assignment(self) -> MemberAssign:
        """`name.dlc = expr;` / `name.byte(n) = expr;`。左辺の読み方は
        _parse_member_access() と共通なので、それを再利用してから `= expr;` を
        追加で読む (id への代入等、代入できないメンバへの代入は _validate_stmt()
        が実行前に弾く。パーサーはメンバ名の意味を判定しない)。"""
        access = self._parse_member_access()
        self._expect("ASSIGN")
        expr = self._parse_expr()
        self._expect("SEMI")
        return MemberAssign(access.name, access.member, access.args, expr, access.line)


def parse(source: str) -> Script:
    return _Parser(tokenize(source)).parse()


# ---------------------------------------------------------------------------
# インタプリタ
# ---------------------------------------------------------------------------


def _iter_child_exprs(node):
    """BinOp/UnaryOp/ThisAccess/Index/MemberAccess の子ノード (部分式) を列挙する。
    Call/Var/TimerName/リテラルは子を持たないので空を返す (Call は _validate_expr()
    側で個別に引数を辿るため、ここには含めない)。_reject_calls()/_validate_expr() の
    再帰下降で同じ形の走査を2箇所に個別に書かないよう、ここに1本化する。"""
    if isinstance(node, BinOp):
        return (node.left, node.right)
    if isinstance(node, UnaryOp):
        return (node.operand,)
    if isinstance(node, ThisAccess):
        return tuple(node.args)
    if isinstance(node, Index):
        return (node.index,)
    if isinstance(node, MemberAccess):
        return tuple(node.args)
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
class _MessageValue:
    """message 変数 (MessageDecl) の実行時状態。can_id は宣言時に固定 (CAPL の
    message 型と同じ、後から書き換える機能は対象外)。data は常に8要素の
    list[int] (0-255) を持ち、dlc (0-8) が output() で実際に送る先頭バイト数を
    決める (dlc を8未満にしても data の残りは保持される。CAPL の message 型と
    同じ考え方)。"""
    can_id: int
    dlc: int
    data: list  # 常に8要素、byte (0-255)


@dataclass
class _Variable:
    """宣言済み変数 (スカラーも配列も message も) の実行時状態。is_array で
    スカラーと配列を区別する (以前は type_name が "byte" かどうかで判定していたが、
    int/float 配列に対応するとスカラー int/float と配列 int/float の type_name が
    同じ文字列になってしまい判定できなくなったため、明示的なフィールドにした)。
    type_name は配列なら要素型 ("byte"/"int"/"float")、message 変数なら "message"
    固定 (is_array=False)。value はスカラーなら int/float/byte、配列なら list、
    message なら _MessageValue を持つ。スカラー・配列・message を別々の dict (self._vars/
    self._arrays 等) で持つと、「この名前は宣言済みか」「何者か」を毎回複数の dict の
    メンバーシップを OR して求める羽目になり判定漏れの温床になる (実際、配列サポート
    追加時にこのパターンが複数箇所で発生した) ため、1つの dict に統一している。"""
    type_name: str  # "int" | "float" | "byte" | "message"
    is_array: bool
    value: object  # int | float | list | _MessageValue


class Interpreter:
    """パース済み Script を実行する。UDS 送受信は capl_api.CaplContext に委譲する。"""

    def __init__(self, script: Script, ctx: capl_api.CaplContext, stop_event):
        self._script = script
        self._ctx = ctx
        self._stop_event = stop_event
        self._armed_timers: dict[str, float] = {}  # name -> 発火時刻 (time.monotonic())
        self._last_message: Optional[object] = None  # can.Message (on message ハンドラ内でのみ有効)
        self._vars: dict[str, _Variable] = {}  # スカラー・配列とも (_Variable.is_array 参照)
        # 現在実行/検証中の関数呼び出しフレームの退避辞書 (_bound_params() が内部で
        # 管理する saved と同じもの。関数の外では None)。仮引数の束縛だけでなく、
        # 関数本体中のローカル変数宣言文 (int x; 等、_exec_stmt()/_validate_stmt() の
        # VarDecl/ArrayDecl 分岐参照) もこの辞書に「シャドーイング前の値」を書き足して
        # いく。関数呼び出しは再帰・ネストしうるため、_bound_params() が with ブロックの
        # 前後でこの属性自体を退避・復元することで、Python の呼び出しスタックに乗せて
        # スタックのように振る舞わせている (インスタンス属性1つで済ませられるのは、
        # 検証は非再入・実行はネストのたびに正しく退避復元するため)。
        self._current_locals: Optional[dict] = None
        self._builtins = self._make_builtins()
        self._this_members = self._make_this_members()
        self._message_members = self._make_message_members()
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
    _VOID_BUILTINS = frozenset({
        "send", "send_can", "wait", "log", "write", "setTimer", "cancelTimer", "output",
    })

    # 配列/message 変数を裸で (添字/フィールドアクセス無しで) 直接の引数として渡せる
    # 組み込み関数名。_validate_expr() の Call 処理がこの集合に載っていない関数への
    # 裸参照を通常の Var チェックまで通す (弾く) ようにするためのもの。ここに
    # 載っていない関数 (例: wait_response()/log()) に配列/message を渡すと、実装が
    # その値を int/float 変換しようとして未処理の TypeError になりうる
    # (例: wait_response(msg) は float(msg_value) をタイムアウト秒数として呼ぼうと
    # してクラッシュする)。あらゆる組み込み関数に一律で裸参照を許してしまうと、
    # 「配列/message は send()/send_can()/output() の直接の引数としてのみ使える」
    # という README/docstring の主張自体が嘘になってしまうため、関数ごとに絞る。
    _ARRAY_ARG_BUILTINS = frozenset({"send", "send_can"})
    _MESSAGE_ARG_BUILTINS = frozenset({"output"})

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
            # message 変数 (MessageDecl) を丸ごと渡して生の CAN フレームとして送る。
            # 引数が実際に message 変数かどうかは _validate_expr() の output 専用
            # チェックが実行前に保証する (_output_message() 参照)。
            "output": (1, lambda args: self._output_message(args[0])),
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

    def _to_int_checked(self, value, line: int, what: str) -> int:
        """ビット演算子 (&/|/^/~/<</>>) から使う、行番号・文脈付きの int() 変換。
        float が NaN/±inf だと Python の int() は ValueError/OverflowError を送出する
        (通常の float 演算はオーバーフローしても例外を出さず黙って inf/nan になる
        ため、例えば float を15回自乗するだけで到達しうる)。他のランタイムエラー
        (ゼロ除算等) と同様、未処理のまま app.py の汎用 except まで飛ばすと行番号も
        文脈も無い生のメッセージになってしまうため、ここで capl_api.ScriptAbort に
        変換する。what は「どちらのオペランドの変換に失敗したか」をエラーメッセージに
        含めるための説明 (例: "左辺"/"右辺"/"シフト量")。<</>> の左辺・右辺・
        シフト量負チェックをまとめて1つの except で処理すると、実際には左辺
        (シフト量ではない) が原因の場合でも「シフト量が負です」という誤診断に
        なってしまうため、呼び出し側でオペランドごとに個別に呼ぶこと。"""
        try:
            return int(value)
        except (ValueError, OverflowError) as exc:
            raise capl_api.ScriptAbort(
                f"{line}行目: {what}を整数に変換できません ({value})"
            ) from exc

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
            if var is not None and var.type_name == "message":
                # 配列と同じ理由 (上のコメント参照)。message 変数は _MessageValue
                # オブジェクトを持つため、そのまま int/float の初期値に流れ込むと
                # _coerce() が未処理の TypeError になる。
                raise DslSyntaxError(
                    f"{node.line}行目: variables の初期値で message 変数 '{node.name}' "
                    f"全体は参照できません (フィールドは '{node.name}.dlc'/"
                    f"'{node.name}.byte(n)' で読み取ってください)"
                )
        for child in _iter_child_exprs(node):
            self._reject_calls(child)

    def _init_variables(self) -> None:
        """宣言順に初期値式を検証・評価して self._vars に格納する。後続の宣言の初期値式が
        先に宣言した変数を参照することもできる（宣言済みかどうかは self._vars への
        追加順でそのまま決まる）。スカラー変数 (VarDecl)・配列 (ArrayDecl)・message
        変数 (MessageDecl) いずれも同じ self._vars に入れる (_Variable.is_array/
        type_name=="message" 参照) ので、名前空間は自然に共有され、同名の重複宣言は
        種類を問わず通常の重複宣言チェックに引っかかる。"""
        for decl in self._script.variables:
            if decl.name in self._vars:
                raise DslSyntaxError(f"{decl.line}行目: 変数 '{decl.name}' は既に宣言されています")
            if isinstance(decl, ArrayDecl):
                if decl.init is not None:
                    for item in decl.init:
                        self._reject_calls(item)
                values = self._eval_array_values(decl.init, decl.size, decl.type_name)
                self._vars[decl.name] = _Variable(decl.type_name, True, values)
                continue
            if isinstance(decl, MessageDecl):
                self._vars[decl.name] = _Variable("message", False, self._make_message_value(decl.can_id))
                continue
            if decl.init is not None:
                self._reject_calls(decl.init)
                value = self._eval(decl.init)
            else:
                value = self._default_value(decl.type_name)
            self._vars[decl.name] = _Variable(decl.type_name, False, self._coerce(value, decl.type_name))

    def _eval_array_values(self, init: Optional[list], size: int, elem_type: str) -> list:
        """ArrayDecl (variables{} のトップレベル配列宣言、および関数内のローカル配列宣言
        _exec_stmt() の ArrayDecl 分岐参照) の初期化リストを評価し、サイズ分だけ
        既定値で埋めた list を返す共通ロジック。要素の型変換は elem_type に応じて
        _coerce_array_element() (実体はスカラーと同じ _coerce()、byte なら 0-255
        マスク) に委ねる。初期化式に関数呼び出しを許すかどうか (_reject_calls の要否) は
        呼び出し元ごとに事情が異なる (variables{} は Interpreter 構築中に評価される
        ため不可、ローカル宣言は run() 開始後にしか実行されないため可、
        _validate_stmt() 参照) ので、ここでは扱わない。"""
        if init is not None:
            values = [self._coerce_array_element(self._eval(item), elem_type) for item in init]
        else:
            values = []
        values.extend([self._default_value(elem_type)] * (size - len(values)))
        return values

    @staticmethod
    def _make_message_value(can_id: int) -> _MessageValue:
        """MessageDecl から実行時の _MessageValue を作る (variables{} のグローバル
        宣言・関数内のローカル宣言・_validate_stmt() でのプレースホルダ生成の
        いずれからも使う共通ロジック)。dlc は既定 8、data は8バイト全て0。"""
        return _MessageValue(can_id, 8, [0] * 8)

    @staticmethod
    def _default_value(type_name: str):
        """初期化式を省略したスカラー変数・配列要素、および _validate_function() が
        仮引数を検証用に一時束縛する際のプレースホルダ値に使う、型ごとの既定値
        (float なら 0.0、それ以外〈int/byte〉は 0)。あちこちで同じ判定を別々に
        書かないよう1箇所にまとめる。"""
        return 0.0 if type_name == "float" else 0

    @staticmethod
    def _coerce(value, type_name: str):
        """代入・初期化のたびに宣言型 (int/float/byte) への変換を行う。C/CAPL の代入と
        同様、float を int 変数に代入すると 0 方向へ切り捨てられる (Python の int() は
        int(2.9)==2, int(-2.9)==-2 と C の (int) キャストと同じ丸め方向なのでそのまま使える)。
        これをやらないと `int half; half = 10 / 4;` が 2 ではなく 2.5 のまま half に
        入ってしまい、int 変数のつもりで書いた比較・分岐が期待通りに動かなくなる。

        byte は 0-255 に丸めるマスク (`& 0xFF`、_to_byte() と同じ処理) をかける。
        代入のたびにここを通るので、CAPL の byte/word/long のような固定幅整数型の
        ラップアラウンド (`byte b; b = 300;` が 44 になる、`b = -1;` が 255 になる
        等) を、byte 型に限って再現できる (Python の int 自体は多倍長で自然には
        ラップアラウンドしないため、この明示的なマスクが無いと際限なく増減する)。
        int/float は元々 CAPL の型ほど幅を持たない値を想定しているため、意図的に
        ラップアラウンドさせていない (16bit/32bit 固定幅にすると、DID 等
        0xFFFF を超える値を int で扱っている既存スクリプトが壊れてしまうため)。"""
        if type_name == "int":
            return int(value)
        if type_name == "float":
            return float(value)
        if type_name == "byte":
            return int(value) & 0xFF
        return value

    def _coerce_array_element(self, value, elem_type: str):
        """配列要素への値の型変換。_coerce() が byte/int/float いずれの要素型も
        扱えるので、スカラー変数と全く同じ変換ロジックにそのまま委譲する
        (以前は byte だけ _to_byte() を個別に呼ぶ特別扱いだったが、_coerce() 自身が
        byte のマスク処理を持つようになったので不要になった)。配列の読み書き
        (初期化リスト・IndexAssign・ローカル配列宣言) 全てがここを通ることで、
        要素型ごとの変換ロジックを1箇所にまとめる。"""
        return self._coerce(value, elem_type)

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
        if isinstance(node, MemberAccess):
            return self._eval_member_access(node)
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
        共通ヘルパー。宣言済みの配列の _Variable 本体 (list ではなく _Variable を返す
        のは、書き込み側が要素型ごとの変換 (_coerce_array_element()) に var.type_name
        を必要とするため) と、範囲チェック済みの添字を返す (範囲外は
        capl_api.ScriptAbort、ゼロ除算等と同じ「スクリプト中断」扱い)。読み書きで
        同じ範囲チェックを別々に手書きすると、将来どちらか片方だけ修正されて挙動が
        食い違う保守リスクになるため、ここに1本化する。"""
        var = self._vars.get(name)
        if var is None or not var.is_array:
            raise DslSyntaxError(f"{line}行目: 未宣言の配列 '{name}'")
        idx = self._to_int(self._eval(index_node))
        if not (0 <= idx < len(var.value)):
            raise capl_api.ScriptAbort(
                f"{line}行目: 配列 '{name}' の添字 {idx} が範囲外です (サイズ {len(var.value)})"
            )
        return var, idx

    def _eval_index(self, node: Index) -> int:
        var, idx = self._resolve_array_index(node.name, node.index, node.line)
        return var.value[idx]

    def _make_message_members(self) -> dict:
        """<message変数>.<member> の (必要な引数の個数, 読み取り実装, 書き込み実装) の
        辞書。_make_this_members()/_make_builtins() と同じ理由で、アリティ専用の別
        テーブルを持たない (別テーブルだと実装だけ引数を変えて片方の更新を忘れる、
        という保守リスクになるため)。書き込み不可のメンバ (id は読み取り専用) は
        書き込み実装を None にし、_exec_member_assign()/_validate_stmt() の
        MemberAssign 分岐がそのまま「代入できません」の判定に使う。読み取り・書き込み
        とも (msg, args, name, line) を受け取る形に揃えてあるのは、byte(n) の範囲外
        エラーメッセージに変数名・行番号が要るため。"""
        def read_byte(msg, args, name, line):
            idx = self._resolve_message_byte_index(msg, args, name, line)
            return msg.data[idx]

        def write_byte(msg, args, value, name, line):
            idx = self._resolve_message_byte_index(msg, args, name, line)
            msg.data[idx] = self._to_byte(value)

        def write_dlc(msg, args, value, name, line):
            dlc = self._to_int(value)
            if not (0 <= dlc <= 8):
                raise capl_api.ScriptAbort(
                    f"{line}行目: message '{name}' の dlc は0〜8の範囲である必要があります ({dlc})"
                )
            msg.dlc = dlc

        return {
            "id": (0, lambda msg, args, name, line: msg.can_id, None),
            "dlc": (0, lambda msg, args, name, line: msg.dlc, write_dlc),
            "byte": (1, read_byte, write_byte),
        }

    def _message_value(self, name: str, line: int) -> _MessageValue:
        """message 変数の値 (_MessageValue) を取り出す (MemberAccess/MemberAssign の
        両方が使う)。型・宣言済みかどうかは _validate_expr()/_validate_stmt() が
        実行前に検証済みなので通常はここに来ないが、_eval_this_access() 等と同じ
        考え方で保険として残しておく。"""
        var = self._vars.get(name)
        if var is None or var.type_name != "message":
            raise DslSyntaxError(f"{line}行目: 未宣言の message 変数 '{name}'")
        return var.value

    def _resolve_message_byte_index(self, msg: _MessageValue, args: list, name: str, line: int) -> int:
        """message 変数の byte(n) の添字を範囲チェックする、読み取り・書き込み双方が
        使う共通ヘルパー。配列の _resolve_array_index() と同じ理由 (読み書きで別々に
        手書きすると片方だけ直されて食い違う保守リスクになる) で1本化する。"""
        idx = self._to_int(self._eval(args[0]))
        if not (0 <= idx < len(msg.data)):
            raise capl_api.ScriptAbort(
                f"{line}行目: message '{name}' の byte({idx}) が範囲外です (0-{len(msg.data) - 1})"
            )
        return idx

    def _unknown_message_member_error(self, var_name: str, member: str, line: int) -> DslSyntaxError:
        known = "/".join(sorted(self._message_members))
        return DslSyntaxError(
            f"{line}行目: 未知の message メンバ '{var_name}.{member}' ({known} のみ対応)"
        )

    def _validate_message_member_arity(self, member: str, args: list, line: int, var_name: str) -> None:
        """message 変数の `.dlc`/`.byte(n)`/`.id` の引数個数を検証する
        (_validate_expr() の MemberAccess 分岐、_validate_stmt() の MemberAssign
        分岐の両方から使う)。未知のメンバ名もここで検出する。"""
        entry = self._message_members.get(member)
        if entry is None:
            raise self._unknown_message_member_error(var_name, member, line)
        expected, _read_fn, _write_fn = entry
        if len(args) != expected:
            raise DslSyntaxError(
                f"{line}行目: {var_name}.{member} には{expected}個の引数が必要です "
                f"({len(args)}個指定されています)"
            )

    def _eval_member_access(self, node: MemberAccess):
        # メンバ名・引数個数とも _validate_expr() が実行前に検証済みなので、通常は
        # ここに来ないが保険として残しておく (_call() の entry is None チェックと
        # 同じ考え方)。
        msg = self._message_value(node.name, node.line)
        entry = self._message_members.get(node.member)
        if entry is None:
            raise self._unknown_message_member_error(node.name, node.member, node.line)
        _arity, read_fn, _write_fn = entry
        return read_fn(msg, node.args, node.name, node.line)

    def _exec_member_assign(self, stmt: MemberAssign) -> None:
        # 書き込み可能なメンバかどうかも _validate_stmt() が実行前に検証済みなので、
        # 通常はここに来ないが保険として残しておく。
        msg = self._message_value(stmt.name, stmt.line)
        entry = self._message_members.get(stmt.member)
        if entry is None or entry[2] is None:
            raise self._unknown_message_member_error(stmt.name, stmt.member, stmt.line)
        _arity, _read_fn, write_fn = entry
        value = self._eval(stmt.expr)
        write_fn(msg, stmt.args, value, stmt.name, stmt.line)

    def _output_message(self, msg_value: _MessageValue) -> None:
        """output(msg) の実装。message 変数の現在の内容を dlc 分だけ生の CAN
        フレームとして送る (UDS 応答待ちの無い send_can() と同じ「投げっぱなし」
        送信)。引数が実際に message 変数であることは _validate_expr() の output
        専用チェックが実行前に保証している。"""
        self._ctx.send_can(msg_value.can_id, msg_value.data[:msg_value.dlc])

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
        if node.op in _BITWISE_BINOPS:
            return _BITWISE_BINOPS[node.op](
                self._to_int_checked(left, node.line, "左辺"),
                self._to_int_checked(right, node.line, "右辺"),
            )
        if node.op in ("<<", ">>"):
            # 左辺・右辺 (シフト量) は別々に _to_int_checked() を呼ぶ (まとめて1つの
            # try/except で捕捉すると、実際には左辺の変換失敗でも「シフト量が原因」と
            # 誤診断してしまうため)。シフト量が負かどうかも Python の << 自体の
            # ValueError に頼らず、変換が成功した後に明示的にチェックする (こうすると
            # 「変換できない」と「シフト量が負」を別のエラーとしてはっきり区別できる)。
            left_i = self._to_int_checked(left, node.line, "左辺")
            right_i = self._to_int_checked(right, node.line, "シフト量")
            if right_i < 0:
                raise capl_api.ScriptAbort(f"{node.line}行目: {node.op} のシフト量が負です")
            return left_i << right_i if node.op == "<<" else left_i >> right_i
        raise DslSyntaxError(f"{node.line}行目: 未対応の演算子 '{node.op}'")

    def _eval_unaryop(self, node: UnaryOp):
        value = self._eval(node.operand)
        if node.op == "-":
            return -value
        if node.op == "!":
            return 0 if self._truthy(value) else 1
        if node.op == "~":
            return ~self._to_int_checked(value, node.line, "オペランド")
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
        (や検証) の後にグローバル変数が恒久的に壊れてしまう。

        併せて self._current_locals をこの with ブロックの間だけ saved に向ける
        (呼び出し元は退避辞書の存在を意識しなくてよい)。関数本体中のローカル変数
        宣言文 (int x; 等) は実行/検証されるたびに同じ saved へ「シャドーイング前の
        値」を書き足していく (_exec_stmt()/_validate_stmt() の VarDecl/ArrayDecl 分岐
        参照) ので、仮引数もローカル変数も1つの退避辞書・1回の with で一括して
        束縛・復元できる。ネスト (関数呼び出しの再帰) しても、with を抜ける際に
        self._current_locals を外側の値へ戻すので、Python の呼び出しスタックに
        乗って正しく独立する。"""
        saved: dict = {}
        for param, value in zip(params, values):
            if param.name not in saved:
                saved[param.name] = self._vars.get(param.name)
            self._vars[param.name] = _Variable(param.type_name, False, value)
        outer_locals = self._current_locals
        self._current_locals = saved
        try:
            yield
        finally:
            self._current_locals = outer_locals
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
        戻り値は _ReturnSignal で受け取る。void 関数は None を返す。int/float/byte を
        返す関数が return 文を1度も実行せずに本体の最後まで到達した場合は (return 漏れは静的には
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

    def _declare_local(self, name: str, type_name: str, is_array: bool, value) -> None:
        """関数の直接の本体中のローカル変数宣言文 (`int x;`/`byte data[n];`/
        `message <id> m;` が VarDecl/ArrayDecl/MessageDecl として通常の文に混ざって
        現れたもの) を実行/検証する共通処理。パーサーが if/while/for/switch の中では
        宣言文を許さない (_parse_nested_block() 参照) ため、ここは必ず「その関数の
        直接の本体を1回だけ順に辿る」呼び出し元 (_run_block(func.body)/
        _validate_block(func.body,...)) からしか呼ばれない。現在の呼び出しフレームの
        退避辞書 (self._current_locals、_bound_params() が管理) に「シャドーイング前の
        値」を初出のときだけ記録してから self._vars を束縛し直す (_bound_params() の
        仮引数束縛と全く同じガード。同名の宣言文が万一同じ本体に複数回書かれていても、
        2回目以降に「今の値」を誤って元の値として記録してしまわないための保険)。
        self._current_locals は関数の中でしか None にならない (VarDecl/ArrayDecl/
        MessageDecl を文として解析できるのはパーサーが関数の直接の本体を読んでいる
        間だけ) ので、ここが None のまま呼ばれることは無い前提で書いている。"""
        if name not in self._current_locals:
            self._current_locals[name] = self._vars.get(name)
        self._vars[name] = _Variable(type_name, is_array, value)

    def _check_local_not_declared(self, name: str, line: int) -> None:
        """ローカル宣言文 (VarDecl/ArrayDecl/MessageDecl) を検証する際、同じ名前が既に
        仮引数またはこの関数内の別のローカル変数として使われていないか確認する
        (_validate_stmt() の VarDecl/ArrayDecl 分岐から使う)。self._current_locals は
        _bound_params() が仮引数で事前に埋め、_declare_local() がローカル宣言のたびに
        追記していく退避辞書なので、そのキーに含まれているかどうかがそのまま
        「この関数のスコープで既に使われている名前かどうか」になる (グローバル変数を
        シャドーイングするだけの新規宣言は許可したいので、self._vars 全体ではなく
        self._current_locals だけを見る)。トップレベルの variables{} 宣言の重複を
        _init_variables() が、仮引数の重複を _register_functions() が、それぞれ実行前に
        弾いているのと同じ理由 (このチェックが無いと、コピペミスによる同名の重複宣言が
        黙って上書き・シャドーイングされてしまい、片方の宣言が意味を持たなくなる)。"""
        if name in self._current_locals:
            raise DslSyntaxError(
                f"{line}行目: ローカル変数 '{name}' は仮引数または既存のローカル変数と"
                "名前が重複しています"
            )

    # ---- 文の実行 ----
    def _exec_stmt(self, stmt) -> None:
        if isinstance(stmt, Call):
            self._call(stmt)
        elif isinstance(stmt, Assign):
            var = self._vars[stmt.name]
            var.value = self._coerce(self._eval(stmt.expr), var.type_name)
        elif isinstance(stmt, IndexAssign):
            var, idx = self._resolve_array_index(stmt.name, stmt.index, stmt.line)
            var.value[idx] = self._coerce_array_element(self._eval(stmt.expr), var.type_name)
        elif isinstance(stmt, VarDecl):
            value = self._eval(stmt.init) if stmt.init is not None else self._default_value(stmt.type_name)
            self._declare_local(stmt.name, stmt.type_name, False, self._coerce(value, stmt.type_name))
        elif isinstance(stmt, ArrayDecl):
            values = self._eval_array_values(stmt.init, stmt.size, stmt.type_name)
            self._declare_local(stmt.name, stmt.type_name, True, values)
        elif isinstance(stmt, MessageDecl):
            self._declare_local(stmt.name, "message", False, self._make_message_value(stmt.can_id))
        elif isinstance(stmt, MemberAssign):
            self._exec_member_assign(stmt)
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
            if var is not None and var.type_name == "message":
                raise DslSyntaxError(
                    f"{stmt.line}行目: message 変数 '{stmt.name}' へ直接代入することは"
                    f"できません (フィールドごとに '{stmt.name}.dlc = ...;'/"
                    f"'{stmt.name}.byte(n) = ...;' の形で代入してください)"
                )
            if var is None:
                raise DslSyntaxError(
                    f"{stmt.line}行目: 未宣言の変数 '{stmt.name}' への代入です "
                    "(variables { int/float/byte ...; } で宣言してください)"
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
        elif isinstance(stmt, VarDecl):
            # ローカル変数の初期値式は (variables{} のグローバル初期値式と違い) 定数式に
            # 制限しない。関数本体はスクリプト全体の検証が完了してから初めて実行される
            # ため (_call_user_function() は run() 開始後にしか呼ばれない)、
            # 副作用のある呼び出しをここで許しても "検証完了前は何も実行しない" という
            # 前提は崩れない (_reject_calls() が variables{} 側だけに要る理由の裏返し)。
            self._check_local_not_declared(stmt.name, stmt.line)
            if stmt.init is not None:
                self._validate_expr(stmt.init)
            self._declare_local(stmt.name, stmt.type_name, False, self._default_value(stmt.type_name))
        elif isinstance(stmt, ArrayDecl):
            self._check_local_not_declared(stmt.name, stmt.line)
            if stmt.init is not None:
                for item in stmt.init:
                    self._validate_expr(item)
            self._declare_local(stmt.name, stmt.type_name, True, [self._default_value(stmt.type_name)] * stmt.size)
        elif isinstance(stmt, MessageDecl):
            self._check_local_not_declared(stmt.name, stmt.line)
            self._declare_local(stmt.name, "message", False, self._make_message_value(stmt.can_id))
        elif isinstance(stmt, MemberAssign):
            var = self._vars.get(stmt.name)
            if var is None or var.type_name != "message":
                raise DslSyntaxError(
                    f"{stmt.line}行目: 未宣言の message 変数 '{stmt.name}' への代入です "
                    f"(variables {{ message <id> {stmt.name}; }} で宣言してください)"
                )
            self._validate_message_member_arity(stmt.member, stmt.args, stmt.line, stmt.name)
            if self._message_members[stmt.member][2] is None:
                raise DslSyntaxError(
                    f"{stmt.line}行目: message の '{stmt.member}' には代入できません "
                    "('dlc'/'byte(n)' のみ代入可能。'id' は読み取り専用です)"
                )
            for arg in stmt.args:
                self._validate_expr(arg)
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
                if node.name == "output":
                    # output() は message 変数を丸ごと渡す専用の関数で、配列や
                    # スカラーを渡しても _output_message() が msg_value.can_id 等の
                    # 属性アクセスで未処理の AttributeError を起こしてしまう。
                    # 「直接の引数なら array/message どちらでも許可」という下の
                    # 汎用チェックだけでは配列を弾けないため、ここで専用に絞り込む。
                    arg = node.args[0] if node.args else None
                    arg_var = self._vars.get(arg.name) if isinstance(arg, Var) else None
                    if not isinstance(arg, Var) or arg_var is None or arg_var.type_name != "message":
                        raise DslSyntaxError(
                            f"{node.line}行目: output() には message 変数を渡してください"
                        )
                for arg in node.args:
                    # 配列・message 変数を丸ごと関数に渡す (send(data)/output(msg) 等) のは、
                    # _ARRAY_ARG_BUILTINS/_MESSAGE_ARG_BUILTINS に載っている関数への
                    # 直接の引数としてだけ許可する (_flatten_bytes() が list をそのまま
                    # 展開できる、_output_message() が _MessageValue をそのまま扱えるのは
                    # send()/send_can()/output() だけ)。他の組み込み関数 (wait_response()/
                    # log() 等) に配列/message をそのまま渡すと、実装がその値を int/float
                    # 変換しようとして未処理の TypeError になりうるため、関数名で絞らずに
                    # 一律で許可してはいけない。data + 1 や total = data のように配列/
                    # message がネストした式・代入に紛れ込む場合も同様に、直接引数以外は
                    # 下の Var の通常チェックで弾く。
                    if isinstance(arg, Var):
                        arg_var = self._vars.get(arg.name)
                        if arg_var is not None:
                            if arg_var.is_array and node.name in self._ARRAY_ARG_BUILTINS:
                                continue
                            if arg_var.type_name == "message" and node.name in self._MESSAGE_ARG_BUILTINS:
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
            if var.type_name == "message":
                raise DslSyntaxError(
                    f"{node.line}行目: message 変数 '{node.name}' は単体の式としては"
                    f"使えません (フィールドは '{node.name}.dlc'/'{node.name}.byte(n)' で"
                    "読み取り、送信は output(...) の直接の引数として渡してください)"
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
        if isinstance(node, MemberAccess):
            var = self._vars.get(node.name)
            if var is None or var.type_name != "message":
                raise DslSyntaxError(
                    f"{node.line}行目: 未宣言の message 変数 '{node.name}' "
                    f"(variables {{ message <id> {node.name}; }} で宣言してください)"
                )
            self._validate_message_member_arity(node.member, node.args, node.line, node.name)
            # 子ノード (byte(n) の n 等) の走査は下の共通ループにフォールスルーさせる
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
