pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- Tests for poke2/peek2 roundtrip.
-- Ported from: test_version

#include test_fwk.lua

function test_poke_peek()
    test_case("poke2_peek2_roundtrip", function()
        poke2(0x4300, 0x1234)
        check_eq(peek2(0x4300), 0x1234)
    end)
    test_case("poke4_peek4_roundtrip", function()
        poke4(0x4300, 1.5)
        check_eq(peek4(0x4300), 1.5)
    end)
end

function _init()
    test_suite("poke_peek", test_poke_peek)
    summary()
end
