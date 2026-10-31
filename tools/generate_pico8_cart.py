#!/usr/bin/env python3
"""
PICO-8 Lua Cart Generator

Generates random valid PICO-8 Lua scripts using the grammar from lua_pico8.lark
and outputs them as complete PICO-8 cartridges with header.

Usage:
    ./generate_pico8_cart.py [options] > output.p8

Options:
    --output FILE   Write to FILE instead of stdout
    --max-length N  Maximum number of Lua statements (default: 500)
    --max-depth N   Maximum expression depth (default: 4)
    --seed SEED     Random seed for reproducibility
    --lua-only      Output only the Lua code without cart header
    --width N       Approximate line width (for formatting)
    --calls-only    Generate only function calls with random args (no other statement types)

The generated carts include:
- PICO-8 cart header with version 43
- Random Lua code that respects variable scoping rules
- __gfx__ section with 128 lines of 64 hex characters each
- __gff__ section with 2 lines of 128 hex characters each
- __map__ section with 32 lines of 128 hex characters each
- __sfx__ section with 64 sound effect entries
- __music__ section with 64 music rows

Example usage:
    # Generate a cart with 100 Lua statements and output to file
    ./generate_pico8_cart.py --max-length 100 --output test.p8

    # Generate with a specific seed for reproducibility
    ./generate_pico8_cart.py --seed 42 --max-depth 3

    # Output only Lua code for testing
    ./generate_pico8_cart.py --lua-only --max-length 20
"""

import argparse
import random
import sys
import os
from typing import List, Set, Optional
import re


class VariableTracker:
    """Track which variables have been defined at each scope depth.

    This ensures that variables are only used after they have been assigned,
    following PICO-8 Lua scoping rules.
    """

    def __init__(self):
        # Each entry is a set of variable names defined at that depth level
        self.definitions: List[Set[str]] = [set()]  # depth 0 is global

    def enter_scope(self):
        """Enter a new scope (e.g., for loop, function body, do-block)."""
        self.definitions.append(set())

    def exit_scope(self):
        """Exit the current scope."""
        if len(self.definitions) > 1:
            self.definitions.pop()

    def get_definitions(self) -> Set[str]:
        """Get all currently visible variable definitions from all enclosing scopes."""
        result = set()
        for scope in self.definitions:
            result.update(scope)
        return result

    def define(self, name: str):
        """Mark a variable as defined in the current scope."""
        if self.definitions:
            self.definitions[-1].add(name)


# PICO-8 cart constants
PICO8_CART_HEADER = "pico-8 cartridge // http://www.pico-8.com\nversion 43\n"
LUA_SECTION_START = "__lua__\n"
GFX_SECTION_START = "__gfx__\n"
GFF_SECTION_START = "__gff__\n"
MAP_SECTION_START = "__map__\n"
SFX_SECTION_START = "__sfx__\n"
MUSIC_SECTION_START = "__music__\n"

# PICO-8 token categories
KEYWORDS = [
    "and", "break", "do", "else", "elseif", "end", "false", "for", "function",
    "goto", "if", "in", "local", "nil", "not", "or", "repeat", "return",
    "then", "true", "until", "while"
]

MATH_FUNCS = [
    "abs", "acos", "asin", "atan", "atan2", "ceil", "cos", "deg", "exp",
    "floor", "fmod", "frexp", "huge", "ldexp", "log", "log10", "max", "min",
    "modf", "pi", "pow", "rad", "random", "randomseed", "sin", "sqrt", "tan"
]

STRING_FUNCS = [
    "chr", "fmt", "gsub", "len", "lower", "match", "rep", "reverse", "str",
    "sub", "upper"
]

TABLE_FUNCS = [
    "concat", "foreach", "getn", "maxn", "insert", "remove", "sort"
]

SYSTEM_FUNCS = [
    "print", "printh", "printtable", "stat", "random"
]

