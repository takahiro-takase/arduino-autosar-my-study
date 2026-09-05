# SecOC（Secure Onboard Communication、メッセージ認証）

> [README](../../README.md) の「[CAN 通信スタック](../../README.md#can-stack)」節から分離。

E2E（README参照）は CRC・カウンタによる「意図しない通信エラー」の検出が目的で、
アルゴリズム自体が公開されており秘密鍵を使わないため、悪意ある攻撃者が正しい
CRC/カウンタを計算して偽のフレームを送ること自体は防げません。**SecOC** は
これとは別の軸として、秘密鍵ベースの MAC（Message Authentication Code）と
フレッシュネス値（リプレイ攻撃対策）で「意図的な改ざん・なりすまし」を検出する
AUTOSAR モジュールです。ユーザーが実際に AUTOSAR CP R4.3.1 の SWS/SRS 仕様書
PDF（`docs/AUTOSAR_SWS_SecureOnboardCommunication.pdf`）を入手したため、
これまでの Com 機能と同様、実 PDF から検証した要求番号を引用しながら実装
しています。

## アーキテクチャ — E2E Transformer 方式とは異なる理由

E2E は「Com のコールバックフック（RxIndicationCbk/TxTransformCbk）経由で Rte 層が
呼ぶ」E2E Transformer 方式（AUTOSAR が定義する 3 つの E2E 統合方式の 1 つ）を
採用していますが、SecOC には対応する「Com フック経由」の統合方式が実 AUTOSAR に
存在しません。SecOC は常に、PduR のルーティング経路上に独立した宛先モジュールと
して構成されます（`Com_IPduConfigType.RxIndicationCbk` のような E2E 用の仕組みを
流用せず、`PduR_RxDestType`——CanTp/Com と同じ立ち位置——として実装しています）。

```
【RX】KeyFobEcu (uds_tester Python が模擬)
  → AES-128-CMAC で Secured PDU を生成（pycryptodome。Arduino 側の自前実装
    との相互検証は下記「検証」節参照）
  → CAN 0x120 送信

  Arduino (MeterEcu):
    Can_Hw → CanIf_RxIndication → PduR_ComRxIndication
      → SecOC_RxIndication()（PduR 宛先モジュール、DestPduId=0）
          Authentic Payload / Freshness Value / 切り詰め MAC を分離
          AES-128-CMAC を自前実装で再計算し MAC 一致を検証
          Freshness（8bit、単調増加）を検証（リプレイ検知）
          両方 OK → Com_RxIndication(ComRxIPduId=2, AuthenticPayload) を直接呼ぶ
          NG → ログのみ、Com へは一切転送しない
      → Com_ReceiveSignal(IMMOBILIZER_CMD) → Rte_COMRxInd_SecureCommand()（ログのみ）

【TX】現在 SecOC を使う TX I-PDU は無い（後述）。以前は E2EHealthStatus
  （CAN 0x220）が以下の経路で SecOC 保護されていたが、E2E を Profile05（CRC16）
  へ強化した際に DLC が classic CAN の 8byte 上限を超えるため撤去した:
    E2EMon → Com_SendSignal() → Com_MainFunctionTx()（PERIODIC、6000ms周期）
      → TxTransformCbk（E2EXf、E2E CRC/Counter を書き込む）
      → PduR_ComTransmit(SrcPduId=3, 4byte)
          → TransmitOverrideFct=SecOC_IfTransmit()（Authentic I-PDU を内部
            バッファへコピーし即座に E_OK を返す。[SWS_SecOC_00058]）
      → 次回 SecOC_MainFunctionTx()（100ms周期）:
          Freshness（自身の単調増加カウンタ）+ AES-128-CMAC を計算
          Secured I-PDU（8byte）を組み立て
          → PduR_SecOCTransmit(SrcPduId=3, 8byte) → CanIf_Transmit() → CAN 0x220 送信
```

PduR の RX 振り分け機構（`PduR_RxDestType`）は元々、1 つの受信 PDU を複数の
上位層モジュールへ配信できる汎用的な関数ポインタテーブルだったため、RX 方向は
SecOC を新しい宛先として追加するだけで済み、`PduR.c` 自体の変更は不要でした。

TX 方向は当初 RX 専用スコープとして見送っていましたが、「E2EHealthStatus に
メッセージ認証を付与したい」という要求をきっかけに、実 AUTOSAR の
`SecOC_Transmit()` アーキテクチャ（`[7.4.1]` "Authentication during direct
transmission"）に忠実な形で追加しました。実装当初の RX 専用スコープでは
「TX 方向は PduR の TX 経路が `CanIf_Transmit()` 直呼びにハードコードされて
おり、汎用化には手が入る」という理由で意図的に見送っていましたが、今回は
その汎用化自体を行っています。`PduR_TxRoutingPathType` に
`TransmitOverrideFct`/`TransmitOverrideId` を追加し、NULL（既定）なら従来どおり
`CanIf_Transmit()` へ直接転送、非 NULL なら中間モジュール（SecOC）へ委譲する形に
一般化しました（既存の全 TX パスはこのフィールドを設定しないため無変更・無
リグレッションです）。中間モジュールは変換完了後、`PduR_ComTransmit()` とは別の
`PduR_SecOCTransmit()`（`TransmitOverrideFct` を再評価しない）を呼んで
`CanIf_Transmit()` まで到達させます（`TransmitOverrideFct`/`TransmitOverrideId`
という機構自体は、現在これを使う TX I-PDU が無くなった後も学習用リファレンス
実装として残している）。

Com/E2E は SecOC の存在を一切知りません（E2E Transformer 方式で Com が E2E の
存在を知らないのと同じ設計思想）。`Com_SendSignal()`/`TxTransformCbk` は
E2E 保護済みペイロードを扱うだけで、SecOC の有無を一切意識しない設計に
なっています（TX 方向で実際に SecOC を使っていた頃も、E2EHealthStatus を
Profile05 へ切り替えた現在も、この点は変わりません）。

## Secured I-PDU バイトレイアウト（SecOC Profile 1 準拠）

`docs/AUTOSAR_SWS_SecureOnboardCommunication.pdf` の **SecOC Profile 1
(24Bit-CMAC-8Bit-FV)**（`[SWS_SecOC_00192]`）に忠実に、CMAC/AES-128・8bit
フレッシュネス・24bit 切り詰め MAC を採用しています。対象フレーム
「ImmobilizerCmd」（CAN ID 0x120、イモビライザー解除コマンドという実車でも
真に認証が必要な典型シナリオ）のレイアウト:

| byte | 内容 | サイズ |
|---|---|---|
| 0 | ImmobilizerCmd（0x00=LOCK, 0x01=UNLOCK） | 1 |
| 1 | Reserved（常に 0x00。将来の鍵 ID 等を想定） | 1 |
| 2 | Freshness Value（8bit、切り詰めなしの全ビットを送信） | 1 |
| 3-5 | 切り詰め MAC（AES-128-CMAC 128bit 出力の上位24bit） | 3 |

**Authenticator の対象データ**（`[7.1.1.2]` "DataToAuthenticator = Data
Identifier | secured part of the Authentic I-PDU | Complete Freshness
Value"）: `DataId(2byte, Big Endian, =0x0120) | AuthenticPayload(2byte) |
FreshnessValue(1byte)` の 5 バイトを AES-128-CMAC へ入力します（Big Endian は
`[SWS_SecOC_00011]`）。

**TX 方向: 現在 SecOC で保護している I-PDU は無い**（`SECOC_TX_PDU_COUNT=0`）。
以前は「E2EHealthStatus」（CAN ID 0x220）を、E2E Profile01 保護済みの 4byte
ペイロード全体を Authentic I-PDU として扱い、SecOC で外側からさらに保護する
構成でした（内側=E2E で意図しない誤りを検出、外側=SecOC で意図的な改ざん・
なりすましを検出、という二重防御の実例）:

| byte | 内容 | サイズ |
|---|---|---|
| 0 | E2E CRC8（Authentic payload、内側の E2E 保護） | 1 |
| 1 | E2E Counter（Authentic payload、下位4bit） | 1 |
| 2 | E2ECrcErrCount（Authentic payload） | 1 |
| 3 | E2ESeqErrCount（Authentic payload） | 1 |
| 4 | Freshness Value（8bit、切り詰めなし） | 1 |
| 5-7 | 切り詰め MAC（AES-128-CMAC 128bit 出力の上位24bit） | 3 |

E2EHealthStatus の E2E を Profile01（CRC8、ヘッダ2byte 相当）から Profile05
（CRC16、ヘッダ3byte）へ強化するにあたり、上記 SecOC 分（Freshness1byte+MAC3byte）
を足すと classic CAN の DLC=8 上限を超えてしまうため、SecOC 側を撤去し
E2E Profile05 単体保護に切り替えました（`E2EXf_PBCfg.c`/`SecOC_PBCfg.c`/
`Csm_PBCfg.c`/`Crypto_PBCfg.c`/`KeyM_PBCfg.c` から関連設定・鍵を削除済み）。
`TransmitOverrideFct`/`SecOC_IfTransmit()` 等の機構自体は削除せず、学習用
リファレンス実装として残しています。

## 明示する簡略化

- **Freshness Value は 8bit 幅全体を送受信し、切り詰めを行いません**（Profile 1 の
  `SecOCFreshnessValueTxLength=8` と一致させ、実車で必要な「送信されない上位
  ビットの推定復元」機構を回避する簡略化）。
- **Csm(Crypto Service Manager)/CryIf(Crypto Interface)/Crypto(Crypto Driver)
  の3層に分離しています**（`src/Bsw/Csm/`・`src/Bsw/CryIf/`・`src/Bsw/Crypto/`）。
  SecOC は `Csm_MacGenerate()`/`Csm_MacVerify()` という抽象 API と `CsmJobId`
  （SecOC_PBCfg.c）しか知らず、実際にどの鍵・どのアルゴリズムを使うかは
  `Csm_PBCfg.c` の CsmJob 設定 → `Crypto_PBCfg.c` の鍵テーブルが決めます
  （AES-128/CMAC の実装自体は `Crypto_Aes128.c`/`Crypto_Cmac.c`。元は
  `SecOC_Aes128.c`/`SecOC_Cmac.c` として SecOC 内に直接持っていたものを移設）。
  また実 AUTOSAR の Csm/CryIf/Crypto はジョブベースの非同期処理
  （`Crypto_JobType` の START/UPDATE/FINISH モード、ジョブキュー）が前提ですが、
  本プロジェクトは全体が同期呼び出しのため常に `CRYPTO_OPERATIONMODE_SINGLECALL`
  のみを使う同期実装です。`Crypto_JobType` 自体も実 AUTOSAR の4段階入れ子構造体
  （`Crypto_JobPrimitiveInputOutputType` 等）ではなく、MAC 生成/検証の2用途に
  必要なフィールドのみを持つ簡略化フラット構造体にしています。
- **鍵バイト列の初期値は `Crypto_PBCfg.c` の固定配列**です（`Crypto_Init()` が
  起動時に RAM キースロットへコピーする。固定鍵をソースコードへ埋め込むことは
  本番運用では絶対に行ってはなりません）。**KeyM（Key Manager）による鍵更新は
  `src/Bsw/KeyM/` に実装済み**で、Dcm の WriteDataByIdentifier（DID
  0x0108 CryptoKeyUpdate）が模擬鍵マスターとして `KeyM_Start()`→
  `KeyM_Update()`→`KeyM_Finalize()` を1回の診断要求内で駆動し、
  `Csm_KeyElementSet()`/`Csm_KeySetValid()` 経由で RAM 上の鍵を書き換えます
  （更新直後は無効化され、Finalize されるまで MAC 生成/検証に使えません）。
  ただし **実 AUTOSAR の KeyM が持つ Certificate submodule（X.509 証明書
  チェーンの格納・検証）は完全に対応除外**（本プロジェクトに PKI の土台が
  一切ないため）で、Crypto key submodule も `KeyM_Start`/`KeyM_Update`/
  `KeyM_Finalize` の3 API のみに絞っています（`KeyM_Prepare`/`KeyM_Verify`、
  SHE M1M2M3 形式の鍵更新、`KEYM_DERIVE_KEY` は未対応）。鍵材料は NVM に
  永続化されないため、再起動すれば `Crypto_PBCfg.c` の初期値に戻ります
  （詳細は `src/Bsw/KeyM/KeyM.h` 冒頭コメント参照）。
- **リプレイ判定（RX）は単調増加チェックのみ**（`(uint8)(received -
  lastAccepted) < 128` という折り返し許容の「半区間」判定）で、実車の
  Freshness Manager（11 章、複数カウンタ・マスタースレーブ同期プロトコル）は
  実装していません。
- **TX の送信確認（TxConfirmation）経路は SecOC を経由しません**（実
  AUTOSAR は `[SWS_SecOC_00063]`/`[SWS_SecOC_00064]` で SecOC が確認結果を
  中継し、動的に確保した Secured I-PDU バッファを解放することを要求しますが、
  本実装は固定長静的バッファ（`SecOC_TxAuthenticBuffer[]`）のみを使い動的確保を
  行わないため、解放処理自体が不要です。したがって `CanIf_TxConfirmation` は
  従来どおり `PduR_CanIfTxConfirmation()` から直接 `Com_TxConfirmation()` へ
  届き、SecOC は一切関与しません）。

## 検証

外部ライブラリに依存しない AES-128+CMAC の自前実装（`Crypto_Aes128.c`/
`Crypto_Cmac.c`、Csm/CryIf/Crypto レイヤの最下層）が正しいことを、以下の
独立した手段で確認しています。

1. **FIPS-197 Appendix B の公式 AES-128 テストベクタ**（既知の鍵・平文に対する
   暗号文が一致するか）を `Crypto_Init()` 起動時セルフテストとして組み込み済み
   （実機ログで毎回 PASS/FAIL を確認できます）。
2. **RFC 4493 (The AES-CMAC Algorithm) の公式テストベクタ 4 件**（メッセージ長
   0/16/40/64 バイトの各ケース、パディングあり/なし・単一/複数ブロック連鎖の
   すべての分岐を網羅）について、本実装のアルゴリズム（K1/K2 サブ鍵導出・
   パディング・CBC-MAC 連鎖）を Python へ忠実に移植し、`pycryptodome`
   （実績のある独立したライブラリ）の CMAC 実装と全件一致することを確認済み
   （ホスト環境に C コンパイラが無く組込みコードを直接実行できなかったための
   代替検証手段。C コードとの対応は目視でも再確認済み）。
3. `tools/can_tool` の `_apply_secoc()`（Python、pycryptodome で本物の
   AES-CMAC を計算）と、Arduino 側の `SecOC_RxIndication()` → `Csm_MacVerify()`
   → `Crypto_ProcessJob()`（同一の自前実装）が同じ鍵・同じメッセージに対して
   同じ MAC を計算することを、バイトレイアウト（DataId の Big Endian 連結順・
   切り詰め位置）も含めて突き合わせ済みです。
4. （履歴）以前 TX 方向で SecOC を使っていた際は、`SecOC_MainFunctionTx()` が
   `Csm_MacGenerate()` 経由で呼ぶ `Crypto_Cmac_Calculate()`（RX と同一実装）が
   計算した MAC を、`tools/can_tool` の `_verify_secoc()`（受信した
   Secured I-PDU から MAC を再計算する、`_apply_secoc()` の逆方向）で
   独立に再検証できていました。現在は TX 方向で SecOC を使う I-PDU が無いため
   （E2EHealthStatus を E2E Profile05 単体保護へ切り替えたため）この経路は
   動作していませんが、`_verify_secoc()` 自体は汎用実装なので残しています。

**実機で検証可能**: `tools/can_tool/config.json` に「ImmobilizerCmd
(0x120, KeyFobEcu)」ボタンを追加しました（UNLOCK/LOCK の 2 プリセット）。

- **正常系**: プリセットを送信すると、Arduino ログに
  `SecOC: RxInd: iPdu=0 verified OK (freshness=N)` に続けて
  `Rte: ImmobilizerCmd: UNLOCK (authenticated via SecOC)` が出力されます。
- **改ざん検知**: 送信前に Entry 欄の MAC バイト（末尾 3 バイト）を手入力で
  1 桁変更してから送信すると、`SecOC: RxInd W: ... MAC verification failed`
  が出力され、`Rte_COMRxInd_SecureCommand()` は一切呼ばれません（E2E の
  WRONGCRC 検証と同じ「Entry を手入力で改ざんしてから送信する」方式。
  送信ボタンは常に Entry の内容をそのまま送るため、意図的な改ざんテストが
  行えます）。
- **リプレイ検知**: 一度送信したフレームのログ表示（またはコピーした Entry の
  内容）をそのまま Entry へ貼り戻し、フレッシュネス値を変えずに再送すると、
  MAC は依然として正しいにもかかわらず `SecOC: RxInd W: ... freshness check
  failed (replay or stale)` が出力され、拒否されることが確認できます。

**TX 方向（履歴）**: 以前は `tools/can_tool` の「E2EHealthStatus (0x220)」
受信モニターが、`crcErr=N seqErr=M` に加えて `SecOC:OK`/`SecOC:NG` を表示し、
Arduino ログにも `SecOC: MainFunction: iPdu=0 secured OK (freshness=N)` が
6000ms 周期で出力されていました。E2EHealthStatus を E2E Profile05 単体保護へ
切り替えた際に SecOC を撤去したため、現在はこの表示・ログは出力されません
（TX 方向で SecOC を使う I-PDU が無いため、`SecOC_MainFunctionTx()` の
TX ループは毎回 0 回実行で終わります）。

## 意図的に応用範囲を限定した理由

本モジュールは Com/SecOC/PduR のアーキテクチャ学習が主目的のため、ドア施錠制御
等の実ハードウェア反応までは作り込んでいません（`Rte_COMRxInd_SecureCommand()`
はログ出力のみ）。他の多くの Com 機能（`ComRxDataTimeoutAction` 等）が
「実利より仕様忠実性」であったのに対し、この機能は EngineInfo/AbsInfo の
E2E 検証と同じく、実際に受信経路を通り実機で検証可能です。
