#!/usr/bin/env python3
"""
Standalone algorithmic simulation of mitsubishi_v0a.c's encoder + decoder FSM.

This is NOT the firmware build (no compiler access to that toolchain) - it's
an independent Python re-implementation that mirrors the C logic in
lib/subghz/protocols/mitsubishi_v0a.c line-for-line, so it can actually be
*executed*: encode a payload to pulses, feed those pulses through the FSM,
and check the decoded result - catching arithmetic/off-by-one errors that
static reading and balance-checking cannot.

Not part of the firmware build (no CMake/fbt target references it) - it's a
dev-only verification artifact, kept here for anyone who changes the
preamble/CRC/field logic later and wants to re-check it without hardware.
Run with: python3 mitsubishi_v0a_sim.py
"""

TE_SHORT = 250
TE_LONG = 500
TE_DELTA = 100
MIN_COUNT_BIT = 61

LONG_PREAMBLE_PAIRS = 0x13F  # 319
SHORT_PREAMBLE_PAIRS = 0x50  # 80
PREAMBLE_MIN = 72
PREAMBLE_MAX = 88
SYNC_US = 750
GAP_US = 1500


def duration_diff(a, b):
    return abs(a - b)


def crc8(data_bytes):
    crc = 0x00
    for b in data_bytes:
        crc ^= b
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ 0x7F
            else:
                crc <<= 1
            crc &= 0xFF
    return crc


def calculate_crc(data):
    crc_data = [
        (data >> 48) & 0xFF,
        (data >> 40) & 0xFF,
        (data >> 32) & 0xFF,
        (data >> 24) & 0xFF,
        (data >> 16) & 0xFF,
        (data >> 8) & 0xFF,
    ]
    return crc8(crc_data)


def verify_crc(data):
    return (data & 0xFF) == calculate_crc(data)


def check_remote_controller(data):
    serial = (data >> 12) & 0x0FFFFFFF
    btn = (data >> 8) & 0x0F
    cnt = (data >> 40) & 0xFFFF
    return serial, btn, cnt


def build_payload(serial, btn, cnt):
    data = 0
    data |= (0x0F << 56)
    data |= ((cnt & 0xFFFF) << 40)
    data |= ((serial & 0x0FFFFFFF) << 12)
    data |= ((btn & 0x0F) << 8)
    data &= ~0xFF & ((1 << 64) - 1)
    data |= calculate_crc(data)
    return data


def bit_read(data, i):
    return (data >> i) & 1


def append_short_pairs(pulses, count):
    for _ in range(count):
        pulses.append((True, TE_SHORT))
        pulses.append((False, TE_SHORT))


def append_data_pairs(pulses, data, bit_count):
    for i in range(bit_count, 0, -1):
        duration = TE_LONG if bit_read(data, i - 1) else TE_SHORT
        pulses.append((True, duration))
        pulses.append((False, duration))


def build_upload(data, long_preamble=LONG_PREAMBLE_PAIRS, short_preamble=SHORT_PREAMBLE_PAIRS):
    """Mirrors mitsubishi_v0a_build_upload() exactly."""
    pulses = []
    pulses.append((True, SYNC_US))
    pulses.append((False, SYNC_US))
    append_short_pairs(pulses, long_preamble)
    append_data_pairs(pulses, data, MIN_COUNT_BIT)

    pulses.append((True, GAP_US))
    pulses.append((False, GAP_US))
    append_short_pairs(pulses, short_preamble)
    append_data_pairs(pulses, data, MIN_COUNT_BIT)

    pulses.append((True, GAP_US))
    pulses.append((False, GAP_US))
    return pulses


# ---------------------------------------------------------------------------
# Decoder: mirrors subghz_protocol_decoder_mitsubishi_v0a_feed() exactly,
# including the classification insert.
# ---------------------------------------------------------------------------
RESET, CHECK_PREAMBULA, SAVE_DURATION, CHECK_DURATION = range(4)


class DecoderState:
    def __init__(self):
        self.parser_step = RESET
        self.te_last = 0
        self.decode_data = 0
        self.decode_count_bit = 0
        self.header_count = 0
        self.have_last = False
        self.last_data = 0
        self.events = []  # list of (kind, info) for every accepted/classified frame


