# コンフィグレータ（試作）

信号表（CAN ビットアサイン表）と、モジュールごとの設定 json から、各モジュールの設定ソース
（`*_PBCfg.c`、`*_Cfg.h`）の該当部分を生成する Python ツールです。
現在の対象は **CanIf**、**Com**、**PduR**、**E2EXf**（E2E Transformer）、**SecOC**、**Csm / Crypto / KeyM**（暗号処理の連鎖）、**Rte**（ASW 向けの信号アクセス関数）、**Dem**（診断イベント）、**Dcm**（DID の表）、**CanTp**（診断のトランスポート層）、**CanNm**（ネットワーク管理）、**CanSM**（バス状態管理）、**ComM**（通信管理）、**Nm**（ネットワーク管理インタフェース）、**Os**（タスク表）、**NvM**（不揮発ブロックの構成）、**WdgM**（ウォッチドッグの監視対象）、**FiM**（機能の抑止）、**Can**（コントローラ設定）、**BswM**（モード遷移のルール）、**Port**（管理ピン）、**Dio**（チャネル・ポート）、**Adc**（アナログ入力のチャネル・グループ）、**EcuM**（RUN 要求ユーザ・タイムアウト・ウェイクアップ要因）、**Gpt**（汎用タイマのチャネル）、**Wdg**（ウォッチドッグドライバの HW タイムアウト）、**Mcu**（MCU ドライバ。実フィールドの無いプレースホルダ）、**CryIf**（Crypto チャネル ID）、**Det**（既定のログレベル）、**IoHwAb**（ボタンのデバウンス・固着判定と ADC 電圧低下の閾値）の 32 モジュールです。Com スタックの他のモジュールへ広げていく計画です。

最終的な構想は `arxml ⇔ json ⇔ Cfg.c` です。このうち json → Cfg.c の部分と、
信号表の変更に設定を連動させる部分を、この範囲で試しています。

## 全体の流れ

```
config/data/can_signals.json（ビットアサイン表の正本。can_tool の GUI で編集）
        │  sync_cfg.py: 追加されたフレームの設定のひな形を、各モジュールの設定 json へ追加
        ▼
config/data/CanIf.json, Com.json, PduR.json, E2EXf.json, SecOC.json, CryptoStack.json, Rte.json, Dem.json, Dcm.json, CanTp.json, CanNm.json, CanSM.json, ComM.json, Nm.json, Os.json, NvM.json, WdgM.json, FiM.json, Can.json, BswM.json, Port.json, Dio.json, Adc.json, EcuM.json, Gpt.json, Wdg.json, Mcu.json, CryIf.json, Det.json, IoHwAb.json
        │  gen_cfg.py: スキーマ・整合性を検査して、Cfg ソースの生成領域を更新（json → C の一方向）
        ▼
src/Bsw/CanIf/CanIf_PBCfg.c, CanIf_Cfg.h / src/Bsw/Com/Com_PBCfg.c, Com_Cfg.h / src/Bsw/PduR/PduR_PBCfg.c, PduR_Cfg.h
/ src/Bsw/E2EXf/E2EXf_PBCfg.c, E2EXf_PBCfg.h / src/Bsw/SecOC/SecOC_PBCfg.c, SecOC_Cfg.h
/ src/Bsw/Csm, Crypto, KeyM の *_PBCfg.c, *_Cfg.h（鍵・ジョブ・鍵名のテーブルと ID マクロ）
/ src/Bsw/BswM/BswM_PBCfg.c, BswM_Cfg.h（ルールテーブルとルール数）
/ src/Rte/Rte.h, Rte.c（信号アクセス関数 Rte_Write_* / Rte_Read_*）
/ src/Bsw/Dem/Dem_Cfg.h, Dem_PBCfg.c（イベント ID・DTC・閾値のマクロと表）
/ src/Bsw/Dcm/Dcm_Cfg.h, Dcm_PBCfg.h, Dcm_PBCfg.c（DID のマクロ・データ関数の宣言・DID の表）
/ src/Bsw/CanTp/CanTp_Cfg.h（バッファ・フロー制御・タイムアウト・N-SDU の ID）
/ src/Bsw/CanNm/CanNm_Cfg.h（NM フレーム・タイマ・CanIf の PDU）
/ src/Bsw/CanSM/CanSM_Cfg.h、ComM/ComM_Cfg.h、Nm/Nm_Cfg.h（チャネル数・Bus-Off 回復時間・ハンドルなど）
/ src/Os/Os_PBCfg.c、Os_Cfg.h（タスク表・タスク数・タスク ID）
/ src/Bsw/NvM/NvM_Cfg.h, NvM_PBCfg.c（ブロック ID・アドレス・バッファ・ROM デフォルト値・記述子テーブル）
/ src/Bsw/WdgM/WdgM_Cfg.h, WdgM_PBCfg.c（エンティティ・チェックポイント・遷移表・Deadline 表）
/ src/Bsw/FiM/FiM_Cfg.h, FiM_PBCfg.c（機能 ID と対応テーブル） / src/Bsw/Can/Can_Cfg.h, Can_PBCfg.c（ピン番号と Can_Config）
/ src/Bsw/Port/Port_Cfg.h, Dio/Dio_Cfg.h, Adc/Adc_Cfg.h, EcuM/EcuM_Cfg.h（設計値の #define）/ src/Bsw/Gpt/Gpt_PBCfg.h, Gpt_PBCfg.c（チャネル ID とチャネルテーブル）
/ src/Bsw/Wdg/Wdg_PBCfg.c（Wdg_Config）/ src/Bsw/Mcu/Mcu_PBCfg.c（Mcu_Config）/ src/Bsw/CryIf/CryIf_Cfg.h（チャネル ID）/ src/Bsw/Det/Det_Cfg.h（ログレベルの既定値）/ src/Bsw/IoHwAb/IoHwAb_Cfg.h（ボタンのサンプル数・ADC 閾値）
```

- **信号表の値は設定 json にコピーしません。** CAN ID、DLC、信号のビット位置・ビット長、update-bit の位置、
  送信周期・受信タイムアウトは、設定 json の `frame`（フレーム名）と `field`（フィールド名）から、
  生成時に信号表を引いて埋めます。値の正本は信号表の 1 か所だけです。
- 信号表に無いフレーム（UDS 診断など）だけ、設定 json に `canId` と `dlc` を直接書きます。
- 設定 json に書くのは、信号表から決まらない値です（PDU ID の割り当て、経路の宛先モジュール、HTH/HRH、
  Com の送信モード・フィルタ・コールバック名など）。
- **PduR は Com の I-PDU を名前で参照します**（`ipdu` / `confIpdu`）。PduR の宛先 ID の数値は、生成時に Com の設定から引きます。
  Com の `pduRId` も、PduR の経路から引きます（SecOC 経由の I-PDU は、SecOC の設定から引きます）。
- **E2E / SecOC のレイアウトも信号表から引きます。** E2EXf の DataLength・Offset・DataID（既定は CAN ID）、
  SecOC の Freshness / MAC の位置と長さ・Authentic Payload 長・Secured I-PDU 長・DataId（既定は CAN ID）は、
  `e2e_crc` / `e2e_counter` / `secoc_freshness` / `secoc_mac` のフィールドから求めます。
  SecOC 経由の Com の I-PDU の `dlc` と `pduRId` も、SecOC の設定から引きます。

## 使い方

信号表にフレームを追加したとき:

