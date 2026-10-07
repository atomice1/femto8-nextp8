#!/usr/bin/env python3
"""
Keyboard layout converter between PC and ZX Spectrum Next membrane keyboard layouts.

This script converts keyboard shortcuts between:
- PC (USB HID) scancodes used in PICO-8
- ZX Spectrum Next membrane keyboard scancodes

The Spectrum Next uses an extended British layout with special keys that map
to PC keys differently than standard US keyboards. The keyboard has an
"EXTEND" mode (Right Shift) that provides access to arrow keys and navigation.

Key Mapping Principle:
---------------------
Both PC and Next use USB HID scancodes as the intermediate format. The membrane
keyboard outputs USB HID scancodes directly. The mapping is:
- PC key -> USB HID scancode -> Next key

Since 'a' and 'A' are equivalent in PICO-8, we use lowercase names for simplicity.

Examples:
    Ctrl+A (PC) -> A + EXTEND (Next)
    Ctrl+Left (PC) -> K + EXTEND (Next)

Usage:
    python keyboard_convert.py --pc "Ctrl+A"
    python keyboard_convert.py --next "A + EXTEND"
    python keyboard_convert.py --list
    python keyboard_convert.py --help

"""

import sys
import argparse

# Import scancode data from existing files
try:
    from .ps2_scancodes import PS2_SCANCODES
    from .mkeyboard_scancodes import MKEYBOARD_SCANCODES
    from .usb_hid_scancodes import USB_HID_SCANCODES
except ImportError:
    from ps2_scancodes import PS2_SCANCODES
    from mkeyboard_scancodes import MKEYBOARD_SCANCODES
    from usb_hid_scancodes import USB_HID_SCANCODES