def add_bit(state, bit):
    state.decode_data = (state.decode_data << 1) | bit
    state.decode_count_bit += 1


def feed(state, level, duration):
    if state.parser_step == RESET:
        if level and duration_diff(duration, TE_SHORT) < TE_DELTA:
            state.parser_step = CHECK_PREAMBULA
            state.te_last = duration
            state.header_count = 0

    elif state.parser_step == CHECK_PREAMBULA:
        if level:
            if (duration_diff(duration, TE_SHORT) < TE_DELTA) or \
               (duration_diff(duration, TE_LONG) < TE_DELTA):
                state.te_last = duration
            else:
                state.parser_step = RESET
        elif (duration_diff(duration, TE_SHORT) < TE_DELTA) and \
             (duration_diff(state.te_last, TE_SHORT) < TE_DELTA):
            state.header_count += 1
        elif (duration_diff(duration, TE_LONG) < TE_DELTA) and \
             (duration_diff(state.te_last, TE_LONG) < TE_DELTA):
            if state.header_count > 15:
                state.parser_step = SAVE_DURATION
                state.decode_data = 0
                state.decode_count_bit = 1
                add_bit(state, 1)
            else:
                state.parser_step = RESET
        else:
            state.parser_step = RESET

    elif state.parser_step == SAVE_DURATION:
        if level:
            if duration >= (TE_LONG + TE_DELTA * 2):
                state.parser_step = RESET
                if state.decode_count_bit == MIN_COUNT_BIT:
                    data = state.decode_data
                    if verify_crc(data):
                        is_repeat_in_range = (
                            state.have_last
                            and state.last_data == data
                            and PREAMBLE_MIN <= state.header_count <= PREAMBLE_MAX
                        )
                        state.events.append(
                            ("crc_ok", data, state.header_count, is_repeat_in_range)
                        )
                        state.last_data = data
                        state.have_last = True
                        if is_repeat_in_range:
                            state.events.append(("CALLBACK", data))
                    else:
                        state.events.append(("crc_fail", data, state.header_count))
                state.decode_data = 0
                state.decode_count_bit = 0
            else:
                state.te_last = duration
                state.parser_step = CHECK_DURATION
        else:
            state.parser_step = RESET

    elif state.parser_step == CHECK_DURATION:
        if not level:
            if (duration_diff(state.te_last, TE_SHORT) < TE_DELTA) and \
               (duration_diff(duration, TE_SHORT) < TE_DELTA):
                add_bit(state, 0)
                state.parser_step = SAVE_DURATION
            elif (duration_diff(state.te_last, TE_LONG) < TE_DELTA) and \
                 (duration_diff(duration, TE_LONG) < TE_DELTA):
                add_bit(state, 1)
                state.parser_step = SAVE_DURATION
            else:
                state.parser_step = RESET
        else:
            state.parser_step = RESET


def run_pulses(pulses):
    state = DecoderState()
    for level, duration in pulses:
        feed(state, level, duration)
    return state


def summarize(label, pulses, expect_callback, expect_serial=None, expect_btn=None, expect_cnt=None):
    state = run_pulses(pulses)
    callbacks = [e for e in state.events if e[0] == "CALLBACK"]
    print(f"--- {label} ---")
    print(f"  total pulses: {len(pulses)}")
    for e in state.events:
        if e[0] == "crc_ok":
            _, data, hc, in_range = e
            s, b, c = check_remote_controller(data)
            print(f"  frame: CRC OK  header_count={hc:3d}  in_range={in_range}  "
                  f"serial={s:#x} btn={b} cnt={c:#x}")
        elif e[0] == "crc_fail":
            _, data, hc = e
            print(f"  frame: CRC FAIL header_count={hc}")
    ok = (len(callbacks) == 1) if expect_callback else (len(callbacks) == 0)
    if expect_callback and callbacks:
        data = callbacks[0][1]
        s, b, c = check_remote_controller(data)
        if expect_serial is not None:
            ok = ok and (s == expect_serial)
        if expect_btn is not None:
            ok = ok and (b == expect_btn)
        if expect_cnt is not None:
            ok = ok and (c == expect_cnt)
    status = "PASS" if ok else "FAIL"
    print(f"  callbacks={len(callbacks)} expected_callback={expect_callback}  [{status}]")
    print()
    return ok


