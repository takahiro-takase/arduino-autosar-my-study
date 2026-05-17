---
description: Scaffold a new BSW module under src/Bsw/ following project AUTOSAR conventions
---

引数 $ARGUMENTS をモジュール名として、このプロジェクトの規約に沿った新しい BSW モジュールを生成してください。

例: `/autosar-bsw-module CanTp` → `src/Bsw/CanTp/CanTp.h` と `src/Bsw/CanTp/CanTp.c` を生成

## 生成するファイル

### 1. `src/Bsw/<ModuleName>/<ModuleName>.h`

```c
/**
 * \file    <ModuleName>.h
 * \brief   <ModuleName> module interface (AUTOSAR SWS_<ModuleName> inspired)
 */

#ifndef <MODULENAME>_H
#define <MODULENAME>_H

#include "Std_Types.h"
#include "Platform_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 型定義をここに追加 */

/* 関数宣言をここに追加 */
void           <ModuleName>_Init(const <ModuleName>_ConfigType* Config);

#ifdef __cplusplus
}
#endif

#endif /* <MODULENAME>_H */
```

### 2. `src/Bsw/<ModuleName>/<ModuleName>.c`

```c
/**
 * \file    <ModuleName>.c
 * \brief   <ModuleName> implementation (AUTOSAR SWS_<ModuleName> inspired)
 * \details ...
 */

#include "<ModuleName>.h"
#include "Det.h"

/**
 * \brief   Initializes the <ModuleName> module.
 *
 * \param[in]  Config  Pointer to configuration structure. Must not be NULL.
 *
 * \pre        ...
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void <ModuleName>_Init(const <ModuleName>_ConfigType* Config)
{
    if (Config == NULL)
    {
        Det_LogP(PSTR("[<ModuleName>_Init] E: Config NULL"));
        return;
    }

    Det_LogP(PSTR("[<ModuleName>_Init] OK"));
}
```

## 規約（必ず守ること）

### 言語
- `.h` / `.c` ファイルは **C 言語**（C99）で記述する
- C++ クラスを使う場合のみ `.cpp` とし、理由をファイルヘッダに明記する

### インクルード
- `#include <Arduino.h>` は **禁止**（`.c` ファイルでは使えない）
- `Serial.print()` は **禁止**。デバッグ出力は必ず `Det.h` の API を使う
  ```c
  Det_LogP(PSTR("[ModuleName_Func] message"));
  Det_PrintDec(value);
  Det_PrintHex(value);
  ```
- `millis()` が必要な場合は `extern unsigned long millis(void);` と宣言する
- `NULL` は `<stddef.h>` または `Platform_Types.h` 経由で使う

### ヘッダガード
- `extern "C"` ガードを必ず付ける（C++ からのインクルードに対応）
- インクルードガードのマクロ名は `<MODULENAME>_H`

### 命名規則
- 関数: `<ModuleName>_<FunctionName>` （例: `CanTp_Init`）
- 型:   `<ModuleName>_<TypeName>Type` （例: `CanTp_StateType`）
- マクロ: `<MODULENAME>_<NAME>` （例: `CANTP_MAX_DLC`）
- static 変数: `s_` プレフィックス（例: `static uint8 s_state = 0U;`）

### メモリ
- `malloc` / `new` は **禁止**（ヒープ不使用、AUTOSAR CP 準拠）
- `nullptr` は **禁止**（C では `NULL` を使う）

### platformio.ini
- 生成後、`build_flags` に `-I src/Bsw/<ModuleName>` を追加するよう案内する

## 手順

1. モジュール名が未指定の場合はユーザーに確認する
2. `src/Bsw/<ModuleName>/` ディレクトリを作成する
3. 上記テンプレートに基づき `.h` と `.c` を生成する
4. `platformio.ini` の `build_flags` に include パスを追加する
5. `pio run` でビルドが通ることを確認する
6. 追加した内容をユーザーに説明する
