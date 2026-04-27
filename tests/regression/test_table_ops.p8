pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- Tests for count(), deli(), split(), table() constructor.
-- Ported from: count_test, deli_test, split_test, test_table, test_table2
-- Also covers all() / del() edge cases.

#include test_fwk.lua

function test_count()
    test_case("count_empty_table", function()
        check_eq(count({}), 0)
    end)
    test_case("count_populated_table", function()
        check_eq(count({1,2,3}), 3)
    end)
    test_case("count_nil", function()
        check_nil(count(nil))
    end)
    test_case("count_string", function()
        check_nil(count("abc"))
    end)
    test_case("count_number", function()
        check_nil(count(42))
    end)
    test_case("count_boolean", function()
        check_nil(count(true))
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
        check_nil(v)
        check_eq(#t, 3)
        check_eq(t[3], 30)
    end)
end

function test_all_and_del()
    test_case("all_empty_table", function()
        local count = 0
        for v in all({}) do
            count += 1
        end
        check_eq(count, 0)
    end)

    test_case("all_numeric_order", function()
        local t = {11, 12, 13}
        local result = {}
        for v in all(t) do
            add(result, v)
        end
        check_eq(#result, 3)
        check_eq(result[1], 11)
        check_eq(result[2], 12)
        check_eq(result[3], 13)
    end)

    test_case("del_first_value", function()
        local t = {1, 10, 2, 11, 3, 12}
        check_eq(del(t, 10), 10)
        check_eq(#t, 5)
        check_eq(t[1], 1)
        check_eq(t[2], 2)
        check_eq(t[3], 11)
        check_eq(t[4], 3)
        check_eq(t[5], 12)
    end)

    test_case("del_missing_value", function()
        local t = {1, 2, 3}
        check_nil(del(t, 99))
        check_eq(#t, 3)
        check_eq(t[1], 1)
        check_eq(t[2], 2)
        check_eq(t[3], 3)
    end)

    test_case("all_del_during_iteration", function()
        local t = {1, 2, 3}
        local result = {}
        for v in all(t) do
            add(result, v)
            del(t, v)
        end
        check_eq(#result, 3)
        check_eq(result[1], 1)
        check_eq(result[2], 2)
        check_eq(result[3], 3)
    end)

    test_case("two_all_concurrent_same_table", function()
        local t = {1, 2, 3}
        local a = all(t)
        local b = all(t)

        check_eq(a(), 1)
        check_eq(b(), 1)
        check_eq(a(), 2)
        check_eq(b(), 2)
        check_eq(a(), 3)
        check_eq(b(), 3)
        check_nil(a())
        check_nil(b())
    end)

    test_case("two_all_concurrent_different_tables", function()
        local t1 = {1, 2}
        local t2 = {10, 20, 30}
        local a = all(t1)
        local b = all(t2)

        check_eq(a(), 1)
        check_eq(b(), 10)
        check_eq(a(), 2)
        check_eq(b(), 20)
        check_nil(a())
        check_eq(b(), 30)
        check_nil(b())
    end)

    test_case("nested_all", function()
        local a = {1, 2}
        local n = {10, 20, 30}
        local result = {}

        for x in all(a) do
            for y in all(n) do
                add(result, x * 100 + y)
            end
        end

        check_eq(#result, 6)
        check_eq(result[1], 110)
        check_eq(result[2], 120)
        check_eq(result[3], 130)
        check_eq(result[4], 210)
        check_eq(result[5], 220)
        check_eq(result[6], 230)
    end)

    test_case("nested_all_concurrent_modification", function()
        local outer = {1, 2}
        local inner = {10, 20}
        local result = {}

        for x in all(outer) do
            if x == 1 then
                add(outer, 3)
            end

            for y in all(inner) do
                add(result, x * 100 + y)
                if y == 10 then
                    add(inner, 30)
                end
            end
        end

        check_eq(#result, 12)
        check_eq(result[1], 110)
        check_eq(result[2], 120)
        check_eq(result[3], 130)
        check_eq(result[4], 210)
        check_eq(result[5], 220)
        check_eq(result[6], 230)
        check_eq(result[7], 230)
        check_eq(result[8], 310)
        check_eq(result[9], 320)
        check_eq(result[10], 330)
        check_eq(result[11], 330)
        check_eq(result[12], 330)
        check_eq(outer[1], 1)
        check_eq(outer[2], 2)
        check_eq(outer[3], 3)
        check_eq(inner[1], 10)
        check_eq(inner[2], 20)
        check_eq(inner[3], 30)
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
        check_nil(table)
    end)
end

function _init()
    test_suite("count", test_count)
    test_suite("deli", test_deli)
    test_suite("all_and_del", test_all_and_del)
    test_suite("split", test_split)
    test_suite("table_constructor", test_table_constructor)
    summary()
end
