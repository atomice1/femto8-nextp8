-- test_print_scroll_lib.lua
-- Shared case-generation logic for print scroll tests.
-- Both gen_print_scroll.p8 (generator) and test_print_scroll.p8 (test)
-- #include this file and call srand(PSCROLL_SEED) before
-- pscroll_generate_cases() to reproduce the same sequence.

PSCROLL_SEED = 0x4321
PSCROLL_N    = 256  -- total test cases

-- ── string pools ─────────────────────────────────────────────────────────────
-- Order must never change (it is part of the deterministic test spec).

local _plain = {
    "a",
    "hi",
    "hello",
    "abcdefghij",
    "abcdefghijklmnopqrst",
    "hi\0",
    "a\nb\0",
    "abc\n\0",
}

local _p8scii = {
    "\^wa\0",           -- wide char, no auto-newline
    "\^ta\0",           -- tall char, no auto-newline
    "\^w\^ta\0",        -- wide+tall, no auto-newline
    "\*5x\0",           -- repeat 'x' five times, no auto-newline
    "\^x8ab\0",         -- custom char-width=8, two chars
    "\^y8a\0",          -- custom char-height=8, one char
    "\^x4hi\0",         -- custom char-width=4 (same as default)
}

-- strings that work correctly with the uniform 8x8 custom font
local _font_u = {"A", "AB", "B", "A\0", "AB\0", "B\0"}

-- strings that work with the variable-width custom font (A=6px, I=4px)
local _font_v = {"A", "AI", "AII", "AIIA", "A\0", "AI\0"}

-- ── other pools ──────────────────────────────────────────────────────────────

local _cursors = {
    {h,   116},
    {120, 116},
    {60,  118},
    {120, 118},
    {60,  120},
    {120, 120},
    {60,  122},
    {120, 122},
    {60,  124},
    {120, 124},
    {60,  126},
    {120, 126}
}

local _margins = {0, 8, 20, 0}   -- 0 appears twice for higher weight

local _miscs = {
    0,
    0x80,           -- enable character wrap
    0x40,           -- disable auto-scroll
    0x04,           -- no newlines after print()
    0x80|0x40,      -- wrap + no scroll
    0x80|0x04,      -- wrap + no newline
    0,              -- extra weight on default
}

local _xys = {
    {0,   0},
    {10,  20},
    {60,  60},
    {0,   116},
    {100, 116},
}

-- ── font helpers ─────────────────────────────────────────────────────────────

function pscroll_setup_font_uniform()
    -- 8×8 uniform-advance custom font.
    -- Defines glyphs for 'A' (65) and 'B' (66); all others are blank.
    memset(0x5600, 0, 128 + 240*8)
    poke(0x5600, 8, 8, 8)   -- char_w=8, char_h=8, cell_w=8
    local ga = 0x5600 + 128 + (65-16)*8
    poke(ga, 24, 36, 66, 66, 126, 66, 66, 0)
    local gb = 0x5600 + 128 + (66-16)*8
    poke(gb, 126, 66, 66, 62, 66, 66, 126, 0)
    poke(0x5f58, 0x81)
end

function pscroll_setup_font_varwidth()
    -- 8×8 variable-width custom font.
    -- 'A' (65) uses the default cell_w advance; 'I' (73) has a narrower
    -- per-character width stored in the font header's variable-width table.
    memset(0x5600, 0, 128 + 240*8)
    poke(0x5600, 8, 8, 8, 0, 0, 1)   -- variable_w_flag = 1
    poke(0x5600 + 36, 0x40)           -- variable width entry for 'I'
    local ga = 0x5600 + 128 + (65-16)*8
    poke(ga, 24, 36, 66, 66, 126, 66, 66, 0)
    local gi = 0x5600 + 128 + (73-16)*8
    poke(gi, 126, 24, 24, 24, 24, 24, 126, 0)
    poke(0x5f58, 0x81)
end

-- ── case generation ──────────────────────────────────────────────────────────

local function _pick(t, r)
    -- Pick from table t using pre-generated random value r in [0,1).
    return t[flr(r * #t) + 1]
end

-- Generate the list of test cases.
-- Caller must call srand(PSCROLL_SEED) immediately before this function.
-- Uses exactly 7 rnd() calls per case so the sequence is stable.
function pscroll_generate_cases()
    local cases = {}
    for i = 1, PSCROLL_N do
        -- call 1 ─ font kind: 0=none, 1=uniform 8×8, 2=variable-width
        local r1 = rnd()
        local font_kind
        if r1 < 0.15 then
            font_kind = 1
        elseif r1 < 0.30 then
            font_kind = 2
        else
            font_kind = 0
        end

        -- call 2 ─ string (pool depends on font_kind, but always 1 rnd call)
        local r2 = rnd()
        local str
        if font_kind == 1 then
            str = _pick(_font_u, r2)
        elseif font_kind == 2 then
            str = _pick(_font_v, r2)
        else
            -- evenly split between plain and P8SCII strings
            if r2 < 0.5 then
                str = _pick(_plain,  r2 * 2)
            else
                str = _pick(_p8scii, (r2 - 0.5) * 2)
            end
        end

        -- call 3 ─ initial cursor position
        local r3  = rnd()
        local cp  = _pick(_cursors, r3)

        -- call 4 ─ left margin (0x5f24)
        local r4     = rnd()
        local margin = _pick(_margins, r4)

        -- call 5 ─ misc flags (0x5f36)
        local r5   = rnd()
        local misc = _pick(_miscs, r5)

        -- call 6 ─ whether to call print(str, x, y) instead of print(str)
        local r6     = rnd()
        local use_xy = (r6 >= 0.5)

        -- call 7 ─ explicit x,y position (only used when use_xy=true)
        local r7 = rnd()
        local xy = _pick(_xys, r7)

        cases[i] = {
            str       = str,
            cx        = cp[1],  cy   = cp[2],
            margin    = margin,
            misc      = misc,
            use_xy    = use_xy,
            xy_x      = xy[1],  xy_y = xy[2],
            font_kind = font_kind,
        }
    end
    return cases
end

-- ── case execution ───────────────────────────────────────────────────────────

-- Set up and execute one test case.  Call this inside a test_case() body.
-- After this returns, peek(0x5f26)/peek(0x5f27) hold the cursor position.
function pscroll_run_case(t)
    cls()
    poke(0x5f36, t.misc)
    poke(0x5f58, 0)      -- clear any default-attribute flags
    cursor(t.cx, t.cy)   -- sets 0x5f24=cx, 0x5f26=cx, 0x5f27=cy
    poke(0x5f24, t.margin)  -- override the margin set by cursor()

    if t.font_kind == 1 then
        pscroll_setup_font_uniform()
    elseif t.font_kind == 2 then
        pscroll_setup_font_varwidth()
    end

    if t.use_xy then
        print(t.str, t.xy_x, t.xy_y)
    else
        print(t.str)
    end
end
