pico-8 cartridge // http://www.pico-8.com
version 43
__lua__

#include test_fwk.lua

local TMPFILE = "cstore_tmp.p8"

-- 8-bit Galois LFSR step (poly 0xb8 = x^8+x^6+x^5+x^4+1, maximal-length)
local function lfsr_step(v)
    local lsb = v & 1
    v = (v >> 1) & 0xff
    if lsb != 0 then v = v ^^ 0xb8 end
    return v
end

-- Basic single-byte round-trip; reload to 0x4300 (different from src)
function test_cstore_basic()
    test_case("cstore_roundtrip", function()
        local addr = 0x0001
        local val  = flr(rnd(256))
        poke(addr, val)
        cstore(addr, addr, 1, TMPFILE)
        reload(0x4300, addr, 1, TMPFILE)
        check_eq(peek(0x4300), val)
    end)
end

-- 16-byte region round-trip in sprite memory
function test_cstore_region()
    test_case("cstore_sprite_region", function()
        local vals = {}
        for i = 0, 15 do
            vals[i] = flr(rnd(256))
            poke(i, vals[i])
        end
        cstore(0, 0, 16, TMPFILE)
        reload(0x4300, 0, 16, TMPFILE)
        for i = 0, 15 do
            check_eq(peek(0x4300 + i), vals[i])
        end
    end)
end

-- Map region round-trip (0x2000)
function test_cstore_map()
    test_case("cstore_map_region", function()
        local base = 0x2000
        local vals = {}
        for i = 0, 7 do
            vals[i] = flr(rnd(256))
            poke(base + i, vals[i])
        end
        cstore(base, base, 8, TMPFILE)
        reload(0x4300, base, 8, TMPFILE)
        for i = 0, 7 do
            check_eq(peek(0x4300 + i), vals[i])
        end
    end)
end

-- SFX speed byte round-trip (offset 65 within the first 68-byte SFX entry)
function test_cstore_sfx()
    test_case("cstore_sfx_speed", function()
        local base   = 0x3200
        local offset = 65
        local val    = flr(rnd(256))
        poke(base + offset, val)
        cstore(base, base, 68, TMPFILE)
        reload(0x4300, base, 68, TMPFILE)
        check_eq(peek(0x4300 + offset), val)
    end)
end

-- Sprite-flags round-trip (0x3000)
function test_cstore_flags()
    test_case("cstore_sprite_flags", function()
        local base = 0x3000
        local v0   = flr(rnd(256))
        local v1   = flr(rnd(256))
        poke(base + 0, v0)
        poke(base + 1, v1)
        cstore(base, base, 2, TMPFILE)
        reload(0x4300, base, 2, TMPFILE)
        check_eq(peek(0x4300 + 0), v0)
        check_eq(peek(0x4300 + 1), v1)
    end)
end

-- Full 0x0-0x42ff round-trip using an LFSR pattern; reload to 0x8000
function test_cstore_lfsr()
    test_case("cstore_lfsr_fullrange", function()
        -- seed from rnd so each run produces a different pattern
        local v = flr(rnd(255)) + 1  -- 1-255, never 0
        local seed = v
        for i = 0, 0x42ff do
            v = lfsr_step(v)
            poke(i, v)
        end
        cstore(0x0, 0x0, 0x4300, TMPFILE)
        -- reload to 0x8000 (different destination)
        reload(0x8000, 0x0, 0x4300, TMPFILE)
        -- verify by recomputing the same LFSR sequence from the same seed
        v = seed
        for i = 0, 0x42ff do
            v = lfsr_step(v)
            local actual = peek(0x8000 + i)
            if actual != v then
                fail("@"..tostr(i,true).." exp "..tostr(v,true).." got "..tostr(actual,true))
                break
            end
        end
    end)
end

function _init()
    srand(stat(93)*3600+stat(94)*60+stat(95))
    test_suite("cstore_basic",  test_cstore_basic)
    test_suite("cstore_region", test_cstore_region)
    test_suite("cstore_map",    test_cstore_map)
    test_suite("cstore_sfx",    test_cstore_sfx)
    test_suite("cstore_flags",  test_cstore_flags)
    test_suite("cstore_lfsr",   test_cstore_lfsr)
    summary()
end
