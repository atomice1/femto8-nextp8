pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- Tests for string indexing, length, sub(), and while-loop iteration.
-- Ported from: test_basic_string, test_string_index, test_string_length,
--   test_string_loop, test_string_nil, test_sub

#include test_fwk.lua

function test_string_indexing()
    test_case("basic_index", function()
        local s = "abc"
        check_eq(s[1], "a")
        check_eq(s[2], "b")
        check_eq(s[3], "c")
    end)
    test_case("index_0_is_nil", function()
        local s = "hello"
        check_nil(s[0])
    end)
    test_case("index_past_end_is_nil", function()
        local s = "hello"
        check_eq(s[5], "o")
        check_nil(s[6])
        check_nil(s[10])
    end)
    test_case("all_chars", function()
        local s = "hello"
        check_eq(s[1], "h")
        check_eq(s[2], "e")
        check_eq(s[3], "l")
        check_eq(s[4], "l")
        check_eq(s[5], "o")
    end)
end

function test_string_length()
    test_case("basic_length", function()
        check_eq(#"hello", 5)
        check_eq(#"", 0)
        check_eq(#"a", 1)
    end)
    test_case("nil_comparison", function()
        local s = "hello"
        check_eq(s[6] == nil, true)
        check_eq(s[10] == nil, true)
    end)
end

function test_string_while_loop()
    test_case("loop_terminates", function()
        local s = "abc"
        local i = 1
        local count = 0
        while s[i] do
            count += 1
            i += 1
            if i > 10 then break end
        end
        check_eq(count, 3)
        check_eq(i, 4)
    end)
end

function test_sub_function()
    test_case("single_chars", function()
        local s = " \n\t"
        check_eq(#s, 3)
        check_eq(ord(sub(s,1,1)), 32)   -- space
        check_eq(ord(sub(s,2,2)), 10)   -- newline
        check_eq(ord(sub(s,3,3)), 9)    -- tab
    end)
    test_case("sub_basic", function()
        local s = "hello"
        check_eq(sub(s,1,3), "hel")
        check_eq(sub(s,4,5), "lo")
        check_eq(sub(s,1,1), "h")
    end)
end

function _init()
    test_suite("string_indexing", test_string_indexing)
    test_suite("string_length", test_string_length)
    test_suite("string_while_loop", test_string_while_loop)
    test_suite("sub", test_sub_function)
    summary()
end
