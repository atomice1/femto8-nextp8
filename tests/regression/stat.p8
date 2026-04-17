pico-8 cartridge // http://www.pico-8.com
version 43
__lua__

-- Test suite for stat() function
-- Based on stat.txt documentation

#include test_fwk.lua

--------------------------------------------------------------------------------
-- Memory and CPU Tests (0-2)
--------------------------------------------------------------------------------

function test_memory_usage()
    -- stat(0): Memory usage in KiB
    test_case("memory_usage_range", function()
        local mem = stat(0)
        check_type(mem, "number")
        check_range(mem, 0, 2048)
    end)
    
    test_case("memory_usage_increases", function()
        local mem1 = stat(0)
        local t = {}
        for i=1,1000 do
            t[i] = {x=i, y=i*2, data="test"}
        end
        local mem2 = stat(0)
        check_eq(mem2 > mem1, true)
    end)
    
    test_case("memory_gc_reduces_usage", function()
        local t = {}
        for i=1,1000 do
            t[i] = {data="garbage"}
        end
        local mem_with_garbage = stat(0)
        t = nil
        -- stat(0) itself triggers GC
        local mem_after_gc = stat(0)
        -- Memory should decrease or stay same after GC
        check_eq(mem_after_gc <= mem_with_garbage, true)
    end)
end

function test_cpu_usage()
    -- stat(1): Total CPU usage
    test_case("cpu_usage_range", function()
        local cpu = stat(1)
        check_type(cpu, "number")
        check_range(cpu, 0, 10)
    end)
    
    test_case("cpu_usage_progresses", function()
        local cpu1 = stat(1)
        -- Do some work
        for i=1,100 do
            local x = i * i
        end
        local cpu2 = stat(1)
        -- Should progress (or stay same if very fast)
        check_eq(cpu2 >= cpu1, true)
    end)
    
    -- stat(2): System CPU usage
    test_case("system_cpu_usage", function()
        local syscpu = stat(2)
        check_type(syscpu, "number")
        check_range(syscpu, 0, 10)
    end)
    
    test_case("system_cpu_increases_with_work", function()
        local cpu1 = stat(2)
        for i=1,50 do
            cls()
        end
        local cpu2 = stat(2)
        check_eq(cpu2 > cpu1, true)
    end)
end

--------------------------------------------------------------------------------
-- Display Tests (3, 11)
--------------------------------------------------------------------------------

function test_display()
    -- stat(3): Current display
    test_case("current_display_valid", function()
        local disp = stat(3)
        check_type(disp, "number")
        check_range(disp, 0, 3)
    end)
    
    -- stat(11): Number of displays
    test_case("num_displays_valid", function()
        local num = stat(11)
        check_type(num, "number")
        check_range(num, 1, 4)
    end)
end

--------------------------------------------------------------------------------
-- Version and Environment Tests (5, 102)
--------------------------------------------------------------------------------

function test_version()
    -- stat(5): PICO-8 version
    test_case("version_number", function()
        local ver = stat(5)
        check_type(ver, "number")
        check_eq(ver > 0, true)
    end)
    
    -- stat(102): Site information
    test_case("site_info", function()
        local site = stat(102)
        -- Can be string (BBS) or number (local)
        local valid = type(site) == "string" or type(site) == "number"
        check_eq(valid, true)
    end)
end

--------------------------------------------------------------------------------
-- Frame Rate Tests (7-9)
--------------------------------------------------------------------------------

function test_frame_rate()
    -- stat(7): Current frame rate
    test_case("current_fps", function()
        local fps = stat(7)
        check_type(fps, "number")
        check_range(fps, 0, 120)
    end)
    
    -- stat(8): Target frame rate
    test_case("target_fps", function()
        local target = stat(8)
        check_type(target, "number")
        -- Should be 30 or 60
        local valid = target == 30 or target == 60
        check_eq(valid, true)
    end)
    
    test_case("current_fps_near_target", function()
        local current = stat(7)
        local target = stat(8)
        -- Current should be reasonably close to target (within 50%)
        check_eq(current > target * 0.5, true)
    end)
    
    -- stat(9): PICO-8 frame rate
    test_case("pico8_fps", function()
        local p8fps = stat(9)
        check_type(p8fps, "number")
        check_range(p8fps, 0, 1000)
    end)
