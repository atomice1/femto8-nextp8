pico-8 cartridge // http://www.pico-8.com
version 43
__lua__

-- Test suite for draw state and hardware state registers
-- Based on registers.txt documentation

#include test_fwk.lua

--------------------------------------------------------------------------------
-- Draw State Tests (0x5f00-0x5f3f)
--------------------------------------------------------------------------------

function test_draw_palette()
    -- 0x5f00..0x5f0f: Draw palette (pal(), palt())
    test_case("draw_palette_pal", function()
        pal(8, 14)
        check_eq(peek(0x5f08), 14)
        pal() -- reset
        check_eq(peek(0x5f08), 8)
    end)
    
    test_case("draw_palette_palt", function()
        palt(0, false)
        check_eq(peek(0x5f00) & 0xf, 0)
        palt(0, true)
        check_eq(peek(0x5f00) >= 16, true)
    end)
    
    test_case("draw_palette_transparency", function()
        palt(5, true)
        local val = peek(0x5f05)
        check_eq(val >= 16, true)
        palt(5, false)
        val = peek(0x5f05)
        check_eq(val < 16, true)
    end)
    
    test_case("draw_palette_reset", function()
        pal(1, 8)
        pal(2, 9)
        palt(3, true)
        pal()
        check_eq(peek(0x5f01), 1)
        check_eq(peek(0x5f02), 2)
        check_eq(peek(0x5f03) & 0xf, 3)
    end)
end

function test_screen_palette()
    -- 0x5f10..0x5f1f: Screen palette (pal(...,1))
    test_case("screen_palette_set", function()
        pal(7, 8, 1)
        check_eq(peek(0x5f17), 8)
        pal() -- reset
    end)
    
    test_case("screen_palette_reset", function()
        pal(3, 11, 1)
        pal(4, 12, 1)
        pal()
        check_eq(peek(0x5f13) & 0xf, 3)
        check_eq(peek(0x5f14) & 0xf, 4)
    end)
end

function test_clip_rect()
    -- 0x5f20..0x5f23: Clipping rectangle
    test_case("clip_set", function()
        clip(10, 20, 50, 40)
        check_eq(peek(0x5f20), 10)
        check_eq(peek(0x5f21), 20)
        check_eq(peek(0x5f22), 60)
        check_eq(peek(0x5f23), 60)
    end)
    
    test_case("clip_actually_clips", function()
        clip(10, 10, 10, 10)
        pset(5, 5, 7)
        pset(15, 15, 7)
        clip()
        check_eq(pget(5, 5), 0)
        check_eq(pget(15, 15), 7)
    end)
end

function test_cursor()
    -- 0x5f24: Left margin
    -- 0x5f26..0x5f27: Print cursor
    test_case("cursor_set", function()
        cursor(10, 20)
        check_eq(peek(0x5f26), 10)
        check_eq(peek(0x5f27), 20)
    end)
    
    test_case("cursor_left_margin", function()
        cursor(15)
        check_eq(peek(0x5f24), 15)
    end)
end

function test_pen_color()
    -- 0x5f25: Pen color
    test_case("pen_color_set", function()
        color(12)
        check_eq(peek(0x5f25) & 0xf, 12)
    end)
end

function test_camera()
    -- 0x5f28..0x5f2b: Camera position (16-bit signed)
    test_case("camera_set", function()
        camera(10, 20)
        check_eq(peek(0x5f28), 10)
        check_eq(peek(0x5f29), 0)
        check_eq(peek(0x5f2a), 20)
        check_eq(peek(0x5f2b), 0)
    end)
    
    test_case("camera_negative", function()
        camera(-10, -20)
        check_eq(peek(0x5f28), 246)
        check_eq(peek(0x5f29), 255)
        check_eq(peek(0x5f2a), 236)
        check_eq(peek(0x5f2b), 255)
    end)
    
    test_case("camera_affects_drawing", function()
        camera(10, 10)
        pset(10, 10, 7)
        camera()
        check_eq(pget(0, 0), 7)
    end)
end