def main():
    all_ok = True

    serial = 0x1234567
    btn = 2
    cnt = 0x00AB
    data = build_payload(serial, btn, cnt)
    assert verify_crc(data), "self-check: freshly built payload must verify"
    s, b, c = check_remote_controller(data)
    assert (s, b, c) == (serial, btn, cnt), "self-check: field round-trip"
    print(f"Payload under test: data={data:#018x} serial={s:#x} btn={b} cnt={c:#x} "
          f"crc_ok={verify_crc(data)}")
    print(f"UPLOAD_CAPACITY expected=1048\n")

    # 1. Full, correct two-burst Mitsubishi transmission -> exactly one callback,
    #    firing on the second (short-preamble) burst, with correct fields.
    pulses = build_upload(data)
    all_ok &= (len(pulses) == 1048)
    print(f"pulse count = {len(pulses)} (expect 1048) [{'PASS' if len(pulses) == 1048 else 'FAIL'}]\n")
    all_ok &= summarize(
        "1. Genuine two-burst Mitsubishi V0-a transmission",
        pulses, expect_callback=True, expect_serial=serial, expect_btn=btn, expect_cnt=cnt,
    )

    # 2. Only the first (long-preamble) burst ever arrives (signal cut off,
    #    or a genuine lone Kia transmission) -> must NOT classify as Mitsubishi.
    first_burst_len = 2 + LONG_PREAMBLE_PAIRS * 2 + MIN_COUNT_BIT * 2
    pulses_first_only = pulses[:first_burst_len]
    all_ok &= summarize(
        "2. First burst only (lone Kia-style transmission)",
        pulses_first_only, expect_callback=False,
    )

    # 3. Two bursts, both with the SAME long preamble (a repeated plain-Kia
    #    signal, not a real Mitsubishi one) -> must NOT classify as Mitsubishi,
    #    because header_count (319) is outside [72, 88].
    pulses_double_kia = build_upload(data, long_preamble=LONG_PREAMBLE_PAIRS,
                                      short_preamble=LONG_PREAMBLE_PAIRS)
    all_ok &= summarize(
        "3. Doubled plain-Kia signal (both bursts long preamble)",
        pulses_double_kia, expect_callback=False,
    )

    # 4. Boundary: measured header_count = configured short_preamble + 1 (bit
    #    60 of the payload is always 0 by construction - see note below - so
    #    it always contributes one extra "preamble" short-short pair before
    #    the decoder recognizes bit 59's long-long pulse as the start bit).
    #    So the *configured* pair count that lands exactly on PREAMBLE_MIN/MAX
    #    is one less than the constant itself.
    for boundary in (PREAMBLE_MIN - 1, PREAMBLE_MAX - 1):
        pulses_b = build_upload(data, short_preamble=boundary)
        all_ok &= summarize(
            f"4. Second burst measured preamble exactly at boundary (configured={boundary}, measured={boundary+1})",
            pulses_b, expect_callback=True, expect_serial=serial, expect_btn=btn, expect_cnt=cnt,
        )

    # 5. Just outside the boundary -> must NOT classify (same +1 adjustment).
    for outside in (PREAMBLE_MIN - 2, PREAMBLE_MAX):
        pulses_o = build_upload(data, short_preamble=outside)
        all_ok &= summarize(
            f"5. Second burst measured preamble just outside boundary (configured={outside}, measured={outside+1})",
            pulses_o, expect_callback=False,
        )

    # 6. Mismatched payload between bursts (corrupted second burst) -> must NOT
    #    classify, since last_data != current data.
    data2 = build_payload(serial, btn, (cnt + 1) & 0xFFFF)
    pulses_first = build_upload(data)[:first_burst_len]
    pulses_second = build_upload(data2)[first_burst_len:]
    all_ok &= summarize(
        "6. Mismatched two bursts (counter changed mid-transmission)",
        pulses_first + pulses_second, expect_callback=False,
    )

    print("=" * 60)
    print(f"ALL TESTS PASS: {all_ok}")
    return 0 if all_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
