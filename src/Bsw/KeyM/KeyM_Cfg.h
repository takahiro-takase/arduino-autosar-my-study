/**
 * \file    KeyM_Cfg.h
 * \brief   Key Manager プリコンパイル設定 (AUTOSAR SWS_KeyManager 準拠)
 * \details KeyM モジュールの DET 定数・鍵名テーブル定数を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する
 *          ファイルに相当する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.4.0 仕様（docs/4.4.0/AUTOSAR_SWS_KeyManager.pdf）
 *          を参考にした学習用実装です。AUTOSAR 認証済み実装ではなく、製品への
 *          適用は想定していません。他モジュールが Release 4.3.1 を参照する中、
 *          KeyM のみ 4.4.0 を参照するのは、KeyM が Release 4.4.0 で新設された
 *          モジュールであり 4.3.1 版の仕様書が存在しないため。
 */
#ifndef KEYM_CFG_H
#define KEYM_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * [SWS_KeyM_00144] により、GetVersionInfo を含む全 API が未初期化チェック
 * の対象になる（Csm/CryIf と同じ「GetVersionInfo が UNINIT 例外にならない」
 * 挙動。他の多くのモジュールとは異なる点に注意）。
 * ----------------------------------------------------------------------- */

/**
 * AUTOSAR Key Manager の ModuleId。
 *
 * 実測不能: 本プロジェクトが基準とする AUTOSAR_TR_BSWModuleList.pdf は
 * Release 4.3.1 版であり、Release 4.4.0 で新設された KeyM は掲載されていない
 * （pdftotext 実測で "key" を含む行が1件もヒットしないことを確認済み）。
 * 他モジュールとの値衝突を避けつつ「未検証」であることを明示するため、
 * 他モジュールが使わない範囲の値を暫定的に割り当てる。正確な値が必要になれば
 * Release 4.4.0 以降の AUTOSAR_TR_BSWModuleList.pdf を実測すること。
 */
#define KEYM_MODULE_ID  116U  /* 暫定値。公式 ModuleId 未確認（本ファイル冒頭コメント参照） */

/** 開発エラーコード（docs/4.4.0/AUTOSAR_SWS_KeyManager.pdf を実測して確認済み） */
#define KEYM_E_PARAM_POINTER  0x01U  /* [SWS_KeyM_00146]: NULL ポインタチェック */
#define KEYM_E_SMALL_BUFFER   0x02U  /* [SWS_KeyM_00145]: 出力バッファ不足 */
#define KEYM_E_UNINIT         0x03U  /* [SWS_KeyM_00144]: 未初期化時の API 呼び出し */
#define KEYM_E_INIT_FAILED    0x04U

/** ApiId（値は docs/4.4.0/AUTOSAR_SWS_KeyManager.pdf の「Service ID[hex]」記載を
 *  実測して確認済み。KeyM_Init が 0x01 から始まる点に注意（他の多くのモジュールは
 *  Init=0x00）。KeyM_Prepare(0x05)/KeyM_Verify(0x08) は本プロジェクトでは
 *  未実装のため定義しない。 */
#define KEYM_API_ID_INIT              0x01U
#define KEYM_API_ID_DEINIT            0x02U
#define KEYM_API_ID_GET_VERSION_INFO  0x03U
#define KEYM_API_ID_START             0x04U
#define KEYM_API_ID_UPDATE            0x06U
#define KEYM_API_ID_FINALIZE          0x07U

/** バージョン情報（Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define KEYM_VENDOR_ID          0U
#define KEYM_SW_MAJOR_VERSION   1U
#define KEYM_SW_MINOR_VERSION   0U
#define KEYM_SW_PATCH_VERSION   0U

/* -----------------------------------------------------------------------
 * 鍵名テーブル（KeyM_Update() の KeyNamePtr で検索する）
 *
 * [SWS_KeyM_00013] の「keyName が指定されればテーブルを検索し、見つかれば
 * CryptoKeyId を得る」に対応。実 AUTOSAR は任意長の鍵名文字列を使えるが、
 * 本プロジェクトは KeyM 仕様書 9 章のシーケンス図例（"1"/"2"/"3" のような
 * 1 バイト ASCII 数字の鍵名）に合わせ、単純な1バイト鍵名にする。
 * ----------------------------------------------------------------------- */

/** RX Secured I-PDU「ImmobilizerCmd」の鍵名 */
#define KEYM_CRYPTO_KEY_NAME_IMMOBILIZER_CMD    '1'

/** TX Secured I-PDU「E2EHealthStatus」の鍵名 */
#define KEYM_CRYPTO_KEY_NAME_E2E_HEALTH_STATUS  '2'

/** 鍵名テーブルの総数 */
#define KEYM_CRYPTO_KEY_COUNT  2U

#endif /* KEYM_CFG_H */
