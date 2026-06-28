"""
UDS (ISO 14229-1) over CAN (ISO 15765-2 / CanTp) の送受信ロジック。

GUI には依存しない。python-can の Bus オブジェクトを受け取って
SF 送信・ISO-TP 受信（複数フレーム再結合 + FC 自動送信）・
NRC/DTC/DID の名前解決を提供する。

本プロジェクトの ECU (Dcm_Cbk.c) は要求ペイロードが常に 7 バイト以内のため
SF 送信のみで送れる。応答側（SID 0x19 の複数 DTC 等）は複数フレームになる
ことがあるため、受信側のみ ISO-TP の FF/CF 再結合と FC 送信を実装する。
"""
from __future__ import annotations

import os
import platform
import time
from dataclasses import dataclass
from typing import Optional


def _register_libusb_dll_dir() -> None:
    """Windows + gs_usb (PyUSB) 用の対策。

    `pip install libusb` は libusb-1.0.dll を同梱するが、PyUSB
    (usb.libloader.locate_library) は ctypes.util.find_library() で探索する。
    この関数は Windows では os.add_dll_directory() の登録を見ず、
    os.environ["PATH"] を文字列として走査するだけの実装のため、
    DLL 自体のディレクトリを PATH に追加しないと見つからず
    "No backend available" (NoBackendError) になる。
    """
    if os.name != "nt":
        return
    try:
        import libusb
    except ImportError:
        return

    arch = {"AMD64": "x86_64", "x86": "x86", "ARM64": "arm64"}.get(
        platform.machine(), "x86_64"
    )
    dll_dir = os.path.join(os.path.dirname(libusb.__file__), "_platform", "windows", arch)
    if not os.path.isdir(dll_dir):
        return

    os.environ["PATH"] = dll_dir + os.pathsep + os.environ.get("PATH", "")
    os.add_dll_directory(dll_dir)


_register_libusb_dll_dir()

import can  # noqa: E402 - DLL 検索パス登録を import より先に行う必要がある

REQUEST_ID = 0x7E0
RESPONSE_ID = 0x7E8

NEGATIVE_RESPONSE_SID = 0x7F

SECURITY_KEY_MASK = 0xA55A  # Dcm_Cfg.h の DCM_SECURITY_KEY_MASK と一致させること

NRC_NAMES = {
    0x10: "generalReject",
    0x11: "serviceNotSupported",
    0x12: "subFunctionNotSupported",
    0x13: "incorrectMessageLengthOrInvalidFormat",
    0x22: "conditionsNotCorrect",
    0x24: "requestSequenceError",
    0x31: "requestOutOfRange",
    0x33: "securityAccessDenied",
    0x35: "invalidKey",
    0x36: "exceedNumberOfAttempts",
    0x37: "requiredTimeDelayNotExpired",
    0x7F: "serviceNotSupportedInActiveSession",
}

DTC_NAMES = {
    0x000101: "ENGINE_OVERHEAT",
    0x000102: "ENGINE_STALL",
    0x000103: "ENGINE_SPEED_NO_FLAG",
    0x000104: "STARTING_TIMEOUT",
    0x000105: "COMM_TIMEOUT",
    0x000106: "BUTTON_STUCK",
    0x000107: "ADC_VOLT_LOW",
    0x000108: "CAN_BUSOFF",
}

DID_NAMES = {
    0x0101: "EngineSpeed (rpm)",
    0x0102: "CoolantTemp (degC)",
    0x0103: "EngineState",
}

ENGINE_STATE_NAMES = {0: "OFF", 1: "STARTING", 2: "RUNNING", 3: "FAULT"}


class UdsTimeoutError(Exception):
    pass


@dataclass
class UdsResponse:
    raw: bytes

    @property
    def is_negative(self) -> bool:
        return len(self.raw) >= 1 and self.raw[0] == NEGATIVE_RESPONSE_SID

    @property
    def nrc(self) -> Optional[int]:
        return self.raw[2] if self.is_negative and len(self.raw) >= 3 else None

    def describe(self) -> str:
        if not self.raw:
            return "(empty response)"
        if self.is_negative:
            req_sid = self.raw[1] if len(self.raw) >= 2 else 0
            nrc = self.nrc
            name = NRC_NAMES.get(nrc, "unknown NRC")
            return f"NEG sid=0x{req_sid:02X} NRC=0x{nrc:02X} ({name})"
        return f"POS " + " ".join(f"{b:02X}" for b in self.raw)