# PICO-8 game functions (drawing, sprite, map, input, etc.)
GAME_FUNCS = [
    "camera", "circ", "circfill", "clip", "cls", "color", "cursor", "fget",
    "fillp", "flip", "holdframe", "fset", "line", "oval", "ovalfill", "pal",
    "palt", "pget", "pset", "rect", "rectfill", "rrect", "rrectfill", "sget",
    "spr", "sset", "sspr", "tline", "btn", "btnp", "_update_buttons", "browse",
    "music", "sfx", "map", "mget", "mset", "cstore", "memcpy", "memset", "peek",
    "peek2", "peek4", "poke", "poke2", "poke4", "reload", "rnd", "srand",
    "cartdata", "dget", "dset", "sub", "t", "time", "menuitem", "extcmd",
    "load", "reset", "run", "serial", "_set_fps", "save", "ls", "reboot", "cd",
    "mkdir", "new", "stat", "stop", "trace"
]

MATH_CONSTS = ["pi", "huge"]

SYSTEM_VARS = [
    "_G", "_VERSION", "arg", "stat", "width", "height", "time", "ticks", "mem",
    "peek", "poke", "peek4", "poke4", "sget", "sset", "pget", "pset", "btn",
    "btnp", "btnh", "btnph", "key", "keyh", "mouse", "mouseb", "srand"
]

# PICO-8 operators
BINOPS = [
    "+", "-", "*", "/", "%", "^", "&", "|", "\\", "<<", ">>", ">>>",
    "==", "~=", "!=", "<", ">", "<=", ">=", ".."
]

UNOPS = ["-", "not", "#"]

# Lua naming pattern for identifiers
NAME_PATTERN = r"[a-zA-Z_][a-zA-Z0-9_]*"


def random_name(rng: Optional[random.Random] = None, prefix: str = "") -> str:
    """Generate a random variable/function name that is not a PICO-8 keyword."""
    if rng is None:
        rng = random.Random()
    length = rng.randint(3, 8)
    chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_"
    name = prefix + "".join(rng.choice(chars) for _ in range(length))
    # Ensure it's not a keyword and doesn't start with a keyword
    if name in KEYWORDS or any(name.startswith(kw) for kw in KEYWORDS):
        name = name + "_"
    return name


def random_int(rng: Optional[random.Random] = None, min_val: int = -32768, max_val: int = 32767) -> int:
    """Generate a random integer within PICO-8 range (-32768 to 32767)."""
    if rng is None:
        rng = random.Random()
    return rng.randint(min_val, max_val)


def random_float(rng: Optional[random.Random] = None) -> float:
    """Generate a random PICO-8 float (fixed-point with 1 decimal place)."""
    if rng is None:
        rng = random.Random()
    return round(rng.uniform(-32768, 32767.5), 1)


def random_hex(rng: Optional[random.Random] = None) -> str:
    """Generate a random hex number literal (e.g., 0x1a2b)."""
    if rng is None:
        rng = random.Random()
    length = rng.randint(2, 8)
    return "0x" + "".join(rng.choice("0123456789abcdef") for _ in range(length))


def random_bin(rng: Optional[random.Random] = None) -> str:
    """Generate a random binary number literal (e.g., 0b1010)."""
    if rng is None:
        rng = random.Random()
    length = rng.randint(2, 16)
    return "0b" + "".join(rng.choice("01") for _ in range(length))


def random_number(rng: Optional[random.Random] = None) -> str:
    """Generate a random number literal in various formats."""
    if rng is None:
        rng = random.Random()
    choice = rng.choice(["int", "float", "hex", "bin"])
    if choice == "int":
        return str(random_int(rng))
    elif choice == "float":
        return str(random_float(rng))
    elif choice == "hex":
        return random_hex(rng)
    else:
        return random_bin(rng)


def random_string(rng: Optional[random.Random] = None, max_length: int = 32) -> str:
    """Generate a random string literal with escaped special characters."""
    if rng is None:
        rng = random.Random()
    length = rng.randint(0, max_length)
    chars = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_!?,.:;/\\|()[]{}<>{}@#$%&*+=^~"
    content = "".join(rng.choice(chars) for _ in range(length))
    # Escape special characters for Lua string
    content = content.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
    return f'"{content}"'


