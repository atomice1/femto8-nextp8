#!/usr/bin/env python3
"""Generate extern header for p8_editor unit tests."""

import re
import sys
from pathlib import Path


def extract_declarations(filepath: Path) -> list[str]:
    """Extract extern declarations from a C source file."""
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Remove comments (both /* */ and // style)
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
    content = re.sub(r'//.*$', '', content, flags=re.MULTILINE)
    
    declarations = []
    
    # Pattern to match static variable declarations
    # Matches: static type name [= initializer];
    # Handles multi-line declarations
    
    # Split into logical lines (join lines that don't end with)
    lines = content.split('\n')
    logical_lines = []
    current_line = ""
    in_decl = False
    in_type = False
    braces = 0
    
    for line in lines:
        stripped = line.strip()

        if not in_decl:
            # Skip if doesn't start with static, struct, union or typedef
            if not re.match(r'^\s*(static|struct|union|typedef)\s+', line):
                if re.match(r'^\s*#\s*define', line):
                    logical_lines.append(stripped)
                continue
            in_decl = True
            if re.match(r'^\s*(|struct|union|typedef)\s+', line):
                in_type = True

        # Skip preprocessor directives
        if re.match(r'^\s*#', line):
            continue
        
        # Skip empty lines
        if not stripped:
            continue
        
        # Join continuation lines
        if current_line:
            current_line += ' ' + stripped
        else:
            current_line = stripped

        if in_type:
            braces += stripped.count('{') - stripped.count('}')

        # If line ends with ;, it's complete
        if re.search(r';\s*$', current_line) and (not in_type or braces == 0):
            logical_lines.append(current_line)
            current_line = ""
            in_decl = False
            in_type = False
            braces = 0
    
    # Process each logical line
    for line in logical_lines:
        line = line.strip()

        if re.match(r'^\s*#\s*define', line):
            declarations.append(f"{line}")
            continue

        # Skip if doesn't start with static, struct, union or typedef
        if not re.match(r'^\s*(static|struct|union|typedef)\s+', line):
            continue

        # Check for function
        is_function = re.search(r'\w+\s*\(', line)
        is_enum = re.search(r'\benum\b', line)
        is_type = re.match(r'^\s*(|struct|union|typedef)\s+', line)

        if is_function:
            # Remove body
            line = re.sub(r'\s*{.*$', '', line)
        elif not is_enum:
            # Remove initializer
            line = re.sub(r'\s*=.*$', '', line)
        
        # Remove "static " prefix
        line = re.sub(r'^static\s+', '', line)

        # Remove inline keyword
        line = re.sub(r'\binline\b\s*', '', line)

        # Remove ; suffix
        line = re.sub(r';\s*$', '', line)
        
        # Skip empty lines
        if not line:
            continue
        
        # Output as extern declaration
        if is_type:
            # For typedefs, structs, unions, output as is
            declarations.append(f"{line};")
        else:
            declarations.append(f"extern {line};")
    
    return declarations


def main():
    if len(sys.argv) < 2:
        print("Usage: generate_test_header.py output_file [source_files...]", file=sys.stderr)
        sys.exit(1)
    
    output_file = Path(sys.argv[1])
    source_files = sys.argv[2:]
    
    all_declarations = []
    
    for src_file in source_files:
        src_path = Path(src_file)
        if src_path.exists():
            decls = extract_declarations(src_path)
            all_declarations.extend(decls)
    
    # Write output
    with open(output_file, 'w') as f:
        f.write("/* Auto-generated extern declarations for p8_editor static symbols */\n\n")
        f.write("#ifndef P8_EDITOR_TEST_IMPL_H\n")
        f.write("#define P8_EDITOR_TEST_IMPL_H\n\n")
        
        f.write("\n/* Include types from other headers */\n")
        f.write('#include "p8_editor_syntax.h"\n')
        f.write('#include "p8_dialog.h"\n\n')

        for decl in all_declarations:
            f.write(f"{decl}\n")
        
        f.write("\n#endif /* P8_EDITOR_TEST_IMPL_H */\n")
    
    print(f"Generated {output_file}")


if __name__ == "__main__":
    main()