def create_bus(interface: str, channel, bitrate: int) -> can.BusABC:
    """python-can の Bus を生成する。アダプタ種別ごとに channel の型が異なる
    （gs_usb は整数 index、slcan はシリアルポート文字列等）ため、呼び出し側
    (config.json) でそのまま渡せる値をそのまま使う。"""
    bus = can.Bus(interface=interface, channel=channel, bitrate=bitrate)
    if interface == "gs_usb":
        _patch_gs_usb_shutdown(bus)
    return bus


def _patch_gs_usb_shutdown(bus) -> None:
    """python-can の GsUsbBus.shutdown() は内部で GsUsb.scan() による
    デバイス再列挙を行うが、Windows + libusb の組み合わせで
    access violation (OSError) を起こすことを確認した（再現性は非決定的）。
    実害のない後始末（停止コマンド送信 + シャットダウン済みフラグ設定）のみ
    行う安全な版にインスタンス単位で置き換える。BusABC.__del__() も
    self.shutdown() 経由でこの置き換え後の版を呼ぶため、明示的に
    shutdown() を呼び忘れた場合の自動後始末にも安全に効く。"""

    def _safe_shutdown(self=bus):
        if self._is_shutdown:
            return
        try:
            self.gs_usb.stop()
        except Exception:  # noqa: BLE001 - 後始末中の失敗は無視してよい
            pass
        try:
            import usb.util

            usb.util.dispose_resources(self.gs_usb.gs_usb)
        except Exception:  # noqa: BLE001 - 解放できなくても続行する
            pass
        self._is_shutdown = True

    bus.shutdown = _safe_shutdown


def send_raw(bus: can.BusABC, payload: bytes, request_id: int = REQUEST_ID) -> None:
    """SF 専用送信。payload は byte0=PCI（長さ）から始まる完成済みフレーム
    （例: [0x02, 0x10, 0x01]）を渡すこと。8 バイトまで 0x00 でパディングする。"""
    data = bytes(payload) + b"\x00" * (8 - len(payload))
    msg = can.Message(arbitration_id=request_id, data=data, is_extended_id=False)
    bus.send(msg)


def _send_flow_control(bus: can.BusABC, request_id: int) -> None:
    """BS=0 (制限なし) / STmin=0 (間隔なし) の FC を送信する。
    Cangaroo で手動送信していた `30 00 00 00 00 00 00 00` と同じ内容。"""
    fc = bytes([0x30, 0x00, 0x00, 0, 0, 0, 0, 0])
    bus.send(can.Message(arbitration_id=request_id, data=fc, is_extended_id=False))


def send_multiframe_request(
    bus: can.BusABC,
    uds_payload: bytes,
    request_id: int = REQUEST_ID,
    response_id: int = RESPONSE_ID,
    timeout: float = 2.0,
) -> None:
    """ISO-TP の FF+CF で UDS 要求を送信する (送信側の組立)。

    send_raw() は SF 専用 (本プロジェクトの既存サービスは全て要求 7 バイト以内
    のため十分だった) だが、CanTp_RxIndication() の FF/CF 受信パスは一度も
    実機を通っていなかった。0x2E WriteDataByIdentifier (DID 0x0104) のように
    要求が 7 バイトを超えるサービスを送るために、FF 送信 → ECU からの FC
    (CTS) 受信待ち → CF 送信、という ISO-TP 送信側プロトコルをここで実装する。

    uds_payload は PCI バイトを含まない生 UDS ペイロード（例: [0x2E, 0x01, 0x04,
    data...]）。8 バイト以下なら send_raw() と同様 SF として送信する。
    """
    msg_len = len(uds_payload)
    if msg_len <= 7:
        send_raw(bus, bytes([msg_len]) + uds_payload, request_id)
        return

    ff_data = bytes([0x10 | ((msg_len >> 8) & 0x0F), msg_len & 0xFF]) + uds_payload[:6]
    bus.send(can.Message(arbitration_id=request_id, data=ff_data, is_extended_id=False))

    deadline = time.monotonic() + timeout
    st_min = 0
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise UdsTimeoutError(f"FC 待ちタイムアウト ({timeout}s)")
        msg = bus.recv(timeout=remaining)
        if msg is None:
            raise UdsTimeoutError(f"FC 待ちタイムアウト ({timeout}s)")
        if msg.arbitration_id != response_id or not msg.data or (msg.data[0] >> 4) != 0x3:
            continue

        fs = msg.data[0] & 0x0F
        if fs == 0x0:  # CTS
            st_min = msg.data[2]
            break
        if fs == 0x1:  # WAIT
            deadline = time.monotonic() + timeout
            continue
        raise RuntimeError("FC OVFLW: ECU 側の受信バッファを超過した")

    pos = 6
    sn = 1
    while pos < msg_len:
        chunk = uds_payload[pos : pos + 7]
        cf_data = bytes([0x20 | (sn & 0x0F)]) + chunk + b"\x00" * (7 - len(chunk))
        bus.send(can.Message(arbitration_id=request_id, data=cf_data, is_extended_id=False))
        pos += len(chunk)
        sn = (sn + 1) & 0x0F
        if st_min > 0:
            time.sleep(st_min / 1000.0)


