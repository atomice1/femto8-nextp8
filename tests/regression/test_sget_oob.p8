pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- Tests for sget() out-of-bounds behavior with sget_default register.
-- Ported from: test_oob

#include test_fwk.lua

function test_sget_oob()
    test_case("sget_default_register", function()
        poke(0x5f36, 0x10)   -- enable sget OOB default
        poke(0x5f59, 21)     -- set default value
        check_eq(sget(-10, -10), 21)
    end)
end

function _init()
    test_suite("sget_oob", test_sget_oob)
    summary()
end
