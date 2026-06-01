#!/usr/bin/python3

try:
    # when run as a module
    from .ps2_scancodes import PS2_SCANCODES
    from .mkeyboard_scancodes import MKEYBOARD_SCANCODES
    from .usb_hid_scancodes import USB_HID_SCANCODES
    from .p8scii_keymap import P8SCII_KEYMAP
    from .spectrum_keymap import SPECTRUM_KEYMAP
except Exception:
    # when run as a script
    from ps2_scancodes import PS2_SCANCODES
    from mkeyboard_scancodes import MKEYBOARD_SCANCODES
    from usb_hid_scancodes import USB_HID_SCANCODES
    from p8scii_keymap import P8SCII_KEYMAP
    from spectrum_keymap import SPECTRUM_KEYMAP

import re
import os
import json

SHIFT=1
CTRL=2
ALT=4

def get_usb_hid_name_from_ps2_name(ps2_name):
    match ps2_name:
        case '` (back tick)':
            return 'Grave Accent and Tilde'
        case 'space':
            return 'Spacebar'
        case "'":
            return '‘ and “'
        case 'enter':
            return 'Return'
        case '\\':
            return '\\ and |'
        case 'backspace':
            return 'DELETE (Backspace)'
        case 'delete':
            return 'Delete Forward'
        case 'NumberLock':
            return 'Locking Num Lock'
    best_match = None
    for _, usb_name in USB_HID_SCANCODES:
        print(usb_name)
        if usb_name.lower() == ps2_name.lower():
            best_match = usb_name
            break
        if usb_name.lower() == ps2_name.lower().replace(' ', ''):
            best_match = usb_name
            break
        if usb_name.lower().replace(' ', '') == ps2_name.lower():
            best_match = usb_name
            break
        match = re.match(r'([^ ]+) and [^ ]+', usb_name)
        if match and match.group(1).lower() == ps2_name.lower():
            best_match = usb_name
            break
        match2 = re.match(r'([^ ]+) \([^)]+\)', ps2_name)
        if match2 and usb_name.lower() == match2.group(1).lower():
            best_match = usb_name
            break
        if match and match2 and match.group(1).lower() == match2.group(1).lower():
            best_match = usb_name
            break
        match3 = re.match(r'\(keypad\) (.+)', ps2_name)
        if match3 and usb_name.lower() == ('Keypad ' + match3.group(1)).lower():
            best_match = usb_name
            break
        match4 = re.match('Keypad (.+) and .+', usb_name)
        if match3 and match4 and match3.group(1).lower() == match4.group(1).lower():
            best_match = usb_name
            break
        match5 = re.match('cursor (.+)', ps2_name)
        if match5 and usb_name.lower() == (match5.group(1) + 'Arrow').lower():
            best_match = usb_name
            break
    if not ps2_name.startswith('(multimedia)' ) and not ps2_name.startswith('(ACPI) ') and ps2_name != 'apps':
        assert best_match, f'No USB HID match found for PS/2 name: >>{ps2_name}<<'
    return best_match

def get_unicode_from_usb_hid_name(keymod, usb_hid_name):
    match usb_hid_name:
        case 'Grave Accent and Tilde':
            return '~' if keymod & SHIFT else '`'
        case 'Tab':
            return '\t'
        case 'Spacebar':
            return ' '
        case '- and (underscore)':
            return '_' if keymod & SHIFT else '-'
        case '‘ and “':
            return '"' if keymod & SHIFT else "'"
        case 'DELETE (Backspace)':
            return '\b'
        case 'ESCAPE':
            return ''
        case 'Return (ENTER)' | 'Keypad ENTER':
            return '\n'
    match = re.match(r'(?:Keypad )?([^ ]+) and ([^ ]+)', usb_hid_name)
    if match:
        return match.group(2) if keymod & SHIFT else match.group(1)
    return None

