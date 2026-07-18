/**
 * \file    Nm_Cfg.h
 * \brief   ネットワークマネジメント プリコンパイル設定 (AUTOSAR SWS_CANNM 準拠)
 * \details Nm が送信する NM PDU のパラメータを定義する。
 *
 *          本プロジェクトでの位置づけ:
 *            実車の Nm（Network Management）は、各 ECU が周期的に
 *            「生存している」ことをバス上に示し、全 ECU が NM フレームの
 *            送信を止めたときにバススリープへ移行できる、という
 *            合意形成の仕組みを提供する。
 *            本プロジェクトは実際のバススリープを実装しないため、
 *            ComM が FULL_COM の間だけ NM フレームを送信することで、
 *            「通信が必要な間は生存を示し続ける」という Nm の役割だけを
 *            簡易的に再現する。
 *
 *          MeterStatus（E2E 保護対象）との違い:
 *            NM フレームはシグナル値を運ばず、安全関連のペイロードでもないため
 *            AUTOSAR でも E2E 保護の対象にしないのが一般的。本プロジェクトの
 *            NM フレームも E2E 保護を付与しない「普通の送信フレーム」とする。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef NM_CFG_H
#define NM_CFG_H

/** NM フレーム送信周期 [ms]
 *  MeterStatus (3000ms) より短い周期で、バス上の生存確認を高頻度に行う
 *  という NM 本来の性質を反映する。WdgM_TriggerHwWatchdog と同じ 1000ms とし、
 *  本プロジェクトにおける「中頻度の BSW ハウスキーピング」周期に合わせている。 */
#define NM_CYCLE_MS  1000UL

/** NM PDU のバイト長
 *  byte[0]=Control Bit Vector, byte[1]=Source Node Identifier */
#define NM_DLC  2U

/** 本 ECU（メータ ECU）の NM ノード識別子 */
#define NM_SOURCE_NODE_ID  0x01U

/**
 * CanIf に登録した本 NM フレームの TxPduId。
 * CanIf_PBCfg.c の CanIf_TxPduConfigData 配列インデックスと一致させること
 * （Nm は PduR/Com を経由せず CanIf_Transmit() を直接呼ぶため、
 * 実車の CanNm と同様に Com スタックとは独立して動作する）。
 */
#define NM_CANIF_TX_PDU_ID  2U

#endif /* NM_CFG_H */