```
python tools/configurator/sync_cfg.py             # 不足している設定のひな形を、CanIf.json / Com.json / PduR.json へ追加
python tools/configurator/gen_cfg.py              # Cfg ソースを生成
```

設定を直接変えるとき: `config/data/*.json` を編集して `gen_cfg.py` を実行します。

```
python tools/configurator/gen_cfg.py              # 生成して書き換える
python tools/configurator/gen_cfg.py --check      # 書き換えず、生成結果と食い違えば終了コード 1
python tools/configurator/gen_cfg.py PduR         # モジュールを指定
python tools/configurator/sync_cfg.py --dry-run   # 追加内容を表示するだけ
```

C ソースの生成領域は手で編集しません（次の生成で上書きされます）。

信号表の編集画面（can_tool の信号定義エディタ）にも、「検査」「同期の確認」「同期」「生成」のボタンがあります（[can_tool の README](../can_tool/README.md) 参照）。
設定 json は、can_tool の左側のナビゲーションの `Com`、`PduR` などの項目から、スキーマに従って GUI で編集できます（同じく [can_tool の README](../can_tool/README.md) 参照）。

### 同期ツールの動き
- 追加するのは、信号表にあって設定に無いフレームの「ひな形」です。CanIf の PDU、Com の I-PDU とシグナル
  （E2E / SecOC 用と update-bit を除く各フィールド）、PduR の経路を、既定値で作ります（経路は CanIf → PduR → Com が既定）。
- 削除や名前変更は自動では扱いません。信号表に無いフレームを参照している設定は、警告として報告します。
- E2E / SecOC 用のフィールドを持つフレームは、E2EXf / SecOC の設定を自動では作りません（Dem イベント、Csm のジョブ、
  `E2EXf.c` や Rte の呼び出し関数が必要なため）。手で `E2EXf.json` / `SecOC.json` に足すまで、`gen_cfg.py` は整合性エラーで止まります。
- 人が決める値（Com の送信モード、タイムアウト、I-PDU グループ、コールバックなど）は、既定値を入れた上で「要確認」として表示します。
  追加後に `Com.json` を直してから、`gen_cfg.py` を実行します。

## 信号を追加する練習（TestMsg）

信号表に信号を足して、ECU から CAN に出てくるところまでを試すための、テスト専用のメッセージ `TestMsg`（CAN 0x300、ECU 送信、1 秒周期）を用意しています。
E2E・SecOC は付けていない素のフレームで、`TestCounter`（1 バイト）が 1 秒ごとに +1 されます（255 の次は 0）。
カウンタは `src/Asw/App_Test.c` が書き込みます。

can_tool の左側で `Tester` を開くと、受信モニタに `TestMsg (0x300, テスト用)` の行があります（`tools/can_tool/config.json` で登録済み）。
CAN のバイト列と、信号表の定義に従ってデコードした信号名つきの値（`TestCounter=…`）が表示されます。
まず何も変更せずに ECU を起動して、値が 1 秒ごとに増えることを確認してください。

### 1 バイト追加してみる

`TestMsg` に 2 つ目の信号 `TestCounter2`（1 バイト）を足す手順です。

1. can_tool の `Signals` を開き、`TestMsg`（0x300）を選びます。
2. DLC を `1` から `2` に変え、フィールド `TestCounter2`（ビット位置 8、ビット幅 8、種類 number、範囲 0〜255）を追加して保存します。
3. 下部の「検査」を押します。信号表にあって Com に無い信号の指摘が出ます（ここまでは想定どおりです）。
4. 「同期の確認 (dry-run)」で追加内容を確認し、「同期」を押します（`Com.json` に `TestMsg_TestCounter2_Tx` の信号が追加されます）。
5. 「生成」を押し、続けてもう一度「検査」を押して、指摘が無いことを確認します。
6. `pio run -e uno_r4 -t upload` で書き込み、can_tool の受信モニタを見ます。`TestMsg` が 2 バイトになり、`TestCounter2` が表示されます。

