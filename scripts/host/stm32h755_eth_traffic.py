#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0

import argparse
import socket
import sys
import time


def parse_args():
    parser = argparse.ArgumentParser(
        description="Validate STM32H755 raw Ethernet TX and inject RX integrity frames."
    )
    parser.add_argument("--iface", required=True)
    parser.add_argument("--tx-count", type=int, default=5)
    parser.add_argument("--rx-count", type=int, default=64)
    return parser.parse_args()


def main():
    args = parse_args()
    if args.tx_count <= 0 or args.rx_count <= 0:
        print("TRAFFIC_RESULT: FAIL (counts must be positive)")
        return 2

    stm_mac = bytes.fromhex("020000000001")
    broadcast = bytes([0xFF]) * 6
    tx_ethertype = bytes.fromhex("88b5")
    rx_ethertype = bytes.fromhex("88b6")
    tx_payload = b"DAS ETH L2 test"
    rx_magic = b"DASRXV1\x00"

    with open(f"/sys/class/net/{args.iface}/address", "r", encoding="ascii") as handle:
        host_mac = bytes.fromhex(handle.read().strip().replace(":", ""))

    sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(0x0003))
    sock.bind((args.iface, 0))

    captured = 0
    timestamps = []
    deadline = time.monotonic() + max(10.0, float(args.tx_count) * 2.0)
    while captured < args.tx_count and time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        sock.settimeout(max(0.05, min(1.0, remaining)))
        try:
            frame = sock.recv(65535)
        except socket.timeout:
            continue

        if (len(frame) >= 60 and
                frame[0:6] == broadcast and
                frame[6:12] == stm_mac and
                frame[12:14] == tx_ethertype and
                frame[14:14 + len(tx_payload)] == tx_payload):
            captured += 1
            timestamps.append(time.monotonic())
            print(f"TX_VALID sequence={captured} length={len(frame)}")

    if captured != args.tx_count:
        print(f"TX_CAPTURED={captured}")
        print(f"TX_EXPECTED={args.tx_count}")
        print("TRAFFIC_RESULT: FAIL (STM32 TX capture timeout)")
        return 1

    if len(timestamps) >= 2:
        intervals = [timestamps[i] - timestamps[i - 1]
                     for i in range(1, len(timestamps))]
        print(f"TX_INTERVAL_MIN_S={min(intervals):.6f}")
        print(f"TX_INTERVAL_MAX_S={max(intervals):.6f}")
    print(f"TX_CAPTURED={captured}")

    for sequence in range(1, args.rx_count + 1):
        payload = bytearray(46)
        payload[0:8] = rx_magic
        payload[8:12] = sequence.to_bytes(4, "big")
        for index in range(12, len(payload)):
            payload[index] = (sequence + index - 12) & 0xFF

        frame = stm_mac + host_mac + rx_ethertype + bytes(payload)
        if len(frame) != 60:
            print(f"TRAFFIC_RESULT: FAIL (frame length {len(frame)} != 60)")
            return 1
        sent = sock.send(frame)
        if sent != len(frame):
            print(f"TRAFFIC_RESULT: FAIL (short raw send {sent}/{len(frame)})")
            return 1
        time.sleep(0.010)

    print(f"RX_INJECTED={args.rx_count}")
    time.sleep(1.0)
    print("TRAFFIC_RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
