/**
 * \file    Wrap_Dem.h
 * \brief   `src/Bsw/Dem/Dem.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクション/呼び出し記録のアクセサ群。
 *
 * \details `test/stub/` 配下の構成規則は `test/stub/Bsw/CanIf/Wrap_CanIf.h`
 *          冒頭コメント参照（「wrap 対象の関数が定義されている元の src ファイル
 *          1 つにつき 1 ファイル」）。
 *
 * \par Dem_SetEventStatus
 * 2026-09、`native_chain` を「Dem をフェイクにする」から「実体の `Dem.c` を
 * リンクする」へ切り替えた際に新設（旧 `Bsw_Dem_fake.h`。CanSM.c/WdgM.c から
 * 見た通知先の呼び出し回数・引数を記録したいだけで、Dem 側のデバウンス・
 * DTC 確定・EEPROM 永続化を無効化する意図は無いため、既定は常に
 * `__real_Dem_SetEventStatus()` へパススルーする（`ForceFail` のような
 * 差し替えトグルは持たない）。CanSM の Bus-Off 報告・WdgM の Global
 * Supervision Status 報告は対象イベントのデバウンス閾値がいずれも 1
 * （`Dem_Cfg.h` の `DEM_DEBOUNCE_LIMIT_CAN_BUSOFF`/
 * `DEM_DEBOUNCE_LIMIT_WDGM_SUPERVISION`）のため、1 回の呼び出しで
 * 確定/回復する既存テストの構造はそのまま維持できる。
 *
 * 呼び出し記録（`WrapDemSetEventStatus_*`）は `test/test_chain/` の各テスト
 * ファイルが `SetUp()` の外（テスト本体の途中）でも「ここまでの報告回数を
 * リセットし、以降だけを見る」ために使うため、`Dem_Init()` とは独立して
 * リセットできるようにしてある（`WrapDemSetEventStatus_Reset()` は Dem 自体の
 * 内部状態には触れない）。Dem 自体の内部状態（`Dem_StatusTable[]` 等）を
 * テストケース間でクリーンに戻すには、各テストの `SetUp()` で別途
 * `Dem_Init(NULL)` を呼ぶこと（`test/test_chain/Fake_NvM.c` により常に
 * 「初回起動」相当の決定的なリセットになる）。
 */
#ifndef WRAP_DEM_H
#define WRAP_DEM_H

#include "Std_Types.h"
#include "Dem.h"

#ifdef __cplusplus
extern "C" {
#endif

/** `__wrap_Dem_SetEventStatus()` が呼ばれた回数。 */
extern uint32 WrapDemSetEventStatus_CallCount;

/** 直近の呼び出し引数。 */
extern Dem_EventIdType     WrapDemSetEventStatus_LastEventId;
extern Dem_EventStatusType WrapDemSetEventStatus_LastEventStatus;

/** 呼び出し記録だけをクリアする（Dem 自体の内部状態には触れない）。 */
void WrapDemSetEventStatus_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_DEM_H */