def get_unicode_from_ps2_name(keymod, ps2_name):
    match ps2_name:
        case '` (back tick)':
            return '~' if keymod & SHIFT else '`'
        case '0 (zero)':
            return ')' if keymod & SHIFT else '0'
        case 'tab':
            return '\t'
        case 'enter' | '(keypad) enter':
            return '\n'
        case 'space':
            return ' '
        case 'backspace':
            return '\b'
        case '-':
            return '_' if keymod & SHIFT else '-'
        case '\'':
            return '"' if keymod & SHIFT else "'"
        case '6':
            return '^' if keymod & SHIFT else '6'
    usb_hid_name = get_usb_hid_name_from_ps2_name(ps2_name)
    if usb_hid_name:
        match = re.match(r'(?:Keypad )?([^ ]+) and ([^ ]+)', usb_hid_name)
        if match:
            unicode = match.group(2) if keymod & SHIFT else match.group(1)
            if len(unicode) == 1:
                return unicode
    return None

def get_ps2_name_from_spectrum_name(spectrum_name):
    match spectrum_name:
        case '0':
            return '0 (zero)'
    return spectrum_name

def get_ps2_scancode_from_nextp8_scancode(nextp8_scancode):
    if nextp8_scancode < 0x80:
        return (nextp8_scancode)
    else:
        return (0xe0, nextp8_scancode - 0x80)

ps2_scancode_to_name = {}
name_to_ps2_scancode = {}
ps2_name_to_usb_hid_name = {}
usb_hid_name_to_ps2_name = {}
ps2_keymap = [{}, {}]
for seq, name, released in PS2_SCANCODES:
    if released or not seq:
        continue
    ps2_scancode_to_name[seq] = name
    name_to_ps2_scancode[name] = seq
    usb_name = get_usb_hid_name_from_ps2_name(name)
    ps2_name_to_usb_hid_name[name] = usb_name
    usb_hid_name_to_ps2_name[usb_name] = name
    ps2_keymap[0][seq] = get_unicode_from_ps2_name(0, name)
    ps2_keymap[SHIFT][seq] = get_unicode_from_ps2_name(SHIFT, name)

usb_hid_scancode_to_name = {}
name_to_usb_hid_scancode = {}
usb_hid_keymap = [{}, {}]
for scancode, name in USB_HID_SCANCODES:
    if not scancode:
        continue
    usb_hid_scancode_to_name[scancode] = name
    name_to_usb_hid_scancode[name] = scancode
    usb_hid_keymap[0][scancode] = get_unicode_from_usb_hid_name(0, name)
    usb_hid_keymap[SHIFT][scancode] = get_unicode_from_usb_hid_name(SHIFT, name)

ps2_scancode_to_usb_hid_scancode = {}
usb_hid_scancode_to_ps2_scancode = {}
for seq, name, released in PS2_SCANCODES:
    if released or not seq:
        continue
    usb_hid_name = ps2_name_to_usb_hid_name.get(name)
    if not usb_hid_name:
        continue
    usb_scancode = name_to_usb_hid_scancode.get(usb_hid_name)
    if not usb_scancode:
        continue
    ps2_scancode_to_usb_hid_scancode[seq] = usb_scancode
    usb_hid_scancode_to_ps2_scancode[usb_scancode] = seq

# Load p8scii.json and build unicode <-> p8scii mappings
p8scii_path = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', 'data', 'p8scii.json'))
try:
    with open(p8scii_path, 'r', encoding='utf-8') as pf:
        p8scii_list = json.load(pf)
except Exception:
    p8scii_list = []

unicode_to_p8scii = {}
p8scii_to_unicode = {}
for idx, ch in enumerate(p8scii_list):
    if not ch:
        continue
    # map the unicode string to its p8scii index
    unicode_to_p8scii[str(ch)] = idx
    p8scii_to_unicode[int(idx)] = str(ch)

