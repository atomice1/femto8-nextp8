#!/usr/bin/env python3
"""Decode P8SCII text sequences using Pico-8 Appendix A semantics.

Input:
- First CLI argument, or
- stdin when no argument is provided.

The decoder understands:
- Backslash escapes used in Pico-8 string literals (for control chars)
- P8 symbol glyphs via src/data/p8_symbols.h mapping
- Control codes CHR(0)..CHR(15)
- Special commands under CHR(6) / "^"
"""

from __future__ import annotations

import argparse
import errno
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Tuple


CTRL_NAMES = {
    0: "terminate",
    1: "repeat",
    2: "bg_color",
    3: "shift_x",
    4: "shift_y",
    5: "shift_xy",
    6: "special",
    7: "audio",
    8: "backspace",
    9: "tab",
    10: "newline",
    11: "decorate_prev",
    12: "fg_color",
    13: "carriage_return",
    14: "font_custom",
    15: "font_default",
}

COLOR_NAMES = {
    0: "black",
    1: "dark-blue",
    2: "dark-purple",
    3: "dark-green",
    4: "brown",
    5: "dark-grey",
    6: "light-grey",
    7: "white",
    8: "red",
    9: "orange",
    10: "yellow",
    11: "green",
    12: "blue",
    13: "lavender",
    14: "pink",
    15: "light-peach",
    128: "brownish-black",
    129: "darker-blue",
    130: "darker-purple",
    131: "blue-green",
    132: "dark-brown",
    133: "darker-grey",
    134: "medium-grey",
    135: "light-yellow",
    136: "dark-red",
    137: "dark-orange",
    138: "lime-green",
    139: "medium-green",
    140: "true-blue",
    141: "mauve",
    142: "dark-peach",
    143: "peach",
    -16: "brownish-black",
    -15: "darker-blue",
    -14: "darker-purple",
    -13: "blue-green",
    -12: "dark-brown",
    -11: "darker-grey",
    -10: "medium-grey",
    -9: "light-yellow",
    -8: "dark-red",
    -7: "dark-orange",
    -6: "lime-green",
    -5: "medium-green",
    -4: "true-blue",
    -3: "mauve",
    -2: "dark-peach",
    -1: "peach",
}


@dataclass
class DecoderState:
    x: int = 0
    y: int = 0
    home_x: int = 0
    home_y: int = 0
    left_margin: int = 0
    fg: int = 6
    bg: int = 0
    solid_bg: bool = False
    border: bool = True
    wide: bool = False
    tall: bool = False
    stripey: bool = False
    invert: bool = False
    underline: bool = False
    char_w: int = 4
    char_h: int = 6
    char_w2: int = 8
    tab_width: int = 16
    wrap_enabled: bool = False
    wrap_boundary: int = 128
    delay_frames: int = 0
    last_advance: int = 4
    use_custom_font: bool = False
    terminated: bool = False


def hexy(ch: int) -> int:
    if 48 <= ch <= 57:  # 0..9
        return ch - 48
    if 97 <= ch <= 122:  # a..z
        return ch - 97 + 10
    if 65 <= ch <= 90:  # A..Z
        return ch - 65 + 10
    return ch


def load_p8_symbols(header_path: Path) -> Tuple[Dict[bytes, int], Dict[int, str]]:
    text = header_path.read_text(encoding="utf-8")
    entries = re.findall(r"\{\{([^}]*)\},\s*(\d+),\s*(\d+)\}", text)

    enc_to_idx: Dict[bytes, int] = {}
    idx_to_glyph: Dict[int, str] = {}

    for enc_csv, _length, idx_s in entries:
        vals = []
        for tok in enc_csv.split(","):
            tok = tok.strip()
            if not tok:
                continue
            vals.append(int(tok, 0))

        b = bytes(vals)
        idx = int(idx_s)
        enc_to_idx[b] = idx
        try:
            idx_to_glyph[idx] = b.decode("utf-8")
        except UnicodeDecodeError:
            idx_to_glyph[idx] = f"<bytes:{b.hex()}>"

    return enc_to_idx, idx_to_glyph


