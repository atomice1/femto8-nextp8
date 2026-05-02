pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- Tests for circfill off-screen clipping behaviour.
-- Regression for: spurious pixels / buffer overwrite when circle is fully
-- or partially off the visible screen area.

#include test_fwk.lua

function check_screen_clean(label)
    for y=0,127 do
        for x=0,127 do
            if pget(x,y) != 0 then
                fail(label .. ": unexpected pixel at " .. x .. "," .. y)
                return
            end
        end
    end
end

function test_circfill_clipping()
    -- Entirely off-screen in each direction: screen must stay black.
    test_case("off_screen_left", function()
        cls()
        circfill(-50, 64, 10, 11)
        check_screen_clean("off_left")
    end)

    test_case("off_screen_right", function()
        cls()
        circfill(180, 64, 10, 11)
        check_screen_clean("off_right")
    end)

    test_case("off_screen_top", function()
        cls()
        circfill(64, -50, 10, 11)
        check_screen_clean("off_top")
    end)

    test_case("off_screen_bottom", function()
        cls()
        circfill(64, 180, 10, 11)
        check_screen_clean("off_bottom")
    end)

    -- Partially off-screen left: pixels should appear at x=0..13, nowhere else.
    test_case("partial_off_left_no_right_spill", function()
        cls()
        circfill(3, 64, 10, 11)  -- centre 3, radius 10 → spans x=-7..13
        -- Nothing should appear at x >= 14 or x >= 20
        for y=0,127 do
            check_eq(pget(14, y), 0)
            check_eq(pget(20, y), 0)
        end
    end)

    test_case("partial_off_left_pixels_visible", function()
        cls()
        circfill(3, 64, 10, 11)
        local found = false
        for y=54,74 do
            if pget(0, y) != 0 then found = true end
        end
        check_true(found)  -- something must be drawn at x=0
    end)

    -- Partially off-screen right: pixels at x >= 127-10=117..127, nothing past 127.
    test_case("partial_off_right_no_left_spill", function()
        cls()
        circfill(124, 64, 10, 11)  -- centre 124, radius 10 → spans x=114..134
        -- Nothing should appear at x <= 113 (far from centre)
        for y=0,127 do
            check_eq(pget(10, y), 0)
            check_eq(pget(0,  y), 0)
        end
    end)

    test_case("partial_off_right_pixels_visible", function()
        cls()
        circfill(124, 64, 10, 11)
        local found = false
        for y=54,74 do
            if pget(127, y) != 0 then found = true end
        end
        check_true(found)  -- something must be drawn at x=127
    end)

    -- Partially off-screen top and bottom (symmetric checks).
    test_case("partial_off_top_no_spill", function()
        cls()
        circfill(64, 3, 10, 11)
        for x=0,127 do
            check_eq(pget(x, 14), 0)
        end
    end)

    test_case("partial_off_bottom_no_spill", function()
        cls()
        circfill(64, 124, 10, 11)
        for x=0,127 do
            check_eq(pget(x, 113), 0)
        end
    end)

    -- On-screen circle must still render correctly (no regression).
    test_case("on_screen_center", function()
        cls()
        circfill(64, 64, 5, 11)
        check_ne(pget(64, 64), 0)
        check_ne(pget(64, 60), 0)  -- top point
        check_ne(pget(64, 68), 0)  -- bottom point
        check_eq(pget(64, 58), 0)  -- just outside radius
        check_eq(pget(64, 70), 0)
    end)
end

function _init()
    test_suite("circfill_clipping", test_circfill_clipping)
    summary()
end
