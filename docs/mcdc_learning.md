# MC/DC 学習ガイド

このプロジェクトの単体テストで MC/DC（Modified Condition/Decision Coverage）を計測し、足りない組み合わせをテストで埋めながら学ぶための手引きです。
計測の仕組みは [unit_test.md](unit_test.md) と `tools/coverage/generate_coverage_report.sh` にあります。

## 1. MC/DC とは

`if ((A) || (B))` のような判定（decision）は、条件（condition）A・B からできています。MC/DC は、次を満たすテストの組を要求します。

- 判定のすべての結果（真・偽）を出す。
- **各条件について、「その条件だけを変えると判定の結果が変わる」2 つのテスト（独立ペア）がある。**

条件が N 個なら、最小で N+1 個のテストで満たせます。分岐カバレッジ（真・偽を 1 回ずつ通す）より厳しく、条件の組み合わせの抜けを見つけられます。

## 2. 計測の手順

### 全体を計測して HTML で見る

```bash
export LLVM_MINGW_BIN="/c/Users/<you>/llvm-mingw-YYYYMMDD-ucrt-x86_64/bin"
bash tools/coverage/generate_coverage_report.sh
```

PowerShell から実行する場合は、Git Bash を明示します（PowerShell の `bash` は WSL の起動用を指すことがあり、そのままでは動きません）。プロジェクトのルートで実行します。

```powershell
$env:LLVM_MINGW_BIN = "C:\Users\<you>\llvm-mingw-YYYYMMDD-ucrt-x86_64\bin"
$env:Path = "$env:LLVM_MINGW_BIN;$env:Path"      # cmake がコンパイラを見つけるために必要
& "C:\Program Files\Git\bin\bash.exe" tools/coverage/generate_coverage_report.sh
```

設定はそのウィンドウの間だけ有効です。毎回入力したくなければ、PowerShell のプロファイルか Windows のユーザー環境変数に登録します。

`coverage_html/index.html` をブラウザで開き、ソースを選ぶと、各判定の下に「MC/DC Test Vectors」が表示されます。末尾に MC/DC の合計（全条件数、未成立の条件数、成立率）も出ます。

### 1 ファイルだけを端末で見る

上のスクリプトを 1 回実行した後なら、次のコマンドで任意のファイルを見られます。

```bash
llvm-cov show build/native_chain_coverage/native_chain_tests.exe \
    -instr-profile=build/native_chain_coverage/coverage.profdata \
    --show-mcdc src/Os/Os.c
```

PowerShell の場合（`LLVM_MINGW_BIN` を設定済みのとき）:

```powershell
& "$env:LLVM_MINGW_BIN\llvm-cov.exe" show build/native_chain_coverage/native_chain_tests.exe `
    "-instr-profile=build/native_chain_coverage/coverage.profdata" `
    --show-mcdc src/Os/Os.c
```

`-instr-profile=...` は必ず引用符で囲みます。囲まないと、Windows PowerShell 5.1 が値の中の `.`（`coverage.profdata`）で引数を 2 つに分け、`could not read profile data` というエラーになります。

### テストを足して計測し直す

スクリプトは毎回 configure からやり直します。テストを 1 件書いて試すだけなら、次の順で十分です。

```bash
cmake --build --preset native-chain-coverage
cd build/native_chain_coverage
LLVM_PROFILE_FILE=native_chain.profraw ./native_chain_tests.exe > /dev/null
llvm-profdata merge -sparse native_chain.profraw -o coverage.profdata
cd ../..
# 上の llvm-cov show で再確認
```

PowerShell の場合:

```powershell
cmake --build --preset native-chain-coverage
Push-Location build/native_chain_coverage
$env:LLVM_PROFILE_FILE = "native_chain.profraw"
.\native_chain_tests.exe | Out-Null
Remove-Item Env:LLVM_PROFILE_FILE
& "$env:LLVM_MINGW_BIN\llvm-profdata.exe" merge -sparse native_chain.profraw -o coverage.profdata
Pop-Location
# 上の llvm-cov show で再確認
```

## 3. レポートの読み方

