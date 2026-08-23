/**
 * \file    CanTp_fake.h
 * \brief   Dcm_Cbk.c の応答送信先（CanTp_Transmit）をキャプチャするテスト用フェイク。
 * \details 本プロジェクトの CanTp はトランスポート層のみを担い、UDS の中身には
 *          関知しないため、実体をリンクせずこのフェイクで置き換える
 *          （platformio.ini [env:native_dcm] のコメント参照）。
 *          Dcm_Transmit() が渡した SduDataPtr/SduLength をそのままコピーして
 *          保持するので、テストからは「DCM がどんな UDS 応答を送ろうとしたか」
 *          を直接検証できる。
 */
#ifndef CANTP_FAKE_H
#define CANTP_FAKE_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CANTP_FAKE_TX_BUF_SIZE 64U

extern uint8  FakeCanTp_TxBuf[CANTP_FAKE_TX_BUF_SIZE];
extern uint8  FakeCanTp_TxLength;
extern uint32 FakeCanTp_TransmitCount;

/** 各テストケースの開始時に呼び、直近の送信記録をクリアする。 */
void FakeCanTp_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* CANTP_FAKE_H */
