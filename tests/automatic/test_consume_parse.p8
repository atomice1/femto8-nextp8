pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- Tests for consume/tokenizer patterns, anonymous functions in while
-- conditions, and variable shadowing in for/all loops.
-- Ported from: test_consume, test_consume_logic, test_consume_pattern,
--   test_exact_consume, test_bounds, test_loop_condition, test_parse_minimal,
--   test_shadow, test_shadowing

#include test_fwk.lua

-- Helper: consume non-delimiter characters from _pstr starting at _ppos.
-- Returns the consumed token. Advances _ppos past the token.
function consume(str, pos, delim)
    local t = pos
    while (function()
        for e in all(delim) do
            if str[pos] == e then return true end
        end
    end)() == nil do
        pos += 1
        if pos > #str + 1 then break end
    end
    return sub(str, t, pos-1), pos
end

function test_consume_basic()
    test_case("consume_identifier", function()
        local tok, pos = consume("id (fn", 1, " \n\t()'\x22")
        check_eq(tok, "id")
        check_eq(pos, 3)
    end)
    test_case("consume_no_delimiter_match", function()
        local tok, pos = consume("abc", 1, "xyz")
        check_eq(tok, "abc")
        check_eq(pos, 4)
    end)
    test_case("consume_full_string_no_match", function()
        local tok, pos = consume("hello", 1, "xyz")
        check_eq(tok, "hello")
        check_eq(pos, 6)
    end)
    test_case("consume_exact_samurise_pattern", function()
        local tok, pos = consume("id test", 1, " \n\t()'\x22")
        check_eq(tok, "id")
        check_eq(pos, 3)
    end)
end

function test_anon_function_return()
    test_case("returns_nil_when_no_match", function()
        local s = "hello"
        local delim = "xyz"
        local result = (function()
            for e in all(delim) do
                if s[6] == e then return true end
            end
        end)()
        check_nil(result)
    end)
    test_case("nil_eq_nil", function()
        local result = (function() end)()
        check_eq(result == nil, true)
    end)
end

function test_whitespace_check()
    test_case("paren_not_whitespace", function()
        local c = "("
        local whitespace = " \n\t"
        local found = false
        for ch in all(whitespace) do
            if c == ch then found = true break end
        end
        check_false(found)
    end)
    test_case("space_is_whitespace", function()
        local c = " "
        local whitespace = " \n\t"
        local found = false
        for ch in all(whitespace) do
            if c == ch then found = true break end
        end
        check_true(found)
    end)
end

function test_variable_shadowing()
    test_case("for_all_shadow_param", function()
        local function test_fn(e)
            local result = {}
            for e in all(e) do
                add(result, e)
            end
            return result
        end
        local r = test_fn(" \n\t")
        check_eq(#r, 3)
        check_eq(ord(r[1]), 32)  -- space
    end)
    test_case("for_all_shadow_string", function()
        local function test_fn(e)
            local result = {}
            for e in all(e) do
                add(result, e)
            end
            return result
        end
        local r = test_fn("abc")
        check_eq(#r, 3)
        check_eq(r[1], "a")
        check_eq(r[2], "b")
        check_eq(r[3], "c")
    end)
end

function _init()
    test_suite("consume_basic", test_consume_basic)
    test_suite("anon_function_return", test_anon_function_return)
    test_suite("whitespace_check", test_whitespace_check)
    test_suite("variable_shadowing", test_variable_shadowing)
    summary()
end