def random_table(rng: Optional[random.Random] = None, depth: int = 0, max_depth: int = 3) -> str:
    """Generate a random table constructor."""
    if rng is None:
        rng = random.Random()
    if depth >= max_depth:
        return "{}"

    num_fields = rng.randint(0, 4)
    fields = []

    for _ in range(num_fields):
        if rng.choice([1, 2, 3]) == 1:
            # Key-value pair
            key = random_int(rng)
            value = random_exp(rng, depth, max_depth)
            fields.append(f"[{key}] = {value}")
        else:
            # Value only (simpler)
            fields.append(random_exp(rng, depth, max_depth))

    return "{" + ", ".join(fields) + "}"


def random_simple_value(rng: Optional[random.Random] = None, depth: int = 0, max_depth: int = 2) -> str:
    """Generate a simple value: nil, string, numerical (hex format), bool, or table.
    
    This is used for --calls-only mode where we want simple arguments,
    not complex nested expressions.
    
    Tables are nested up to max_depth (default 2).
    """
    if rng is None:
        rng = random.Random()
    
    choice = rng.choice(["nil", "string", "hex", "bool", "table"])
    
    if choice == "nil":
        return "nil"
    elif choice == "bool":
        return rng.choice(["true", "false"])
    elif choice == "hex":
        # Hex format like 0x1234.5678
        # Generate a hex integer and optionally add fractional part
        integer_part = rng.randint(0, 0xffffffff)
        # 50% chance to add fractional part
        if rng.random() < 0.5:
            fractional_part = rng.randint(0, 0xffff)
            return f"0x{integer_part:x}.{fractional_part:x}"
        else:
            return f"0x{integer_part:x}"
    elif choice == "table":
        return random_table_simple(rng, depth, max_depth)
    else:  # string
        return random_string(rng)


def random_table_simple(rng: Optional[random.Random] = None, depth: int = 0, max_depth: int = 2) -> str:
    """Generate a random table constructor with simple values (no nested expressions).
    
    Tables are nested up to max_depth (default 2).
    """
    if rng is None:
        rng = random.Random()
    
    if depth >= max_depth:
        # At max depth, return empty table
        return "{}"
    
    num_fields = rng.randint(0, 4)
    fields = []
    
    for _ in range(num_fields):
        if rng.choice([1, 2, 3]) == 1:
            # Key-value pair
            key = rng.randint(-32768, 32767)
            value = random_simple_value(rng, depth + 1, max_depth)
            fields.append(f"[{key}] = {value}")
        else:
            # Value only (simpler)
            fields.append(random_simple_value(rng, depth + 1, max_depth))
    
    return "{" + ", ".join(fields) + "}"