end

--------------------------------------------------------------------------------
-- Pause Menu Tests (12-15)
--------------------------------------------------------------------------------

function test_pause_menu()
    -- stat(12-15): Pause menu location
    test_case("pause_menu_coords", function()
        local x1 = stat(12)
        local y1 = stat(13)
        local x2 = stat(14)
        local y2 = stat(15)
        
        check_type(x1, "number")
        check_type(y1, "number")
        check_type(x2, "number")
        check_type(y2, "number")
        
        -- x2 should be >= x1, y2 should be >= y1
        check_eq(x2 >= x1, true)
        check_eq(y2 >= y1, true)
    end)
    
    test_case("pause_menu_reasonable_size", function()
        local x1 = stat(12)
        local y1 = stat(13)
        local x2 = stat(14)
        local y2 = stat(15)
        
        local width = x2 - x1
        local height = y2 - y1
        
        -- Menu should be reasonable size (at least 20x20, max 128x128)
        check_eq(width >= 20 and width <= 128, true)
        check_eq(height >= 20 and height <= 128, true)
    end)
end

--------------------------------------------------------------------------------
-- Audio Tests (46-56)
--------------------------------------------------------------------------------

function test_audio_status()
    -- stat(46-49): Sound effect playing on channels
    test_case("sfx_channels_idle", function()
        for ch=0,3 do
            local sfx = stat(46 + ch)
            check_eq(sfx, -1)
        end
    end)
    
    -- stat(50-53): Note number on channels
    test_case("note_channels_idle", function()
        for ch=0,3 do
            local note = stat(50 + ch)
            check_eq(note, -1)
        end
    end)
    
    -- stat(54): Music pattern ID
    test_case("music_pattern_idle", function()
        local pat = stat(54)
        check_eq(pat, -1)
    end)
    
    -- stat(55): Music patterns played
    test_case("music_count_idle", function()
        local count = stat(55)
        check_type(count, "number")
        check_range(count, -1, 1000)
    end)
    
    -- stat(56): Music ticks
    test_case("music_ticks_idle", function()
        local ticks = stat(56)
        check_type(ticks, "number")
        check_range(ticks, -1, 1000)
    end)
end

--------------------------------------------------------------------------------
-- Time Tests (80-85, 90-95)
--------------------------------------------------------------------------------

function test_time()
    -- stat(90-95): Local time
    test_case("local_time_year", function()
        local year = stat(90)
        check_type(year, "number")
        check_range(year, 2000, 2100)
    end)
    
    test_case("local_time_month", function()
        local month = stat(91)
        check_type(month, "number")
        check_range(month, 1, 12)
    end)
    
    test_case("local_time_day", function()
        local day = stat(92)
        check_type(day, "number")
        check_range(day, 1, 31)
    end)
    
    test_case("local_time_hour", function()
        local hour = stat(93)
        check_type(hour, "number")
        check_range(hour, 0, 23)
    end)
    
    test_case("local_time_minute", function()
        local min = stat(94)
        check_type(min, "number")
        check_range(min, 0, 59)
    end)
    
    test_case("local_time_second", function()
        local sec = stat(95)
        check_type(sec, "number")
        check_range(sec, 0, 61)
    end)
    
    test_case("time_is_consistent", function()
        local sec1 = stat(95)
        local sec2 = stat(95)
        -- Seconds should be same or advance by 1
        local diff = sec2 - sec1
        check_eq(diff >= 0 and diff <= 1, true)
    end)
    
    -- stat(80-85): UTC time
    test_case("utc_time_valid", function()
        local year = stat(80)
        local month = stat(81)
        local day = stat(82)
        
        check_type(year, "number")
        check_type(month, "number")
        check_type(day, "number")
        
        check_range(year, 2000, 2100)
        check_range(month, 1, 12)
        check_range(day, 1, 31)
    end)
    
    test_case("utc_local_reasonable_offset", function()
        local utc_hour = stat(83)
        local local_hour = stat(93)
        -- Offset should be within 24 hours
        local diff = (local_hour - utc_hour + 24) % 24
        check_eq(diff < 24, true)
    end)
