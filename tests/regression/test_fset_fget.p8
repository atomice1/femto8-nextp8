pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- Tests for fset()/fget() sprite flag operations.
-- Ported from: apitest1

#include test_fwk.lua

function test_fset_fget()
    test_case("roundtrip_all_sprites", function()
        for n = 0, 255 do
            fset(n, n)
        end
        for n = 0, 255 do
            check_eq(fget(n), n)
        end
    end)
    test_case("bit_toggle", function()
        for n = 0, 255 do
            fset(n, n)
            fset(n, band(n, 7), false)
            fset(n, band(n+1, 7), true)
        end
        for n = 0, 255 do
            check_false(fget(n, band(n, 7)))
            check_true(fget(n, band(n+1, 7)))
        end
    end)
end

function _init()
    test_suite("fset_fget", test_fset_fget)
    summary()
end
