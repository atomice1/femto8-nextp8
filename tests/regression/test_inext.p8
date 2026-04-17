pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- Tests for inext() function: basic iteration, for-loop, ipairs comparison,
-- empty tables, gaps, and no-index calls.
-- Ported from: test_inext

#include test_fwk.lua

function test_inext_basic()
    test_case("first_call_nil_index", function()
        local t = {"a", "b", "c", "d"}
        local i, v = inext(t, nil)
        check_eq(i, 1)
        check_eq(v, "a")
    end)
    test_case("sequential_calls", function()
        local t = {"a", "b", "c", "d"}
        local i, v = inext(t, 1)
        check_eq(i, 2)
        check_eq(v, "b")
        i, v = inext(t, 2)
        check_eq(i, 3)
        check_eq(v, "c")
        i, v = inext(t, 3)
        check_eq(i, 4)
        check_eq(v, "d")
    end)
    test_case("past_end_returns_nil", function()
        local t = {"a", "b", "c", "d"}
        local i, v = inext(t, 4)
        check_nil(i)
        check_nil(v)
    end)
end

function test_inext_for_loop()
    test_case("for_in_inext", function()
        local t = {"a", "b", "c", "d"}
        local result = {}
        for i, v in inext, t do
            result[i] = v
        end
        check_eq(result[1], "a")
        check_eq(result[2], "b")
        check_eq(result[3], "c")
        check_eq(result[4], "d")
    end)
    test_case("matches_ipairs", function()
        local t = {"a", "b", "c", "d"}
        local r1 = {}
        for i, v in inext, t do r1[i] = v end
        local r2 = {}
        for i, v in ipairs(t) do r2[i] = v end
        check_eq(r1[1], r2[1])
        check_eq(r1[2], r2[2])
        check_eq(r1[3], r2[3])
        check_eq(r1[4], r2[4])
    end)
end

function test_inext_edge_cases()
    test_case("empty_table", function()
        local i, v = inext({}, nil)
        check_nil(i)
        check_nil(v)
    end)
    test_case("gaps_stop_at_nil", function()
        local gaps = {"first", nil, "third"}
        local result = {}
        for i, v in inext, gaps do
            result[i] = v
        end
        check_eq(result[1], "first")
        check_nil(result[2])
    end)
    test_case("no_index_arg", function()
        local t = {"x", "y", "z"}
        local i, v = inext(t)
        check_eq(i, 1)
        check_eq(v, "x")
    end)
    test_case("chained_calls", function()
        local t = {"one", "two", "three"}
        local i, v = inext(t)
        check_eq(i, 1) check_eq(v, "one")
        i, v = inext(t, i)
        check_eq(i, 2) check_eq(v, "two")
        i, v = inext(t, i)
        check_eq(i, 3) check_eq(v, "three")
        i, v = inext(t, i)
        check_nil(i) check_nil(v)
    end)
    test_case("start_from_index_2", function()
        local t = {"a", "b", "c", "d", "e"}
        local result = {}
        for i, v in inext, t, 2 do
            result[i] = v
        end
        check_nil(result[1])
        check_nil(result[2])
        check_eq(result[3], "c")
        check_eq(result[4], "d")
        check_eq(result[5], "e")
    end)
end

function _init()
    test_suite("inext_basic", test_inext_basic)
    test_suite("inext_for_loop", test_inext_for_loop)
    test_suite("inext_edge_cases", test_inext_edge_cases)
    summary()
end