`TestCounter2` に値を入れる処理はまだ無いため、値は 0 のままです。ASW から書く関数 `Rte_Write_TestMsg_TestCounter2()` は、手順 5 の「生成」で `src/Rte/Rte.h` / `Rte.c` に自動で作られています（[Rte の関数の生成](#rte-generation) 参照）。
値を変えたいときは、`src/Asw/App_Test.c` の `App_Test_Run()` から、その関数を呼びます。

元に戻すときは、信号表の変更を取り消し（`git checkout` または手で戻す）、「同期」「生成」を押し直します。

## 仕組み

C ソースが「テンプレート」を兼ねます。生成する範囲を、目印のコメントで挟んであります。

```c
/* @@GEN-BEGIN pdur-routing  生成元: config/data/PduR.json  （自動生成: 手編集禁止） */
...  ここが json から生成される ...
/* @@GEN-END pdur-routing */
```

- 目印の外（ファイル冒頭の説明、インクルード、帯見出しなど）には触れません。
- 領域の ID と生成関数の対応は、`modules/` の各モジュール（`SPEC`）と、`gen_cfg.py` の `MODULES` に書いてあります。
- 一方向（json → C）です。C → json の逆変換は行いません。json が正本です。

<a id="rte-generation"></a>
## Rte の関数の生成

信号を足したとき、ASW が使う読み書きの関数も自動で作ります（`config/data/Rte.json`、`modules/rte.py`）。

| 信号の向き | 生成する関数 | 中身 |
|------------|--------------|------|
| TX | `Std_ReturnType Rte_Write_<ポート>_<要素>(<型> value)` | `Com_SendSignal(ComConf_ComSignal_xxx, &value)` |
| RX | `Std_ReturnType Rte_Read_<ポート>_<要素>(<型>* data)` | `Com_ReceiveSignal(ComConf_ComSignal_xxx, data)` |

- 仕様（SWS_RTE）では、`<ポート>` はポートの名前、`<要素>` はデータ要素の名前で、どちらも SW-C の設計者が決める名前です。
  ECU 間の送信では、データ要素ごとに Com の信号 1 つへ書き込む（[SWS_Rte_06023]）ことになっています。
- 名前は、フレーム名を `<ポート>`、信号名を `<要素>` として自動で作ります（空白や括弧で区切って、各語の先頭を大文字にして連結。
  例: `TestMsg` の `TestCounter` → `Rte_Write_TestMsg_TestCounter`）。合わない名前は、`Rte.json` の `names` で
  Com の信号の `name` を `signal` で指して上書きします（`port` と `element` のどちらか片方だけでもよい）。
- 型は信号のビット長から決めます（8 bit 以下は `uint8`、16 bit 以下は `uint16`、32 bit 以下は `uint32`）。
- 生成先は `Rte.h` と `Rte.c` の `@@GEN-BEGIN rte-signal-api` から `@@GEN-END rte-signal-api` の間です。

生成するのは、次のどれにも当てはまらない信号だけです。手書きの関数を置き換えたり、二重に作ったりしないためです。

1. 信号グループの I-PDU の信号（確定のコミットが要るため、手書きの `Rte_SendSignalGroup_*` で扱う）
2. ゲートウェイの入出力の信号（SWC を介さず、Com の中で転送する）
3. Com 以外の `src/` のコードから、すでに使われている信号（手書きの Rte 関数、E2EMon など）。信号 ID マクロの文字が、生成領域の外のコードにあるかで判定する
4. `Rte.json` の `exclude` に書いた信号

3 の判定は文字の検索なので、コメントに信号 ID マクロを書いただけでも「使われている」と見なされます。
生成した関数名が、手書きの宣言・定義と同じになる場合や、生成した関数どうしで同じになる場合は、生成エラーになります（`names` で上書きしてください）。

<a id="dem-generation"></a>
## Dem のイベント定義の生成

診断イベント（DTC）の定義を `config/data/Dem.json` の `events[]` に書いて、`Dem_Cfg.h` と `Dem_PBCfg.c` を生成します（`modules/dem.py`）。
NvM、WdgM と同じく、信号表とは関係なく Dem.json だけが入力です。

| Dem.json の項目 | 生成されるもの |
|-----------------|----------------|
| `events[]` の並び順 / `name` | イベント ID（シンボリック名 `DemConf_DemEventParameter_<name>`）、イベント数（`DEM_EVENT_COUNT`）、各表の添字 |
| `dtc` | DTC コード（`DEM_DTC_<name>`）と、イベント ID から DTC を引く表（`Dem_DtcTable`、`Dem_PBCfg.c`） |
| `debounceLimit` | デバウンスの確定閾値（`DEM_DEBOUNCE_LIMIT_<name>`）と表（`Dem_DebounceLimitTable`） |
| `agingThreshold` | 経年回復の閾値（`DEM_AGING_THRESHOLD_<name>`）と表（`Dem_AgingThresholdTable`） |

- イベントを足すには、`Dem.json` の `events` に 1 件足して `gen_cfg.py` を実行します。ID・DTC・閾値のマクロと 3 つの表が、揃って増えます。
- 3 つの表は `Dem_PBCfg.c`（新設）の目印の間に生成します。宣言は `Dem_PBCfg.h`（手書き）にあり、`Dem.c` はそれを参照するだけです。設定データは `Dem.c` から完全になくなりました。
- 検査: `name` の形、DTC コードの重複と範囲（グループコード 0xFFFFFF は使えない）。
- **他のモジュールとの連動**:
  - NvM: `demEventCount: true` のブロック（Dem のステータス・Aging・故障確定回数）の長さが、Dem.json のイベント数と一致することを検査します。
    ROM デフォルト値の各要素には、対応するイベント名の注釈（`event N: <name>`）を自動で付けます。
  - FiM・E2EXf: 監視する（報告する）イベントは、`Dem.json` のイベント名（`events[].name`）で指定します。名前が無ければ生成エラーです。生成物では `DemConf_DemEventParameter_<name>` になります。
  - イベントを足したら、NvM のブロック長（`NvM.json` の `length`）も同じ数に直します（食い違うと生成エラー）。
- 表を `Dem_PBCfg.c` に分離した後は、3 つの表の値（DTC・閾値）が分離前と同じであることをホスト側のテストと実機で確認します（表の位置が変わるため、コンパイル結果は完全には一致しません）。

<a id="dcm-generation"></a>
## Dcm の DID の表・セッション許可の表の生成

UDS の DID（0x22 読み出し・0x2E 書き込み・0x2F 入出力制御）の定義を `config/data/Dcm.json` の `dids[]` に書いて、`Dcm_Cfg.h`・`Dcm_PBCfg.h`・`Dcm_PBCfg.c` を生成します（`modules/dcm.py`）。

| Dcm.json の項目 | 生成されるもの |
|---|---|
| `dids[]` の並び順 / `name` / `did` | DID のマクロ（`DCM_DID_<name>`）と、表（`Dcm_DidTable`、`Dcm_PBCfg.c`）の行。個数は `DCM_DID_COUNT` |
| `length` / `lengthMacro` | 表のデータ長。`lengthMacro` があれば長さのマクロも生成（テストが使う `DCM_VIN_LENGTH` など） |
| `readFct` / `writeFct` | 0x22 / 0x2E のデータ関数。宣言は `Dcm_PBCfg.h` に生成し、中身は `Dcm.c` に手書きする |
| `ioControlLamp` | 0x2F で制御するランプ（`Rte_LampIdType`） |
| `s3TimeoutMs.value` / `doc` | S3Server タイマの時間（`Dcm_Cfg.h` の `DCM_S3_TIMEOUT_MS`）。1000 ms の倍数 |

- `Dcm.c` の 0x22 / 0x2E / 0x2F は、表を `Dcm_FindDid()` で引いて処理する汎用の形になり、DID ごとの `switch` はなくなりました。DID を足すには、`Dcm.json` に 1 件書き、データ関数を手書きして `gen_cfg.py` を実行します。
- NRC の判定順は従来のままです。0x2E は 0x33（セキュリティ）→ 0x13（3 バイト未満）→ 0x31（表に無い、または書き込み関数が無い）→ 0x13（固定長と不一致）→ 0x31（書き込み失敗）。
- 0x22 の応答用バッファの大きさは、読み出し対応の DID の最大データ長（`DCM_DID_READ_MAX_LENGTH`）から生成します。
- SID ごとの許可セッションは `sidSessions[]`（`sid` は `Dcm_Cfg.h` の `DCM_SID_<sid>` の名前、`sessions` は `DEFAULT` / `EXTENDED`）に書きます。表（`Dcm_SidSessionTable`、`Dcm_PBCfg.c`）と行数（`DCM_SID_SESSION_COUNT`）を生成し、表に無い SID は全セッションで許可です。`Dcm.c` は表を引いて判定するだけです。SID の名前が `Dcm_Cfg.h` に無ければ生成エラーです。表の説明文は `sidSessionDoc` に書きます。

<a id="cantp-cannm-generation"></a>
## CanTp・CanNm の定義の生成

どちらも信号表とは関係なく、`config/data/CanTp.json` と `CanNm.json` が入力です（`modules/cantp.py`、`modules/cannm.py`）。
定義の説明コメントも json の `doc` に持ち、`CanTp_Cfg.h`・`CanNm_Cfg.h` の目印の間に生成します（DET のエラーコード、API ID、バージョン情報などの手書きの定義は、そのまま残しています）。

- **他のモジュールの PDU は名前で指します。** CanNm の `canifTxPdu` / `canifRxPdu` は CanIf.json の PDU の名前、CanTp の `pdurTxSrcPdu` は PduR.json の `txPaths` の名前です。
  生成物ではシンボリック名（`CanIfConf_CanIfTxPduCfg_CanNm_Tx`、`PduRConf_PduRSrcPdu_DiagResp_Tx` など）になり、以前の `2U` / `1U` のような添字の手合わせはなくなりました。
  名前が無いとき、または PDU の上位層が違うとき（CanNm なら CANNM、CanTp なら CANTP でないとき）は生成エラーです。
- **周期の検査**: CanNm の `cycleMs` が `Os_PBCfg.c` の `CanNm_MainFunction` の周期と一致するかを検査します。`timeoutMs` は `cycleMs` より長くします。
- 見出しコメントは `*Section`、MISRA 2.5（未使用マクロ）の抑止コメントは `unused: true` で指定します。
- 生成の前後で、`src/` の 70 ファイルをコンパイルした結果（セクションの内容とリロケーション）が完全に一致することを確認しています。

### CanSM・ComM・Nm（チャネルと時間の設定）

`config/data/CanSM.json`・`ComM.json`・`Nm.json` から、`CanSM_Cfg.h`（チャネル数・Bus-Off 回復待機時間 L1/L2・L1 から L2 への試行回数・ウェイクアップ検証時間）、
`ComM_Cfg.h`（チャネル数・ユーザ数・チャネル 0 とユーザ 0 の ID）、`Nm_Cfg.h`（NM チャネルのハンドル）の設定値を生成します（`modules/cansm.py`・`comm.py`・`nm.py`）。

- **時間の検査**: CanSM の L1・L2・ウェイクアップ検証時間は、`Os_PBCfg.c` の `CanSM_MainFunction` の周期の倍数であること（周期の途中の値は、次の周期まで丸められて意図より長くなるため）。L2 は L1 より長くします。
- **チャネルの整合**: CanSM と ComM のチャネル数が一致すること、Nm のハンドルが ComM のチャネル 0 の ID と一致することを検査します（食い違えば生成エラー）。
- 説明コメントは各 json の `doc` に持ちます。DET のエラーコード・API ID・バージョン情報などの手書きの定義は、そのまま残しています。
- 生成の前後で、`src/` の 70 ファイルをコンパイルした結果が完全に一致することを確認しています。

### Os のタスク表

`config/data/Os.json` の `tasks[]`（関数・周期 [ms]・関数のヘッダ・1 行の説明・補足）から、`Os_PBCfg.c` のタスク表（`Os_TaskTable`）と、`Os_Cfg.h` のタスク数（`OS_TASK_COUNT`）・タスク ID のシンボリック名（`OsConf_OsTask_<function>`）を生成します（`modules/ostask.py`）。
`tasks` の並び順がそのままタスク ID と実行順序（インデックス昇順）です。

- **周期は Os.json が正本です。** `WdgM`（`osTask`）・`CanNm`（`cycleMs`）・`CanSM`（回復時間は周期の倍数）は、これまで `Os_PBCfg.c` を読み取っていましたが、今は Os.json から周期を引きます。
- **BswM のタスク ID とマスクも Os.json から生成します**: `BswM.json` の `osTasks`（BswM が制御するタスクと、`BSWM_OS_TASK_` の後に付ける名前）から、`BswM_Cfg.h` の `BSWM_OS_TASK_*`（値は `OsConf_OsTask_<function>`、つまり Os.json でのタスクの位置）と、`BSWM_TASK_MASK_ALL`（`osTasks` のすべて）・`BSWM_TASK_MASK_APP`（`maskApp.tasks`）・`BSWM_TASK_MASK_BSW`・`BSWM_TASK_MASK_SHUTDOWN`（`osTasks` から `maskShutdown.keepTasks` を除いたもの）を生成します。タスクを挿入・入れ替えても、ID とマスクは自動で追従します。`osTasks` のタスクが Os.json に無い、マスクの対象が `osTasks` に無い、といった食い違いは、毎回の整合性検査でエラーになります。BswM が制御しないタスクは `excludedOsTasks` に挙げます（`App_GptDemo_Run` など）。Os.json のタスクは `osTasks` か `excludedOsTasks` のどちらかに必ず挙げる必要があり、新しいタスクを足して挙げ忘れると、整合性検査でエラーになります（黙って制御対象外になることはありません）。タスク ID は 31 以下（ビットマスクは 32 ビット）です。
- 関数のヘッダは `header` で指定し、`Os_PBCfg.c` がインクルードしていなければ警告します。補足（`note`）はその行の直前にコメントで出ます。
- 生成の前後で、`src/` の 70 ファイルをコンパイルした結果が完全に一致することを確認しています。

<a id="nvm-generation"></a>
## NvM のブロック構成の生成

NvM が管理する不揮発ブロックの構成を `config/data/NvM.json` に書いて、`NvM_Cfg.h` と `NvM_PBCfg.c` を生成します（`modules/nvm.py`）。
信号表とは関係なく、NvM.json だけが入力です。

| NvM.json の項目 | 生成されるもの |
|-----------------|----------------|
| `blocks[]` の並び順 | ブロック ID（`NVM_BLOCK_ID_<name>`）、ブロック数（`NVM_BLOCK_COUNT`）、ブロック記述子テーブルの添字 |
| `name`、`length` | `NVM_BLOCK_<name>_LENGTH`、RAM ミラーバッファ（`NvM_Ram_<Name>`） |
| （前のブロックから計算） | EEPROM の先頭アドレス（`NVM_BLOCK_<name>_EEPROM_ADDR`）。前のブロックのデータ長 + CRC 1 バイトからの連鎖式。冗長ブロック（`redundant`）はミラー面の分も確保する |
| `default` | ROM デフォルト値。`none`（NULL）、`scalar`（1 バイトの定数）、`repeat`（同じ式を `length` 個並べる） |
| （全ブロックの最大） | `NVM_MAX_BLOCK_LENGTH`、EEPROM レイアウトのコメント |

- ブロックを足すには、`NvM.json` の `blocks` に 1 件足して `gen_cfg.py` を実行します。ID、アドレス、バッファ、テーブルの行が揃って増えます。
- `demEventCount: true` のブロックは、長さが `Dem.json` のイベント数と一致するかを検査します（食い違うと生成エラー）。
- ROM デフォルト値の `value` は C の式をそのまま書きます（`Dem_Cfg.h` のマクロを使う場合は、`NvM_PBCfg.c` がそれをインクルードしていることを検査します）。
- 生成の前後で、`NvM_PBCfg.c`・`NvM.c`・`Dem.c` をコンパイルした結果（セクションの内容とリロケーション）が一致することを確認しています。
- 生成するのはサイズ・アドレス・テーブルまでです。ブロックを実際に読み書きする処理（Dem など）は、手で書きます。

<a id="wdgm-generation"></a>
## WdgM の監視対象の生成

WdgM が監視するタスク（Supervised Entity）の構成を `config/data/WdgM.json` に書いて、`WdgM_Cfg.h` と `WdgM_PBCfg.c` を生成します（`modules/wdgm.py`）。
NvM と同じく、信号表とは関係なく WdgM.json だけが入力です。

| WdgM.json の項目 | 生成されるもの |
|------------------|----------------|
| `entities[]` の並び順 | エンティティ ID（`WDGM_ENTITY_<name>`）、エンティティ数、エンティティテーブルの添字 |
| `supervisionCycleMs` | 判定サイクル（`WDGM_SUPERVISION_CYCLE_MS`） |
| `osTask` | 生成物には出ない。監視する Runnable を呼ぶ Os のタスクの関数名で、周期を `Os_PBCfg.c` から読んで整合性の検査に使う |
| `expectedAliveIndications` | Alive の期待回数（`WDGM_<name>_EXPECTED_ALIVE_INDICATIONS`） |
| `checkpoints[]` | チェックポイント ID（`WDGM_CP_<name>_<cp>`） |
| `transitions[]` | Logical 監視の許可遷移テーブル（`WdgM_<Name>Transitions`）。`from` に `INITIAL`（起動直後）が使える |
| `deadlines[]` | Deadline 監視の許容時間（`WDGM_<name>_DEADLINE_<from>_TO_<to>_MIN_MS` / `MAX_MS`）と、テーブル（`WdgM_<Name>Deadlines`） |

- エンティティを足すには、`WdgM.json` の `entities` に 1 件足して `gen_cfg.py` を実行します。ID、マクロ、2 つのテーブル、エンティティテーブルの行が揃って増えます。
- 次の整合性を検査します。
  - チェックポイント ID が全エンティティで重複しない（`WDGM_CP_INITIAL` の 0xFF とも衝突しない）。
  - 遷移と Deadline が、そのエンティティのチェックポイントを指している。
  - Deadline の `minMs` が `maxMs` 以下である。
  - `osTask`（監視する Os のタスクの関数名）の周期が、開始から終了までを戻る区間（最後のチェックポイントから最初のチェックポイントへの Deadline）の許容範囲に入っている。
  - `expectedAliveIndications` が、判定サイクルの間に呼ばれる回数（サイクル ÷ タスク周期）を超えない。
- タスクの周期は `WdgM.json` に書かず、`osTask` で指したタスクの周期を `Os_PBCfg.c` の `Os_TaskTable` から読みます（Os の設定を変えると、食い違いが生成エラーになります）。
- `WdgM_Cfg.h` の他の定数（EXPIRED の許容サイクル数、HW ウォッチドッグの周期とタイムアウトなど）と、チェックポイントを実際に報告する処理（`WdgM_CheckpointReached()` の呼び出し）は、手書きのままです。
- 生成の前後で、`WdgM_PBCfg.c`・`WdgM.c`・`Wdg_PBCfg.c` をコンパイルした結果（セクションの内容とリロケーション）が一致することを確認しています。

<a id="fim-can-generation"></a>
## FiM と Can の設定の生成

NvM、WdgM と同じく、信号表とは関係なく、それぞれの JSON だけが入力です。

**FiM**（`config/data/FiM.json`、`modules/fim.py`）

| FiM.json の項目 | 生成されるもの |
|-----------------|----------------|
| `functions[]` の並び順 | 機能 ID（`FIM_FID_<name>`）、機能数（`FIM_FUNCTION_COUNT`）、対応テーブルの添字 |
| `event`、`statusMask` | 対応テーブル（`FiM_Functions[]`）。監視する Dem イベントと、抑止するステータスのビット |

- `event` は `Dem.json` のイベント名（例: `CAN_BUSOFF`）、`statusMask` は `Dem_Cfg.h` に実在するマクロでなければなりません（検査します）。

**Can**（`config/data/Can.json`、`modules/can.py`）

| Can.json の項目 | 生成されるもの |
|-----------------|----------------|
| `csPin`、`intPin` | `Can_Cfg.h` の `CAN_CS_PIN`、`CAN_INT_PIN` |
| `baudrate`、`crystalMhz`、`filter` | `Can_PBCfg.c` の `Can_Config`（フィルタ、ピン、ボーレート、水晶） |
| `controller`、`fd` | `Can_Config` の `fdEnabled`、`fdBaudrate`、`txBitRateSwitch`（CAN FD の設定。AUTOSAR の `CanControllerFdBaudrateConfig` / `CanControllerTxBitRateSwitch` 相当） |

- ボーレートと水晶の指定は、`Can_Cfg.h` の `CAN_BAUDRATE_<値>` / `CAN_CRYSTAL_<値>MHZ` に実在する名前でなければなりません。
- `controller`（`MCP2515` または `MCP2518FD`）に応じて、次を検査します。

  | controller | CAN FD | 使える水晶 [MHz] |
  |------------|--------|------------------|
  | `MCP2515` | 非対応（`fd.enabled` を true にするとエラー） | 8、16、20 |
  | `MCP2518FD` | 対応 | 20、40 |

- `fd.enabled` が true のときは、`fd.dataBitrate`（データ部のビットレート）が、通常のビットレート（`baudrate`）の整数倍でなければなりません。
  MCP2518FD 用の ACAN2517FD ライブラリは、データ部のビットレートを通常のビットレートの倍率で指定するためです。
  トランシーバ（ATA6560 など）の上限を超えない値にしてください。上限はデータシートで確認してください。
- 現行の `Can_Hw`（MCP2515）は `fdEnabled` などの値を参照しません。MCP2518FD の `Can_Hw` を作るときに使う想定の準備です。
- `Can_Cfg.h` の他の定数（エラーコード、ApiId、ボーレートの数値表など）は手書きのままです。
- MCP2518FD に載せ替えるときは、`Can.json` の `controller` を `MCP2518FD`、`crystalMhz` を実際の水晶（20 または 40）に変え、
  `fd.enabled` を true にして `gen_cfg.py` を実行します（ピン番号も新しい配線に合わせて直します）。

どちらも、生成の前後で `FiM_PBCfg.c`・`FiM.c`・`Can_PBCfg.c`・`Can.c` をコンパイルした結果（セクションの内容とリロケーション）が一致することを確認しています。

<a id="symbolic-names"></a>
## ハンドル ID のシンボリック名

CanIf・PduR・Com・SecOC が持つ PDU / 信号の ID は、AUTOSAR のシンボリック名の規則（ECU Configuration の [TPS_ECUC_02108]、BSW General の [SWS_BSW_00200]）でも生成します。
名前は `<モジュールの略称>Conf_<コンテナ定義の名前>_<設定 JSON の name>`、定義先は値を提供するモジュールの `*_Cfg.h` の生成領域です（`modules/symbols.py` が共通の部品）。

| モジュール | 名前 | 値 | 生成領域 |
|------------|------|----|----------|
| CanIf | `CanIfConf_CanIfTxPduCfg_<name>`、`CanIfConf_CanIfRxPduCfg_<name>` | TX / RX PDU の添字 | `CanIf_Cfg.h` の `canif-symbols` |
| PduR | `PduRConf_PduRSrcPdu_<name>` | 経路の `srcPduId` | `PduR_Cfg.h` の `pdur-symbols` |
| Com | `ComConf_ComIPdu_<name>`、`ComConf_ComSignal_<name>` | I-PDU / 信号の添字 | `Com_Cfg.h` の `com-symbols` |
| SecOC | `SecOCConf_SecOCRxSecuredPdu_<name>` | `SecOCRxPduId`（Secured I-PDU の添字） | `SecOC_Cfg.h` の `secoc-symbols` |

- 生成したコードでは、他のモジュールの ID を指す数値をこの名前に置き換えています。PduR の経路の `SrcPduId`・送信先の CanIf の PDU・配信先と送信確認先の Com の I-PDU、
  CanIf が上位層へ返す ID（PduR の経路に対応するもの）、Com の I-PDU の `IPduId` / `PduRId` と信号の `IPduId`、SecOC の `SecOCRxPduId` と `ComRxPduId`（Com の RX I-PDU）です。CanTp が相手の ID（PduR の `DestPduId` / `ConfDestPduId`）は、`CanTp_Cfg.h` のマクロ（`CANTP_RX_SDU_ID` / `CANTP_TX_SDU_ID`）で書き、`PduR.json` の数値が `CanTp.json` と食い違えば生成エラーです。CanIf の `UpperLayerTxPduId` / `UpperLayerRxPduId` のうち CanNm 宛てのものは、CanNm が持つ ID です。`CanNm.json` の `nmTxPduId` / `nmRxPduId`（値）から `CanNm_Cfg.h` に `CanNmConf_CanNmTxPdu_<名前>` / `CanNmConf_CanNmRxPdu_<名前>` を生成し、`CanIf_PBCfg.c` はそのシンボリック名を使います（`<名前>` は `CanNm.json` の `canifTxPdu.pdu` / `canifRxPdu.pdu`、つまり CanIf の PDU の名前）。`CanIf.json` の CanNm 宛ての PDU には `upperLayerTxPduId` / `upperLayerRxPduId` を書きません（書くと生成エラー）。
- 信号 ID は `ComConf_ComSignal_<name>` に統一しました。以前の `COM_SIGNAL_*` マクロは廃止し、手書きのコード（`Rte.c`・`E2EMon.c`）も置き換え済みです（`Com.json` の信号の `macro` 項目もなくなりました）。
- `name` は、C の識別子として使える形（英数字と `_`、先頭は英字）で、同じモジュールの中で重複しないこと（生成時に検査）。PduR の経路は RX と TX で名前空間が共通なので、`_Rx` / `_Tx` のように方向を名前に含めてください（同じ名前で値が違う定義は不正な設定です、[TPS_ECUC_06074]）。
- CanIf の `upperLayerTxPduId` などが PduR の経路と一致しない場合は、生成の前の整合性検査でエラーになります。使うヘッダのインクルードが足りないときは、生成時に警告が出ます。
- 生成するコードが使わない名前には、MISRA C:2012 Rule 2.5（未使用マクロ）の抑止コメントを付けています（CanIf は、PduR の設定に出力が左右されないよう全部に付けます）。
- 注意: 仕様の名前は長く、先頭 31 文字が同じ名前が出ます（例: `PduRConf_PduRSrcPdu_ImmobilizerCmd_Rx` と `..._ImmobilizerStatus_Tx`）。MISRA C:2012 Rule 5.4 を厳密に適用すると逸脱ですが、
  このプロジェクトの MISRA チェック（cppcheck のアドオン）は指摘せず、コンパイラは 63 文字まで区別するため、動作には影響しません。
- 検証: 置き換えの前後で、`CanIf_PBCfg.c`・`PduR_PBCfg.c`・`Com_PBCfg.c` と、各モジュール本体のコンパイル結果が完全に一致することを確認しています。

## ファイル構成

| パス | 内容 |
|------|------|
| `gen_cfg.py` | 目印コメントの差し込み、スキーマ検証、生成の実行 |
| `sync_cfg.py` | 信号表に追加されたフレームの設定を、設定 json へ追加 |
| `checks.py` | 信号表と各モジュール設定の整合性検査 |
| `modules/canif.py`, `com.py`, `pdur.py`, `cantp.py`, `cannm.py`, `cansm.py`, `comm.py`, `nm.py`, `ostask.py`, `dcm.py`, `e2exf.py`, `secoc.py`, `cryptostack.py`, `rte.py`, `dem.py`, `nvm.py`, `wdgm.py`, `fim.py`, `can.py`, `bswm.py` | モジュールごとの生成関数（`SPEC` に生成先と領域 ID を持つ） |
| `modules/common.py`, `defs.py` | コメント・構造体フィールドの整形、設定値の `#define`（説明コメント付き）の整形、`Os_PBCfg.c` の周期の読み込み |
| `../../config/schema/*.schema.json` | 設定 json のスキーマ（型・範囲・列挙） |
| `../../config/data/*.json` | モジュールごとの設定データ（リポジトリ直下の `config/`。コードと分けて置く） |

## 検証

生成時に、次を検査します。

1. スキーマ検証（必須項目、型、PduId の範囲、モジュール名の列挙など）
2. モジュール内の整合性（`name` と `srcPduId` の重複、`frame` と `canId` の指定の排他など）
3. **モジュールをまたぐ整合性**（`checks.py`）
   - 設定が参照するフレームが、信号表に実在し、向き（TX/RX）が合っている
   - 信号表のフレームが、CanIf に漏れなく設定されている
   - CAN ID が、TX 同士・RX 同士で重複していない
   - PduR の TX 経路の `srcPduId` と、CanIf の `upperLayerTxPduId` が一致している（RX も同様）
   - PduR の経路と CanIf の PDU が、同じフレームを指している
   - 信号表のフレームが、Com に漏れなく設定されている（CanNm が直接扱うフレームは `excludedFrames` で除外）
   - 信号表のフィールドが、Com のシグナルとして漏れなく設定されている（意図して使わないものは I-PDU の `unusedFields`）
   - Com のシグナルのフィールドが信号表に実在し、ビット範囲が DLC 内に収まり、互いに重ならない
   - update-bit の位置が、信号表の `(update-bit)` フィールドと一致している
   - PduR が参照する Com の I-PDU が実在し、Com の I-PDU に PduR の経路（または SecOC の経路）がある
   - E2E 用フィールド（`e2e_crc` + `e2e_counter`）を持つフレームに E2EXf の設定がある。Profile05 のレイアウト（CRC16 の直後に Counter8）である、DataID が重複しない、Com の I-PDU に E2E の呼び出し元（RX は `rxIndicationCbk`、TX は `txTransformCbk`）がある
   - SecOC 用フィールドを持つフレームに SecOC の設定がある。Com の I-PDU・PduR の経路・CanIf の RX PDU が、同じフレームでつながっている
   - SecOC の `csmJobId` が、Csm のジョブとして存在し、MACVERIFY である。ジョブが使う鍵が存在する（未使用の鍵・ジョブは警告）
   - BswM のルールが指す I-PDU グループが Com に存在する。I-PDU が属するグループに、**起動する BswM のルールがある**（無いとグループは既定で停止状態のまま動かない。停止ルールが無い場合、属する I-PDU が無いグループは警告）
4. 使うモジュールのヘッダが、C ソースにインクルードされているか（警告）

導入時は、生成の前後でコンパイル結果（オブジェクトファイル）が完全に一致することを確認しています。
生成によって変わるのはコメントの体裁（桁揃え、DaVinci 対応コメントの付与）だけで、構造体の値は変わりません。
追加のフレームを加えた場合の生成結果も、コンパイルが通ることを確認しています。

## json の項目と ECUC パラメータの対応

| json | C の構造体 | DaVinci（ECUC） |
|------|------------|-----------------|
| CanIf `txPdus[].upperLayerTxPduId` | `UpperLayerTxPduId` | `CanIfTxPduId` |
| CanIf `txPdus[].frame` → CAN ID | `CanId` | `CanIfTxPduCanId` |
| CanIf `txPdus[].frame` → DLC | `Dlc` | `CanIfTxPduDlc` |
| CanIf `txPdus[].hth` | `Hth` | `CanIfTxPduHthIdRef` |
| CanIf `txPdus[].txConfirm` | `TxConfirmFct` | `CanIfTxPduUserTxConfirmationName` |
| CanIf `rxPdus[].upperLayerRxPduId` | `UpperLayerRxPduId` | `CanIfRxPduUpperLayerPduId` |
| CanIf `rxPdus[].hrh` | `Hrh` | `CanIfRxPduHrhIdRef` |
| CanIf `rxPdus[].rxIndication` | `RxIndicationFct` | `CanIfRxPduUserRxIndicationName` |
| PduR `rxPaths[].srcPduId` | `SrcPduId` | `PduRSrcPdu/PduRSrcPduHandleId` |
| PduR `rxPaths[].dests[].module` | `Module` | `PduRDestPdu/PduRDestModule` |
| PduR `rxPaths[].dests[].ipdu`（Com 宛て）→ Com の RX I-PDU の添字 | `DestPduId` | `PduRDestPdu/PduRDestPduHandleId` |
| PduR `txPaths[].srcPduId` | `SrcPduId` | `PduRSrcPdu/PduRSrcPduHandleId` |
| PduR `txPaths[].canIfTxPduId` | `CanIfTxPduId` | `PduRDestPdu/PduRDestPduHandleId` |
| PduR `txPaths[].confModule` | `ConfFct` | `PduRTxConfirmation` |
| Com `rxIpdus[]` / `txIpdus[]` の並び順 | `IPduId` | `ComIPduHandleId` |
| Com `rxIpdus[].frame` → DLC（または `dlc`） | `DLC` | `ComIPduLength` |
| Com `*Ipdus[].txModeMode` ほか | `TxModeMode` ほか | `ComTxModeMode` ほか |
| Com `signals[]` の並び順 / `name` | `SignalId`（`ComConf_ComSignal_<name>`） | `ComHandleId` |
| Com `signals[].ipdu` | `IPduId` | `ComIPduRef` |
| Com `signals[].field` → ビット位置・ビット長 | `BitPosition` / `BitSize` | `ComBitPosition` / `ComBitSize` |
| E2EXf `instances[]`（`frame` → DataLength / Offset / DataID） | `E2E_P05ConfigType` | `E2EXf` の E2EProfile 設定（`E2EXf_*_E2EXf`） |
| E2EXf `smConfig` | `E2E_SMConfigType` | E2E ステートマシンの設定（`ECUC_E2E_*`） |
| SecOC `rxPdus[]`（`frame` → レイアウト） | `SecOC_RxPduConfigType` | `SecOCRxSecuredPduLayer` ほか |
| CryptoStack `keys[]` / `jobs[]` / `keys[].keyM` | `Crypto_KeyTable` / `Csm_JobConfigType` / `KeyM_CryptoKeyConfigType` | `CryptoKeyType` / `CsmJob` / `KeyMCryptoKey` |
| BswM `rules[]` | `BswM_RuleType` | `BswModeRequestPort` / `BswMRule` / `BswMAction` |
| Com `ipduGroups[]` の並び順 | `COM_IPDU_GROUP_*` | `ComIPduGroup`（`ComIPduGroupHandleId`） |
| `name` | （なし） | `PduRRoutingPath` / `CanIfTxPduCfg` / `CanIfRxPduCfg` / `ComIPdu` / `ComSignal` の ShortName |

## 方式の検討（なぜ「目印差し込み」にしたか）

生成方式を比較したときの記録です（2026-10）。

| # | 方式 | 概要 | 長所 | 短所 |
|---|------|------|------|------|
| 1 | **目印差し込み（採用）** | 実ソースが兼テンプレート。目印の間だけ生成 | テンプレートが別にない。生成前でもビルドできる。手書きの説明や帯見出しを残せる | 領域内の手編集は消える。目印の外に設定の重複が残りうる |
| 2 | 別ファイルのテンプレート（Jinja2 など） | `.c.j2` から全体を生成し、`.c` は純粋な生成物 | ソース全体を一括管理できる。条件分岐や繰り返しが書きやすい | テンプレートと実ファイルの二重管理。依存が増える。テンプレートは C としてビルドできない |
| 3 | Python が全文を直接出力 | 生成コードを文字列組み立てで全部書く | 依存なし。ロジックと出力が近い | C の体裁が Python 側に埋もれ、見た目を変えにくい |
| 4 | ファイル全体を生成物にする（DaVinci/tresos と同じ流儀） | `*_PBCfg.c` を丸ごと生成。手書き部分は別ファイルに分離 | 実ツールと同じ形で学習価値が高い。目印も混在もない。ARXML の導入時に自然 | 既存の説明コメントの移し先が必要。生成物をコミットするか決める必要がある |
| 5 | データだけを生成（`.inc` や X マクロ） | json → 初期化子の中身だけの `.inc`。`.c` は手書きで `#include` | 手書きと生成の境界がファイル単位で明確 | `.inc` が読みにくい。型の宣言と配列サイズの二重管理が残る |
| 6 | C → json の逆解析（C を正本にする） | C をパースして json に取り込む | 今のコードをそのまま使える | コメントやマクロの解析が難しい。正本が曖昧になる |

### 判断
- 試作の段階では方式 1 が最も軽く、既存の注釈やコメントを残したまま、コンパイル結果が一致することを確認できた。
- 方式 4 へは、目印の範囲を「ファイル全体」に広げるだけで移れる。生成ロジックは変わらない。
  ARXML を扱う段階（ファイルを全面的に機械生成する形）で移行を判断する。
- 方式 2 は二重管理が増え、方式 3 は体裁の変更が難しくなるため、現時点では採らない。
  方式 6 は逆解析の難しさに見合う利点がない。

### 方式 1 を保つ間の注意
1. ファイル冒頭の概要コメントは、json の内容を説明する文面に限る。数値や ID は書かない（手で合わせる必要が出る）。
2. モジュールを足すときは、生成領域の ID と生成関数を `gen_cfg.py` の `MODULES` に登録する。
3. 全モジュールへ展開して手書き部分が薄くなってきたら、方式 4 へ切り替えるかを判断する。

## Com の構成

Com（`Com_PBCfg.c`、`Com_Cfg.h`）の設定は、信号表に由来する値と、Com としての振る舞いを分けて持ちます。

### 信号表に置くものと、Com の設定 json に置くもの

| 信号表（フレームの事実） | Com の設定 json（Com としての振る舞い） |
|--------------------------|------------------------------------------|
| フレーム名、CAN ID、DLC、方向 | I-PDU ID、信号 ID（RX / TX で別の番号空間。並び順が ID） |
| 送信周期、受信タイムアウト（`constants` の `fromFrame` で参照） | 送信モード（DIRECT / PERIODIC / MIXED）、最小送信間隔、再送回数・間隔、送信デッドライン |
| フィールド名、ビット位置・サイズ、型、単位・スケール・範囲 | フィルタ、TransferProperty、初期値、タイムアウト時の動作、無効値の扱い |
| update-bit フィールド（位置） | どのフィールドを Com の信号にするか（`field` で名前参照）、update-bit への寄与の指定 |
| E2E / SecOC 用のフィールド（種別のみ） | コールバック名（通知、タイムアウト、callout、変換関数）、I-PDU グループ、ゲートウェイの対応 |

- 信号表のフィールドのうち、`e2e_*` / `secoc_*` の種別と、名前が `(` で始まるもの（update-bit）は、Com のシグナルにならない（自動で除外）。
- 信号表の値と異なる DLC を使う場合だけ、I-PDU に `dlc` を書く（例: SecOC 保護のフレームで、Com が見るのは認証対象のペイロードだけ）。
- `comments` に、各フィールドの注釈を書ける（無ければ、標準の注釈または注釈なし）。

### I-PDU グループと BswM
I-PDU グループは `Com.json` の `ipduGroups`（並び順が ID）に書く。I-PDU は `ipduGroupId` にマクロ名で所属を指定し（省略すると `COM_IPDU_GROUP_NONE`）、
BswM のルール（`BswM.json`）は `ipduGroup` に同じマクロ名を書いて、グループを起動・停止する。
BswM のルールは `perValue` で、値ごとに 1 本ずつ展開できる（Dcm の通信モード 12 通りなど）。
ルールの番号は並び順（添字）で、コメント中の `Rule N` は手で書いた文字なので、ルールを挿入・削除したときは直す。

### 既定値（json で省略したときの値）
`Com.json` には、**既定値と違う値だけ**を書く。省略した項目は、次の値になる（`modules/com.py` の `DEFAULT_*` と `ENUM_ZERO`）。

| 項目 | 省略したときの値 | 備考 |
|------|------------------|------|
| 数値の項目（`timeoutMs`、`isSignalGroup`、`mask`、`filterX` など） | 0 | C の初期化子の省略と同じ |
| コールバック（`*Cbk`） | なし（NULL） | 同上 |
| 列挙の項目（`filterAlgorithm`、`transferProperty`、`rxDataTimeoutAction`、`dataInvalidAction`） | 値が 0 の列挙値（`COM_FILTER_ALWAYS` など） | C の初期化子の省略と同じ |
| `ipduGroupId` | `COM_IPDU_GROUP_NONE` | **C の省略（0）とは異なる。** 0 は `COM_IPDU_GROUP_TELEMETRY` |
| `updateBitPosition` | 信号表の `(update-bit)` の位置。無ければ 65535（0xFFFF。なし） | **C の省略（0）とは異なる** |
| `txModeMode`（TX の I-PDU のみ） | `COM_TX_MODE_DIRECT` | **C の省略（0）とは異なる。** 0 は `COM_TX_MODE_MIXED` |

- 生成される C では、C の省略（0）と値が異なる上の 3 項目は、省略しても必ず明示して出力する。
- `comments` に注釈があるフィールドは、値を省略していても既定値で出力する（注釈と一緒に残す）。
- `txModeModeTrue` は、TMS を使う I-PDU だけが持つ項目なので、既定値は置かない（省略すると 0 = `COM_TX_MODE_MIXED`）。

### 移行で見つかった食い違い
- `EngineInfo` / `AbsInfo` の受信タイムアウトは、信号表では 3000 ms だったが、Com の実際の値（`COM_TIMEOUT_*_MS`）は 5000 ms だった。
  信号表の注記に「`COM_TIMEOUT_*_MS` の実測値」とあったため、信号表を実際の動作に合わせて 5000 ms に直した。

### E2E / SecOC の設定 json
E2E と SecOC の設定は、Com から分けて別の設定 json（`E2EXf.json`、`SecOC.json`）にしてある。

- **E2EXf.json**: どのフレームをどのインスタンスで保護するか（`frame`、`direction`、`profile`）、MaxDeltaCounter、Dem イベント、ステートマシンのしきい値。
  対応するプロファイルは Profile05 のみ。インスタンスごとの C の変数名は `stem` から作る（`E2EXf_<stem>CfgP05` など）。
- **SecOC.json**: どのフレームを保護するか、検証成功後に渡す Com の RX I-PDU（`comIpdu`）、Csm のジョブ、検証結果の通知関数。
  TX の Secured I-PDU は未対応（現状 TX 方向で SecOC を使う PDU は無い）。
- Com の変換関数・callout の名前（`txTransformCbk`、`rxIndicationCbk` など）は、Com の設定 json に書いてある。E2EXf の設定とつながっているかは検査する。
- 鍵・ジョブ・鍵名は `CryptoStack.json` にある（Crypto の鍵表、Csm のジョブ表、KeyM の鍵名表を 1 つの json から生成）。鍵は学習用の固定鍵で、16 進 32 桁で書く。
  ジョブの ID と鍵の ID は、並び順（添字）が値になる。CryIf の設定は対象外。

## 今後の課題

- Com の設定 json の整理。既定値を導入して、通常と違う値だけを書く形にする。ID マクロの説明など、json の中の長い説明文の置き場所も見直す。
- E2E の他のプロファイル（P01、P02 など）への対応。SecOC の TX Secured I-PDU への対応。
- 他のモジュール。信号表との関係は薄いため、必要になったら足す。NvM、WdgM、FiM、Can は対応済み。CanSM と ComM は、表になる設定が無いため対象外。
- Rte の信号アクセス関数の生成は、信号グループの I-PDU・ゲートウェイの信号・手書きの関数がある信号を対象外にしている。信号グループ（`Rte_SendSignalGroup_*`）や、E2E 変換つきの Read は手書きのまま。
- CanSM と ComM は、表になる設定が無いため対象外（BswM は対応済み）。
- 診断の設定のうち、Dem のイベント定義は対応済み。Dcm（UDS）は、DID が表ではなく `switch` 文のコードになっているため、DID を表にして `Dcm.c` を表を引く作りに変える改造が先に必要。
- ARXML の入出力。json の項目名を ECUC のパラメータ名に寄せてあるので、変換層を足す形にできる。

- **Wdg の HW タイムアウトは WdgM_Cfg.h の値を指します**: `Wdg.json` の `defaultTimeout.macro` に、WdgM_Cfg.h の `WDGM_HW_WATCHDOG_TIMEOUT_MS` のようなマクロ名を書くと、`Wdg_PBCfg.c` の `Wdg_Config`（`DefaultTimeoutMs`）を生成します。値の実体は WdgM_Cfg.h の 1 か所だけで、Wdg.c は WdgM_Cfg.h を知りません（下位層が上位層の設定ヘッダに依存しない）。マクロが WdgM_Cfg.h に実在しない、または値が 1〜65535（uint16）に収まらないときは、整合性検査でエラーになります。
- **Mcu は今のところ中身がありません**: 本プロジェクトの Mcu は、クロック初期化を Arduino フレームワークに任せ、RAM セクション初期化も複数の電源モードもモデル化しないため、`Mcu_ConfigType` は実フィールドの無いプレースホルダ（`Dummy`）です。`Mcu.json` は、AUTOSAR の McuModuleConfiguration コンテナの置き場所として用意したもので、クロック設定やリセット要因の扱いを足すときに、ここへ項目を足していきます。
- **設計値のマクロだけのモジュール**: CryIf の `CRYIF_CHANNEL_ID`（`CryIf.json`、形式は `defines.py` 参照）と、Det の既定のログレベル `DET_LOG_LEVEL`（`Det.json`。ビルド設定の `-DDET_LOG_LEVEL=...` で上書きできるよう、`#ifndef` の中の既定値を生成します。値は `LOG_T` / `LOG_E` / `LOG_W` / `LOG_I` / `LOG_D` のどれか）を生成します。E2E・Fee・MemIf・WdgIf・Gpt の `*_Cfg.h` は、ModuleId・エラーコード・ApiId・バージョンなど AUTOSAR の仕様で決まる固定値と型定義だけで、プロジェクトの設計値を含まないため、対象にしていません（Gpt のチャネルは `Gpt_PBCfg.h` / `Gpt_PBCfg.c` から生成済み）。
- **IoHwAb のサンプル数は、時間とタスク周期から求めます**: `IoHwAb.json` の `button.debounceMs`（ボタンの生レベルが連続して変わり続けたら確定値を変える時間）と `button.stuckMs`（押下確定から固着と判定するまでの時間）を、`osTask`（`IoHwAb_MainFunction` を呼ぶタスク。Os.json の `function`）の周期で割って、`IoHwAb_Cfg.h` の `IOHWAB_BUTTON_DEBOUNCE_COUNT` と `IOHWAB_BUTTON_STUCK_COUNT` を生成します。タスクの周期を変えても、時間が保たれるようにサンプル数が追従します。時間が周期の整数倍でない、サンプル数がカウンタの型に収まらない（デバウンス 1〜255、固着 1〜65535）、電圧低下の閾値が Adc.json の基準電圧 `ADC_REF_VOLTAGE_MV` を超える、といった食い違いは、整合性検査でエラーになります。ADC 電圧低下の閾値 `adc.lowVoltageThresholdMv` は、`IOHWAB_ADC_LOW_VOLT_THRESHOLD_MV` になります。
