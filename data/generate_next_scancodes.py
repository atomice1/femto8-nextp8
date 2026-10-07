#!/usr/bin/env python3
"""
Generate MKEYBOARD_SCANCODES from mkeyboard.v Verilog file.

This script parses the mkeyboard.v file and extracts the scancode mappings
to create a correct MKEYBOARD_SCANCODES definition.
"""

import re
import sys

def parse_mkeyboard_v(filename):
    """Parse mkeyboard.v and extract scancode mappings."""

    # Define the row mappings based on comments in mkeyboard.v
    # Format: (bit_position, key_name)
    row_mappings = {
        0: [  # Row 0: bit6=UP, bit5=EXTEND, bit4=V, bit3=C, bit2=X, bit1=Z, bit0=CapsShift
            (6, 'Up arrow'),
            (5, 'Extended'),
            (4, 'V'),
            (3, 'C'),
            (2, 'X'),
            (1, 'Z'),
            (0, 'Caps Shift'),
        ],
        1: [  # Row 1: bit6=GRAPH, bit5=CAPS LOCK, bit4=G, bit3=F, bit2=D, bit1=S, bit0=A
            (6, 'Graph'),
            (5, 'Caps Lock'),
            (4, 'G'),
            (3, 'F'),
            (2, 'D'),
            (1, 'S'),
            (0, 'A'),
        ],
        2: [  # Row 2: bit6=INV VIDEO, bit5=TRUE VIDEO, bit4=T, bit3=R, bit2=E, bit1=W, bit0=Q
            (6, 'Inv Video'),
            (5, 'True Video'),
            (4, 'T'),
            (3, 'R'),
            (2, 'E'),
            (1, 'W'),
            (0, 'Q'),
        ],
        3: [  # Row 3: bit6=EDIT, bit5=BREAK, bit4=5, bit3=4, bit2=3, bit1=2, bit0=1
            (6, 'Edit'),
            (5, 'Break'),
            (4, '5'),
            (3, '4'),
            (2, '3'),
            (1, '2'),
            (0, '1'),
        ],
        4: [  # Row 4: bit6=" , bit5=; , bit4=6, bit3=7, bit2=8, bit1=9, bit0=0
            (6, '"'),
            (5, ';'),
            (4, '6'),
            (3, '7'),
            (2, '8'),
            (1, '9'),
            (0, '0'),
        ],
        5: [  # Row 5: bit6=., bit5=,, bit4=Y, bit3=U, bit2=I, bit1=O, bit0=P
            (6, '.'),
            (5, ','),
            (4, 'Y'),
            (3, 'U'),
            (2, 'I'),
            (1, 'O'),
            (0, 'P'),
        ],
        6: [  # Row 6: bit6=RIGHT, bit5=DELETE, bit4=H, bit3=J, bit2=K, bit1=L, bit0=Enter
            (6, 'Right arrow'),
            (5, 'Delete'),
            (4, 'H'),
            (3, 'J'),
            (2, 'K'),
            (1, 'L'),
            (0, 'Enter'),
        ],
        7: [  # Row 7: bit6=DOWN, bit5=LEFT, bit4=B, bit3=N, bit2=M, bit1=SymShift, bit0=Space
            (6, 'Down arrow'),
            (5, 'Left arrow'),
            (4, 'B'),
            (3, 'N'),
            (2, 'M'),
            (1, 'Symbol'),
            (0, 'Space'),
        ],
    }

    scancodes = []

    with open(filename, 'r') as f:
        content = f.read()

    # Extract all p8matrix assignments
    # Pattern: p8matrix['hXX] <= matrix[Y];   // comment
    # Skip commented-out lines (those starting with //)
    # The pattern matches lines that contain p8matrix assignments with // comments
    # We need to filter out lines where the entire line is commented out (starts with //)
    # Note: some lines have complex expressions like "matrix[5] | matrix[38]"
    # Use a more flexible pattern that handles multiple matrix[] expressions
    pattern = r"^\s*(p8matrix\['h([0-9a-fA-F]+)\]\s*<=\s*matrix\[.+?\];\s*//\s*(.+))$"

    matches = re.findall(pattern, content, re.MULTILINE)

    # Filter out lines where the entire line is commented
    filtered_matches = []
    for match in matches:
        # match is (full_line, scancode_hex, comment) - note: matrix_index is embedded in full_line
        full_line = match[0]
        scancode_hex = match[1]
        comment = match[2]
        # Extract matrix_index from full_line
        matrix_match = re.search(r'matrix\[(\d+)\]', full_line)
        matrix_index = matrix_match.group(1) if matrix_match else None

        if matrix_index and not full_line.strip().startswith('//'):
            filtered_matches.append((scancode_hex, matrix_index, comment))

    matches = filtered_matches

    seen_scancodes = set()

    for scancode_hex, matrix_index, comment in matches:
        scancode = int(scancode_hex, 16)
        matrix_idx = int(matrix_index)

        # Get the key name from the comment (format: "key name (optional note)")
        # Extract just the key name part before any parentheses
        comment_stripped = comment.strip()
        if '(' in comment_stripped:
            key_name = comment_stripped[:comment_stripped.index('(')].strip()
        else:
            key_name = comment_stripped

        if key_name and scancode not in seen_scancodes:
            seen_scancodes.add(scancode)
            scancodes.append((scancode, key_name))

    return scancodes


def generate_mkeyboard_scancodes_file(scancodes, output_filename):
    """Generate the MKEYBOARD_SCANCODES Python file."""

    # Sort by scancode
    scancodes.sort(key=lambda x: x[0])

    # Generate Python code
    lines = ['MKEYBOARD_SCANCODES = (']

    for scancode, key_name in scancodes:
        # Format scancode as hex - match original format
        # Original: ((0x52, 0x59), '"') or ((0x41), ',')
        if isinstance(scancode, tuple):
            scancode_str = str(scancode)
        else:
            # Single scancode: (0x04, 'A')
            scancode_str = f"(0x{scancode:02x})"

        # Format key name (escape quotes)
        key_name_escaped = key_name.replace('"', '\\"')

        lines.append(f"    ({scancode_str}, '{key_name_escaped}'),")

    lines.append(')')

    with open(output_filename, 'w') as f:
        f.write('\n'.join(lines) + '\n')

    print(f"Generated {output_filename} with {len(scancodes)} scancodes")

    return scancodes


def main():
    if len(sys.argv) < 2:
        print("Usage: generate_next_scancodes.py <mkeyboard.v> [output_file]")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else 'mkeyboard_scancodes.py'

    scancodes = parse_mkeyboard_v(input_file)
    generate_mkeyboard_scancodes_file(scancodes, output_file)

    # Print summary
    print("\nScancodes generated:")
    for scancode, key_name in sorted(scancodes, key=lambda x: x[0]):
        print(f"  0x{scancode:02x}: {key_name}")


if __name__ == '__main__':
    main()
