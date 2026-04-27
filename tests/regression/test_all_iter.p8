pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- Tests for all(), foreach(), and concurrent modification during iteration.
-- Ported from: all_test, test_all, test_all2, test_all3, test_all_init,
--   test_all_order, test_all_strings, test_add_all, test_del_current,
--   concurrent_test

#include test_fwk.lua

function test_all_basic_types()
    test_case("all_empty_table_no_iter", function()
        local count = 0
        for v in all({}) do count += 1 end
        check_eq(count, 0)
    end)
    test_case("all_empty_string_no_iter", function()
        local count = 0
        for c in all("") do count += 1 end
        check_eq(count, 0)
    end)
    test_case("all_nil_no_iter", function()
        local count = 0
        for v in all(nil) do count += 1 end
        check_eq(count, 0)
    end)
end

function test_all_table()
    test_case("basic_table", function()
        local t = {11,12,13}
        add(t,14)
        add(t,10)
        add(t,"hi")
        local result = {}
        for v in all(t) do add(result, v) end
        check_eq(#result, 6)
        check_eq(result[1], 11)
        check_eq(result[6], "hi")
    end)
    test_case("large_table_20", function()
        local t = {}
        for i=1,20 do add(t, i*10) end
        local result = {}
        for v in all(t) do add(result, v) end
        check_eq(#result, 20)
        check_eq(result[1], 10)
        check_eq(result[20], 200)
    end)
    test_case("mixed_keys_only_numeric", function()
        local t = {10,20,30}
        t.foo = "bar"
        t.baz = "qux"
        local result = {}
        for v in all(t) do add(result, v) end
        check_eq(#result, 3)
        check_eq(result[1], 10)
        check_eq(result[2], 20)
        check_eq(result[3], 30)
    end)
    test_case("basic_iteration", function()
        local t = {1,2,3}
        local result = {}
        for v in all(t) do add(result, v) end
        check_eq(#result, 3)
        check_eq(result[1], 1)
        check_eq(result[2], 2)
        check_eq(result[3], 3)
    end)
end

function test_all_order()
    test_case("out_of_order_insert", function()
        local t = {}
        t[3] = "third"
        t[2] = "second"
        t[1] = "first"
        local result = {}
        for v in all(t) do add(result, v) end
        check_eq(result[1], "first")
        check_eq(result[2], "second")
        check_eq(result[3], "third")
    end)
    test_case("gaps_stop_at_nil", function()
        local t = {}
        t[1] = "one"
        t[3] = "three"
        t[5] = "five"
        local result = {}
        for v in all(t) do add(result, v) end
        -- all() stops at first nil gap (index 2)
        check_eq(#result, 1)
        check_eq(result[1], "one")
    end)
end

function test_all_strings()
    test_case("iterate_string", function()
        local result = {}
        for c in all("hello") do add(result, c) end
        check_eq(#result, 5)
        check_eq(result[1], "h")
        check_eq(result[5], "o")
    end)
    test_case("iterate_abc", function()
        local result = {}
        for c in all("abc") do add(result, c) end
        check_eq(#result, 3)
        check_eq(result[1], "a")
        check_eq(result[2], "b")
        check_eq(result[3], "c")
    end)
    test_case("empty_string", function()
        local count = 0
        for c in all("") do count += 1 end
        check_eq(count, 0)
    end)
    test_case("all_nil_no_iter", function()
        local count = 0
        for v in all(nil) do count += 1 end
        check_eq(count, 0)
    end)
end

function test_all_concurrent()
    test_case("add_during_iteration", function()
        local t = {1, 2, 3}
        local result = {}
        local count = 0
        for v in all(t) do
            add(result, v)
            count += 1
            if count == 2 then add(t, 4) end
            if count > 10 then break end
        end
        check_eq(#result, 4)
        check_eq(result[4], 4)
    end)
    test_case("del_during_iteration", function()
        local t = {1, 2, 3}
        local result = {}
        for v in all(t) do
            add(result, v)
            del(t, v)
        end
        -- PICO-8 continues iterating over the original array order here.
        check_eq(#result, 3)
        check_eq(result[1], 1)
        check_eq(result[2], 2)
        check_eq(result[3], 3)
    end)
    test_case("add_during_foreach", function()
        local t = {10, 20}
        local result = {}
        local count = 0
        foreach(t, function(v)
            add(result, v)
            count += 1
            if count == 1 then add(t, 30) end
        end)
        check_eq(#result, 3)
        check_eq(result[3], 30)
    end)
end

function _init()
    test_suite("all_basic_types", test_all_basic_types)
    test_suite("all_table", test_all_table)
    test_suite("all_order", test_all_order)
    test_suite("all_strings", test_all_strings)
    test_suite("all_concurrent", test_all_concurrent)
    summary()
end