def receive_uds_response(
    bus: can.BusABC,
    timeout: float = 2.0,
    response_id: int = RESPONSE_ID,
    request_id: int = REQUEST_ID,
) -> UdsResponse:
    """ISO-TP の SF/FF/CF を再結合して UDS ペイロードを返す。
    マルチフレーム受信時は FF を受け取った直後に FC を自動送信する
    （本ツール最大の目的: Cangaroo での FC 手動送信を不要にする）。"""
    deadline = time.monotonic() + timeout
    payload = bytearray()
    expected_len = None

    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise UdsTimeoutError(f"応答タイムアウト ({timeout}s)")

        msg = bus.recv(timeout=remaining)
        if msg is None:
            raise UdsTimeoutError(f"応答タイムアウト ({timeout}s)")
        if msg.arbitration_id != response_id or not msg.data:
            continue

        pci = msg.data[0]
        frame_type = pci >> 4

        if frame_type == 0x0:  # SF
            length = pci & 0x0F
            return UdsResponse(bytes(msg.data[1 : 1 + length]))

        if frame_type == 0x1:  # FF
            expected_len = ((pci & 0x0F) << 8) | msg.data[1]
            payload = bytearray(msg.data[2:8])
            _send_flow_control(bus, request_id)
            continue

        if frame_type == 0x2:  # CF
            if expected_len is None:
                continue  # FF を見ていないのに CF が来た場合は無視
            payload.extend(msg.data[1:8])
            if len(payload) >= expected_len:
                return UdsResponse(bytes(payload[:expected_len]))
            continue

        # FC (0x3) 等、応答 ID 上では本来来ないフレームは無視


def decode_did_value(did: int, data: bytes) -> str:
    name = DID_NAMES.get(did, f"DID 0x{did:04X}")
    if did == 0x0101 and len(data) >= 2:
        return f"{name} = {(data[0] << 8) | data[1]}"
    if did == 0x0102 and len(data) >= 1:
        return f"{name} = {data[0]}"
    if did == 0x0103 and len(data) >= 1:
        return f"{name} = {data[0]} ({ENGINE_STATE_NAMES.get(data[0], '?')})"
    return f"{name} = " + " ".join(f"{b:02X}" for b in data)


def dtc_name(dtc: int) -> str:
    return DTC_NAMES.get(dtc, f"0x{dtc:06X}")


def security_access_auto(
    bus: can.BusABC,
    key_mask: int = SECURITY_KEY_MASK,
    timeout: float = 2.0,
    request_id: int = REQUEST_ID,
    response_id: int = RESPONSE_ID,
) -> str:
    """SID 0x27 requestSeed → key 計算 (seed XOR key_mask) → sendKey を
    1 アクションで実行する。Dcm_ComputeSecurityKey() と同一の計算式。"""
    send_raw(bus, bytes([0x02, 0x27, 0x01]), request_id)
    resp = receive_uds_response(bus, timeout, response_id, request_id)
    if resp.is_negative:
        return f"requestSeed 拒否: {resp.describe()}"
    if len(resp.raw) < 4 or resp.raw[0] != 0x67:
        return f"requestSeed 予期しない応答: {resp.describe()}"

    seed = (resp.raw[2] << 8) | resp.raw[3]
    if seed == 0:
        return "既にアンロック済み (allZeroSeed)"

    key = seed ^ key_mask
    send_raw(
        bus,
        bytes([0x04, 0x27, 0x02, (key >> 8) & 0xFF, key & 0xFF]),
        request_id,
    )
    resp2 = receive_uds_response(bus, timeout, response_id, request_id)
    if resp2.is_negative:
        return f"seed=0x{seed:04X} key=0x{key:04X} -> sendKey 拒否: {resp2.describe()}"
    if len(resp2.raw) >= 1 and resp2.raw[0] == 0x67:
        return f"seed=0x{seed:04X} key=0x{key:04X} -> アンロック成功"
    return f"seed=0x{seed:04X} key=0x{key:04X} -> 予期しない応答: {resp2.describe()}"
