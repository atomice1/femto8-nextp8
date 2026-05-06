pico-8 cartridge // http://www.pico-8.com
version 43
__lua__

#include test_fwk.lua

local TMPBASE = "cstore_tmp"
local TMPFILE = TMPBASE .. ".p8"

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

function test_cstore_lfsr()
    -- Full 0x0-0x42ff round-trip using an LFSR pattern; reload to 0x8000
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

    test_case("cstore_lfsr_patches", function()
        -- Fill cart memory with an LFSR pattern
        local v = flr(rnd(255)) + 1  -- 1-255, never 0
        for i = 0, 0x42ff do
            v = lfsr_step(v)
            poke(i, v)
        end
        for i=0x8000, 0xffff do
            poke(i, 0)
        end

        -- Two fixed-size patches that partially overlap; only base addresses vary.
        -- Patch 2 starts halfway into patch 1, extending past its end.
        --   patch1: [destaddr1,        destaddr1+0x80)
        --   patch2: [destaddr1+0x40,   destaddr1+0xc0)
        --   overlap:[destaddr1+0x40,   destaddr1+0x80)  (0x40 bytes)
        local len1 = 0x80
        local len2 = 0x80
        -- non-zero destaddr1; leave room for both patches to fit in cart
        local destaddr1  = flr(rnd(0x4300 - len1 - len2/2)) + 1
        local destaddr2  = destaddr1 + len1 / 2
        -- sourceaddr2 is offset from sourceaddr1 so the two patches carry different data
        local sourceaddr1 = flr(rnd(0x4300 - len1))
        local sourceaddr2 = (sourceaddr1 + len1) % (0x4300 - len2)

        local newfn = TMPFILE .. flr(rnd(1000)) .. ".p8"
        -- Base cstore, then apply the two partial patches
        cstore(0x0, 0x0, 0x4300, newfn)
        cstore(destaddr1, sourceaddr1, len1, newfn)
        cstore(destaddr2, sourceaddr2, len2, newfn)

        -- Reload to 0x8000 (above LFSR data; doesn't disturb verification values)
        reload(0x8000, 0x0, 0x4300, newfn)

        -- Patch 2 wins for its entire range [destaddr2, destaddr2+len2)
        -- (covers both the overlap region and its exclusive tail)
        for i = 0, len2 - 1 do
            local expected = peek(sourceaddr2 + i)
            local actual   = peek(0x8000 + destaddr2 + i)
            if actual != expected then
                fail("p2@"..tostr(destaddr2+i,true).." exp "..tostr(expected,true).." got "..tostr(actual,true))
                break
            end
        end

        -- Patch 1 prefix [destaddr1, destaddr2) was not overwritten by patch 2
        for i = 0, destaddr2 - destaddr1 - 1 do
            local expected = peek(sourceaddr1 + i)
            local actual   = peek(0x8000 + destaddr1 + i)
            if actual != expected then
                fail("p1@"..tostr(destaddr1+i,true).." exp "..tostr(expected,true).." got "..tostr(actual,true))
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
