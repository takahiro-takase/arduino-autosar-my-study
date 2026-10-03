# can_tool（UDS ボタン送信ツール / CAPL 風スクリプト）

> [README](../../README.md) の「[診断スタック](../../README.md#diag-stack)」節と「[テスト](../../README.md#testing)」節から分離しました。

<a id="uds-tester-tool"></a>
## UDS ボタン送信ツール（tools/can_tool）

セッション制御・SecurityAccess・複数フレーム応答の FC 送信など、手動操作する
項目が増えて Cangaroo での都度のフレーム手入力が煩雑になってきたため、
よく使う UDS コマンドをボタン 1 つで送信できる Python/Tkinter 製の補助ツールを
`tools/can_tool/` に用意しています（同じウィンドウの別タブに CAN 信号定義
エディタ（`data/can_signals.json` を編集する GUI）も同居しています）。

| 機能 | 説明 |
|------|------|
| ボタン送信 | `config.json` に定義した SF フレームをそのまま送信 |
| E2E 自動付加 | `config.json` の `"e2e"` フィールドを持つボタン（EngineInfo/AbsInfo 等）は、Counter と CRC を自動計算して付加。`"profile": "p05"` を指定したボタン（EngineInfo/AbsInfo）は Counter（0–255 のフルレンジ、予約値なし）と CRC16（多項式0x1021、E2E Profile05）、未指定（既定）のボタンは Counter（0–15 のリングカウンタ）と CRC8 SAE J1850（E2E Profile01）を使う。Counter は送信のたびにインクリメントされ、データ入力欄にもリアルタイム反映。入力欄の値を手動編集してから送信した場合はその値をそのまま送信（E2E バイトを上書きして送信 = 意図的な E2E エラーテストが可能） |
| 複数フレーム要求の送信 | `type: "multiframe"` のボタンは FF 送信 → ECU からの FC(CTS) 待ち → CF 送信、という ISO-TP 送信側を自前で実装（0x2E WriteDataByIdentifier 用） |
| 複数フレーム応答の自動 FC | 応答が FF で始まったら `30 00 00 00 00 00 00 00` を自動送信し、CF を再結合（上記の Cangaroo 手動 FC 送信が不要になる） |
| SecurityAccess Level1 自動実行 | requestSeed → `key = seed XOR 0xA55A`（`Dcm_ComputeSecurityKey()` と同一式）を計算 → sendKey を 1 クリックで実行 |
| 応答の簡易デコード | 0x22 の DID 値、0x19 の DTC 名・FreezeFrame、0x2F の controlOptionRecord 名・適用後レベル、0x31 の routineStatusRecord（実行中/PASS/FAIL）、0x7F の NRC 名を人間が読める形式で表示 |
| ランプ IOControl (0x2F) | RunLamp/FaultLamp/AbsLamp ごとに returnControlToECU / resetToDefault / freezeCurrentState / shortTermAdjustment(ON/OFF) をプリセットから送信 |
| RoutineControl (0x31) | EngineHealthCheck (RID 0203) の startRoutine / requestRoutineResults / stopRoutine をプリセットから送信 |
| 周期送信 + 周期(ms)入力欄 | コマンド一覧の各ボタン（`can_frame`型・`raw`型（UDS）とも）に「定期」トグルボタンと周期(ms)の編集可能な入力欄を用意。Tester Present もこの仕組みで周期送信する（既定2000ms） |
| Serial ログ表示 | `Serial.println()` のデバッグログ（`[ms] LEVEL TAG: message`）を USB シリアル経由で表示。CAN 接続とは独立した別の COM ポート接続（Serial 接続パネル）で、EcuM/ComM/CanSM の状態遷移（RUN/SHUTDOWN、FULL_COM/NO_COM、CanSM 独自状態 等）をログから抽出してリアルタイム表示 |

```
cd tools/can_tool
pip install -r requirements.txt
python src/app.py
```

Windows で `pip install` 済みなら `tools/can_tool/run.bat` をダブルクリックしても
起動できます（内部で自分自身のディレクトリへ `cd` してから `python src\app.py` を
実行するだけの薄いランチャーです）。UDS Tester は「UDS Tester」タブに、CAN 信号
定義エディタはもう一方のタブに表示されます。

接続先は「CAN 接続」パネルの `interface` / `channel` / `bitrate` で指定します
（既定値は `config.json` の `can` セクション）。CANable / candleLight 互換
アダプタの場合は `interface=gs_usb`, `channel=0`。SLCAN 系の COM ポートアダプタ
の場合は `interface=slcan`, `channel=COM3` のように変更してください。

> **Cangaroo と同時に同じアダプタへ接続することはできません。** 干渉する場合は
> どちらか一方を切断してください。

デバッグログ（`Serial.println()`、`[ms] LEVEL TAG: message` 形式。
「[シリアルモニタ出力例](../../docs/archive/README_2026-10-03.md#serial-log-example)」参照）を見るには、「Serial 接続」
パネルで COM ポート（`platformio.ini` の `monitor_speed=115200` と同じ既定
baud）を選んで Connect してください。CAN 接続とは完全に独立した別デバイス接続の
ため、両方同時に接続できます（ただし PlatformIO のシリアルモニタ自体とは同じ COM
ポートを同時に掴めないため、どちらか一方のみ）。EcuM（`ECU State`表示。
`ECUM_STATE_RUN`等の`EcuM_StateType`に対応）/ComM（`Comm Mode`表示。
`ComM.c`の`ComM_BusSM_ModeIndication()`が出す`"ch%u ->mode=%u"`ログから数値を抽出し、
`ComM_ModeType`（NO_COM/SILENT_COM/FULL_COM）へ変換）/CanSM（`->FULL_COM`等の
ログ行から正規表現で抽出、`docs/modules/CanSM_Notes.md`「状態遷移」参照）の
最新状態は「ECU 状態」パネルに左から EcuM・ComM・CanSM の順、値は表示位置が
ずれないよう固定幅で常時表示され、「Serialログ」チェックボックスでログパネルを
開かなくても確認できます。CanSM 側の表示ラベルだけは、対応する AUTOSAR 公式型が
ないため（`CanSM_InternalStateType`はこのプロジェクト独自の内部表現で、
`ComM_ModeType`相当の3状態とCanSM固有の3状態[BUS_OFF等]が混在する）、
「CanSM」のままとしている（ComM 列を独立させたことで、CanSM 列と
ComM_ModeType が重複して見える問題は解消済み）。専用パネルにしているのは、
CAN 送受信の観測からの推測値だったため撤去した旧「トラッキング状態」パネルとは
異なり、ECU 自身のログという一次情報源に基づく値であり、接続の可否とは別の
関心事のため（今後 Dcm セッション/SecurityAccess レベル等の追加も想定）。生ログ
自体は「Serialログ」チェックボックスで表示/非表示を切り替えます。抽出はログ
文字列に依存したベストエフォートのため、Bus-Off 回復成功時のように専用の状態
変化ログを出さない遷移では、次に別の遷移ログが出るまで表示が古いままになる
ことがあります。画面に表示中のものと同じ行は、Serial に Connect するたびに
`tools/can_tool/logs/`（`config.json` の `serial.log_dir` で変更可）へ
タイムスタンプ付きファイル名で自動保存され、Disconnect まで（Disconnect の
ログ行も含めて）書き続けます（常時有効・GUIでの切り替えは無し）。長時間の
実機ログをそのままの内容で後から見返す・共有する用途を想定しています。

「ECU 状態」パネル右端の `ECU Reset` ボタンは、UDS 0x11 ECUReset (hardReset) を
1回送信するショートカットです。セッション制約・SecurityAccess のどちらも
不要なため単独で完結し、送信すると実機のリセットボタン押下と同等に ECU が
即座にリセットされます（`Mcu_PerformReset()` 経由、[シリアルモニタ出力例](../../docs/archive/README_2026-10-03.md#serial-log-example)
参照。`tools/can_tool/capl_scripts/01_ecu_reset.capl` と同じ内容を
ボタン1つで実行できるようにしたもの）。

ボタンの追加・変更はコードを触らず `config.json` の `buttons` 配列に項目を
追加するだけで行えます（本プロジェクトの `*_PBCfg.c` と同じ「コードと設定の分離」
の考え方です）。

GUI のボタン送信に加え、複数手順を一連の操作としてスクリプト化できる CAPL 風の
スクリプト機能も用意しています。詳細は「[CAPL 風スクリプト機能](#capl-scripting)」
（テスト章）を参照してください。


<a id="capl-scripting"></a>
## CAPL 風スクリプト機能（tools/can_tool）

ツール自体（ボタン送信・`config.json` 設定・複数フレーム応答の自動 FC 等）の説明は
「[UDS ボタン送信ツール](#uds-tester-tool)」（診断スタック）を参照してください。
ここでは、複数手順を一連の操作としてスクリプト化する CAPL 風の機能のみを説明します。

ボタンの単発送信だけでは「セッション遷移→SecurityAccess→DID 読み出し」のような
複数手順の一連の操作や、応答内容による分岐を再現しにくいため、Vector CAPL に
近い書き味で一連の手順をスクリプトとして書ける機能を用意しています。GUI の
「スクリプト実行...」ボタンからファイルを選択するとバックグラウンドスレッドで
実行されます（Connect 済みの `bus` をそのまま使用）。「停止」ボタンで途中中断
できます。拡張子で以下の2種類を自動判別します（どちらも `tools/can_tool/src/capl_api.py`
の `CaplContext` を実行時のランタイムとして共通で使うため、送受信の挙動は揃っています）。

**`.py`（Python 構文、`capl_api.py`）**

ファイルの内容をそのまま `exec()` する方式。Python 構文ですが、
`tools/can_tool/src/capl_api.py` が公開する以下の関数だけを使えば CAPL に近い
書き味で書けます。

| 関数 | 説明 |
|------|------|
| `send(payload)` | UDS 要求を送信（7 バイト以下は SF、超える場合は自動で FF+CF） |
| `send_can(can_id, data)` | 任意 CAN ID への生フレーム送信（応答待ちなし） |
| `wait_response(timeout=2.0)` | UDS 応答を待って返す（タイムアウト時はスクリプト中断） |
| `assert_positive(resp=None)` / `assert_negative(resp=None, nrc=None)` | 応答を検証し、不一致ならスクリプトを中断（`resp` 省略時は直前の `wait_response()` の結果を使う） |
| `security_unlock()` | SecurityAccess の seed→key 自動計算（ボタンの `security_access_auto` と同一処理） |
| `wait(seconds)` | 指定秒数待機（`@ctx.on_timer` を登録済みならその間もポーリングして発火させる） |
| `log(*args)` | GUI のログ欄に出力 |
| `@ctx.on_timer(interval_s)` | interval_s 秒毎に呼ばれる関数を登録するデコレータ（`wait()` の実行中のみ発火） |

サンプルは `tools/can_tool/capl_scripts/example_session_check.py` を参照してください。
Python の全機能（if/while/変数等）が使えるため、複雑な分岐が必要な場合はこちらが
向いています。

**`.capl`（CAPL 風の独自 DSL、`tools/can_tool/src/capl_dsl.py`）**

`on start`/`on timer`/`on message` という実際の CAPL に近いイベント構文に加えて、
`variables { }` での変数宣言 (`byte`/`int`/`float`/`word` 配列、`byte`/`word` スカラーの
0〜255/0〜65535 ラップアラウンド、`message` 変数を含む)・
`if`/`else`/`while`/`do`-`while`/`for`/`switch`/`break`/`continue`・四則演算/比較/論理/
ビット演算子 (`& | ^ ~ << >>`)・複合代入 (算術系 `+=` 等・ビット演算系 `&=` 等)・`++`/`--`・ユーザー定義関数・`this.byte(n)`/`this.id`/
`this.dlc`・`write()` の printf 風フォーマットにも対応した、自作の字句解析・
構文解析・インタプリタによるミニ言語です（対応していないもの: 構造体）。

```
variables
{
    int i;
    int engineStatusCount;
}

on start
{
    write("start");
    send(0x10, 0x03);      // ExtendedSession へ遷移
    wait_response();
    assert_positive();
    setTimer(keepAlive, 1000);

    for (i = 0; i < 3; i++)
    {
        write("loop ", i);
    }
}

on timer keepAlive
{
    send(0x3E, 0x00);      // TesterPresent
    wait_response(1.0);
    setTimer(keepAlive, 1000);  // 単発タイマーなので繰り返すには再度アームする
}

on message 0x200
{
    engineStatusCount++;
    switch (msgData(0))
    {
        case 0:
            write("OFF (", engineStatusCount, "回目)");
            break;
        case 2:
            write("RUNNING (", engineStatusCount, "回目)");
            break;
        default:
            write("state=", msgData(0));
    }
}
```

| 構文/関数 | 説明 |
|------|------|
| `variables { int x; float y = 1.5; byte data[8]; word w; }` | ファイル冒頭に1つだけ書ける変数宣言ブロック（省略可）。スカラー型は `int`/`float`/`byte`/`word`（`byte`/`word` は代入のたびにそれぞれ `0`〜`255`/`0`〜`65535` にラップアラウンドする、下記参照）。初期値省略時は `0`/`0.0`。初期値式は定数式のみ（`send()` 等の関数呼び出しは不可。実行前検証が完了する前に副作用のある呼び出しが走ってしまうのを防ぐため）で、前方の宣言を参照することはできる（例: `int b = a * 10;`）。配列宣言は下記参照 |
| `byte name[size];` / `int name[size] = {v0, v1, ...};` / `float name[] = {v0, ...};` / `word name[] = {v0, ...};` | 固定長配列（`variables{}` の中でのみ宣言可）。要素型は `byte`/`int`/`float`/`word` のいずれか（いずれもスカラーとしても配列としても使える）。要素は宣言型に応じて変換される（`byte`/`word` は暗黙に `0`〜`255`/`0`〜`65535` に丸められる、`int`/`float` はスカラー変数と同じ変換）。サイズ省略時は初期化リストの長さになる。初期化リストがサイズより短い場合は残りが型ごとの既定値 (`0`/`0.0`) で埋まる。配列名を添字なしで直接式に書けるのは `send()`/`send_can()` の直接の引数として渡す場合と、対応する仮引数が配列 (`byte data[]` 等) として宣言されたユーザー定義関数へ渡す場合のみ（下記参照。それ以外の組み込み関数や、仮引数がスカラーのユーザー定義関数に渡すのはエラーになる）で、`total = data;` のような代入・`data + 1` のような算術・`data == 0` のような比較には使えない（実行前検証でエラーになる。配列全体への代入もできない（`data = ...;` はエラー）ので、要素ごとに `data[i] = ...;` と書く） |
| `byte name;` / `byte name = expr;`（スカラー） | `byte` はスカラー変数としても宣言できる。**代入のたびに `0`〜`255` にラップアラウンドする** (`& 0xFF`、C/CAPL の固定幅整数型と同じ挙動)。`byte b; b = 300;` は `44`、`b = -1;` は `255` になる。この DSL の `int`/`float` は Python の多倍長整数・浮動小数点数なのでオーバーフローしてもラップアラウンドしない (意図的な設計: DID や CAN ID のような `0xFFFF` を超える値を `int` で扱っている既存スクリプトを壊さないため) が、`byte`/`word` だけは固定幅 (8bit/16bit 符号無し) のラップアラウンドを再現している。8bit 幅の信号・カウンタのラップアラウンド挙動をテストしたい場合に使う。仮引数・戻り値としても使え (`byte checksum8(byte a, byte b) { ... }`)、呼び出しのたびに同じマスクがかかる |
| `word name;` / `word name = expr;`（スカラー） | `byte` の16bit版。**代入のたびに `0`〜`65535` にラップアラウンドする** (`& 0xFFFF`)。`word w; w = 70000;` は `4464`、`w = -1;` は `65535` になる。DataID・CAN ID・DID のような16bit1個の値をひとまとまりで扱いたい場合（`byte` 2個に手動で分解・結合する代わり）に使う。仮引数・戻り値・配列要素としても使え、呼び出し/代入のたびに同じマスクがかかる |
| `message <id> name;`（例: `message 0x123 msg;`） | 実際の CAPL の `message` 型に近い書き味の変数（`variables{}` の中、またはユーザー定義関数の直接の本体でのみ宣言可）。`id` は宣言時に固定（後から書き換える機能は対象外）。`dlc`（既定 `8`）と8バイトの `data`（全て `0` で初期化）を持つ。フィールドは `msg.dlc`（`0`〜`8`、範囲外は実行時にスクリプト中断）/`msg.byte(n)`（`n` は `0`〜`7`、範囲外は実行時にスクリプト中断）/`msg.id`（読み取り専用）の3つのみ。`msg.dlc = expr;` / `msg.byte(n) = expr;` で書き込み、`msg.dlc`/`msg.byte(n)`/`msg.id` で読み取る（`msg.id = ...;` は読み取り専用なのでエラー）。配列と同様、変数名を添字なしで直接式に書けるのは `output(...)` の直接の引数として渡す場合のみで、それ以外（代入・算術・比較）には使えない（実行前検証でエラー）。関数内でローカル宣言した場合は呼び出しごとに独立した新しい `message` になる |
| `on start { ... }` | スクリプト開始時に1回実行 |
| `on timer <name> { ... }` | `setTimer(<name>, ms)` でアームしたタイマーが満了した時に実行（**単発**。CAPL の `msTimer` と同様、繰り返すにはハンドラ内で再度 `setTimer()` を呼ぶ）。タイマー名は `variables{}` での宣言は不要（後述） |
| `on message <id> { ... }` | 指定 CAN ID のフレームを受信した時に実行（`id` は `0x200` のような16進数か10進数） |
| `x = expr;` / `x += expr;` `x -= expr;` `x *= expr;` `x /= expr;` `x %= expr;` `x &= expr;` `x \|= expr;` `x ^= expr;` `x <<= expr;` `x >>= expr;` / `x++;` `x--;` | 代入文（`x` は `variables{}` で宣言済みであること）。複合代入（算術系 `+= -= *= /= %=`、ビット演算系 `&= \|= ^= <<= >>=`）・インクリメント/デクリメント（後置のみ）は `x = x <op> expr` の代入に脱糖される。C/CAPL の代入と同様、`x` の宣言型 (`int`/`float`/`byte`) に変換してから代入する（`int` 変数への代入は 0 方向へ切り捨て） |
| `name[expr] = expr;` | 配列要素への代入文（`name` は `variables{}` で配列として宣言済みであること。要素型に応じて変換される）。添字が範囲外の場合はスクリプト中断（ゼロ除算等と同じ扱い） |
| `if (expr) { ... } else if (expr) { ... } else { ... }` | 条件分岐（`else if`/`else` は省略可、いくつでも連結可） |
| `while (expr) { ... }` | 条件が真の間繰り返す（「停止」ボタンでの中断はループの各周回でチェックされる） |
| `do { ... } while (expr);` | `while` と違い、`cond` を最初に評価する前に本体を必ず1回実行する（C/CAPL と同じ）。`break`/`continue` の扱いは `while` と同じ |
| `for (init; cond; update) { ... }` | C の `for` と同じ（`init`/`cond`/`update` はいずれも省略可、`for (;;) { ... }` も可）。`init`/`update` には代入・複合代入・`i++`/`i--` のいずれも書ける（例: `for (i = 0; i < 10; i++) { ... }`） |
| `break;` / `continue;` | `break` は `while`/`do-while`/`for`/`switch` の中でのみ使用可、最も内側のものを抜ける。`continue` は `while`/`do-while`/`for` の中でのみ使用可（`switch` の中に書いた場合は `switch` を素通りして外側の `while`/`do-while`/`for` に効く）。ループ・`switch` の外で使うと実行前検証でエラーになる |
| `switch (expr) { case N: ... break; case M: case K: ... default: ... }` | C/CAPL と同じフォールスルー動作の多分岐（`break` が無いと次の `case`/`default` に実行が流れ込む）。`case` の値は整数定数のみ（変数・式は不可）。同じ値の `case` の重複、`default` の複数指定はパース時にエラーになる |
| `int name(int a, float b) { ... }` / `void name(...) { ... }` | ユーザー定義関数。`variables{}`/`on ...` と同じトップレベルにいくつでも書ける。戻り値の型は `int`/`float`/`byte`/`word`/`void`（配列は戻り値にできない。`byte`/`word` は呼び出しのたびにそれぞれ `0`〜`255`/`0`〜`65535` にラップアラウンドする）。仮引数は `int`/`float`/`byte`/`word` のスカラー、または `byte data[]`（サイズ指定なしの `[]`）のような配列（要素型は `byte`/`int`/`float`/`word` いずれも可）。配列仮引数への実引数は配列変数をそのまま渡す（例: `sum(len, data)`）ことのみ許され、`data[0]` のような要素参照や式は渡せない。呼び出しのたびにその時点の配列のコピーが束縛される（`send(data)` 等と同じ挙動）ため、関数内で配列仮引数の要素を書き換えても呼び出し元の配列には影響しない。要素は束縛のたびに仮引数の宣言型へ変換される（スカラー仮引数と同じ規則。例えば実引数が `int` 配列でも `byte data[]` で受け取れば各要素は `0`〜`255` にマスクされる）ので、呼び出し元の配列の宣言型と仮引数の型が違っていても構わない。定義順に関係なく呼び出せる（前方参照・相互再帰も可）。呼び出しは `add(1, 2)` のように式の中でも `logMsg("x");` のように文としても書ける。組み込み関数と同名の定義、関数名の重複定義、仮引数名の重複はパース/検証時にエラーになる。仮引数は呼び出し中だけ同名のグローバル変数をシャドーイングし、呼び出しから戻ると元の値に復元される（再帰呼び出しでも各呼び出しフレームが独立して正しく退避・復元される） |
| `int x;` / `int x = expr;` / `byte b;` / `word w;` / `byte data[n];` / `message <id> m;`（関数の**直接の本体**でのみ） | ユーザー定義関数の本体でだけ書けるローカル宣言（`variables{}` で包む必要はない）。**関数スコープ**（ブロックスコープではない）: 宣言した位置から関数の終わりまでどこからでも参照できる。ただし**宣言できるのは関数の直接の本体だけ**で、`if`/`while`/`for`/`switch` の中に書くとパース時にエラーになる（意図的な制約: 検証は条件分岐の全枝を無条件に辿るが、実行は実際に実行された枝でしかローカル変数を束縛しないため、分岐の中での宣言を許すと「検証は通るのに、その枝を通らない呼び出しでだけ実行時にクラッシュする」という食い違いが起きてしまう。ループ内で毎回リセットしたい作業用変数は、ループの外〈関数の直接の本体〉で宣言してからループの中で代入する）。仮引数と同様、同名のグローバル変数をその関数の中でだけシャドーイングし、呼び出しから戻ると元の値に復元される（再帰呼び出しでも各呼び出しフレームが独立して正しく退避・復元される）。初期値式は `variables{}` の初期値式と違い定数式に限定されない（関数本体はスクリプト全体の検証が完了してから初めて実行されるため）。**実際の CAPL との違いに注意**: 実 CAPL の関数内ローカル変数は C の `static` ローカル変数のように呼び出しをまたいで値を保持し続ける（スタックベースの自動変数という概念が無く、初期化式も一度しか評価されない）が、この DSL のローカル変数は呼び出しのたびに再初期化され、呼び出しが終わると値も消える。前回呼び出し時の値を次回に持ち越したい場合（カウンタの継続性チェック等）は、関数内ローカルではなく `variables{}` のグローバル変数として宣言すること |
| `return;` / `return expr;` | 関数の中でのみ使える（関数の外で使うと実行前検証でエラー）。`void` 関数は `return;`（または何も `return` せず本体の最後まで到達）のみ可、`int`/`float` 関数は `return expr;` が必須（値なしの `return;` は実行前検証でエラー）。`int`/`float` 関数が分岐によって一度も `return` を実行せずに本体の最後まで到達した場合は（全分岐が `return` するかどうかまでは静的検証しない）、実行時にスクリプト中断になる。void 関数の戻り値を式の中で使おうとする（例: `x = voidFunc();`）のも実行前検証でエラーになる |
| `+ - * / %`、`== != < > <= >=`、`&& \|\| !`、`( )` | 四則演算・比較・論理演算子（`&&`/`\|\|` は短絡評価）。比較・論理式の結果は `0`/`1` の `int`。`/`・`%` は両辺が `int` の場合は C/CAPL と同じ 0 方向への切り捨て演算（`-7 / 2` は `-3`、Python の `//` のような床方向の丸めにはならない）、どちらかが `float` なら通常の除算・剰余になる。ゼロ除算はスクリプト中断（`wait_response()` のタイムアウト等と同じ扱い） |
| `& \| ^ ~ << >>` | ビット演算子（AND/OR/XOR/NOT/左シフト/右シフト）。優先順位は C/CAPL と同じ（`\|` < `^` < `&` < 比較演算子 < シフト演算子 < `+`/`-`、詳細は `capl_dsl.py` 冒頭のコメント参照）。両辺（`~`/シフトの左辺は片辺）を `int` に変換してから計算する（`float` を渡すと 0 方向への切り捨て）。右シフトは Python の算術シフト（符号を保持）。シフト量が負だとスクリプト中断。複合代入 (`&=`/`\|=`/`^=`/`<<=`/`>>=`) にも対応（`x = x & y;` と同じ意味）。DID のような16bit値を上位/下位バイトに分解する典型例: `(did >> 8) & 0xFF`（上位バイト）、`did & 0xFF`（下位バイト） |
| `send(b0, b1, ...)` / `send_can(can_id, b0, b1, ...)` | Python 版の `send()`/`send_can()` と同じ（バイトは可変長引数）。引数に配列 (`byte`/`int`/`float`/`word` いずれも) を添字なしで渡すと（例: `send(data)`）、配列の全要素を展開してペイロードに含める（各要素は `& 0xFF` でバイト範囲にマスクされる。`int`/`float`/`word` 配列を渡した場合も同様）。個別バイトと配列は混在させられる（例: `send_can(0x100, 0x01, data)`） |
| `output(msg)` | `message` 変数を丸ごと渡し、`msg.dlc` バイト分を生の CAN フレームとして送る（`send_can()` と同様 UDS 応答待ちはしない）。引数は `message` 変数の直接の参照である必要があり、配列やスカラーを渡すと実行前検証でエラーになる |
| `wait_response()` / `wait_response(timeout)` | Python 版と同じ |
| `assert_positive()` / `assert_negative()` / `assert_negative(nrc)` | Python 版と同じ（`resp` 引数はなく常に直前の応答を見る） |
| `respSid()` / `respNrc()` / `respByte(n)` / `respIsNegative()` | 直近の `wait_response()`/`security_unlock()` が受信した UDS 応答の SID（正応答なら要求 SID+0x40、負応答なら常に `0x7F`）/NRC（負応答でなければ `0`）/`n` バイト目/負応答かどうか（`0`/`1`）を取得。応答が無ければいずれも `0` を返す。**`msgData(n)`/`this.byte(n)` 等とは別物**で、`wait_response()` の応答フレームは受信ループが直接消費するため `on message`/`msgData()` 側には流れてこない（応答 SID・NRC で分岐したい場合は `switch (respNrc()) { ... }` のようにこちらを使う） |
| `security_unlock()` | Python 版と同じ |
| `wait(seconds)` | 指定秒数待機（Python 版と異なり、この待機中も `setTimer` タイマーの発火・`on message` ディスパッチは止まらず動き続ける） |
| `log(fmt, ...)` / `write(fmt, ...)` | 同じ動作（`write` は CAPL の `write()` に合わせたエイリアス）。第1引数が `%` を含む文字列で、かつ他に引数がある場合は CAPL の `write()` と同様 printf 風の書式文字列（`%d`/`%f`/`%s`/`%x`/`%X`/`%%` 等、Python の `%` 演算子と同じ書式）として扱う。それ以外（引数1つだけ、または `%` を含まない）は従来通りスペース区切りで連結するので、`write("50% 完了")` のような `%` を含む単なるテキストはそのまま出力される |
| `setTimer(name, ms)` / `cancelTimer(name)` | タイマーのアーム/解除。`name` は識別子そのもの（実際の CAPL の `msTimer` 変数と違い、`variables{}` での事前宣言は不要） |
| `msgData(n)` / `msgId()` / `msgDlc()` | `on message` ハンドラ内で、直近に受信したフレームの byte[n]/CAN ID/データ長を取得（`on message` ハンドラ外でも呼べ、その場合は `0` を返す） |
| `this.byte(n)` / `this.id` / `this.dlc` | 上記と同じ内容を返す、実際の CAPL の `on message` ハンドラでの書き方に近いプロパティ風の構文。**`on message` ハンドラ内でのみ使用可**（ハンドラ外で使うと実行前検証でエラーになる。実際の CAPL でも `this` は message ハンドラの外では使えない） |

未宣言の変数への代入・参照、関数/`this.byte(n)`等の引数の個数が足りない・多すぎる場合も、
未知の関数名と同様にスクリプト実行開始前に検出します（例: `this.byte` や `msgData()` の
ように引数を書き忘れた場合も、実際にメッセージが届くまで待たされることなく、`on start`
の副作用が走る前にエラーになります）。

未宣言の変数への代入・参照も、未知の関数名と同様にスクリプト実行開始前に検出します。

未知の関数名（タイポ等）は `on timer`/`on message` ブロックの中身であっても、
スクリプト実行開始前（`on start` が走り出す前）に一括検出してエラーにします。
そうしないと、`on timer`/`on message` 内のタイポは実際にそのイベントが発火するまで
見つからず、`on start` でのセッション変更や SecurityAccess アンロックのような
副作用のある処理を実行し終えた後になってようやく判明する、ということになるためです。

`on start` の実行後、`on timer`/`on message` が1つでも定義されていれば「停止」
ボタンが押されるまでイベント待受を続けます（何も定義されていなければ `on start`
だけで完了します）。`wait_response()`/`security_unlock()` が応答待ちでブロックして
いる間に届いた（応答 ID 以外の）フレームは内部で一旦退避され、ブロックが終わった
後に `on message` 側へきちんと配送されるため、MeterStatus (0x200) のような
周期送信フレームの監視は取りこぼしなく行えます。ただし同じ CAN ID を
`wait_response()` と `on message` の両方で待ち受けようとした場合（例: UDS 応答 ID
の 0x7E8 を `on message 0x7E8` でも監視しようとした場合）は、その ID 宛のフレーム
自体を `wait_response()` が応答として直接消費してしまうため、`on message` 側には
回ってきません。`on message` は UDS 応答以外の周期送信フレーム（EngineStatus
0x200 等）を監視する用途に向いています。

（実装メモ）`on message` は自前で CAN バスを読みには行かず、GUI の RX モニタ表示を
更新している `_rx_monitor_worker`（Connect 中ずっと動くバスの読み取り役）が受信した
フレームを橋渡ししてもらう形にしてあります。両者が別々に受信しようとすると同じ
フレームを奪い合ってどちらかが取りこぼす（`on message` が発火しない、または RX
モニタ表示が更新されない）ため。CAN アダプタの切断等で `bus.recv()` が実エラーを
送出した場合は（python-can の仕様上、単なる受信タイムアウトは例外ではなく `None`
を返すだけなので、これは区別できる）、ログに出したうえで `_rx_monitor_worker`
自体を停止します（デッドなバスに対して無言でポーリングし続けることはしません）。
このファンアウト用のキューは Connect 中ずっと共有されているため、「スクリプト実行」
ボタンを押した時点で溜まっていた古いフレームは、スクリプト開始前に捨てます
（そうしないと `on message` がスクリプト開始より前に届いていたフレームをまとめて
受け取ってしまい、開始直後にバックログが一気に発火してその後は静かに見える、という
紛らわしい挙動になるため）。

サンプルは `tools/can_tool/capl_scripts/example_session_check.capl`（最小構成）、
`tools/can_tool/capl_scripts/example_variables_control_flow.capl`（変数宣言・
`if`/`else`/`while` を使った例）、
`tools/can_tool/capl_scripts/example_for_this_printf.capl`（`for`・
`this.byte(n)`/`this.id`/`this.dlc`・printf 風フォーマットを使った例）、
`tools/can_tool/capl_scripts/example_switch_array.capl`（`switch`/`case`・
`break`/`continue`・`byte` 配列・複合代入/`++`/`--`・`respSid()`/`respNrc()`
による UDS 応答の NRC 分岐を使った例）、
`tools/can_tool/capl_scripts/example_functions.capl`（`int`/`void` のユーザー定義
関数で TesterPresent 送信・DID 読み出しの共通処理を関数化し、関数内ローカル変数
(ループカウンタ等) も使った例）、
`tools/can_tool/capl_scripts/example_message.capl`（`int`/`float` 配列、`message`
変数の宣言・`.dlc`/`.byte(n)` フィールドの読み書き・`output()` による送信、
ビット演算子 (`>>`/`&`) による DID の上位/下位バイト分解を使った例）、
`tools/can_tool/capl_scripts/example_byte_wraparound.capl`（`byte` スカラー変数の
0〜255 ラップアラウンド、`byte` 仮引数/戻り値/ローカル変数を使ったチェックサム
計算・8bit カウンタ信号のラップアラウンドをシミュレートする例）、
`tools/can_tool/capl_scripts/example_do_while.capl`（`do`-`while` が条件を最初に
評価する前に本体を必ず1回実行すること、`break` で途中脱出できること、
TesterPresent を成功する/上限回数に達するまで送信するリトライ処理での
実用例）を参照してください。
