pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- Tests for RNG: state changes, deterministic sequences, save/restore.
-- Ported from: rng

#include test_fwk.lua

function test_rng()
    test_case("state_changes_after_rnd", function()
        local before = {}
        for i=0,7 do before[i] = peek(0x5f44+i) end
        rnd()
        local changed = false
        for i=0,7 do
            if peek(0x5f44+i) != before[i] then
                changed = true
                break
            end
        end
        check_true(changed)
    end)
    test_case("deterministic_sequence", function()
        srand(999)
        local seq1 = {}
        for i=1,5 do seq1[i] = rnd() end
        srand(999)
        local seq2 = {}
        for i=1,5 do seq2[i] = rnd() end
        for i=1,5 do
            check_eq(seq1[i], seq2[i])
        end
    end)
    test_case("save_restore_state", function()
        srand(777)
        rnd() rnd()
        local saved = {}
        for i=0,7 do saved[i] = peek(0x5f44+i) end
        local val1 = rnd()
        for i=0,7 do poke(0x5f44+i, saved[i]) end
        local val2 = rnd()
        check_eq(val1, val2)
    end)
end

function _init()
    test_suite("rng", test_rng)
    summary()
end
