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

-- Reference sequences captured from pico-8 v0.2.7 (version 43).
-- Each table is 10 rnd() values after srand(seed).
rng_ref = {
    seed_0       = {0x0000.8474,0x0000.5724,0x0000.758c,0x0000.a9c5,0x0000.1c9b,0x0000.e12a,0x0000.50d7,0x0000.1070,0x0000.a980,0x0000.b42c},
    seed_1       = {0x0000.14b7,0x0000.fd18,0x0000.97e2,0x0000.904f,0x0000.941d,0x0000.6cda,0x0000.6e56,0x0000.dade,0x0000.dadf,0x0000.6b11},
    seed_999     = {0x0000.9b49,0x0000.5d61,0x0000.2f9a,0x0000.94d7,0x0000.a515,0x0000.d3fc,0x0000.669e,0x0000.6931,0x0000.6e06,0x0000.8346},
    seed_65535   = {0x0000.b708,0x0000.712a,0x0000.a66d,0x0000.cc47,0x0000.1288,0x0000.099d,0x0000.fcb9,0x0000.7d43,0x0000.fe31,0x0000.9125},
    seed_0p5     = {0x0000.e117,0x0000.9047,0x0000.f957,0x0000.2960,0x0000.7a06,0x0000.098f,0x0000.5ec2,0x0000.93ad,0x0000.9957,0x0000.f661},
    seed_0p25    = {0x0000.cfb2,0x0000.6cab,0x0000.4048,0x0000.4872,0x0000.574d,0x0000.6ff8,0x0000.695d,0x0000.ef64,0x0000.f8d6,0x0000.b26b},
    seed_mixed   = {0x0000.a304,0x0000.1ed6,0x0000.5c7b,0x0000.5600,0x0000.a7e8,0x0000.037f,0x0000.c6c0,0x0000.16b1,0x0000.a771,0x0000.cfad},
    seed_mixed2  = {0x0000.12e0,0x0000.39cf,0x0000.1e8e,0x0000.64b2,0x0000.d660,0x0000.281d,0x0000.7280,0x0000.0999,0x0000.550b,0x0000.7a7c},
}

function test_rng_compat()
    local seeds = {
        {name="seed_0",      val=0},
        {name="seed_1",      val=1},
        {name="seed_999",    val=999},
        {name="seed_65535",  val=65535},
        {name="seed_0p5",    val=0.5},
        {name="seed_0p25",   val=0.25},
        {name="seed_mixed",  val=0x5b04.17cb},
        {name="seed_mixed2", val=0x1234.5678},
    }
    for _,s in ipairs(seeds) do
        test_case(s.name, function()
            srand(s.val)
            local ref = rng_ref[s.name]
            for i=1,10 do
                check_eq(rnd(), ref[i])
            end
        end)
    end
end

function _init()
    test_suite("rng", test_rng)
    test_suite("rng_compat", test_rng_compat)
    summary()
end