def expand_backslash_escapes(text: str) -> str:
    """Expand Pico-8 control escape sequences from literal text.

    Example: "\\^c1" -> CHR(6) + "c1".
    """
    ctrl_map = {
        "0": 0,
        "*": 1,
        "#": 2,
        "-": 3,
        "|": 4,
        "+": 5,
        "^": 6,
        "a": 7,
        "b": 8,
        "t": 9,
        "n": 10,
        "v": 11,
        "f": 12,
        "r": 13,
    }

    out: List[str] = []
    i = 0
    n = len(text)

    while i < n:
        ch = text[i]
        if ch != "\\" or i + 1 >= n:
            out.append(ch)
            i += 1
            continue

        nxt = text[i + 1]

        if nxt in ctrl_map:
            out.append(chr(ctrl_map[nxt]))
            i += 2
            continue

        # Octal escapes, e.g. \014 and \015
        if "0" <= nxt <= "7":
            j = i + 1
            oct_digits = []
            while j < n and len(oct_digits) < 3 and "0" <= text[j] <= "7":
                oct_digits.append(text[j])
                j += 1
            out.append(chr(int("".join(oct_digits), 8)))
            i = j
            continue

        # Backslash escaping for other chars: keep literal next char
        out.append(nxt)
        i += 2

    return "".join(out)


def to_p8_bytes(text: str, enc_to_idx: Dict[bytes, int]) -> List[int]:
    text = expand_backslash_escapes(text)
    raw = text.encode("utf-8")

    # Longest-first symbol matching
    encodings = sorted(enc_to_idx.keys(), key=len, reverse=True)

    out: List[int] = []
    i = 0
    while i < len(raw):
        matched = False
        for enc in encodings:
            if raw.startswith(enc, i):
                out.append(enc_to_idx[enc])
                i += len(enc)
                matched = True
                break
        if not matched:
            out.append(raw[i])
            i += 1
    return out


def fmt_ch(v: int, idx_to_glyph: Dict[int, str]) -> str:
    if 32 <= v < 127:
        return chr(v)
    if v in idx_to_glyph:
        return idx_to_glyph[v]
    if v < 16:
        return f"<ctrl:{CTRL_NAMES.get(v, '?')}>"
    return f"<0x{v:02x}>"


def fmt_color(v: int) -> str:
    name = COLOR_NAMES.get(v)
    if name is None:
        return str(v)
    return f"{v} ({name})"


