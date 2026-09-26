/**
 * \file    Wrap_Dem.h
 * \brief   `src/Bsw/Dem/Dem.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          フォールトインジェクション/呼び出し記録のアクセサ群。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/CanIf/Wrap_CanIf.h`
 *          冒頭コメント参照（「wrap 対象の関数が定義されている元の src ファイル
 *          1 つにつき 1 ファイル」）。命名は
 *          [[reference_wrap_stub_naming_convention]] の標準テンプレートへ
 *          統一する（`<種類>_<Module>_<関数名>`）。
 *
 *          `Dem.h` が宣言する公開API全19関数を wrap 対象とする。
 *
 * \par Dem_SetEventStatus
 * 2026-09、`native_chain` を「Dem をフェイクにする」から「実体の `Dem.c` を
 * リンクする」へ切り替えた際に新設（旧 `Bsw_Dem_fake.h`。CanSM.c/WdgM.c から
 * 見た通知先の呼び出し回数・引数を記録したいだけで、Dem 側のデバウンス・
 * DTC 確定・EEPROM 永続化を無効化する意図は無いため、既定は常に
 * `__real_Dem_SetEventStatus()` へパススルーする（他の関数と異なり故障注入の
 * トグルは持たない）。CanSM の Bus-Off 報告・WdgM の Global Supervision Status
 * 報告は対象イベントのデバウンス閾値がいずれも 1
 * （`Dem_Cfg.h` の `DEM_DEBOUNCE_LIMIT_CAN_BUSOFF`/
 * `DEM_DEBOUNCE_LIMIT_WDGM_SUPERVISION`）のため、1 回の呼び出しで
 * 確定/回復する既存テストの構造はそのまま維持できる。呼び出し引数
 * （`LastEventId_Dem_SetEventStatus`/`LastEventStatus_Dem_SetEventStatus`）を
 * 個別に記録するのも本関数だけの特別扱い（他の19関数は呼び出し回数のみ）。
 *
 * \par 2026-09、残り18関数を追加
 * 戻り値を持つ11関数（GetDTCStatusAvailabilityMask/GetEventUdsStatus/
 * GetDTCOfEvent/ClearAllDTCs/ClearOneDtc/GetFreezeFrameOfEvent/
 * GetEventIdOfDTC/GetOccurrenceCounterOfEvent/GetFaultDetectionCounter/
 * EnableDTCSetting/DisableDTCSetting）には他モジュールの Wrap_XXX.c と同じ
 * 「指定した呼び出し回数以降は常に失敗を返す」回数閾値方式の故障注入を
 * 実装する。`FailFromCallCount_Dem_Xxx`（既定
 * `WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED`）に N を設定すると、
 * `CallCount_Dem_Xxx` が N 以上になった回から（その回を含め、以降ずっと）
 * `ForcedReturn_Dem_Xxx` を返すようになる。`Dem_GetTranslationType()` は
 * 戻り値が `Std_ReturnType` ではなく `Dem_DTCTranslationFormatType`（成功/失敗
 * を表さない列挙型。本プロジェクトは常に `DEM_DTC_TRANSLATION_ISO14229_1` の
 * みを構成し、呼び出し元もエラー値を想定していない）のため、故障注入は
 * 実装せず呼び出し回数のみ記録する（`Init`/`GetAllDTCs`/`GetSupportedDTCs`/
 * `GetPrefailedDTCs`/`SetFreezeFrameContext`/`GetVersionInfo` の6つの
 * void 関数と同じ扱い）。
 *
 * 呼び出し記録（`CallCount_Dem_Xxx` 等）は各テストファイルが `SetUp()` の
 * 外（テスト本体の途中）でも「ここまでの呼び出し回数をリセットし、以降だけを
 * 見る」ために使うため、`Dem_Init()` とは独立してリセットできるようにして
 * ある（`WrapDem_Reset()` は Dem 自体の内部状態には触れない）。Dem 自体の
 * 内部状態（`Dem_StatusTable[]` 等）をテストケース間でクリーンに戻すには、
 * 各テストの `SetUp()` で別途 `Dem_Init(NULL)` を呼ぶこと
 * （`stub/Bsw/NvM/Fake_NvM.c` により常に「初回起動」相当の決定的な
 * リセットになる）。
 */
#ifndef WRAP_DEM_H
#define WRAP_DEM_H

#include "Std_Types.h"
#include "Dem.h"