p8scii_keymap = [{}, {}]
for keymod in range(0, 2):
    for (scancode, unicode) in usb_hid_keymap[keymod].items():
        if unicode and len(unicode) == 1 and ord(unicode) >= ord('a') and ord(unicode) <= ord('z'):
            unicode = unicode.upper()
        p8scii = unicode_to_p8scii.get(unicode, None)
        p8scii_keymap[keymod][scancode] = p8scii

for unicode, keymod, p8scii in P8SCII_KEYMAP:
    usb_hid_name = ps2_name_to_usb_hid_name[unicode]
    scancode = name_to_usb_hid_scancode[usb_hid_name]
    p8scii_keymap[keymod][scancode] = p8scii

mkeyboard_scancode_to_name = {}
name_to_mkeyboard_scancode = {}
for scancode, key_name in MKEYBOARD_SCANCODES:
    mkeyboard_scancode_to_name[scancode] = key_name
    name_to_mkeyboard_scancode[key_name] = scancode

mkeyboard_keymap = [{}, {}, {}, {}]
for keymod in range(0, 2):
    for scancode, unicode in ps2_keymap[keymod].items():
        mkeyboard_keymap[keymod][scancode] = unicode
        mkeyboard_keymap[keymod | CTRL][scancode] = None

for (key_name, keymod, unicode) in SPECTRUM_KEYMAP:
    ps2_name = get_ps2_name_from_spectrum_name(key_name)
    ps2_scancode = name_to_ps2_scancode[ps2_name]
    if unicode == '↑':
        unicode = '^'
    elif unicode == '£':
        unicode = '$'
    if len(unicode) == 1 and ord(unicode) >= 32 and ord(unicode) <= 127:
        mkeyboard_keymap[keymod][ps2_scancode] = unicode