def build_scancode_tables():
    """Build mapping tables like generate_nextp8_keymap.py does."""

    # Build USB HID scancode -> name mapping
    usb_hid_scancode_to_name = {}
    name_to_usb_hid_scancode = {}
    for scancode, name in USB_HID_SCANCODES:
        if not scancode:
            continue
        usb_hid_scancode_to_name[scancode] = name
        name_to_usb_hid_scancode[name] = scancode

    # Build Next membrane scancode -> name mapping
    next_scancode_to_name = {}
    name_to_next_scancode = {}
    for seq, name in MKEYBOARD_SCANCODES:
        next_scancode_to_name[seq] = name
        name_to_next_scancode[name] = seq

    # Build PC key name -> USB HID scancode mapping
    # PC uses lowercase names for letters (a-z)
    pc_key_to_usb_hid = {}
    for seq, name in USB_HID_SCANCODES:
        if not seq:
            continue

        # Handle different name formats
        key_name = None

        # Extract the first character (lowercase) from names like "a and A"
        if ' and ' in name:
            first_char = name.split(' and ')[0].lower()
            if len(first_char) == 1 and first_char.isalpha():
                pc_key_to_usb_hid[first_char] = seq
            continue

        # Handle special keys with specific names
        # Extract main key name from names like "LeftArrow", "PageUp", etc.
        if 'Keypad' in name:
            # For keypad keys, extract the part after "Keypad "
            key_part = name.replace('Keypad ', '').split(' ')[0].lower()
            # Map common keypad names to their key
            if key_part in ('1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '.'):
                key_name = key_part
            elif key_part == 'enter':
                key_name = 'enter'
            elif key_part == 'backspace':
                key_name = 'backspace'
            elif key_part == 'plus':
                key_name = 'plus'
            elif key_part == 'minus':
                key_name = 'minus'
            elif key_part == 'multiply':
                key_name = 'multiply'
            elif key_part == 'divide':
                key_name = 'divide'
        elif 'Arrow' in name:
            # Arrow keys: LeftArrow, RightArrow, UpArrow, DownArrow
            if 'Left' in name:
                key_name = 'leftarrow'
            elif 'Right' in name:
                key_name = 'rightarrow'
            elif 'Up' in name:
                key_name = 'uparrow'
            elif 'Down' in name:
                key_name = 'downarrow'
        elif 'Page' in name:
            if 'Up' in name:
                key_name = 'pageup'
            elif 'Down' in name:
                key_name = 'pagedown'
        elif name in ('Insert', 'Home', 'End', 'Delete Forward', 'Delete'):
            key_name = name.lower().replace(' forward', '').replace(' ', '')
        elif name in ('ESCAPE', 'Enter', 'Tab', 'Spacebar', 'Backspace'):
            key_name = name.lower()
        elif name.startswith('F') and name[1:].isdigit():
            # Function keys F1-F24
            key_name = name.lower()

        if key_name:
            pc_key_to_usb_hid[key_name] = seq

    # Build Next key name -> USB HID scancode mapping
    # Next keys output USB HID scancodes directly
    next_key_to_usb_hid = {}
    for seq, name in MKEYBOARD_SCANCODES:
        if not seq:
            continue
        # The name is what the key is labeled, e.g., 'A', 'Z', etc.
        # Convert to lowercase for case-insensitive matching
        name_lower = name.lower()
        if isinstance(seq, tuple):
            next_key_to_usb_hid[name_lower] = seq[0]  # Use first scancode
        else:
            next_key_to_usb_hid[name_lower] = seq

    # Build reverse mappings
    usb_hid_to_next_key = {}
    for name, scancode in next_key_to_usb_hid.items():
        if isinstance(scancode, tuple):
            scancode = scancode[0]
        usb_hid_to_next_key[scancode] = name.upper()

    return {
        'usb_hid_scancode_to_name': usb_hid_scancode_to_name,
        'name_to_usb_hid_scancode': name_to_usb_hid_scancode,
        'next_scancode_to_name': next_scancode_to_name,
        'name_to_next_scancode': name_to_next_scancode,
        'pc_key_to_usb_hid': pc_key_to_usb_hid,
        'next_key_to_usb_hid': next_key_to_usb_hid,
        'usb_hid_to_next_key': usb_hid_to_next_key,
    }


def get_modifier_scancode(modifier_name, tables):
    """Get the scancode for a modifier key."""
    modifier_name = modifier_name.lower()

    if modifier_name in ('ctrl', 'control', 'ctl'):
        # Left Ctrl in USB HID
        return tables['name_to_usb_hid_scancode'].get('LeftControl')
    elif modifier_name == 'shift':
        # Right Shift (used as regular Shift in PICO-8 context)
        return tables['name_to_usb_hid_scancode'].get('RightShift')
    elif modifier_name == 'extend':
        # Right Shift (for EXTEND mode on Next)
        return (0x59, 0x94)

    return None


# Key aliases for common variations
KEY_ALIASES = {
    'left': 'leftarrow',
    'leftarrow': 'leftarrow',
    'leftarrowkey': 'leftarrow',
    'left cursor': 'leftarrow',
    'right': 'rightarrow',
    'rightarrow': 'rightarrow',
    'rightarrowkey': 'rightarrow',
    'right cursor': 'rightarrow',
    'up': 'uparrow',
    'uparrow': 'uparrow',
    'uparrowkey': 'uparrow',
    'up cursor': 'uparrow',
    'down': 'downarrow',
    'downarrow': 'downarrow',
    'downarrowkey': 'downarrow',
    'down cursor': 'downarrow',
    'home': 'home',
    'end': 'end',
    'pgup': 'pageup',
    'pgup': 'pageup',
    'pageup': 'pageup',
    'pgdn': 'pagedown',
    'pgdown': 'pagedown',
    'pagedown': 'pagedown',
    'ins': 'insert',
    'insert': 'insert',
    'del': 'delete',
    'delete': 'delete',
    'esc': 'escape',
    'escape': 'escape',
    'enter': 'enter',
    'return': 'enter',
    'backspace': 'backspace',
    'bs': 'backspace',
    'tab': 'tab',
    'space': 'space',
    'spacebar': 'space',
}


def parse_shortcut(shortcut, tables):
    """
    Parse a keyboard shortcut string.

    Accepts formats like: Ctrl+A, ctrl-a, A+Ctrl, case insensitive.
    Returns: (modifier_scancode, key_scancode, key_name, modifier_name)
    """
    shortcut = shortcut.strip()

    # Replace + with - for uniform parsing
    shortcut = shortcut.replace('+', '-')

    # Split by -
    parts = [p.strip() for p in shortcut.split('-') if p.strip()]

    if len(parts) < 1 or len(parts) > 2:
        raise ValueError(f"Invalid shortcut format: {shortcut}. Use 'Modifier+Key' or 'Modifier-Key'")

    modifier = None
    modifier_scancode = None
    key_part = None

    for part in parts:
        normalized = part.lower().strip()

        # Check if this is a modifier
        if normalized in ('ctrl', 'control', 'ctl'):
            modifier = 'Ctrl'
            modifier_scancode = get_modifier_scancode('ctrl', tables)
        elif normalized == 'shift':
            modifier = 'Shift'
            modifier_scancode = get_modifier_scancode('shift', tables)
        elif normalized == 'extend':
            modifier = 'EXTEND'
            modifier_scancode = get_modifier_scancode('extend', tables)
        else:
            key_part = part

    # If no parts were modifiers, the single part is the key
    if modifier is None and key_part is None and len(parts) == 1:
        key_part = parts[0]

    # Find the key
    key_scancode = None
    key_name = None

    if key_part:
        normalized_key = key_part.lower().strip()

        # Apply aliases for common variations
        if normalized_key in KEY_ALIASES:
            normalized_key = KEY_ALIASES[normalized_key]

        # Try lookup in PC keys
        if normalized_key in tables['pc_key_to_usb_hid']:
            key_scancode = tables['pc_key_to_usb_hid'][normalized_key]
            key_name = normalized_key
        # Try lookup in Next keys
        elif normalized_key in tables['next_key_to_usb_hid']:
            sc = tables['next_key_to_usb_hid'][normalized_key]
            key_scancode = sc[0] if isinstance(sc, tuple) else sc
            key_name = normalized_key

    if key_scancode is None:
        raise ValueError(f"Unknown key: {key_part}")

    return modifier_scancode, key_scancode, key_name, modifier


def convert_pc_to_next(modifier_scancode, key_scancode, key_name, modifier_name, tables):
    """
    Convert a PC keyboard shortcut to Spectrum Next equivalent.

    PC shortcuts use Left Ctrl. On Next, Ctrl+key combinations become EXTEND+key.

    Returns: (next_scancodes, next_key_names, display_strings)
    where display_strings is a list of (pc_display, next_display) tuples
    """
    displays = []

    # Handle arrow keys with Ctrl -> EXTEND mode
    if modifier_name == 'Ctrl':
        # Check if this is an arrow key
        if key_scancode == 0x50:  # Left arrow -> K + EXTEND
            displays.append(('Ctrl+Left', 'EXTEND+K'))
        elif key_scancode == 0x4f:  # Right arrow -> M + EXTEND
            displays.append(('Ctrl+Right', 'EXTEND+M'))
        elif key_scancode == 0x52:  # Up arrow -> I + EXTEND
            displays.append(('Ctrl+Up', 'EXTEND+I'))
        elif key_scancode == 0x51:  # Down arrow -> L + EXTEND
            displays.append(('Ctrl+Down', 'EXTEND+L'))
        elif key_scancode == 0x4a:  # Home -> O + EXTEND
            displays.append(('Ctrl+Home', 'EXTEND+O'))
        elif key_scancode == 0x4d:  # End -> N + EXTEND
            displays.append(('Ctrl+End', 'EXTEND+N'))
        elif key_scancode == 0x49:  # Page Up -> T + EXTEND
            displays.append(('Ctrl+PgUp', 'EXTEND+T'))
        elif key_scancode == 0x53:  # Page Down -> G + EXTEND
            displays.append(('Ctrl+PgDn', 'EXTEND+G'))
        elif key_scancode == 0x4b:  # Insert -> H + EXTEND
            displays.append(('Ctrl+Ins', 'EXTEND+H'))
        elif key_scancode == 0x4c:  # Delete -> P + EXTEND
            displays.append(('Ctrl+Del', 'EXTEND+P'))
        elif key_name.startswith('f') and key_name[1:].isdigit():
            # F1-F12 keys - no Next equivalent even with Ctrl (Ctrl+FX stays as FX on PC)
            displays.append((f'Ctrl+{key_name.upper()}', f'N/A (no Next equivalent)'))
        else:
            # Regular letter: Ctrl+X -> EXTEND + X
            displays.append((f'Ctrl+{key_name.upper()}', f'EXTEND+{key_name.upper()}'))

    elif modifier_name == 'Shift':
        # Shift+key -> SHIFT + key (same key on Next)
        displays.append((f'Shift+{key_name.upper()}', f'SHIFT+{key_name.upper()}'))

    else:
        # No modifier - same key on both for most keys
        # But some PC keys have different Next key names, and some don't exist on Next

        # Special case: Page Up/Down on PC = True Video/Inv Video on Next
        if key_scancode == 0x4b:  # Page Up -> True Video
            displays.append(('Page Up', 'True Video'))
        elif key_scancode == 0x4e:  # Page Down -> Inv Video
            displays.append(('Page Down', 'Inv Video'))
        elif key_name.startswith('f') and key_name[1:].isdigit():
            # F1-F12 keys - no Next equivalent
            displays.append((f'{key_name.upper()}', 'N/A (no Next equivalent)'))
        elif key_scancode in tables['next_scancode_to_name']:
            # Key exists on Next - show the Next key name
            next_key = tables['next_scancode_to_name'][key_scancode]
            displays.append((key_name.upper(), next_key))
        else:
            # Key doesn't exist on Next
            displays.append((key_name.upper(), 'N/A (no Next equivalent)'))

    # Return the scancodes and all display options
    return displays


def convert_next_to_pc(modifier_scancode, key_scancode, key_name, modifier_name, tables):
    """
    Convert a Spectrum Next keyboard shortcut to PC equivalent.

    EXTEND mode on Next = Ctrl on PC.
    Returns: list of (pc_display, next_display) tuples
    """
    displays = []

    if modifier_name == 'EXTEND':
        # EXTEND + key -> Ctrl + key
        displays.append((f'Ctrl+{key_name.upper()}', f'EXTEND+{key_name.upper()}'))

    elif modifier_name == 'Shift':
        # Shift + key -> Shift + key (same on PC)
        displays.append((f'Shift+{key_name.upper()}', f'SHIFT+{key_name.upper()}'))

    else:
        # No modifier - same on both
        displays.append((key_name.upper(), key_name.upper()))

    return displays


# Modifier priority order for display
MODIFIER_ORDER = ['Ctrl', 'EXTEND', 'Alt', 'Shift']


def format_modifier_display(modifiers):
    """
    Format multiple modifiers in the correct order: Ctrl, Extend, Alt, Shift.
    modifiers is a list of (scancode, display_name) tuples.
    Returns a formatted string like "Ctrl + SHIFT" or "EXTEND".
    """
    if not modifiers:
        return None

    # Sort by priority order
    sorted_mods = sorted(modifiers, key=lambda x: MODIFIER_ORDER.index(x[1]) if x[1] in MODIFIER_ORDER else 99)

    # Format the display string
    display_parts = [m[1] for m in sorted_mods]
    return ' + '.join(display_parts)


def format_scancode(scancode):
    """Format a scancode tuple for display."""
    if isinstance(scancode, tuple):
        return ' + '.join(f"0x{s:02x}" for s in scancode)
    return f"0x{scancode:02x}"


def list_all_mappings(tables):
    """Print all keyboard mappings."""
    print("USB HID Scancodes (common keys):")
    print("=" * 60)
    for code in range(4, 30):  # Letters a-z
        name = tables['usb_hid_scancode_to_name'].get(code, 'Unknown')
        print(f"  0x{code:02x}: {name}")

    print("\n\nNext Membrane Key Labels (scancodes):")
    print("=" * 60)
    # Group by row
    for row_name, keys in [
        ('Row 0', ['Caps Shift', 'Z', 'X', 'C', 'V', 'Extended', 'Up']),
        ('Row 1', ['A', 'S', 'D', 'F', 'G', 'Caps Lock', 'Graph']),
        ('Row 2', ['Q', 'W', 'E', 'R', 'T', 'Page Up', 'Page Down']),
        ('Row 3', ['1', '2', '3', '4', '5', 'Break', 'Edit']),
        ('Row 4', ['0', '9', '8', '7', '6', ';', '"']),
        ('Row 5', ['P', 'O', 'I', 'U', 'Y', ',', '.']),
        ('Row 6', ['Enter', 'L', 'K', 'J', 'H', 'Delete', 'Right']),
        ('Row 7', ['Space', 'Symbol Shift', 'M', 'N', 'B', 'Left', 'Down']),
    ]:
        print(f"\n{row_name}:")
        for key in keys:
            sc = tables['name_to_next_scancode'].get(key)
            if sc:
                if isinstance(sc, tuple):
                    sc_str = ' + '.join(f"0x{s:02x}" for s in sc)
                else:
                    sc_str = f"0x{sc:02x}"
                print(f"  {key}: {sc_str}")


def main():
    # Build the scancode tables
    tables = build_scancode_tables()

    parser = argparse.ArgumentParser(
        description='Convert keyboard shortcuts between PC and ZX Spectrum Next layouts.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python keyboard_convert.py --pc "Ctrl+A"
  python keyboard_convert.py --pc "ctrl-a"
  python keyboard_convert.py --pc "A+Ctrl"
  python keyboard_convert.py --next "A + EXTEND"
  python keyboard_convert.py --next "a+extend"
  python keyboard_convert.py --list
        """
    )

    parser.add_argument('--pc', metavar='SHORTCUT',
                        help='PC shortcut to convert (e.g., "Ctrl+A", "ctrl-a", "A+Ctrl")')
    parser.add_argument('--next', metavar='SHORTCUT',
                        help='Spectrum Next shortcut to convert (e.g., "A + EXTEND")')
    parser.add_argument('--list', action='store_true',
                        help='List all key mappings')

    args = parser.parse_args()

    if args.list:
        list_all_mappings(tables)
        return 0

    if args.pc and args.next:
        print("Error: Cannot specify both --pc and --next")
        return 1

    if args.pc:
        try:
            mod_sc, key_sc, key_name, mod_name = parse_shortcut(args.pc, tables)
            displays = convert_pc_to_next(mod_sc, key_sc, key_name, mod_name, tables)

            print(f"PC: {args.pc}")
            if mod_sc is not None:
                print(f"  -> PC modifiers: {format_scancode(mod_sc)}")
            else:
                print(f"  -> PC modifiers: (none)")
            print(f"  -> PC key: 0x{key_sc:02x} ({key_name})")
            print()
            print(f"Spectrum Next: {displays[0][1]}")
            print(f"  -> Next scancode: {format_scancode(mod_sc) if mod_sc else 'none'} + {format_scancode(key_sc)}")

            return 0
        except ValueError as e:
            print(f"Error: {e}")
            return 1

    if args.next:
        try:
            mod_sc, key_sc, key_name, mod_name = parse_shortcut(args.next, tables)
            displays = convert_next_to_pc(mod_sc, key_sc, key_name, mod_name, tables)

            print(f"Spectrum Next: {args.next}")
            if mod_sc is not None:
                print(f"  -> Next modifiers: {format_scancode(mod_sc)}")
            else:
                print(f"  -> Next modifiers: (none)")
            print(f"  -> Next key: {format_scancode(key_sc)}")
            print()
            print(f"PC: {displays[0][0]}")
            if mod_sc is not None:
                print(f"  -> PC scancode: {format_scancode(mod_sc)} + {format_scancode(key_sc)}")

            return 0
        except ValueError as e:
            print(f"Error: {e}")
            return 1

    parser.print_help()
    return 1


if __name__ == '__main__':
    sys.exit(main())
