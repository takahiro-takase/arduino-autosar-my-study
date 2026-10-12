"""ハンドル ID のシンボリック名（AUTOSAR ECUC の symbolicNameValue）。

名前の規則は [TPS_ECUC_02108]: `<モジュールの略称>Conf_<コンテナ定義の名前>_<コンテナ(設定 json の name)>`。
値を提供する（宣言する）モジュールのヘッダ（本プロジェクトでは *_Cfg.h）に #define し、使う側のモジュールは
そのヘッダ経由で取り込む（[SWS_BSW_00200]）。

  CanIf: CanIfConf_CanIfTxPduCfg_<name>、CanIfConf_CanIfRxPduCfg_<name>   （値: テーブルの添字）
  PduR : PduRConf_PduRSrcPdu_<name>                                         （値: srcPduId）
  Com  : ComConf_ComIPdu_<name>、ComConf_ComSignal_<name>                   （値: 各テーブルの添字）
  SecOC: SecOCConf_SecOCRxSecuredPdu_<name>                                 （値: SecOCRxPduId = テーブルの添字）
  Os   : OsConf_OsTask_<function>                                           （値: タスク ID = タスク表の添字）
"""

import re

from .common import doc_comment

IDENT_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")

CANIF_TX = "CanIfConf_CanIfTxPduCfg_"
CANIF_RX = "CanIfConf_CanIfRxPduCfg_"
PDUR_SRC = "PduRConf_PduRSrcPdu_"
COM_IPDU = "ComConf_ComIPdu_"
COM_SIGNAL = "ComConf_ComSignal_"
SECOC_RX = "SecOCConf_SecOCRxSecuredPdu_"

# 使われないマクロに付ける MISRA C:2012 Rule 2.5（未使用マクロ）の抑止コメント（Can_Cfg.h などと同じ書き方）
MISRA_UNUSED_MACRO = "/* cppcheck-suppress misra-c2012-2.5 */"


def canif_tx(ctx, index, where):
    pdus = ctx.configs["CanIf"]["txPdus"]
    if not 0 <= index < len(pdus):
        raise ValueError("%s: CanIf の TX PDU の添字 %d が範囲外" % (where, index))
    return CANIF_TX + pdus[index]["name"]


def com_ipdu_index(ctx, direction, name, where):
    """Com の設定における I-PDU の添字（ID）。direction は "RX" か "TX"。"""
    key = "rxIpdus" if direction == "RX" else "txIpdus"
    for i, p in enumerate(ctx.configs["Com"][key]):
        if p["name"] == name:
            return i
    raise ValueError("%s: Com の %s I-PDU '%s' が存在しない" % (where, direction, name))


def com_ipdu_symbol(ctx, direction, name, where):
    """Com の I-PDU のシンボリック名。I-PDU が存在することも確認する。"""
    com_ipdu_index(ctx, direction, name, where)
    return COM_IPDU + name


def pdur_src_for_canif_tx(ctx, pdu, index):
    """CanIf の TX PDU が上位層（PduR）へ返す ID（upperLayerTxPduId）に対応する、PduR の送信元 PDU のシンボル。"""
    for p in ctx.configs["PduR"]["txPaths"]:
        if p["canIfTxPduId"] == index and p["srcPduId"] == pdu["upperLayerTxPduId"]:
            return PDUR_SRC + p["name"]
    raise ValueError("CanIf %s: upperLayerTxPduId=%d に対応する PduR の TX 経路がない" % (pdu["name"], pdu["upperLayerTxPduId"]))


def pdur_src_for_canif_rx(ctx, pdu):
    """CanIf の RX PDU が上位層（PduR）へ渡す ID（upperLayerRxPduId）に対応する、PduR の受信パスのシンボル。"""
    for p in ctx.configs["PduR"]["rxPaths"]:
        if p["srcPduId"] == pdu["upperLayerRxPduId"]:
            return PDUR_SRC + p["name"]
    raise ValueError("CanIf %s: upperLayerRxPduId=%d に対応する PduR の RX 経路がない" % (pdu["name"], pdu["upperLayerRxPduId"]))


# シンボリック名の接頭辞 → 定義しているヘッダ。C ソースにこの接頭辞の名前があれば、そのヘッダのインクルードが要る。
SYMBOL_HEADERS = {"CanIfConf_": "CanIf_Cfg.h", "PduRConf_": "PduR_Cfg.h", "ComConf_": "Com_Cfg.h", "SecOCConf_": "SecOC_Cfg.h", "OsConf_": "Os_Cfg.h"}


def include_warnings(text):
    """生成した C ソース（text）が使っているシンボリック名のヘッダが、インクルードされているかを確認する。"""
    return [
        "%s がインクルードされていない（%s のシンボリック名を使用）" % (header, prefix[:-5])
        for prefix, header in SYMBOL_HEADERS.items()
        if prefix in text and '#include "%s"' % header not in text
    ]


def check_unique(names, label):
    """シンボリック名の元になる name が、C の識別子として使え、重複しないことを確認する。"""
    seen = set()
    for n in names:
        if not IDENT_RE.match(n):
            raise ValueError("%s: name '%s' はシンボリック名（C の識別子）に使えない（英数字と _ だけ、先頭は英字）" % (label, n))
        if n in seen:
            raise ValueError("%s: シンボリック名の元になる name '%s' が重複している" % (label, n))
        seen.add(n)


def define_lines(groups):
    """#define の行を作る。groups は [[(名前, 値, 抑止コメントを付けるか[, 説明の行のリスト]), ...], ...]
    （グループの間に空行を入れる）。名前の幅と数値の桁数を全体でそろえ、値の列が縦にそろうようにする。
    説明の行（Doxygen コメントにする）がある定義は、前に空行を入れて区切る。"""
    entries = [e for g in groups for e in g]
    if not entries:
        return []
    name_w = max(len(e[0]) for e in entries)
    val_w = max(len(str(e[1])) for e in entries)
    out = []
    for g in groups:
        if not g:
            continue
        if out:
            out.append("")
        for k, (name, value, unused, *rest) in enumerate(g):
            doc = rest[0] if rest else None
            if doc:
                if k:
                    out.append("")
                out += doc_comment(doc)
            if unused:
                out.append(MISRA_UNUSED_MACRO)
            out.append("#define %s  %sU" % (name.ljust(name_w), str(value).rjust(val_w)))
    return out
