pico-8 cartridge // http://www.pico-8.com
version 43
__lua__

-- Tests that PICO-8 built-in functions remain accessible when _ENV is shadowed
-- PICO-8 has certain builtins (math, bitwise, graphics opcodes) that bypass _ENV
-- Other registered functions (btn, time, printh, etc.) go through _ENV

#include test_fwk.lua

--------------------------------------------------------------------------------
-- Test 1: _ENV as function parameter
-- PICO-8 builtins (opcodes) work, registered functions don't
--------------------------------------------------------------------------------

function test_env_param()
    test_case("_env_param_opcode_builtins", function()
        local result = {}
        local function f(_ENV)
            -- These are PICO-8 opcodes - they work even with _ENV parameter
            result.sin = (sin ~= nil)
            result.cos = (cos ~= nil)
            result.flr = (flr ~= nil)
            result.band = (band ~= nil)
            result.circfill = (circfill ~= nil)
            result.circ = (circ ~= nil)
            result.line = (line ~= nil)
            result.btn = (btn ~= nil)  -- registered, won't work
            result.printh = (printh ~= nil)  -- registered, won't work
        end
        f({})
        -- Opcodes should work
        check_eq(result.sin, true, "sin")
        check_eq(result.cos, true, "cos")
        check_eq(result.flr, true, "flr")
        check_eq(result.band, true, "band")
        check_eq(result.circfill, true, "circfill")
        check_eq(result.circ, true, "circ")
        check_eq(result.line, true, "line")
        -- Registered functions won't work
        check_eq(result.btn, false, "btn")
        check_eq(result.printh, false, "btnp")
    end)
end

--------------------------------------------------------------------------------
-- Test 2: _ENV as loop variable
-- PICO-8 builtins (opcodes) work, registered functions don't
--------------------------------------------------------------------------------

function test_env_loop_var()
    test_case("_env_loop_var_opcode_builtins", function()
        local particles = {{}, {}}
        local result = {}
        
        for _ENV in all(particles) do
            -- Opcodes should work
            result.sin = (sin ~= nil)
            result.cos = (cos ~= nil)
            result.flr = (flr ~= nil)
            result.band = (band ~= nil)
            result.circfill = (circfill ~= nil)
            result.circ = (circ ~= nil)
            result.line = (line ~= nil)
            result.btn = (btn ~= nil)  -- registered, won't work
            result.printh = (printh ~= nil)  -- registered, won't work
            break
        end
        
        check_eq(result.sin, true, "sin")
        check_eq(result.cos, true, "cos")
        check_eq(result.flr, true, "flr")
        check_eq(result.band, true, "band")
        check_eq(result.circfill, true, "circfill")
        check_eq(result.circ, true, "circ")
        check_eq(result.line, true, "line")
        check_eq(result.btn, false, "btn")
        check_eq(result.printh, false, "printh")
    end)
end

--------------------------------------------------------------------------------
-- Test 3: Nested functions with _ENV
-- Opcodes should work at all levels
--------------------------------------------------------------------------------

function test_nested_env()
    test_case("nested_env_param_opcodes", function()
        local outer_result = {}
        local inner_result = {}
        
        local function outer(_ENV)
            outer_result.sin = (sin ~= nil)
            outer_result.circfill = (circfill ~= nil)
            
            local function inner(_ENV)
                inner_result.sin = (sin ~= nil)
                inner_result.circfill = (circfill ~= nil)
                inner_result.btn = (btn ~= nil)  -- registered, won't work
            end
            inner({})
        end
        
        outer({})
        check_eq(outer_result.sin, true, "sin")
        check_eq(outer_result.circfill, true, "circfill")
        check_eq(inner_result.sin, true, "sin")
        check_eq(inner_result.circfill, true, "circfill")
        check_eq(inner_result.btn, false, "btn")
    end)
end

--------------------------------------------------------------------------------
-- Test 4: Graphics primitives (all opcodes)
--------------------------------------------------------------------------------

function test_graphics()
    test_case("graphics_opcodes_in_env_param", function()
        local result = {}
        local function f(_ENV)
            result.circ = (circ ~= nil)
            result.circfill = (circfill ~= nil)
            result.line = (line ~= nil)
            result.rect = (rect ~= nil)
            result.pset = (pset ~= nil)
            result.pget = (pget ~= nil)
            result.spr = (spr ~= nil)
        end
        f({})
        check_eq(result.circ, true, "circ")
        check_eq(result.circfill, true, "circfill")
        check_eq(result.line, true, "line")
        check_eq(result.rect, true, "rect")
        check_eq(result.pset, true, "pset")
        check_eq(result.pget, true, "pget")
        check_eq(result.spr, true, "spr")
    end)
end

--------------------------------------------------------------------------------
-- Main
--------------------------------------------------------------------------------

function _init()
    test_suite("env_param", test_env_param)
    test_suite("env_loop_var", test_env_loop_var)
    test_suite("nested_env", test_nested_env)
    test_suite("graphics", test_graphics)
    summary()
end
