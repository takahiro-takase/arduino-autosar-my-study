"""暗号処理の連鎖（Csm / Crypto / KeyM）の生成。

鍵（keys）を中心に、次の 3 つのモジュールの設定を 1 つの json から生成する。
  - Crypto: 鍵テーブル（鍵の ID マクロ、鍵のバイト列）
  - Csm   : ジョブテーブル（ジョブ ID マクロ、プリミティブ、使う鍵）
  - KeyM  : 鍵名テーブル（鍵名の 1 バイト文字、対応する鍵）
SecOC の `csmJobId` が、ここのジョブを指しているかは checks.py で検査する。
"""

from .common import block_comment, doc_comment, struct_fields

KEY_SIZE = 16


def validate(cfg):
    errors = []
    for group, label in (("keys", "keys"), ("jobs", "jobs")):
        names = [p["name"] for p in cfg[group]]
        macros = [p["macro"] for p in cfg[group]]
        if len(set(names)) != len(names):
            errors.append("%s: name が重複している" % label)
        if len(set(macros)) != len(macros):
            errors.append("%s: macro が重複している" % label)
    key_names = {k["name"] for k in cfg["keys"]}
    for j in cfg["jobs"]:
        if j["key"] not in key_names:
            errors.append("jobs %s: key '%s' が keys に存在しない" % (j["name"], j["key"]))
    for k in cfg["keys"]:
        if len(k["hex"]) != KEY_SIZE * 2:
            errors.append("keys %s: hex は %d 桁（%d byte）が必要" % (k["name"], KEY_SIZE * 2, KEY_SIZE))
    km = [k["keyM"] for k in cfg["keys"] if "keyM" in k]
    if len({m["name"] for m in km}) != len(km):
        errors.append("keys: keyM.name が重複している")
    if len({m["macro"] for m in km}) != len(km):
        errors.append("keys: keyM.macro が重複している")
    return errors


# ----------------------------------------------------------------------
# Crypto
# ----------------------------------------------------------------------
def render_crypto_ids(cfg, ctx):
    out = []
    for i, k in enumerate(cfg["keys"]):
        out += doc_comment(k["doc"])
        out.append("#define %s  %dU" % (k["macro"], i))
        out.append("")
    out += block_comment(cfg["keysTrailingNote"])
    out += doc_comment(cfg["keyCountDoc"])
    out.append("#define CRYPTO_KEY_COUNT  %dU" % len(cfg["keys"]))
    return "\n".join(out)


def render_crypto_table(cfg, ctx):
    out = ["const uint8 Crypto_KeyTable[CRYPTO_KEY_COUNT][CRYPTO_AES128_KEY_SIZE] = {"]
    n = len(cfg["keys"])
    for i, k in enumerate(cfg["keys"]):
        data = bytes.fromhex(k["hex"])
        cells = ["0x%02XU" % b for b in data]
        out += block_comment(k["note"], 4)
        out.append("    {")
        out.append("        " + ",".join(cells[:8]) + ",")
        out.append("        " + ",".join(cells[8:]))
        out.append("    }" + ("," if i < n - 1 else ""))
    out += block_comment(cfg["keyTablePostNote"], 4)
    out.append("};")
    return "\n".join(out)


# ----------------------------------------------------------------------
# Csm
# ----------------------------------------------------------------------
def render_csm_ids(cfg, ctx):
    out = []
    for i, j in enumerate(cfg["jobs"]):
        out += doc_comment(j["doc"])
        out.append("#define %s     %dU" % (j["macro"], i))
        out.append("")
    out += block_comment(cfg["jobsTrailingNote"])
    out += doc_comment(cfg["jobCountDoc"])
    out.append("#define CSM_JOB_COUNT  %dU" % len(cfg["jobs"]))
    return "\n".join(out)


def key_macro(cfg, name):
    return next(k["macro"] for k in cfg["keys"] if k["name"] == name)


def render_csm_table(cfg, ctx):
    out = ["const Csm_JobConfigType Csm_JobConfigData[CSM_JOB_COUNT] = {"]
    n = len(cfg["jobs"])
    for i, j in enumerate(cfg["jobs"]):
        out.append("    {")
        out += struct_fields(
            [
                ("JobId", j["macro"], None),
                ("Service", "CRYPTO_" + j["service"], None),
                ("CryptoKeyId", key_macro(cfg, j["key"]), None),
            ],
            8,
        )
        out.append("    }" + ("," if i < n - 1 else ""))
    out.append("};")
    return "\n".join(out)


# ----------------------------------------------------------------------
# KeyM
# ----------------------------------------------------------------------
def render_keym_ids(cfg, ctx):
    out = []
    bound = [k for k in cfg["keys"] if "keyM" in k]
    for k in bound:
        out += doc_comment(k["keyM"]["doc"])
        out.append("#define %s    '%s'" % (k["keyM"]["macro"], k["keyM"]["name"]))
        out.append("")
    out += block_comment(cfg["keyMTrailingNote"])
    out += doc_comment(cfg["keyMCountDoc"])
    out.append("#define KEYM_CRYPTO_KEY_COUNT  %dU" % len(bound))
    return "\n".join(out)


def render_keym_table(cfg, ctx):
    out = ["const KeyM_CryptoKeyConfigType KeyM_CryptoKeyConfigData[KEYM_CRYPTO_KEY_COUNT] = {"]
    bound = [k for k in cfg["keys"] if "keyM" in k]
    for i, k in enumerate(bound):
        out.append("    {")
        out += struct_fields([("KeyName", k["keyM"]["macro"], None), ("CsmKeyTargetRef", k["macro"], None)], 8)
        out.append("    }" + ("," if i < len(bound) - 1 else ""))
    out += block_comment(cfg["keyMTablePostNote"], 4)
    out.append("};")
    return "\n".join(out)


SPEC = {
    "config": "config/data/CryptoStack.json",
    "schema": "config/schema/CryptoStack.schema.json",
    "validate": validate,
    "targets": [
        ("src/Bsw/Crypto/Crypto_Cfg.h", "crypto-key-ids", render_crypto_ids, None),
        ("src/Bsw/Crypto/Crypto_PBCfg.c", "crypto-key-table", render_crypto_table, None),
        ("src/Bsw/Csm/Csm_Cfg.h", "csm-job-ids", render_csm_ids, None),
        ("src/Bsw/Csm/Csm_PBCfg.c", "csm-job-table", render_csm_table, None),
        ("src/Bsw/KeyM/KeyM_Cfg.h", "keym-key-names", render_keym_ids, None),
        ("src/Bsw/KeyM/KeyM_PBCfg.c", "keym-key-table", render_keym_table, None),
    ],
}
