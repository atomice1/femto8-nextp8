pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- Tests for count(), deli(), split(), table() constructor.
-- Ported from: count_test, deli_test, split_test, test_table, test_table2

#include test_fwk.lua

function test_count()
    test_case("count_empty_table", function()
        check_eq(count({}), 0)
    end)
    test_case("count_populated_table", function()
        check_eq(count({1,2,3}), 3)
    end)
    test_case("count_nil", function()
        check_eq(count(nil), 0)
    end)
    test_case("count_string", function()
        check_eq(count("abc"), 0)
    end)
    test_case("count_number", function()
        check_eq(count(42), 0)
    end)
    test_case("count_boolean", function()
        check_eq(count(true), 0)
    end)
end

function test_deli()
    test_case("deli_basic", function()
        local t = {10, 20, 30}
        local v = deli(t, 1)
        check_eq(v, 10)
        check_eq(#t, 2)
        check_eq(t[1], 20)
    end)
    test_case("deli_last", function()
        local t = {10, 20, 30}
        local v = deli(t)
        check_eq(v, 30)
        check_eq(#t, 2)
    end)
    test_case("deli_negative_index", function()
        local t = {10, 20, 30}
        local v = deli(t, -1)
        check_eq(v, 30)
        check_eq(#t, 2)
    end)
end

function test_split()
    test_case("split_nil", function()
        check_nil(split(nil))
    end)
    test_case("split_number", function()
        local r = split(123)
        check_ne(r, nil)
        check_eq(type(r), "table")
        check_eq(r[1], 123)
    end)
    test_case("split_basic_string", function()
        local r = split("1,2,3")
        check_eq(#r, 3)
        check_eq(r[1], 1)
        check_eq(r[2], 2)
        check_eq(r[3], 3)
    end)
end

function test_table_constructor()
    test_case("table_creates_empty_table", function()
        local t = table()
        check_ne(t, nil)
        check_eq(type(t), "table")
    end)
end

function _init()
    test_suite("count", test_count)
    test_suite("deli", test_deli)
    test_suite("split", test_split)
    test_suite("table_constructor", test_table_constructor)
    summary()
end
