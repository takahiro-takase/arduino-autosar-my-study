/**
 * \file    Fake_CanTp.h
 * \brief   Dcm_Cbk.c の応答送信先（CanTp_Transmit）をキャプチャするテスト用フェイク。
 * \details 本プロジェクトの CanTp はトランスポート層のみを担い、UDS の中身には
 *          関知しないため、実体をリンクせずこのフェイクで置き換える
 *          （platformio.ini [env:native_dcm] のコメント参照）。
 *          Dcm_Transmit() が渡した SduDataPtr/SduLength をそのままコピーして
 *          保持するので、テストからは「DCM がどんな UDS 応答を送ろうとしたか」
 *          を直接検証できる。
 */
#ifndef FAKE_CANTP_H
#define FAKE_CANTP_H

#include "Std_Types.h"
#include "CanTp_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 実体の CanTp.c と同じ上限を使う（独自の固定値だと native テストが
 *  実体側のバッファ超過バグを検知できない。経緯は CanTp_Cfg.h の
 *  CANTP_TX_BUFFER_SIZE コメント参照）。 */
#define CANTP_FAKE_TX_BUF_SIZE CANTP_TX_BUFFER_SIZE

extern uint8  FakeCanTp_TxBuf[CANTP_FAKE_TX_BUF_SIZE];
extern uint8  FakeCanTp_TxLength;
extern uint32 FakeCanTp_TransmitCount;

/** `CanTp_IsTxBusy()` の戻り値を制御する。デフォルト FALSE（アイドル）。
 *  Dcm_ComIndication() のビジー時無視分岐（[SWS_Dcm_00557]）を検証する
 *  テストが TRUE に設定する。`FakeCanTp_Reset()` で FALSE へ戻る。 */
extern boolean FakeCanTp_Busy;

/** 各テストケースの開始時に呼び、直近の送信記録をクリアする。 */
void FakeCanTp_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* FAKE_CANTP_H */