function test_fillp()
    -- 0x5f31..0x5f33: Fill pattern
    test_case("fillp_affects_rectfill", function()
        fillp(0b1010101010101010)
        rectfill(0, 0, 7, 0, 7)
        local has_pattern = false
        for x=0,7 do
            local c = pget(x, 0)
            if c == 0 then
                has_pattern = true
            end
        end
        check_eq(has_pattern, true)
    end)
    
    test_case("fillp_secondary_color", function()
        fillp(0b1010101010101010)
        color(0x17)
        rectfill(0, 0, 3, 0)
        color(7)
        local found_primary = false
        local found_secondary = false
        for x=0,3 do
            local c = pget(x, 0)
            if c == 7 then found_primary = true end
            if c == 1 then found_secondary = true end
        end
        check_eq(found_primary or found_secondary, true)
    end)
end

function test_line_endpoint()
    -- 0x5f35: Line endpoint valid flag
    -- 0x5f3c..0x5f3f: Line endpoint coordinates
    test_case("line_no_args_invalidates", function()
        line(10, 20, 30, 40)
        line()
        check_eq(peek(0x5f35), 1)
    end)
    
    test_case("line_continues_from_endpoint", function()
        line(10, 20, 30, 40)
        line(50, 60)
        check_eq(peek(0x5f3c), 50)
        check_eq(peek(0x5f3e), 60)
    end)
    
    test_case("line_negative_coords", function()
        line(-10, -20, -30, -40)
        check_eq(peek(0x5f3c), 226)
        check_eq(peek(0x5f3d), 255)
        check_eq(peek(0x5f3e), 216)
        check_eq(peek(0x5f3f), 255)
    end)
end

function test_chipset_features()
    -- 0x5f36: Miscellaneous chipset features
    test_case("chipset_no_newline", function()
        poke(0x5f36, 0x04)
        cursor(0, 0)
        print("a")
        local y1 = peek(0x5f27)
        print("b")
        local y2 = peek(0x5f27)
        check_eq(y1, y2)
        poke(0x5f36, 0)
    end)
    
    test_case("chipset_char_wrap", function()
        poke(0x5f36, 0x80)
        cursor(120, 0)
        print("abcdefgh")
        check_eq(peek(0x5f27) > 0, true)
        poke(0x5f36, 0)
    end)
end

--------------------------------------------------------------------------------
-- Hardware State Tests (0x5f40-0x5f7f)
--------------------------------------------------------------------------------

function test_rng_state()
    -- 0x5f44..0x5f4b: RNG state
    test_case("rng_changes", function()
        local state = {}
        for i=0,7 do
            state[i] = peek(0x5f44 + i)
        end
        local val = rnd()
        local changed = false
        for i=0,7 do
            if peek(0x5f44 + i) != state[i] then
                changed = true
                break
            end
        end
        check_eq(changed, true)
    end)
    
    test_case("rng_save_restore", function()
        local state = {}
        for i=0,7 do
            state[i] = peek(0x5f44 + i)
        end
        rnd()
        for i=0,7 do
            poke(0x5f44 + i, state[i])
        end
        for i=0,7 do
            check_eq(peek(0x5f44 + i), state[i])
        end
    end)
end

