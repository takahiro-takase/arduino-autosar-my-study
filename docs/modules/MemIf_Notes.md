# MemIf

> [README](../../README.md) の「[モジュール一覧](../../README.md#module-list)」節から分離。

NvM（上位）と Fee（下位ドライバ）の間のディスパッチ層。実 AUTOSAR は Device
引数で複数の Fee/Ea インスタンスへ振り分けるが、本プロジェクトは下位ドライバが
Fee 1 個のみのため実質パススルー（CryIf → Crypto の関係と同様）。
`MemIf_Init`/`MemIf_MainFunction` は実 AUTOSAR の SWS_MemIf には存在しない
（[SWS_MemIf_00019] により、ドライバが1個の構成では EcuM/Os が
Fee_Init/Fee_MainFunction を直接呼んでよいと規定されている）本プロジェクト
独自の拡張で、プラットフォーム分岐をこの層に閉じ込めるために追加した。