[Os.c:324](../src/Os/Os.c#L324) の判定 `(Active != 0U) && (Os_TaskActive[TaskId] == 0U)` の出力です。

```
Number of Conditions: 2          ← 条件は 2 つ（C1 = Active != 0U、C2 = Os_TaskActive[TaskId] == 0U）
Executed:
   C1, C2    Result
1 { F,  -  = F }                 ← C1 が偽。&& なので C2 は評価されない（-）
2 { T,  F  = F }
3 { T,  T  = T }
C1-Pair: covered: (1,3)          ← C1 だけを変えて結果が変わる組（1 と 3）がある
C2-Pair: covered: (2,3)          ← C2 だけを変えて結果が変わる組（2 と 3）がある
MC/DC Coverage for Decision: 100.00%
```

見るポイントは次の 3 つです。

- **`-`**: 短絡評価で評価されなかった条件です（`&&` は左が偽、`||` は左が真のとき右を評価しない）。
- **ベクタ 1 と 3**: C1 だけが違い（F と T）、C2 は 1 側が `-` で 3 側が `T` です。短絡のとき、評価されなかった側は「どちらでもよい」とみなされます。
- **Pair が `not covered`**: その条件の独立ペアが無い、つまりテストが足りないということです。

## 4. 例題（すでに 100% の判定）

[Os.c:319](../src/Os/Os.c#L319) の `(Os_Cfg == NULL) || (TaskId >= Os_Cfg->TaskCount)` は、次の 3 つのベクタで MC/DC を満たします。

| 番号 | C1（未初期化） | C2（ID が範囲外） | 結果 | 対応するテスト |
|---|---|---|---|---|
| 1 | F | F | F | 初期化後の正常な ID |
| 2 | T | `-` | T | `Os_Init` の前に呼ぶ |
| 3 | F | T | T | 初期化後に範囲外の ID で呼ぶ |

- C1 の独立ペアは (1, 2) です。C1 だけが変わり、結果が F から T になります。
- C2 の独立ペアは (1, 3) です。C2 だけが変わり、結果が F から T になります。
- テストは [Os 関連テスト](../test/Bsw/Os/) の `SetTaskActiveReEnable_test.cpp` にあります。

## 5. 演習

### 演習 A: 抜けている組み合わせを埋める（Gpt.c:417、現在 50%）

[Gpt.c:417](../src/Bsw/Gpt/Gpt.c#L417) の `Gpt_EnableNotification` にある判定です。

```c
if (!Gpt_IsValidChannel(Channel) || (Gpt_Cfg->Channels[Channel].Notification == NULL))
```

現状のレポートです。

```
1 { F,  F  = F }
2 { F,  T  = T }
C1-Pair: not covered      ← C1（チャネルが不正）の独立ペアが無い
C2-Pair: covered: (1,2)
```

**考えること**: C1 が真（不正なチャネル）で結果が真になるベクタ `T, -` を作るには、どんなテストが要りますか。

**ヒント**: 既存の [Gpt_test.cpp](../test/Bsw/Gpt/Gpt_test.cpp) には、不正チャネルで `Gpt_DisableNotification` を呼ぶテストはありますが、`Gpt_EnableNotification` を呼ぶテストはありません。`Gpt_StartTimer_NG_RejectsInvalidChannel` を参考に、1 件追加します。

**答え合わせ**: `Gpt_Init(&config)` の後に `Gpt_EnableNotification((Gpt_ChannelType)1U)` を呼び、`GPT_E_PARAM_CHANNEL` が報告されることを確認します。追加後に再計測すると、このファイルの 417 行目が 100% になります。

### 演習 B: 到達できない組み合わせを考える（Gpt.c:141）

[Gpt.c:141](../src/Bsw/Gpt/Gpt.c#L141) の `Gpt_OnTick` にある判定です。

```c
if ((Gpt_NotificationEnabled[Channel] != 0U) && (chCfg->Notification != NULL))
```

現状のレポートでは、`C2-Pair: not covered`（50%）です。C2 の独立ペアには、`T, F`（通知は有効だが関数ポインタが NULL）のベクタが要ります。

**考えること**: このベクタを作れるでしょうか。

**ヒント**: `Gpt_NotificationEnabled` を立てる関数は何で、そのとき `Notification == NULL` を拒否していないか、[Gpt.c](../src/Bsw/Gpt/Gpt.c) の `Gpt_EnableNotification` を読んで確かめます。

**考察**: 公開 API 経由では、`Notification` が NULL のチャネルに通知を有効化できないため、この組み合わせはおそらく到達できません（読んで確かめてください）。MC/DC が 100% にならない原因が、テストの不足ではなく「防御的な条件」にある例です。実務では、次のように扱います。
1. 本当に到達できないなら、その根拠を記録する（コードの検証メモ、レビュー記録）。
2. 防御として残す価値があるなら、残して 100% 未達の理由を説明できるようにする。
3. 不要なら条件を削る（コードを簡単にする）。

## 6. 注意点

- **短絡評価**: 評価されない条件は `-` になります。MC/DC は、これを前提にベクタを見ます。
- **ツールの制約**: Clang の MC/DC は、条件数が多い判定（既定では 7 条件以上）を計測しません。条件数が少ない判定から読むのが現実的です。
- **行カバレッジが 100% でも MC/DC は別**: 各条件が結果に効いていることまでは、行や分岐のカバレッジでは分かりません。
- **「通す」だけでなく「何を確認するか」**: ベクタを満たすためだけのテストにせず、そのベクタで期待する結果（戻り値、DET の報告、状態の変化）を確認します。

## 7. 参考: 現在の値

2026-10-16 時点の MC/DC は、428 条件中 220 条件が成立（208 条件は未成立）で 51.4% です。モジュールごとの値は `llvm-cov report ... --show-mcdc-summary` で見られます。
