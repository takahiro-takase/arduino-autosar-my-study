# WdgIf

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

WdgM（上位）と Wdg（下位ドライバ）の間のディスパッチ層。実 AUTOSAR は Device
引数で複数の Wdg インスタンスへ振り分けるが、本プロジェクトは物理ウォッチドッグが
Wdg 1 個のみのため実質パススルー（MemIf → Fee と同じ簡略化）。実 AUTOSAR の
WdgIf に Init/MainFunction が存在しない（[SWS_WdgIf_00018] により、ドライバが
1個の構成では WdgM が Wdg_Init() を直接呼んでよいと規定されている）点は MemIf
と共通するが、WdgIf 自体には MemIf のような非標準の Init/MainFunction 拡張を
追加していない（プラットフォーム分岐を隠す必要がないため）。
