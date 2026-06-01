pico-8 cartridge // http://www.pico-8.com
version 43
__lua__

-- gen_print_scroll.p8
-- Run on real Pico-8 to produce the expected cursor positions for the
-- print_scroll regression test.
--
-- Usage:
--   ~/pico-8/pico8 -x tests/regression/gen_print_scroll.p8 \
--       2>&1 | grep -E '^-?[0-9]+,-?[0-9]+$'
--
-- The filtered output is one "cx,cy" line per test case.
-- Paste the result into test_print_scroll.p8 as the `expected` table.

#include test_print_scroll_lib.lua

function _init()
    printh("PSCROLL_START")

    srand(PSCROLL_SEED)
    local cases = pscroll_generate_cases()

    for i = 1, #cases do
        pscroll_run_case(cases[i])
        local cx = peek(0x5f26)
        local cy = peek(0x5f27)
        printh(cx .. "," .. cy)
    end

    printh("PSCROLL_END")
end
