pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- Tests for hex literal for-loop iteration (fixed-point edge cases).
-- Ported from: hexloop

#include test_fwk.lua

function test_hex_for_loops()
    test_case("negative_range_0x8000_to_0xffff", function()
        -- 0x8000 = -32768, 0xffff = -0.0000...1 (just below 0)
        -- step is 1 (0x0001.0000), so this iterates 32768 times
        local count = 0
        for i=0x8000,0xffff do
            count += 1
        end
        check_eq(count, 32768)
    end)
    test_case("wrap_around_0xfffe_to_0x0002", function()
        -- 0xfffe..0x0002 with default step 1:
        -- -2, -1, 0, 1, 2 = 5 iterations
        local count = 0
        for i=0xfffe,0x0002 do
            count += 1
        end
        check_eq(count, 5)
    end)
    test_case("positive_range_0x0001_to_0x0005", function()
        local count = 0
        for i=0x0001,0x0005 do
            count += 1
        end
        check_eq(count, 5)
    end)
end

function _init()
    test_suite("hex_for_loops", test_hex_for_loops)
    summary()
end