def main():
    # Build rows first so we can compute column widths for neat alignment
    rows = []
    for ps2_scancode, ps2_name, released in PS2_SCANCODES:
        if not ps2_scancode or released:
            continue
        usb_name = ps2_name_to_usb_hid_name[ps2_name]
        usb_hid_scancode = name_to_usb_hid_scancode.get(usb_name)
        usb_hid_name = usb_hid_scancode_to_name.get(usb_hid_scancode)
        unshifted = ps2_keymap[0].get(ps2_scancode)
        shifted = ps2_keymap[SHIFT].get(ps2_scancode)
        unshifted_memb = mkeyboard_keymap[0].get(ps2_scancode)
        shifted_memb = mkeyboard_keymap[SHIFT].get(ps2_scancode)
        ctrl_memb = mkeyboard_keymap[CTRL].get(ps2_scancode)
        shift_ctrl_memb = mkeyboard_keymap[SHIFT | CTRL].get(ps2_scancode)
        rows.append((repr(ps2_scancode),
                     repr(usb_hid_scancode),
                     repr(ps2_name),
                     repr(unshifted),
                     repr(shifted),
                     repr(unshifted_memb),
                     repr(shifted_memb),
                     repr(ctrl_memb),
                     repr(shift_ctrl_memb)))

    headers = ('PS/2 scancode', 'USB HID scancode', 'Key name', 'Unshifted', 'Shifted', 'Unshifted (memb)', 'Shift (memb)', 'Ctrl (memb)', 'Shift + Ctrl (memb)')
    # compute column widths
    widths = [len(h) for h in headers]
    for r in rows:
        for i, cell in enumerate(r):
            widths[i] = min(max(widths[i], len(cell)), 10)

    # print header
    header_line = ' | '.join(h.ljust(widths[i])[0:widths[i]] for i, h in enumerate(headers))
    sep_line = '-+-'.join('-' * widths[i] for i in range(len(widths)))
    print(header_line)
    print(sep_line)
    for r in rows:
        print(' | '.join(r[i].ljust(widths[i])[0:widths[i]] for i in range(len(r))))

    # PS/2 scancodes to USB HID / SDL2 scancodes:
    '''
    unsigned nextp8_scancode_to_sdl_scancode[NUM_SCANCODES] = {
        0, 66, 0, 62, 60, 58, 59, 69,     // F9, F5, F3, F1, F2, F12
        ...
    '''

    print('unsigned nextp8_scancode_to_sdl_scancode[NUM_SCANCODES] = {')
    row = []
    comment = []
    for nextp8_scancode in range(0, 256):
        ps2_scancode = get_ps2_scancode_from_nextp8_scancode(nextp8_scancode)
        usb_hid_scancode = ps2_scancode_to_usb_hid_scancode.get(ps2_scancode, 0)
        row.append(usb_hid_scancode)
        name = ps2_scancode_to_name.get(ps2_scancode, None)
        if name:
            name = name.replace('(keypad) ', 'KP ').replace('(multimedia) ', '').replace('(ACPI) ', '').replace('WWW ', '')
            comment.append(name)
        if len(row) == 8:
            code = ', '.join([str(s) for s in row]) + ','
            comment_str = ', '.join(comment)
            print(f'    {code:<32} // {comment_str}')
            row = []
            comment = []
    print('};')

    # PS/2 scancodes + modifiers to ASCII:

    '''
    char ps2_scancode_to_name[2][256] = {
        {
            ...
    '''
    print('char ps2_scancode_to_name[2][256] = {')
    row = []
    comment = []
    for keymod in range(0, 2):
        print(f'    // {"Shift" if keymod & SHIFT else "Unshifted"}')
        print('    {')
        for nextp8_scancode in range(0, 256):
            ps2_scancode = get_ps2_scancode_from_nextp8_scancode(nextp8_scancode)
            unicode = ps2_keymap[keymod].get(ps2_scancode)
            if not unicode:
                unicode = '\\0'
            else:
                assert len(unicode) == 1 and ord(unicode) <= 127
            if unicode == '\n':
                unicode = '\\n'
            elif unicode == '\t':
                unicode = '\\t'
            elif unicode == '\b':
                unicode = '\\b'
            row.append(unicode)
            name = ps2_scancode_to_name.get(ps2_scancode)
            if name:
                name = name.replace('(keypad) ', 'KP ').replace('(multimedia) ', '').replace('(ACPI) ', '').replace('WWW ', '')
                comment.append(name)
            if len(row) == 8:
                code = ', '.join([f"'{s}'" for s in row]) + ','
                comment_str = ', '.join(comment)
                print(f'        {code:<50} // {comment_str}')
                row = []
                comment = []
        print('    },')
    print('};')

    # nextp8 membrane scancodes (PS/2 scancodes) + modifiers to PS/2 scancodes + modifiers
    '''
    char memb_scancode_to_name[4][256] = {
        {
            ...
    '''
    print('char memb_scancode_to_name[4][256] = {')
    row = []
    comment = []
    for keymod in range(0, 4):
        print(f'    // {["Unshifted", "Shift", "Ctrl", "Shift + Ctrl"][keymod]}')
        print('    {')
        for nextp8_scancode in range(0, 256):
            ps2_scancode = get_ps2_scancode_from_nextp8_scancode(nextp8_scancode)
            unicode = mkeyboard_keymap[keymod].get(ps2_scancode)
            if not unicode:
                unicode = '\\0'
            else:
                assert len(unicode) == 1 and ord(unicode) <= 127
            if unicode == '\n':
                unicode = '\\n'
            elif unicode == '\t':
                unicode = '\\t'
            elif unicode == '\b':
                unicode = '\\b'
            row.append(unicode)
            name = ps2_scancode_to_name.get(ps2_scancode)
            if name:
                name = name.replace('(keypad) ', 'KP ').replace('(multimedia) ', '').replace('(ACPI) ', '').replace('WWW ', '')
                comment.append(name)
            if len(row) == 8:
                code = ', '.join([f"'{s}'" for s in row]) + ','
                comment_str = ', '.join(comment)
                print(f'        {code:<50} // {comment_str}')
                row = []
                comment = []
        print('    },')
    print('};')



if __name__ == '__main__':
    main()