function test_memory_mapping()
    -- 0x5f54: Sprite sheet mapping
    -- 0x5f55: Screen mapping
    -- 0x5f56: Map region
    test_case("sprite_to_screen_mapping", function()
        poke(0x5f54, 0x60)
        poke(0x6000, 0x42)
        check_eq(peek(0x0000), 0x42)
        poke(0x5f54, 0)
    end)
    
    test_case("sprite_to_extended_ram", function()
        poke(0x5f54, 0x80)
        poke(0x8000, 0x43)
        check_eq(peek(0x0000), 0x43)
        poke(0x5f54, 0)
    end)
    
    test_case("screen_to_sprites", function()
        poke(0x5f55, 0)
        pset(0, 0, 7)
        check_eq(peek(0x0000) & 0x0f, 7)
        poke(0x5f55, 0x60)
    end)
    
    test_case("map_scan_memory", function()
        poke(0x5f54, 0x0)
        poke(0x5f55, 0x60)
        poke(0x5f56, 0x20)
        -- Scan memory to find where mset actually writes data
        -- for different map_start values
        
        local function find_value(val)
            for addr=0x0000,0x3fff do
                if peek(addr) == val then
                    return addr
                end
            end
            for addr=0x8000,0xffff do
                if peek(addr) == val then
                    return addr
                end
            end
            return nil
        end
        
        local function test_map_start(map_start, celx, cely, marker, expected_addr)
            -- Clear memory
            for addr=0x0000,0x3fff do
                poke(addr, 0)
            end
            for addr=0x8000,0xffff do
                poke(addr, 0)
            end
            
            -- Set map_start and write a unique marker
            poke(0x5f56, map_start)
            poke(0x5f57, 128)
            mset(celx, cely, marker)
            
            -- Find where it went
            local addr = find_value(marker)
            
            printh("map_start = " .. tostr(map_start, 1) .. ", celx=" .. tostr(celx) .. ", cely=" .. tostr(cely) .. ", address=" .. tostr(addr) .. ", expected=" .. tostr(expected_addr));

            -- Check against expected address
            if expected_addr == nil then
                check_eq(addr, nil)
            else
                check_eq(addr, expected_addr)
            end
        end
        
        -- Test different map_start values (expectations from PICO-8 behavior)
        test_map_start(0x20, 0, 0, 11, 0x2000)   -- 0x20 row 0 -> 0x2000
        test_map_start(0x20, 0, 32, 12, 0x1000)  -- 0x20 row 32 -> 0x3000 redirected to 0x1000
        test_map_start(0x20, 0, 63, 13, 0x1f80)  -- 0x20 row 63 -> 0x3f80 redirected to 0x1f80
        test_map_start(0x21, 0, 0, 14, 0x2100)   -- 0x21 row 0 -> 0x2100
        test_map_start(0x21, 0, 32, 15, 0x1000)  -- 0x21 row 32 -> 0x3100 redirected to 0x1000
        test_map_start(0x21, 0, 63, 16, nil)     -- 0x21 row 63 -> 0x3f80 out of range
        test_map_start(0x00, 0, 0, 17, 0x2000)   -- 0x00 masked to 0x00 -> 0x2000 (wraps?)
        test_map_start(0x00, 0, 32, 18, 0x1000)  -- 0x00 row 32 -> 0x1000
        test_map_start(0x01, 0, 0, 19, 0x2000)   -- 0x01 masked to 0x01 -> 0x2000 (wraps?)
        test_map_start(0x01, 0, 32, 20, 0x1000)  -- 0x01 row 32 -> 0x1000
        test_map_start(0x10, 0, 0, 21, nil)      -- 0x10 masked to 0x00 -> NOT FOUND
        test_map_start(0x10, 0, 32, 22, nil)     -- 0x10 masked to 0x00 -> NOT FOUND
        test_map_start(0x11, 0, 0, 23, nil)      -- 0x11 masked to 0x01 -> NOT FOUND
        test_map_start(0x11, 0, 32, 24, nil)     -- 0x11 masked to 0x01 -> NOT FOUND
        test_map_start(0x30, 0, 0, 25, nil)      -- 0x30 masked to 0x00 -> NOT FOUND
        test_map_start(0x30, 0, 32, 26, nil)     -- 0x30 masked to 0x00 -> NOT FOUND
        test_map_start(0x31, 0, 0, 27, nil)      -- 0x31 masked to 0x01 -> NOT FOUND
        test_map_start(0x31, 0, 32, 28, nil)     -- 0x31 masked to 0x01 -> NOT FOUND
        test_map_start(0x40, 0, 0, 29, 0x2000)   -- 0x40 row 0 -> 0x2000 (wraps?)
        test_map_start(0x40, 0, 32, 30, 0x1000)  -- 0x40 row 32 -> 0x1000
        test_map_start(0x50, 0, 0, 31, 0x2000)   -- 0x50 row 0 -> 0x2000 (wraps?)
        test_map_start(0x50, 0, 32, 32, 0x1000)  -- 0x50 row 32 -> 0x1000
        test_map_start(0x60, 0, 0, 33, 0x2000)   -- 0x60 row 0 -> 0x2000 (wraps?)
        test_map_start(0x60, 0, 32, 34, 0x1000)  -- 0x60 row 32 -> 0x1000
        test_map_start(0x70, 0, 0, 35, 0x2000)   -- 0x70 row 0 -> 0x2000 (wraps?)
        test_map_start(0x70, 0, 32, 36, 0x1000)  -- 0x70 row 32 -> 0x1000
        test_map_start(0x80, 0, 0, 37, 0x8000)   -- 0x80 row 0 -> 0x8000 (direct)
        test_map_start(0x80, 0, 32, 38, 0x9000)  -- 0x80 row 32 -> 0x9000 (direct)
        test_map_start(0x81, 0, 0, 39, 0x8100)   -- 0x81 row 0 -> 0x8100 (direct)
        test_map_start(0x81, 0, 32, 40, 0x9100)  -- 0x81 row 32 -> 0x9100 (direct)
        test_map_start(0xff, 0, 0, 41, 0xff00)   -- 0xff row 0 -> 0xff00 (direct)
        test_map_start(0xff, 0, 32, 42, nil)     -- 0xff row 32 -> out of range
        test_map_start(0xff, 0, 63, 43, nil)     -- 0xff row 63 -> out of range
    end)
    
    test_case("map_lower_region", function()
        -- Default map starts at 0x2000
        poke(0x5f56, 0x20)
        poke(0x5f57, 128)
        mset(0, 0, 42)
        check_eq(peek(0x2000), 42)
        
        -- 0x21 is a valid address
        poke(0x5f56, 0x21)
        mset(0, 0, 77)
        check_eq(peek(0x2100), 77)
        check_eq(peek(0x2000), 42)  -- Previous data unchanged
        
        -- 0x80+ work directly
        poke(0x5f56, 0x80)
        mset(0, 0, 88)
        check_eq(peek(0x8000), 88)
        
        -- Verify 0x20 still has original data
        poke(0x5f56, 0x20)
        check_eq(mget(0, 0), 42)
        
        -- And 0x21 still has its data
        poke(0x5f56, 0x21)
        check_eq(mget(0, 0), 77)
    end)
    
    test_case("map_split_default", function()
        -- Default map (0x20) with width 128:
        -- Rows 0-31 map to 0x2000-0x2fff (no redirect)
        -- Rows 32-63 map to 0x3000-0x3fff (redirected to 0x1000-0x1fff)
        poke(0x5f56, 0x20)
        poke(0x5f57, 128)
        
        -- Upper map (y=0-31): 0x2000 + y*128
        mset(0, 0, 50)
        mset(0, 31, 51)
        check_eq(peek(0x2000), 50)
        check_eq(peek(0x2000 + 31 * 128), 51)
        
        -- Lower map (y=32-63): 0x2000 + y*128 → 0x3000+ → redirected to 0x1000+
        mset(0, 32, 60)
        mset(0, 63, 61)
        check_eq(peek(0x1000), 60)
        check_eq(peek(0x1000 + 31 * 128), 61)
    end)
    
    test_case("map_width", function()
        -- Default width is 128
        poke(0x5f56, 0x20)
        mset(0, 0, 10)
        mset(0, 1, 20)
        check_eq(peek(0x2000), 10)
        check_eq(peek(0x2000 + 128), 20)
        
        -- Set width to 64
        poke(0x5f57, 64)
        mset(0, 1, 30)
        check_eq(peek(0x2000 + 64), 30)
        
        -- Width of 0 means 256
        poke(0x5f57, 0)
        mset(0, 1, 40)
        check_eq(peek(0x2000 + 256), 40)
        
        -- Reset to default
        poke(0x5f57, 128)
    end)
    
    test_case("screen_to_extended_ram", function()
        poke(0x5f55, 0x80)
        pset(0, 0, 9)
        poke(0x5f55, 0x60)
        check_eq(peek(0x8000) & 0x0f, 9)
    end)
