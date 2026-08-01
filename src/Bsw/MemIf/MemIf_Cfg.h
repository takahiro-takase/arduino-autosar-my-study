/**
 * \file    MemIf_Cfg.h
 * \brief   Memory Abstraction Interface プリコンパイル設定 (AUTOSAR SWS_MemIf 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef MEMIF_CFG_H
#define MEMIF_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * Memory Abstraction Interface (MemIf) に割り当てられた固定値 22 を使う。
 *
 * 開発エラーコード・ApiId は docs/4.3.1/AUTOSAR_SWS_MemoryAbstractionInterface.pdf
 * ([SWS_MemIf_00006] 7.1.1 Development Errors 表、8 章 Service ID[hex]) を
 * 実測して確認済み。
 *
 * [SWS_MemIf_00018]/[SWS_MemIf_00019]/[SWS_MemIf_00022] により、実 AUTOSAR は
 * 「メモリ抽象化モジュールが 1 個しか構成されていない場合、DeviceIndex は
 * 無視してよく、MemIf 自体は下位モジュール API へマッピングするだけのマクロ集合
 * として実装してよい（DeviceIndex の妥当性チェックも省略可）」と規定する。
 * 本プロジェクトは常にちょうど 1 個（Fee）しか構成しないため、
 * 厳密にはこの規定に従い MemIf_CheckDevice() のチェック自体を省略してよい。
 * それでもあえて残しているのは、CryIf_ProcessJob() の channelId 検証
 * （CryIf_Cfg.h 参照、単一 Crypto Driver Object のみでも検証だけは残す方針）
 * と同じ理由: 複数デバイス構成という一般形の存在を読み手に示すため。
 * ----------------------------------------------------------------------- */

/** AUTOSAR Memory Abstraction Interface の ModuleId（AUTOSAR_TR_BSWModuleList
 *  参照、固定値 22） */
#define MEMIF_MODULE_ID  22U

/** 開発エラーコード（[SWS_MemIf_00006] 7.1.1 表より） */
#define MEMIF_E_PARAM_DEVICE   0x01U  /**< Device が MEMIF_DEVICE_0 以外        */
#define MEMIF_E_PARAM_POINTER  0x02U  /**< NULL ポインタ                        */

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。実装している API は
 *  [SWS_MemIf_00038]〜[SWS_MemIf_00046] の Service ID[hex] と一致させた。
 *  MemIf_Init/MemIf_MainFunction は実 AUTOSAR の MemIf には存在しない
 *  （EcuM/Os が Fee_Init/Fee_MainFunction 等を直接呼ぶ設計。MemIf.c 冒頭の
 *  コメント参照）本プロジェクト独自の拡張のため、実仕様が 0x01〜0x09 を
 *  使い切った範囲の外側 (0x00, 0x0A) を割り当てる。 */
#define MEMIF_API_ID_READ              0x02U
#define MEMIF_API_ID_WRITE             0x03U
#define MEMIF_API_ID_CANCEL            0x04U
#define MEMIF_API_ID_GET_STATUS        0x05U
#define MEMIF_API_ID_GET_JOB_RESULT    0x06U
#define MEMIF_API_ID_GET_VERSION_INFO  0x08U
#define MEMIF_API_ID_INIT              0x00U  /**< AUTOSAR 非標準（MemIf.c 参照） */
#define MEMIF_API_ID_MAIN_FUNCTION     0x0AU  /**< AUTOSAR 非標準（MemIf.c 参照） */
#define MEMIF_API_ID_WRITE_IMMEDIATE   0x0BU  /**< AUTOSAR 非標準（Fee.h 参照） */

/** バージョン情報（Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define MEMIF_VENDOR_ID          0U
#define MEMIF_SW_MAJOR_VERSION   1U
#define MEMIF_SW_MINOR_VERSION   0U
#define MEMIF_SW_PATCH_VERSION   0U

#endif /* MEMIF_CFG_H */
