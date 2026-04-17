pico-8 cartridge // http://www.pico-8.com
version 42
__lua__

#include test_fwk.lua

function test_all_string_iteration()
    test_case("100_iterations", function()
        local delim = " \n\t"
        local test_count = 100
        
        for test_num = 1, test_count do
            local chars = {}
            local idx = 1
            for ch in all(delim) do
                chars[idx] = ch
                idx += 1
            end
            
            check_eq(#chars, 3)
            check_eq(chars[1], " ")
            check_eq(chars[2], "\n")
            check_eq(chars[3], "\t")
        end
    end)
end

function _init()
    test_suite("all_stress", test_all_string_iteration)
    summary()
end