def random_exp(
    rng: Optional[random.Random] = None,
    depth: int = 0,
    max_depth: int = 3,
    vars: Optional[VariableTracker] = None
) -> str:
    """Generate a random expression.

    Args:
        rng: Random number generator
        depth: Current expression depth
        max_depth: Maximum allowed expression depth
        vars: Variable tracker to ensure variables are only used after assignment
    """
    if rng is None:
        rng = random.Random()
    if vars is None:
        vars = VariableTracker()

    if depth >= max_depth:
        # Base case: use existing variables or simple literals
        definitions = vars.get_definitions()
        if definitions and rng.random() < 0.5:
            return rng.choice(list(definitions))
        else:
            return rng.choice([
                "nil", "true", "false",
                random_number(rng),
                random_string(rng)
            ])

    choice = rng.choice([
        "value", "binop", "unop", "call", "table", "paren"
    ])

    if choice == "value":
        definitions = vars.get_definitions()
        if definitions and rng.random() < 0.4:
            return rng.choice(list(definitions))
        else:
            return rng.choice([
                "nil", "true", "false",
                random_number(rng),
                random_string(rng)
            ])

    elif choice == "binop":
        left = random_exp(rng, depth + 1, max_depth, vars)
        op = rng.choice(BINOPS)
        right = random_exp(rng, depth + 1, max_depth, vars)
        # Wrap sub-expressions in parentheses to avoid precedence issues
        return f"({left}) {op} ({right})"

    elif choice == "unop":
        op = rng.choice(UNOPS)
        exp = random_exp(rng, depth + 1, max_depth, vars)
        # Wrap the expression in parentheses to avoid precedence issues
        return f"{op} ({exp})"

    elif choice == "call":
        func = rng.choice(
            SYSTEM_FUNCS + MATH_FUNCS + STRING_FUNCS + TABLE_FUNCS + GAME_FUNCS
        )
        args = random_args(rng, depth + 1, max_depth, vars)
        return f"{func}({args})"

    elif choice == "table":
        # Wrap table constructor in parentheses to make it a valid suffixedexp
        return f"({random_table(rng, depth, max_depth)})"

    elif choice == "paren":
        exp = random_exp(rng, depth + 1, max_depth, vars)
        return f"({exp})"


def random_args(
    rng: Optional[random.Random] = None,
    depth: int = 0,
    max_depth: int = 3,
    vars: Optional[VariableTracker] = None,
    simple_values: bool = False
) -> str:
    """Generate a random argument list for function calls.
    
    Args:
        rng: Random number generator
        depth: Current expression depth (unused if simple_values=True)
        max_depth: Maximum expression depth (unused if simple_values=True)
        vars: Variable tracker (unused if simple_values=True)
        simple_values: If True, use simple values (nil/string/hex) instead of full expressions
    """
    if rng is None:
        rng = random.Random()
    
    if simple_values:
        num_args = rng.randint(0, 7)
        args = [random_simple_value(rng, max_depth=2) for _ in range(num_args)]
    else:
        if vars is None:
            vars = VariableTracker()
        num_args = rng.randint(0, 8)
        args = [random_exp(rng, depth, max_depth, vars) for _ in range(num_args)]

    return ", ".join(args)


def indent_block(block: str, depth: int) -> str:
    """Indent each line of a block by the specified depth (4 spaces per depth level)."""
    indent = "    " * depth
    lines = block.split("\n")
    indented_lines = [indent + line if line.strip() else line for line in lines]
    return "\n".join(indented_lines)


