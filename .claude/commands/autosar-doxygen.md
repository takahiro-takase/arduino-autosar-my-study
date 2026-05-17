---
description: Add AUTOSAR-compliant Doxygen comments to C functions in the current file or selection
---

対象ファイルまたは選択中の関数に、このプロジェクトの AUTOSAR 規約に沿った Doxygen コメントを追加してください。

## 追加するタグ（必須）

```
/**
 * \brief   1行の概要（動詞始まり、英語）
 *
 * \details 詳細説明。参照する AUTOSAR SWS 番号があれば記載する。
 *          例: (AUTOSAR SWS_Can_00246)
 *
 * \param[in]     paramName  説明（入力パラメータ）
 * \param[out]    paramName  説明（出力パラメータ）
 * \param[in,out] paramName  説明（入出力パラメータ）
 *
 * \return  戻り値の説明（void の場合は省略）
 * \retval  VALUE  その値の意味（複数の戻り値がある場合）
 *
 * \pre        呼び出し前提条件
 * \note       AUTOSAR 仕様との差異や実装上の注意
 *
 * \ServiceID      {0xXX}
 * \Reentrancy     {Non Reentrant | Reentrant}
 * \Synchronicity  {Synchronous | Asynchronous}
 */
```

## このプロジェクトの AUTOSAR ServiceID 規則

| 関数名パターン | ServiceID |
|---------------|-----------|
| `*_Init`              | 0x00 |
| `*_GetVersionInfo`    | 0x01 |
| `*_SetControllerMode` | 0x03 |
| `*_Write`             | 0x06 |
| `*_MainFunction_Read` | 0x08 |
| `*_RxIndication`      | 0x10 |
| `*_TxConfirmation`    | 0x11 |
| `*_Transmit`          | 0x49 |
| ベンダー定義（Isr等） | 0xF0〜 |

## ファイルヘッダ

ファイル先頭に以下を追加する：

```c
/**
 * \file    FileName.c
 * \brief   モジュール名 (AUTOSAR SWS_XXX inspired)
 * \details 実装概要。AUTOSAR との差異があれば記載。
 */
```

## 注意事項

- 既存のインラインコメント（`/* ... */` や `// ...`）はコメントが重複する場合は削除する
- `void` 関数に `\return` は不要
- AUTOSAR 標準にない関数（Arduino 固有等）は `\note` に明記し ServiceID は 0xF0 番台を使う
- 英語で記述する（コード内コメントの言語統一のため）
- Can.c の既存コメントスタイルを参考にする

## 手順

1. 対象ファイルを読み込む（引数 $ARGUMENTS があればそのファイル、なければ現在の選択や会話から判断）
2. 各関数定義を特定する
3. 上記規則に従い Doxygen コメントを追加する
4. ビルドして問題ないことを確認する
