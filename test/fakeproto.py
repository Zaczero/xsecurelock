#!/usr/bin/env python3

import os
import sys
import time


def write_packet(packet_type: str, message: str) -> None:
    payload = message.encode("utf-8")
    sys.stdout.buffer.write(f"{packet_type} {len(payload)}\n".encode("ascii"))
    sys.stdout.buffer.write(payload)
    sys.stdout.buffer.write(b"\n")
    sys.stdout.buffer.flush()


def read_packet() -> tuple[str, str]:
    header = sys.stdin.buffer.readline()
    if not header:
        return ("", "")
    packet_type, length = header.rstrip(b"\n").split(b" ", 1)
    payload = sys.stdin.buffer.read(int(length))
    newline = sys.stdin.buffer.read(1)
    if newline != b"\n":
        raise RuntimeError("Malformed authproto packet")
    return (packet_type.decode("ascii"), payload.decode("utf-8"))


def main() -> int:
    mode = os.environ.get("XSECURELOCK_FAKEPROTO_MODE", "info-block")
    message = os.environ.get("XSECURELOCK_FAKEPROTO_MESSAGE", "Please touch the device")
    prompt = os.environ.get("XSECURELOCK_FAKEPROTO_PROMPT", "Password:")
    expected = os.environ.get("XSECURELOCK_FAKEPROTO_EXPECTED", "hunter2")
    delay = float(os.environ.get("XSECURELOCK_FAKEPROTO_DELAY", "1"))

    if mode == "info-block":
        write_packet("i", message)
        while True:
            time.sleep(3600)

    if mode == "info-then-prompt":
        write_packet("i", message)
        time.sleep(delay)
        write_packet("P", prompt)
        packet_type, response = read_packet()
        if packet_type == "p" and response == expected:
            return 0
        if packet_type == "x":
            return 1
        return 1

    raise RuntimeError(f"Unknown fakeproto mode: {mode}")


if __name__ == "__main__":
    raise SystemExit(main())