def random_statement(
    rng: Optional[random.Random] = None,
    depth: int = 0,
    max_depth: int = 4,
    vars: Optional[VariableTracker] = None
) -> str:
    """Generate a random Lua statement.

    Args:
        rng: Random number generator
        depth: Current statement depth
        max_depth: Maximum allowed statement depth
        vars: Variable tracker to ensure variables are only used after assignment
    """
    if rng is None:
        rng = random.Random()
    if vars is None:
        vars = VariableTracker()

    if depth >= max_depth:
        # At max depth, use existing variables or simple return
        definitions = vars.get_definitions()
        if definitions:
            var = rng.choice(list(definitions))
            return f"{var} = nil"
        else:
            return "return"

    choice = rng.choice([
        "block", "while", "repeat", "if", "for_num", "for_in", "func_def",
        "local_assign", "print", "assignment", "expr"
    ])

    if choice == "block":
        vars.enter_scope()
        num_stmts = rng.randint(1, 3)
        stmts = [random_statement(rng, depth + 1, max_depth, vars) for _ in range(num_stmts)]
        vars.exit_scope()
        return "do\n" + "\n".join(indent_block(s, 1) for s in stmts) + "\nend"

    elif choice == "while":
        cond = random_exp(rng, depth + 1, max_depth, vars)
        vars.enter_scope()
        block = random_statement(rng, depth + 1, max_depth, vars)
        vars.exit_scope()
        return f"while {cond} do\n{indent_block(block, 1)}\nend"

    elif choice == "repeat":
        vars.enter_scope()
        block = random_statement(rng, depth + 1, max_depth, vars)
        cond = random_exp(rng, depth + 1, max_depth, vars)
        vars.exit_scope()
        return f"repeat\n{indent_block(block, 1)}\nuntil {cond}"

    elif choice == "if":
        cond = random_exp(rng, depth + 1, max_depth, vars)
        then_block = random_statement(rng, depth + 1, max_depth, vars)
        statements = [f"if {cond} then\n{indent_block(then_block, 1)}"]

        num_elseif = rng.randint(0, 2)
        for _ in range(num_elseif):
            elseif_cond = random_exp(rng, depth + 1, max_depth, vars)
            elseif_block = random_statement(rng, depth + 1, max_depth, vars)
            statements.append(f"elseif {elseif_cond} then\n{indent_block(elseif_block, 1)}")

        if rng.choice([True, False]):
            else_block = random_statement(rng, depth + 1, max_depth, vars)
            statements.append(f"else\n{indent_block(else_block, 1)}")

        statements.append("end")
        return "\n".join(statements)

    elif choice == "for_num":
        var = random_name(rng)
        # Define the loop variable in the current scope
        vars.define(var)
        start = random_int(rng)
        stop = random_int(rng, start, start + 100)
        step = rng.choice(["", f", {random_int(rng, 1, 10)}"])
        vars.enter_scope()
        block = random_statement(rng, depth + 1, max_depth, vars)
        vars.exit_scope()
        return f"for {var} = {start}, {stop}{step} do\n{indent_block(block, 1)}\nend"

    elif choice == "for_in":
        var1 = random_name(rng)
        var2 = random_name(rng)
        # Generate iterator expression before defining loop variables
        # so it only uses outer scope variables
        explist = random_exp(rng, depth + 1, max_depth, vars)
        vars.define(var1)
        vars.define(var2)
        vars.enter_scope()
        block = random_statement(rng, depth + 1, max_depth, vars)
        vars.exit_scope()
        return f"for {var1}, {var2} in {explist} do\n{indent_block(block, 1)}\nend"

    elif choice == "func_def":
        name = random_name(rng)
        # Define the function name in current scope
        vars.define(name)
        parlist = random_parlist(rng, vars)
        vars.enter_scope()
        block = random_statement(rng, depth + 1, max_depth, vars)
        vars.exit_scope()
        return f"function {name}({parlist})\n{indent_block(block, 1)}\nend"

    elif choice == "local_assign":
        if rng.choice([1, 2]) == 1:
            # With initialization - define variables and assign
            num_vars = rng.randint(1, 3)
            var_names = [random_name(rng) for _ in range(num_vars)]
            for v in var_names:
                vars.define(v)
            values = random_exp(rng, depth + 1, max_depth, vars)
            return f"local {', '.join(var_names)} = {values}"
        else:
            # Just local declaration without initialization
            num_vars = rng.randint(1, 3)
            var_names = [random_name(rng) for _ in range(num_vars)]
            for v in var_names:
                vars.define(v)
            return f"local {', '.join(var_names)}"

    elif choice == "print":
        args = random_args(rng, depth + 1, max_depth, vars)
        # Ensure at least one argument for print shorthand
        if not args:
            args = random_exp(rng, depth + 1, max_depth, vars)
        return f"? {args}"

    elif choice == "assignment":
        # Use existing variables for assignment
        definitions = vars.get_definitions()
        if definitions:
            num_vars = rng.randint(1, min(3, len(definitions)))
            varlist = ", ".join(rng.choice(list(definitions)) for _ in range(num_vars))
        else:
            # No variables defined yet, create one
            var = random_name(rng)
            vars.define(var)
            varlist = var
        explist = random_exp(rng, depth + 1, max_depth, vars)
        return f"{varlist} = {explist}"

    else:  # expr_stat - only generate valid statement expressions (function calls, method calls, etc.)
        # Generate a function call (which is a valid statement)
        # Note: bare variable references are not valid statements in femto8
        func = rng.choice(
            SYSTEM_FUNCS + MATH_FUNCS + STRING_FUNCS + TABLE_FUNCS + GAME_FUNCS
        )
        args = random_args(rng, depth + 1, max_depth, vars)
        return f"{func}({args})"