end

--------------------------------------------------------------------------------
-- Garbage Collection Tests (0, 99)
--------------------------------------------------------------------------------

function test_garbage_collection()
    -- stat(99): Raw GC value
    test_case("gc_raw_vs_collected", function()
        local raw = stat(99)
        local collected = stat(0)
        
        check_type(raw, "number")
        check_type(collected, "number")
        
        -- Both should be in valid range
        check_range(raw, 0, 2048)
        check_range(collected, 0, 2048)
    end)
    
    test_case("stat0_triggers_gc", function()
        -- Create garbage
        local t = {}
        for i=1,500 do
            t[i] = {data="test"}
        end
        t = nil
        
        local raw = stat(99)
        local collected = stat(0)
        -- stat(0) should trigger GC, so collected <= raw
        check_eq(collected <= raw, true)
    end)
end

--------------------------------------------------------------------------------
-- Misc Tests
--------------------------------------------------------------------------------

function test_clipboard()
    -- stat(4): Clipboard (may not be accessible)
    test_case("clipboard_type", function()
        local clip = stat(4)
        -- Should be string or nil
        local valid = type(clip) == "string" or type(clip) == "nil"
        check_eq(valid, true)
    end)
end

function test_load_param()
    -- stat(6): Load parameter
    test_case("load_param_type", function()
        local param = stat(6)
        -- Should be string or nil
        local valid = type(param) == "string" or type(param) == "nil"
        check_eq(valid, true)
    end)
    
    -- stat(100): Breadcrumb
    test_case("breadcrumb_type", function()
        local bc = stat(100)
        local valid = type(bc) == "string" or type(bc) == "nil"
        check_eq(valid, true)
    end)
end

function test_bbs_info()
    -- stat(101): BBS ID
    test_case("bbs_id_type", function()
        local id = stat(101)
        local valid = type(id) == "number" or type(id) == "nil"
        check_eq(valid, true)
    end)
end

function test_unknown_stats()
    -- stat(10), stat(27): Unknown but should return number
    test_case("unknown_stats_exist", function()
        local val10 = stat(10)
        local val27 = stat(27)
        
        check_type(val10, "number")
        check_type(val27, "number")
    end)
end

function test_frame_by_frame()
    -- stat(110): Frame-by-frame mode
    test_case("frame_mode_normal", function()
        local mode = stat(110)
        -- Can be number or boolean
        local valid = type(mode) == "number" or type(mode) == "boolean"
        check_eq(valid, true)
        -- Should be falsy in normal execution
        if type(mode) == "number" then
            check_eq(mode, 0)
        else
            check_eq(mode, false)
        end
    end)
end

function test_current_path()
    -- stat(124): Current path
    test_case("current_path_type", function()
        local path = stat(124)
        check_type(path, "string")
    end)
end

--------------------------------------------------------------------------------
-- Run all tests
--------------------------------------------------------------------------------

test_suite("memory_usage", test_memory_usage)
test_suite("cpu_usage", test_cpu_usage)
test_suite("display", test_display)
test_suite("version", test_version)
test_suite("frame_rate", test_frame_rate)
test_suite("pause_menu", test_pause_menu)
test_suite("audio_status", test_audio_status)
test_suite("time", test_time)
test_suite("garbage_collection", test_garbage_collection)
test_suite("clipboard", test_clipboard)
test_suite("load_param", test_load_param)
test_suite("bbs_info", test_bbs_info)
test_suite("unknown_stats", test_unknown_stats)
test_suite("frame_by_frame", test_frame_by_frame)
test_suite("current_path", test_current_path)

summary()

__gfx__
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