end

function test_print_attributes()
    -- 0x5f58..0x5f5b: Default print attributes
    test_case("print_attr_wide", function()
        poke(0x5f58, 0x05)
        cursor(0, 0)
        print("a")
        poke(0x5f58, 0)
    end)
    
    test_case("print_attr_tall", function()
        poke(0x5f58, 0x09)
        cursor(0, 0)
        print("a")
        poke(0x5f58, 0)
    end)
    
    test_case("print_attr_solid_bg", function()
        poke(0x5f58, 0x11)
        cursor(0, 0)
        pset(0, 0, 8)
        print("a")
        poke(0x5f58, 0)
    end)
end

function test_read_write_mask()
    -- 0x5f5e: Read/write mask
    test_case("rw_mask_write_planes", function()
        pset(0, 0, 0)
        poke(0x5f5e, 0xff)
        pset(0, 0, 15)
        local c1 = pget(0, 0)
        poke(0x5f5e, 0x0f)
        pset(0, 0, 0)
        local c2 = pget(0, 0)
        poke(0x5f5e, 255)
        check_eq(c1, 15)
        check_eq(c2, 0)
    end)
    
    test_case("rw_mask_read_bits", function()
        poke(0x5f5e, 0xf0)
        pset(0, 0, 15)
        poke(0x5f5e, 255)
    end)