def random_parlist(
    rng: Optional[random.Random] = None,
    vars: Optional[VariableTracker] = None
) -> str:
    """Generate a random parameter list for function definitions."""
    if rng is None:
        rng = random.Random()
    if vars is None:
        vars = VariableTracker()

    choice = rng.choice(["none", "names", "names_vararg", "vararg"])

    if choice == "none":
        return ""
    elif choice == "names":
        num_params = rng.randint(1, 3)
        params = [random_name(rng) for _ in range(num_params)]
        for p in params:
            vars.define(p)
        return ", ".join(params)
    elif choice == "names_vararg":
        num_params = rng.randint(1, 2)
        params = [random_name(rng) for _ in range(num_params)]
        for p in params:
            vars.define(p)
        return ", ".join(params) + ", ..."
    else:
        return "..."


def random_chunk(
    rng: Optional[random.Random] = None,
    max_statements: int = 50,
    max_depth: int = 4,
    vars: Optional[VariableTracker] = None,
    calls_only: bool = False
) -> str:
    """Generate a random Lua chunk (sequence of statements).
    
    Args:
        rng: Random number generator
        max_statements: Maximum number of Lua statements to generate
        max_depth: Maximum expression depth
        vars: Variable tracker to ensure variables are only used after assignment
        calls_only: If True, generate only function calls as statements
    """
    if rng is None:
        rng = random.Random()
    if vars is None:
        vars = VariableTracker()

    if calls_only:
        # In calls-only mode, always generate exactly max_statements function calls
        num_stmts = max_statements
    else:
        # In normal mode, generate random number of statements between 1 and max_statements
        num_stmts = rng.randint(1, max_statements)

    statements = []

    for i in range(num_stmts):
        if calls_only:
            # Generate only function calls with simple arguments
            func = rng.choice(
                SYSTEM_FUNCS + MATH_FUNCS + STRING_FUNCS + TABLE_FUNCS + GAME_FUNCS
            )
            args = random_args(rng, 0, max_depth, vars, simple_values=True)
            stmt = f"{func}({args})"
        else:
            stmt = random_statement(rng, 0, max_depth, vars)
        statements.append(stmt)

    return "\n".join(statements)


def generate_gfx_section(rng: Optional[random.Random] = None) -> str:
    """Generate a random __gfx__ section (128 lines of 64 hex chars each)."""
    if rng is None:
        rng = random.Random()

    lines = []
    for _ in range(128):
        line = "".join(rng.choice("0123456789abcdef") for _ in range(64))
        lines.append(line)
    return "\n".join(lines)


def generate_gff_section(rng: Optional[random.Random] = None) -> str:
    """Generate a random __gff__ section (2 lines of 128 hex chars each)."""
    if rng is None:
        rng = random.Random()

    lines = []
    for _ in range(2):
        line = "".join(rng.choice("0123456789abcdef") for _ in range(128))
        lines.append(line)
    return "\n".join(lines)


def generate_map_section(rng: Optional[random.Random] = None) -> str:
    """Generate a random __map__ section (32 lines of 128 hex chars each)."""
    if rng is None:
        rng = random.Random()

    lines = []
    for _ in range(32):
        line = "".join(rng.choice("0123456789abcdef") for _ in range(128))
        lines.append(line)
    return "\n".join(lines)


def generate_sfx_section(rng: Optional[random.Random] = None) -> str:
    """Generate a random __sfx__ section (64 sound effect entries)."""
    if rng is None:
        rng = random.Random()

    lines = []
    for _ in range(64):
        # Format: editor_mode speed loop_start loop_end + 16 notes * 5 bytes
        entry = "00000000"
        for _ in range(16):
            entry += "".join(rng.choice("0123456789abcdef") for _ in range(5))
        lines.append(entry)
    return "\n".join(lines)