def decode(p8: List[int], idx_to_glyph: Dict[int, str]) -> Tuple[List[str], DecoderState]:
    s = DecoderState()
    lines: List[str] = []

    def append(msg: str) -> None:
        lines.append(msg)

    i = 0
    while i < len(p8) and not s.terminated:
        c = p8[i]
        x0, y0 = s.x, s.y

        if c < 16:
            name = CTRL_NAMES.get(c, "?")

            if c == 0:
                s.terminated = True
                append(f"[{i:04d}] CTRL 0 (terminate): stop decoding")

            elif c == 1:
                if i + 1 < len(p8):
                    rep = hexy(p8[i + 1])
                    append(f"[{i:04d}] CTRL 1 (repeat): next char repeated {rep} times")
                    i += 1
                else:
                    append(f"[{i:04d}] CTRL 1 (repeat): missing parameter")

            elif c == 2:
                if i + 1 < len(p8):
                    s.bg = hexy(p8[i + 1])
                    s.solid_bg = True
                    append(f"[{i:04d}] CTRL 2 (bg_color): bg={fmt_color(s.bg)}, solid_bg=on")
                    i += 1
                else:
                    append(f"[{i:04d}] CTRL 2 (bg_color): missing parameter")

            elif c == 3:
                if i + 1 < len(p8):
                    p0 = hexy(p8[i + 1])
                    s.x += p0 - 16
                    append(f"[{i:04d}] CTRL 3 (shift_x): p0={p0}, cursor ({x0},{y0})->({s.x},{s.y})")
                    i += 1
                else:
                    append(f"[{i:04d}] CTRL 3 (shift_x): missing parameter")

            elif c == 4:
                if i + 1 < len(p8):
                    p0 = hexy(p8[i + 1])
                    s.y += p0 - 16
                    append(f"[{i:04d}] CTRL 4 (shift_y): p0={p0}, cursor ({x0},{y0})->({s.x},{s.y})")
                    i += 1
                else:
                    append(f"[{i:04d}] CTRL 4 (shift_y): missing parameter")

            elif c == 5:
                if i + 2 < len(p8):
                    p0 = hexy(p8[i + 1])
                    p1 = hexy(p8[i + 2])
                    s.x += p0 - 16
                    s.y += p1 - 16
                    append(
                        f"[{i:04d}] CTRL 5 (shift_xy): p0={p0} p1={p1}, cursor ({x0},{y0})->({s.x},{s.y})"
                    )
                    i += 2
                else:
                    append(f"[{i:04d}] CTRL 5 (shift_xy): missing parameters")

            elif c == 6:
                if i + 1 >= len(p8):
                    append(f"[{i:04d}] CTRL 6 (special): missing command")
                else:
                    cmd = p8[i + 1]
                    i += 1
                    ch = chr(cmd) if 32 <= cmd < 127 else f"0x{cmd:02x}"

                    if 49 <= cmd <= 57:
                        frames = 1 << (cmd - 49)
                        append(f"[{i-1:04d}] SPECIAL ^{ch}: skip {frames} frame(s)")

                    elif cmd == ord("c") and i + 1 < len(p8):
                        clear_col = hexy(p8[i + 1])
                        s.x = s.y = s.home_x = s.home_y = s.left_margin = 0
                        append(f"[{i-1:04d}] SPECIAL ^c{chr(p8[i+1])}: cls color={fmt_color(clear_col)}, cursor/home reset")
                        i += 1

                    elif cmd == ord("d") and i + 1 < len(p8):
                        s.delay_frames = hexy(p8[i + 1])
                        append(f"[{i-1:04d}] SPECIAL ^d{chr(p8[i+1])}: delay_frames={s.delay_frames}")
                        i += 1

                    elif cmd == ord("g"):
                        s.x, s.y = s.home_x, s.home_y
                        append(f"[{i-1:04d}] SPECIAL ^g: cursor -> home ({s.x},{s.y})")

                    elif cmd == ord("h"):
                        s.home_x, s.home_y = s.x, s.y
                        append(f"[{i-1:04d}] SPECIAL ^h: home <- cursor ({s.home_x},{s.home_y})")

                    elif cmd == ord("j") and i + 2 < len(p8):
                        p0 = hexy(p8[i + 1])
                        p1 = hexy(p8[i + 2])
                        s.x, s.y = p0 * 4, p1 * 4
                        append(f"[{i-1:04d}] SPECIAL ^j: cursor=({s.x},{s.y}) from p0={p0}, p1={p1}")
                        i += 2

                    elif cmd == ord("r") and i + 1 < len(p8):
                        p0 = hexy(p8[i + 1])
                        s.wrap_boundary = p0 * 4
                        s.wrap_enabled = True
                        append(f"[{i-1:04d}] SPECIAL ^r: wrap_boundary={s.wrap_boundary}, wrap_enabled=on")
                        i += 1

                    elif cmd == ord("s") and i + 1 < len(p8):
                        p0 = hexy(p8[i + 1])
                        s.tab_width = p0
                        append(f"[{i-1:04d}] SPECIAL ^s: tab_width={s.tab_width}")
                        i += 1

                    elif cmd == ord("u"):
                        s.underline = True
                        append(f"[{i-1:04d}] SPECIAL ^u: underline=on")

                    elif cmd == ord("x") and i + 1 < len(p8):
                        s.char_w = hexy(p8[i + 1])
                        append(f"[{i-1:04d}] SPECIAL ^x: char_w={s.char_w}")
                        i += 1

                    elif cmd == ord("y") and i + 1 < len(p8):
                        s.char_h = hexy(p8[i + 1])
                        append(f"[{i-1:04d}] SPECIAL ^y: char_h={s.char_h}")
                        i += 1

                    elif cmd == ord("w"):
                        s.wide = True
                        append(f"[{i-1:04d}] SPECIAL ^w: wide=on")

                    elif cmd == ord("t"):
                        s.tall = True
                        append(f"[{i-1:04d}] SPECIAL ^t: tall=on")

                    elif cmd == ord("="):
                        s.stripey = True
                        append(f"[{i-1:04d}] SPECIAL ^=: stripey=on")

                    elif cmd == ord("p"):
                        s.wide = s.tall = s.stripey = True
                        append(f"[{i-1:04d}] SPECIAL ^p: wide=on, tall=on, stripey=on")

                    elif cmd == ord("i"):
                        s.invert = True
                        append(f"[{i-1:04d}] SPECIAL ^i: invert=on")

                    elif cmd == ord("b"):
                        s.border = not s.border
                        append(f"[{i-1:04d}] SPECIAL ^b: border={'on' if s.border else 'off'}")

                    elif cmd == ord("#"):
                        s.solid_bg = True
                        append(f"[{i-1:04d}] SPECIAL ^#: solid_bg=on")

                    elif cmd == ord("-") and i + 1 < len(p8):
                        flag = p8[i + 1]
                        i += 1
                        detail = "unknown flag"
                        if flag == ord("w"):
                            s.wide = False
                            detail = "wide=off"
                        elif flag == ord("t"):
                            s.tall = False
                            detail = "tall=off"
                        elif flag == ord("="):
                            s.stripey = False
                            detail = "stripey=off"
                        elif flag == ord("p"):
                            s.wide = s.tall = s.stripey = False
                            detail = "wide=off, tall=off, stripey=off"
                        elif flag == ord("i"):
                            s.invert = False
                            detail = "invert=off"
                        elif flag == ord("b"):
                            s.border = not s.border
                            detail = f"border={'on' if s.border else 'off'}"
                        elif flag == ord("#"):
                            s.solid_bg = False
                            detail = "solid_bg=off"
                        append(f"[{i-2:04d}] SPECIAL ^-{fmt_ch(flag, idx_to_glyph)}: {detail}")

                    elif cmd in (ord("."), ord(",")):
                        mode = "respect-padding" if cmd == ord(",") else "no-padding"
                        if i + 8 < len(p8):
                            payload = p8[i + 1 : i + 9]
                            i += 8
                            s.x += 8
                            s.last_advance = 8
                            append(
                                f"[{i-8:04d}] SPECIAL ^{chr(cmd)}: one-off raw 8x8 ({mode}), payload={[f'0x{b:02x}' for b in payload]}, x+=8"
                            )
                        else:
                            append(f"[{i:04d}] SPECIAL ^{chr(cmd)}: missing 8-byte payload")

                    elif cmd in (ord(":"), ord(";")):
                        mode = "respect-padding" if cmd == ord(";") else "no-padding"
                        if i + 16 < len(p8):
                            raw = p8[i + 1 : i + 17]
                            i += 16
                            rows = []
                            for r in range(8):
                                hi = hexy(raw[r * 2])
                                lo = hexy(raw[r * 2 + 1])
                                rows.append((hi << 4) | lo)
                            s.x += 8
                            s.last_advance = 8
                            append(
                                f"[{i-16:04d}] SPECIAL ^{chr(cmd)}: one-off hex 8x8 ({mode}), rows={[f'0x{b:02x}' for b in rows]}, x+=8"
                            )
                        else:
                            append(f"[{i:04d}] SPECIAL ^{chr(cmd)}: missing 16-nybble payload")

                    elif cmd == ord("@") and i + 8 < len(p8):
                        p = p8[i + 1 : i + 9]
                        addr = (hexy(p[0]) << 12) | (hexy(p[1]) << 8) | (hexy(p[2]) << 4) | hexy(p[3])
                        count = (hexy(p[4]) << 12) | (hexy(p[5]) << 8) | (hexy(p[6]) << 4) | hexy(p[7])
                        i += 8
                        remaining = len(p8) - (i + 1)
                        consume = min(count, max(remaining, 0))
                        i += consume
                        append(
                            f"[{i-consume-8:04d}] SPECIAL ^@: poke addr=0x{addr:04x}, count={count}, consumed={consume} data byte(s)"
                        )

                    elif cmd == ord("!") and i + 4 < len(p8):
                        p = p8[i + 1 : i + 5]
                        addr = (hexy(p[0]) << 12) | (hexy(p[1]) << 8) | (hexy(p[2]) << 4) | hexy(p[3])
                        i += 4
                        consume = len(p8) - (i + 1)
                        i = len(p8) - 1
                        append(f"[{i-consume-4:04d}] SPECIAL ^!: poke addr=0x{addr:04x}, consumed all remaining {consume} byte(s)")

                    elif cmd == ord("o") and i + 3 < len(p8):
                        c0 = p8[i + 1]
                        h = hexy(p8[i + 2])
                        l = hexy(p8[i + 3])
                        i += 3
                        col = fmt_ch(c0, idx_to_glyph)
                        append(f"[{i-3:04d}] SPECIAL ^o: outline color={col}, mask=0x{((h << 4) | l):02x}")

                    else:
                        append(f"[{i-1:04d}] SPECIAL ^{ch}: unsupported or missing params")

            elif c == 7:
                if i + 2 < len(p8):
                    p0 = hexy(p8[i + 1])
                    p1 = hexy(p8[i + 2])
                    append(f"[{i:04d}] CTRL 7 (audio): play sfx index {((p0 << 4) | p1)}")
                    i += 2
                else:
                    append(f"[{i:04d}] CTRL 7 (audio): beep or incomplete sfx index")

            elif c == 8:
                s.x -= s.last_advance
                append(f"[{i:04d}] CTRL 8 (backspace): cursor ({x0},{y0})->({s.x},{s.y})")

            elif c == 9:
                s.x = ((s.x // s.tab_width) + 1) * s.tab_width
                append(f"[{i:04d}] CTRL 9 (tab): cursor ({x0},{y0})->({s.x},{s.y}), tab_width={s.tab_width}")

            elif c == 10:
                s.x = s.left_margin
                s.y += s.char_h
                append(f"[{i:04d}] CTRL 10 (newline): cursor ({x0},{y0})->({s.x},{s.y})")

            elif c == 11:
                if i + 2 < len(p8):
                    off = hexy(p8[i + 1])
                    deco = p8[i + 2]
                    ox = (off % 4) - 2
                    oy = (off // 4) - 8
                    append(f"[{i:04d}] CTRL 11 (decorate_prev): offset=({ox},{oy}), char={fmt_ch(deco, idx_to_glyph)}")
                    i += 2
                else:
                    append(f"[{i:04d}] CTRL 11 (decorate_prev): missing params")

            elif c == 12:
                if i + 1 < len(p8):
                    s.fg = hexy(p8[i + 1])
                    append(f"[{i:04d}] CTRL 12 (fg_color): fg={fmt_color(s.fg)}")
                    i += 1
                else:
                    append(f"[{i:04d}] CTRL 12 (fg_color): missing parameter")

            elif c == 13:
                s.x = s.left_margin
                append(f"[{i:04d}] CTRL 13 (carriage_return): cursor ({x0},{y0})->({s.x},{s.y})")

            elif c == 14:
                s.use_custom_font = True
                append(f"[{i:04d}] CTRL 14 (font_custom): custom font enabled")

            elif c == 15:
                s.use_custom_font = False
                append(f"[{i:04d}] CTRL 15 (font_default): default font enabled")

            else:
                append(f"[{i:04d}] CTRL {c} ({name})")

        else:
            ch = fmt_ch(c, idx_to_glyph)
            adv = (s.char_w2 if c >= 0x80 else s.char_w) * (2 if s.wide else 1)
            s.x += adv
            s.last_advance = adv

            if s.wrap_enabled and s.x >= s.wrap_boundary:
                s.x = s.left_margin
                s.y += s.char_h

            append(f"[{i:04d}] CHAR {ch}: advance {adv}, cursor ({x0},{y0})->({s.x},{s.y})")

        i += 1

    return lines, s


def get_input_text(args: argparse.Namespace) -> str:
    if args.file is not None:
        try:
            return Path(args.file).read_text(encoding="utf-8")
        except OSError as exc:
            raise ValueError(f"error: could not read input file '{args.file}': {exc}") from exc

    if args.text is not None:
        return args.text

    data = sys.stdin.read()
    return data


def main() -> int:
    ap = argparse.ArgumentParser(description="Decode Pico-8 P8SCII strings and show command effects")
    ap.add_argument("text", nargs="?", help="Input text to decode (if omitted, read stdin)")
    ap.add_argument("-f", "--file", help="Read input text from file")
    ap.add_argument("--symbols", default="src/data/p8_symbols.h", help="Path to p8_symbols.h")
    ap.add_argument("--show-bytes", action="store_true", help="Show normalized P8 byte stream")
    args = ap.parse_args()

    if args.file is not None and args.text is not None:
        print("error: use either positional text or -f/--file, not both", file=sys.stderr)
        return 2

    symbol_path = Path(args.symbols)
    if not symbol_path.exists():
        print(f"error: symbols file not found: {symbol_path}", file=sys.stderr)
        return 2

    enc_to_idx, idx_to_glyph = load_p8_symbols(symbol_path)
    try:
        text = get_input_text(args)
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    p8 = to_p8_bytes(text, enc_to_idx)

    print("P8SCII decode")
    print("=============")
    print(f"input length (chars): {len(text)}")
    print(f"normalized p8 length: {len(p8)}")

    if args.show_bytes:
        print("p8 bytes:")
        print(" ".join(f"{b:02x}" for b in p8))

    events, final_state = decode(p8, idx_to_glyph)
    print("events:")
    for line in events:
        print(line)

    print("final state:")
    print(
        f"cursor=({final_state.x},{final_state.y}) "
        f"home=({final_state.home_x},{final_state.home_y}) "
        f"fg={fmt_color(final_state.fg)} bg={fmt_color(final_state.bg)} "
        f"solid_bg={'on' if final_state.solid_bg else 'off'} "
        f"wide={'on' if final_state.wide else 'off'} "
        f"tall={'on' if final_state.tall else 'off'} "
        f"terminated={'yes' if final_state.terminated else 'no'}"
    )

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except BrokenPipeError:
        try:
            sys.stdout.close()
        except OSError:
            pass
        raise SystemExit(128 + errno.EPIPE)
