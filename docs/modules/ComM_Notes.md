# ComM（通信マネージャ）

> [README](../../README.md) の「[ECU 管理層](../../README.md#ecu-management)」節から分離。

ComM (Communication Manager) は、複数の「ユーザ」からの通信モード要求を集約し、
CAN バスの通信モード（NO_COM / SILENT_COM / FULL_COM）を決定するモジュールです。
実際の CAN コントローラ操作は CanSM (`CanSM_RequestComMode`) に委譲します。
調停ロジック・Dcm セッション連携・ボランタリスリープ連携・ウェイクアップ時の
再同期の詳細を以下にまとめます。

## 複数ユーザの調停（AUTOSAR SWS_ComM_00069）

当初は EcuM（`COMM_USER_0`）だけが起動時に一度 FULL_COM を要求し、以後誰も要求を
変えない「実質1ユーザ」の実装でした。Dcm の SID 0x2F (IOControl) 実装を機に、
Dcm を2人目のユーザ（`COMM_USER_1`）として追加し、`ComM_RequestComMode()` に
複数ユーザの要求を実際に集約するロジックを実装しました。

```
ComM_RequestComMode(User, ComMode):
  ComM_UserRequest[User] = ComMode            ← このユーザの要求を記録
  aggregated = max(ComM_UserRequest[0..N-1])  ← 全ユーザの要求のうち最も通信レベルの
                                                高いモードを採用
                                                (FULL_COM(2) > SILENT_COM(1) > NO_COM(0))
  aggregated == 現在のチャネルモード ?
    YES → 何もしない（要求は記録されたがチャネルへの反映は不要）
    NO  → CanSM_RequestComMode(0, aggregated) ← チャネルへ実際に反映
```

「誰か一人でも通信を必要としていればバスは落とさない」という考え方で、
1人が NO_COM を要求しても他のユーザが FULL_COM を要求していればチャネルは
FULL_COM のまま維持されます。

## Dcm との連携（`COMM_USER_1`）

`Dcm_Cbk.c` は診断セッションの状態に応じて `COMM_USER_1` の要求を更新します
（`Dcm_UpdateComMRequest()`、セッション遷移が起こるすべての経路から呼ばれる）。

| タイミング | 要求するモード |
|-----------|---------------|
| extendedSession に入ったとき（SID 0x10/0x03） | `COMM_FULL_COMMUNICATION` |
| defaultSession へ戻ったとき（明示要求・S3タイムアウト・ECUReset のいずれも） | `COMM_NO_COMMUNICATION` |

「診断ツールが繋がっている間はバスを落とさない」という実車でもよくある要件を、
EcuM（`COMM_USER_0`）とは独立したユーザ要求として表現しています。

## App_EngineManager との連携（`COMM_USER_0`）

当初は EcuM が起動時に要求した FULL_COM を一度も解放しない「実質固定」でしたが、
`App_EngineManager_Run()` が `ENGINE_STATE_OFF` の継続（既定 5 周期、実質15秒）を
検知すると `Rte_Call_ComM_RequestComMode(NO_COM)` 経由で `COMM_USER_0` の要求を
実際に解放するようになりました（ボランタリスリープ。詳細は README の「CAN 通信スタック」
セクションの「ボランタリスリープとウェイクアップ」を参照）。

これにより、複数ユーザ調停が実際に意味を持つ場面が生まれました。
「エンジンが止まっていて（`COMM_USER_0` が NO_COM 要求）、かつ診断ツールも
繋がっていない（`COMM_USER_1` も NO_COM 要求）」ときだけ集約結果が NO_COM になり、
どちらか一方でも通信を必要としていればチャネルは FULL_COM のまま維持されます。

```
[Extended Session 突入中にエンジン OFF が継続した場合]
INFO AppEng: OFF continued 5 cycles -> release COMM_USER_0 (voluntary sleep)
INFO ComM: User0 req=0 -> aggregated=2 (channel=2)   ← User1(Dcm)がFULL_COM(2)要求中のため変化なし

[Extended Session 終了後、なおエンジン OFF が継続していた場合]
INFO Dcm: S3 timeout -> session=Default
INFO ComM: User1 req=0 -> aggregated=0 (channel=2)   ← User0も既にNO_COM要求済みのため今度こそ集約結果が変化
INFO CanSM: ->NO_COM (CAN controller SLEEP)
```

## ウェイクアップ時の User0 要求の再同期

CanSM がウェイクアップ検証成功時に `ComM_BusSMIndication(FULL_COM)` を呼んで
チャネル状態を更新するのは、どのユーザの要求でもない自動的な変化です。これを
放置すると `ComM_UserRequest[COMM_USER_0]` がスリープ突入時の古い値（`NO_COM`）
のまま残り、`App_EngineManager_Run()` がまだ 1 周期も再評価していない
（Task 2 が次に実行されるのは最大3000ms後）わずかな間に他ユーザ（Dcm）が
要求を変化させただけで、User0 の古い要求と誤って再集約され、ウェイクアップ
直後に意図せず即座に再スリープしてしまいます。

これを防ぐため `ComM_BusSMIndication()` は `FULL_COM`/`NO_COM` を通知するとき、
`ComM_UserRequest[COMM_USER_0]` もその値へ同期します。CanSM 側の自動的な状態変化を
「User0 の暫定的な要求」とみなすことで、App_EngineManager が次に実際のエンジン
状態に基づいて要求し直すまでの間、矛盾のない値を保持できます。Dcm
（`COMM_USER_1`）の要求はセッション状態に基づく独立した判断のため、これには
同期させません（実機で発見された経緯は
後述の「[開発の経緯](#ウェイクアップ直後の再集約による即座の再スリープ)」参照）。

## ComM 設定（`ComM_Cfg.h`）

| 定数 | 既定値 | 意味 |
|------|--------|------|
| `COMM_USER_COUNT` | 2 | 通信モードを要求できるユーザ数 |
| `COMM_USER_0` | 0 | EcuM/App_EngineManager（エンジン運転中は FULL_COM、OFF 継続時は NO_COM を要求） |
| `COMM_USER_1` | 1 | Dcm（extendedSession の間だけ FULL_COM を要求） |

## 開発の経緯（実機で見つかった不具合・設計変更）

> 現在の仕様を理解するだけなら読む必要はありません。実機検証で見つかった
> 不具合や、その結果としての設計変更の経緯を時系列でまとめています。

### ウェイクアップ直後の再集約による即座の再スリープ

ウェイクアップ検証の実機テスト中に、次のような不具合が見つかった。

```
[ウェイクアップ検証成功 → FULL_COM 確定 → EcuM: SHUTDOWN -> RUN]
INFO CanSM: Wakeup validated (RX confirmed) -> FULL_COM
INFO ComM: ch0 ->mode=2
INFO EcuM: SHUTDOWN ->RUN (wakeup) user=0

[同じ受信フレームの中身が defaultSession への SessionControl だった場合]
INFO Dcm: req SID=0x10
INFO ComM: User1 req=0 -> aggregated=0 (channel=2)   ← 直後に再スリープ！
INFO CanSM: ->NO_COM (CAN controller SLEEP)
```

`extendedSession` への切替では再現せず、`defaultSession` への切替でのみ
再現していた。原因は `ComM_BusSMIndication()` にあった。CanSM がウェイクアップ
検証成功時に `ComM_BusSMIndication(FULL_COM)` を呼んでチャネル状態を更新しても、
**どのユーザの要求でもない自動的な変化**であるため `ComM_UserRequest[COMM_USER_0]`
は更新されず、スリープ突入時の古い値（`NO_COM`）のまま残っていた。

`App_EngineManager_Run()` がまだ 1 周期も再評価していない（Task 2 が次に実行
されるのは最大3000ms後）そのわずかな間に、受信したフレームの内容が
defaultSession への SessionControl だったため、Dcm が
`ComM_RequestComMode(COMM_USER_1, NO_COM)` を呼んだ。この時点で再集約すると
`max(User0=NO_COM（古い値）, User1=NO_COM) = NO_COM` となり、実際には
ウェイクアップ検証を通過してチャネルが FULL_COM になったばかりにも関わらず、
即座に再スリープしてしまっていた。`extendedSession` の場合は `User1=FULL_COM`
になるため、User0 の古い値が埋もれて表面化しなかっただけである。

**修正**: `ComM_BusSMIndication()` が `FULL_COM`/`NO_COM` を通知するとき、
`ComM_UserRequest[COMM_USER_0]` もその値へ同期するようにした。CanSM 側の
自動的な状態変化を「User0 の暫定的な要求」とみなすことで、App_EngineManager が
次に実際のエンジン状態に基づいて要求し直すまでの間、矛盾のない値を保持できる。
Dcm（`COMM_USER_1`）の要求はセッション状態に基づく独立した判断のため、これには
同期させない。
