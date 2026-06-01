#!/usr/bin/env python3
"""update_print_scroll_expected.py

Runs gen_print_scroll.p8 on real Pico-8, parses the cx,cy output, and
rewrites the `expected` table in test_print_scroll.p8.

Usage (from any directory):
    python3 tests/regression/update_print_scroll_expected.py [options]

Options:
    --pico8 PATH   Path to pico8 binary (default: ~/pico-8/pico8)
    --gen   PATH   Path to gen_print_scroll.p8 (default: next to this script)
    --test  PATH   Path to test_print_scroll.p8 (default: next to this script)
    --dry-run      Print the new expected table but don't write the file
"""

import argparse
import os
import re
import subprocess
import sys


MARKER_START = "PSCROLL_START"
MARKER_END   = "PSCROLL_END"

# Matches the entire expected block in test_print_scroll.p8, inclusive of the
# opening and closing lines.
EXPECTED_RE = re.compile(
    r"^local expected = \{[^\n]*\n.*?^\}",
    re.MULTILINE | re.DOTALL,
)


def run_generator(pico8: str, gen_cart: str) -> list[tuple[int, int]]:
    result = subprocess.run(
        [pico8, "-x", gen_cart],
        capture_output=True,
        text=True,
    )
    # pico8 -x writes printh output to stdout
    lines = result.stdout.splitlines()

    in_block = False
    pairs: list[tuple[int, int]] = []
    for line in lines:
        if line.strip() == MARKER_START:
            in_block = True
            continue
        if line.strip() == MARKER_END:
            break
        if in_block:
            m = re.fullmatch(r"(-?\d+),(-?\d+)", line.strip())
            if m:
                pairs.append((int(m.group(1)), int(m.group(2))))
    return pairs


def build_table(pairs: list[tuple[int, int]]) -> str:
    lines = ["local expected = {"]
    lines.append("-- i   cx   cy")
    for i, (cx, cy) in enumerate(pairs, 1):
        lines.append(f"{{ {cx:3}, {cy:3} }}, -- {i:2}")
    lines.append("}")
    return "\n".join(lines)


def update_test_file(test_path: str, new_table: str, dry_run: bool) -> None:
    with open(test_path, "r") as f:
        src = f.read()

    if not EXPECTED_RE.search(src):
        sys.exit(f"error: could not find `local expected = {{...}}` block in {test_path}")

    new_src = EXPECTED_RE.sub(new_table, src, count=1)

    if dry_run:
        print(new_table)
        return

    with open(test_path, "w") as f:
        f.write(new_src)
    print(f"Updated {test_path}")


def main() -> None:
    here = os.path.dirname(os.path.abspath(__file__))

    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--pico8", default=os.path.expanduser("~/pico-8/pico8"),
                        help="Path to pico8 binary")
    parser.add_argument("--gen",  default=os.path.join(here, "gen_print_scroll.p8"),
                        help="Path to gen_print_scroll.p8")
    parser.add_argument("--test", default=os.path.join(here, "test_print_scroll.p8"),
                        help="Path to test_print_scroll.p8")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print new table without writing")
    args = parser.parse_args()

    for path, label in [(args.pico8, "pico8"), (args.gen, "--gen"), (args.test, "--test")]:
        if not os.path.exists(path):
            sys.exit(f"error: {label} not found: {path}")

    print(f"Running {args.gen} ...", file=sys.stderr)
    pairs = run_generator(args.pico8, args.gen)
    if not pairs:
        sys.exit("error: no cx,cy pairs found in pico8 output")
    print(f"Captured {len(pairs)} expected values.", file=sys.stderr)

    new_table = build_table(pairs)
    update_test_file(args.test, new_table, args.dry_run)


if __name__ == "__main__":
    main()