end

--------------------------------------------------------------------------------
-- Hex Literal Tests
--------------------------------------------------------------------------------

function test_hex_literals()
    -- Test that hex literals in for loops are interpreted correctly
    -- In PICO-8's fixed point format, 0x8000 = -32768.0 and 0xffff = -1.0
    
    test_case("hex_for_loop_negative", function()
        local count = 0
        for i=0x8000,0xffff do
            count += 1
            if count > 100 then break end  -- Safety limit
        end
        -- 0x8000 = -32768, 0xffff = -1
        -- Loop should iterate from -32768 to -1, which is 32768 iterations
        check_eq(count > 100, true)  -- Should hit our safety limit
    end)
    
    test_case("hex_literal_values", function()
        -- Verify the actual values of hex literals
        check_eq(0x8000 < 0, true)  -- 0x8000 should be negative
        check_eq(0xffff < 0, true)  -- 0xffff should be negative
        check_eq(0x7fff > 0, true)  -- 0x7fff should be positive
        
        -- Check specific values
        check_eq(0x8000, -32768.0)
        check_eq(0xffff, -1.0)
        check_eq(0x7fff, 32767.0)
    end)
    
    test_case("hex_for_loop_positive", function()
        local count = 0
        for i=0x0001,0x0005 do
            count += 1
        end
        check_eq(count, 5)
    end)
    
    test_case("hex_for_loop_wraparound", function()
        -- Test loop that would wrap around
        local count = 0
        for i=0xfffe,0x0002 do
            count += 1
            if count > 100 then break end
        end
        -- 0xfffe = -2, 0x0002 = 2
        -- Should iterate -2, -1, 0, 1, 2 = 5 iterations
        check_eq(count, 5)
    end)
end

--------------------------------------------------------------------------------
-- Run all tests
--------------------------------------------------------------------------------

test_suite("draw_palette", test_draw_palette)
test_suite("screen_palette", test_screen_palette)
test_suite("clip_rect", test_clip_rect)
test_suite("cursor", test_cursor)
test_suite("pen_color", test_pen_color)
test_suite("camera", test_camera)
test_suite("fillp", test_fillp)
test_suite("line_endpoint", test_line_endpoint)
test_suite("chipset_features", test_chipset_features)
test_suite("rng_state", test_rng_state)
test_suite("memory_mapping", test_memory_mapping)
test_suite("print_attributes", test_print_attributes)
test_suite("read_write_mask", test_read_write_mask)
test_suite("hex_literals", test_hex_literals)

summary()

__gfx__
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