#ifdef __cplusplus
extern "C" {
#endif

/** `FailFromCallCount_Dem_Xxx` の「無効（常にパススルー）」を表す番兵値。
 *  `uint32` の最大値のため、テストの呼び出し回数が現実的に到達することはない。 */
#define WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED 0xFFFFFFFFU

/* ----------------------------------------------------------------------
 * 呼び出し回数（公開API全19関数共通）
 * ---------------------------------------------------------------------- */
extern uint32 CallCount_Dem_GetVersionInfo;
extern uint32 CallCount_Dem_Init;
extern uint32 CallCount_Dem_GetEventUdsStatus;
extern uint32 CallCount_Dem_GetDTCOfEvent;
extern uint32 CallCount_Dem_GetFaultDetectionCounter;
extern uint32 CallCount_Dem_SetEventStatus;
extern uint32 CallCount_Dem_GetTranslationType;
extern uint32 CallCount_Dem_GetDTCStatusAvailabilityMask;
extern uint32 CallCount_Dem_DisableDTCSetting;
extern uint32 CallCount_Dem_EnableDTCSetting;
extern uint32 CallCount_Dem_ClearAllDTCs;
extern uint32 CallCount_Dem_ClearOneDtc;
extern uint32 CallCount_Dem_GetAllDTCs;
extern uint32 CallCount_Dem_GetSupportedDTCs;
extern uint32 CallCount_Dem_GetPrefailedDTCs;
extern uint32 CallCount_Dem_SetFreezeFrameContext;
extern uint32 CallCount_Dem_GetFreezeFrameOfEvent;
extern uint32 CallCount_Dem_GetEventIdOfDTC;
extern uint32 CallCount_Dem_GetOccurrenceCounterOfEvent;

/** `Dem_SetEventStatus` のみの直近呼び出し引数（既存の特別扱い、上記コメント参照）。 */
extern Dem_EventIdType     LastEventId_Dem_SetEventStatus;
extern Dem_EventStatusType LastEventStatus_Dem_SetEventStatus;

/* ----------------------------------------------------------------------
 * 回数閾値故障注入（戻り値を持つ11関数のみ。Dem_SetEventStatus と
 * Dem_GetTranslationType は対象外、上記コメント参照）
 * ---------------------------------------------------------------------- */
/** `WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED`（既定）: 常に対応する
 *  `__real_Dem_Xxx()` へパススルー。それ以外の値を設定すると、対応する
 *  `CallCount_Dem_Xxx` がこの値に達した回から（以降ずっと）
 *  対応する `ForcedReturn_Dem_Xxx` を返す。 */
extern uint32 FailFromCallCount_Dem_GetEventUdsStatus;
extern uint32 FailFromCallCount_Dem_GetDTCOfEvent;
extern uint32 FailFromCallCount_Dem_GetFaultDetectionCounter;
extern uint32 FailFromCallCount_Dem_GetDTCStatusAvailabilityMask;
extern uint32 FailFromCallCount_Dem_DisableDTCSetting;
extern uint32 FailFromCallCount_Dem_EnableDTCSetting;
extern uint32 FailFromCallCount_Dem_ClearAllDTCs;
extern uint32 FailFromCallCount_Dem_ClearOneDtc;
extern uint32 FailFromCallCount_Dem_GetFreezeFrameOfEvent;
extern uint32 FailFromCallCount_Dem_GetEventIdOfDTC;
extern uint32 FailFromCallCount_Dem_GetOccurrenceCounterOfEvent;

/* ----------------------------------------------------------------------
 * 閾値到達後の強制戻り値（戻り値を持つ11関数のみ）
 * ---------------------------------------------------------------------- */
extern Std_ReturnType ForcedReturn_Dem_GetEventUdsStatus;              /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Dem_GetDTCOfEvent;                  /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Dem_GetFaultDetectionCounter;       /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Dem_GetDTCStatusAvailabilityMask;   /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Dem_DisableDTCSetting;              /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Dem_EnableDTCSetting;               /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Dem_ClearAllDTCs;                   /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Dem_ClearOneDtc;                    /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Dem_GetFreezeFrameOfEvent;          /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Dem_GetEventIdOfDTC;                /**< 既定 E_NOT_OK */
extern Std_ReturnType ForcedReturn_Dem_GetOccurrenceCounterOfEvent;    /**< 既定 E_NOT_OK */

/** すべての呼び出し回数・回数閾値・強制戻り値を初期状態へ戻す
 *  （Dem 自体の内部状態には触れない）。各テストケースの開始時（SetUp()）に
 *  1回呼ぶ。 */
void WrapDem_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_DEM_H */