def generate_music_section(rng: Optional[random.Random] = None) -> str:
    """Generate a random __music__ section (64 music rows)."""
    if rng is None:
        rng = random.Random()

    lines = []
    for _ in range(64):
        # Format: flags byte1 byte2 byte3 byte4
        flags = rng.randint(0, 15)
        bytes_ = [rng.randint(0, 127) for _ in range(4)]
        line = f"{flags:02x} {bytes_[0]:02x} {bytes_[1]:02x} {bytes_[2]:02x} {bytes_[3]:02x}"
        lines.append(line)
    return "\n".join(lines)


def generate_cart(
    rng: Optional[random.Random] = None,
    max_lua_statements: int = 50,
    max_depth: int = 4,
    lua_only: bool = False,
    calls_only: bool = False,
    seed: Optional[int] = None
) -> str:
    """Generate a complete PICO-8 cartridge.

    Args:
        rng: Random number generator
        max_lua_statements: Maximum number of Lua statements to generate
        max_depth: Maximum expression depth
        lua_only: If True, output only Lua code without cart header and data sections
        calls_only: If True, generate only function calls as statements
        seed: Random seed for reproducibility (included in cart header)

    Returns:
        A complete PICO-8 cartridge as a string
    """
    if rng is None:
        rng = random.Random()

    vars = VariableTracker()

    # Generate Lua code
    lua_code = random_chunk(rng, max_lua_statements, max_depth, vars, calls_only)

    if lua_only:
        return lua_code

    # Build the cart with all required sections
    # Add seed comment to cart header if provided
    cart = PICO8_CART_HEADER
    if seed is not None:
        cart = f"pico-8 cartridge // http://www.pico-8.com\nversion 43 // seed={seed}\n"
    else:
        cart = PICO8_CART_HEADER
    cart += LUA_SECTION_START
    cart += lua_code
    if not lua_code.endswith("\n"):
        cart += "\n"
    cart += GFX_SECTION_START
    cart += generate_gfx_section(rng)
    cart += "\n" + GFF_SECTION_START
    cart += generate_gff_section(rng)
    cart += "\n" + MAP_SECTION_START
    cart += generate_map_section(rng)
    cart += "\n" + SFX_SECTION_START
    cart += generate_sfx_section(rng)
    cart += "\n" + MUSIC_SECTION_START
    cart += generate_music_section(rng)
    cart += "\n"

    return cart


def main():
    parser = argparse.ArgumentParser(
        description="Generate random PICO-8 Lua cartridges",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument("--output", "-o", help="Output file (default: stdout)")
    parser.add_argument("--max-length", "-l", type=int, default=500,
                       help="Maximum number of Lua statements (default: 500)")
    parser.add_argument("--max-depth", "-d", type=int, default=4,
                       help="Maximum expression depth (default: 4)")
    parser.add_argument("--seed", "-s", type=int, default=None,
                       help="Random seed for reproducibility")
    parser.add_argument("--lua-only", action="store_true",
                       help="Output only Lua code without cart header")
    parser.add_argument("--width", "-w", type=int, default=80,
                       help="Approximate line width (for formatting)")
    parser.add_argument("--calls-only", action="store_true",
                       help="Generate only function calls with random args")

    args = parser.parse_args()

    # Set up random generator - if seed is None, generate one now for reproducibility
    if args.seed is None:
        args.seed = random.randint(0, 2**32 - 1)
    rng = random.Random(args.seed)

    # Generate the cart with seed in header
    cart = generate_cart(
        rng=rng,
        max_lua_statements=args.max_length,
        max_depth=args.max_depth,
        lua_only=args.lua_only,
        calls_only=args.calls_only,
        seed=args.seed
    )

    # Output
    if args.output:
        with open(args.output, "w") as f:
            f.write(cart)
    else:
        sys.stdout.write(cart)


if __name__ == "__main__":
    main()
