pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- Tests for _ENV, _G, rawget/rawset, and required global functions.
-- Ported from: test_env, test_functions

#include test_fwk.lua

function test_env()
    test_case("env_is_table", function()
        check_eq(type(_ENV), "table")
    end)
    test_case("g_is_table", function()
        check_eq(type(_G), "nil")
    end)
    test_case("env_eq_g", function()
        check_eq(_G, nil)
    end)
    test_case("rawget_basic", function()
        local t = {a=1, b=2}
        check_eq(rawget(t, "a"), 1)
        check_eq(rawget(t, "b"), 2)
    end)
    test_case("rawget_env", function()
        check_ne(rawget(_ENV, "printh"), nil)
    end)
end

function test_required_functions()
    test_case("select_exists", function() check_ne(select, nil) end)
    test_case("unpack_exists", function() check_ne(unpack, nil) end)
    test_case("rawget_exists", function() check_ne(rawget, nil) end)
    test_case("rawset_exists", function() check_ne(rawset, nil) end)
    test_case("deli_exists", function() check_ne(deli, nil) end)
    test_case("split_exists", function() check_ne(split, nil) end)
    test_case("count_exists", function() check_ne(count, nil) end)
    test_case("next_exists", function() check_ne(next, nil) end)
    test_case("inext_exists", function() check_ne(inext, nil) end)
    test_case("ipairs_exists", function() check_ne(ipairs, nil) end)
end

function test_env_param()
    -- PICO-8 treats _ENV as a parameter name with special semantics:
    -- reads look up the passed table first, then fall back to real globals.
    -- writes go to the passed table (not to the proxy or real globals).

    test_case("env_param_read_field", function()
        -- field present in passed table is returned
        local function f(_ENV) return x end
        check_eq(f({x=42}), 42)
    end)

    test_case("env_param_read_missing_falls_back", function()
        -- field not in passed table falls back to real globals (flr is a builtin)
        local function f(_ENV) return flr(3.7) end
        check_eq(f({}), 3)
    end)

    test_case("env_param_write_field", function()
        -- assignment inside _ENV-param function writes to the passed table
        local function f(_ENV) x = 99 end
        local t = {x=42}
        f(t)
        check_eq(t.x, 99)
    end)

    test_case("env_param_write_new_field", function()
        -- writing a new field also goes to the passed table
        local function f(_ENV) newfield = 7 end
        local t = {}
        f(t)
        check_eq(t.newfield, 7)
    end)

    test_case("env_param_anon_write", function()
        -- anonymous function(_ENV) pattern writes to passed table
        local fn = function(_ENV) typ = 55 end
        local tl = {typ=1}
        fn(tl)
        check_eq(tl.typ, 55)
    end)

    test_case("env_param_does_not_pollute_globals", function()
        -- writing inside _ENV-param function does NOT affect real globals
        local prev = rawget(_ENV, "_p8_test_pollution_sentinel")
        local function f(_ENV) _p8_test_pollution_sentinel = 1 end
        f({})
        check_eq(rawget(_ENV, "_p8_test_pollution_sentinel"), prev)
    end)
end

function _init()
    test_suite("env", test_env)
    test_suite("required_functions", test_required_functions)
    test_suite("env_param", test_env_param)
    summary()
end
