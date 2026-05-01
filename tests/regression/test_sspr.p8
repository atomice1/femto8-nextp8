pico-8 cartridge // http://www.pico-8.com
version 43
__lua__

-- test_sspr.p8: tests for sspr() scaled sprite drawing
-- sprite 0 is concentric squares using colors 1, 2, 3:
--   rows 0 & 7  : 11111111  (outer border)
--   rows 1 & 6  : 12222221  (inner ring)
--   rows 2-5    : 12333321  (center)
-- color 0 is the default transparent color so all drawn colors (1,2,3) are opaque.

#include test_fwk.lua

function ssetup()
    camera(0,0)
    clip(0,0,128,128)
    pal()
    palt()
    cls(15)  -- clear to 15 (distinct from colors 1,2,3 used by the sprite)
end

-- expected color of sprite-0 pixel at (sx, sy)
function sp(sx, sy)
    if sy == 0 or sy == 7 then return 1 end
    if sx == 0 or sx == 7 then return 1 end
    if sy == 1 or sy == 6 then return 2 end
    if sx == 1 or sx == 6 then return 2 end
    return 3
end

------------------------------------------------------------------------
-- on-screen cases (all of dest rect is within the 128x128 screen)
------------------------------------------------------------------------

function test_on_screen()
    test_case("1to1", function()
        ssetup()
        sspr(0, 0, 8, 8, 10, 10)
        -- corners → outer border color 1
        check_pixel(10, 10, 1)    -- src(0,0)
        check_pixel(17, 10, 1)    -- src(7,0)
        check_pixel(10, 17, 1)    -- src(0,7)
        check_pixel(17, 17, 1)    -- src(7,7)
        -- inner ring → color 2
        check_pixel(11, 11, 2)    -- src(1,1)
        check_pixel(16, 16, 2)    -- src(6,6)
        -- center → color 3
        check_pixel(12, 12, 3)    -- src(2,2)
        check_pixel(15, 15, 3)    -- src(5,5)
        -- adjacent pixels must remain background
        check_pixel( 9, 10, 15)
        check_pixel(18, 10, 15)
        check_pixel(10,  9, 15)
        check_pixel(10, 18, 15)
    end)

    test_case("scale_up_2x", function()
        ssetup()
        sspr(0, 0, 8, 8, 20, 20, 16, 16)
        -- dst_offset k -> src_col = flr(k*8/16) = flr(k/2)
        -- offsets 0,1 -> src 0;  2,3 -> src 1;  4,5 -> src 2; etc.
        check_pixel(20, 20, sp(0,0))   -- src(0,0)=1
        check_pixel(21, 20, sp(0,0))   -- src(0,0)=1  (same texel)
        check_pixel(22, 22, sp(1,1))   -- src(1,1)=2
        check_pixel(23, 23, sp(1,1))   -- src(1,1)=2
        check_pixel(24, 24, sp(2,2))   -- src(2,2)=3
        check_pixel(25, 24, sp(2,2))   -- src(2,2)=3
        check_pixel(34, 34, sp(7,7))   -- src(7,7)=1
        check_pixel(35, 35, sp(7,7))   -- src(7,7)=1  (last texel block)
        -- adjacent background
        check_pixel(19, 20, 15)
        check_pixel(36, 20, 15)
        check_pixel(20, 19, 15)
        check_pixel(20, 36, 15)
    end)

    test_case("scale_down_2x", function()
        ssetup()
        sspr(0, 0, 8, 8, 20, 20, 4, 4)
        -- dst_offset k -> src_col = flr(k*8/4) = k*2
        -- offset 0->src 0; 1->src 2; 2->src 4; 3->src 6
        check_pixel(20, 20, sp(0,0))   -- src(0,0)=1
        check_pixel(21, 20, sp(2,0))   -- src(2,0)=1
        check_pixel(22, 20, sp(4,0))   -- src(4,0)=1
        check_pixel(23, 20, sp(6,0))   -- src(6,0)=1
        check_pixel(21, 21, sp(2,2))   -- src(2,2)=3
        check_pixel(22, 22, sp(4,4))   -- src(4,4)=3
        check_pixel(21, 23, sp(2,6))   -- src(2,6)=2
        check_pixel(23, 23, sp(6,6))   -- src(6,6)=2
        -- adjacent background
        check_pixel(19, 20, 15)
        check_pixel(24, 20, 15)
        check_pixel(20, 19, 15)
        check_pixel(20, 24, 15)
    end)
end

------------------------------------------------------------------------
-- partially off-screen (1:1 scale)
-- key invariant: visible pixels must sample the *correct* source texels,
-- i.e. those that correspond to the clipped portion of the dest rect.
------------------------------------------------------------------------

function test_partial_1to1()
    test_case("off_left", function()
        ssetup()
        sspr(0, 0, 8, 8, -4, 10)
        -- visible: x=0..3, y=10..17
        -- dst_offset_x = x-(-4) = x+4 -> src cols 4,5,6,7
        check_pixel(0, 10, sp(4,0))   -- sp(4,0)=1
        check_pixel(3, 10, sp(7,0))   -- sp(7,0)=1
        check_pixel(0, 11, sp(4,1))   -- sp(4,1)=2
        check_pixel(1, 11, sp(5,1))   -- sp(5,1)=2
        check_pixel(2, 11, sp(6,1))   -- sp(6,1)=2
        check_pixel(3, 11, sp(7,1))   -- sp(7,1)=1
        check_pixel(0, 12, sp(4,2))   -- sp(4,2)=3
        check_pixel(1, 12, sp(5,2))   -- sp(5,2)=3
        check_pixel(2, 12, sp(6,2))   -- sp(6,2)=2
        check_pixel(3, 12, sp(7,2))   -- sp(7,2)=1
        -- x=4 is outside the dest rect; must stay background
        check_pixel(4, 10, 15)
        check_pixel(4, 17, 15)
        -- row above sprite must be untouched (detects off-screen wrap writes)
        check_pixel(124, 9, 15)
        check_pixel(125, 9, 15)
        check_pixel(126, 9, 15)
        check_pixel(127, 9, 15)
        -- rightmost visible pixel of rows inside sprite must be unaffected
        check_pixel(124, 10, 15)
        check_pixel(124, 17, 15)
    end)

    test_case("off_right", function()
        ssetup()
        sspr(0, 0, 8, 8, 124, 10)
        -- visible: x=124..127, y=10..17
        -- dst_offset_x = x-124 -> src cols 0,1,2,3
        check_pixel(124, 10, sp(0,0))  -- sp(0,0)=1
        check_pixel(127, 10, sp(3,0))  -- sp(3,0)=1
        check_pixel(124, 11, sp(0,1))  -- sp(0,1)=1
        check_pixel(125, 11, sp(1,1))  -- sp(1,1)=2
        check_pixel(126, 11, sp(2,1))  -- sp(2,1)=2
        check_pixel(127, 11, sp(3,1))  -- sp(3,1)=2
        check_pixel(124, 12, sp(0,2))  -- sp(0,2)=1
        check_pixel(125, 12, sp(1,2))  -- sp(1,2)=2
        check_pixel(126, 12, sp(2,2))  -- sp(2,2)=3
        check_pixel(127, 12, sp(3,2))  -- sp(3,2)=3
        -- x=123 is outside the dest rect; must stay background
        check_pixel(123, 10, 15)
        check_pixel(123, 17, 15)
        -- row above and below sprite must be untouched
        check_pixel(124,  9, 15)
        check_pixel(124, 18, 15)
    end)

    test_case("off_top", function()
        ssetup()
        sspr(0, 0, 8, 8, 10, -4)
        -- visible: x=10..17, y=0..3
        -- dst_offset_y = y-(-4) = y+4 -> src rows 4,5,6,7
        check_pixel(10, 0, sp(0,4))   -- sp(0,4)=1
        check_pixel(11, 0, sp(1,4))   -- sp(1,4)=2
        check_pixel(12, 0, sp(2,4))   -- sp(2,4)=3
        check_pixel(17, 0, sp(7,4))   -- sp(7,4)=1
        check_pixel(10, 1, sp(0,5))   -- sp(0,5)=1
        check_pixel(11, 1, sp(1,5))   -- sp(1,5)=2
        check_pixel(10, 2, sp(0,6))   -- sp(0,6)=1
        check_pixel(11, 2, sp(1,6))   -- sp(1,6)=2
        check_pixel(10, 3, sp(0,7))   -- sp(0,7)=1
        check_pixel(11, 3, sp(1,7))   -- sp(1,7)=1
        -- y=4 is outside; must stay background
        check_pixel(10, 4, 15)
        check_pixel(17, 4, 15)
        -- cols outside sprite must be untouched
        check_pixel(9, 0, 15)
        check_pixel(18, 0, 15)
    end)

    test_case("off_bottom", function()
        ssetup()
        sspr(0, 0, 8, 8, 10, 124)
        -- visible: x=10..17, y=124..127
        -- dst_offset_y = y-124 -> src rows 0,1,2,3
        check_pixel(10, 124, sp(0,0))  -- sp(0,0)=1
        check_pixel(11, 124, sp(1,0))  -- sp(1,0)=1
        check_pixel(10, 125, sp(0,1))  -- sp(0,1)=1
        check_pixel(11, 125, sp(1,1))  -- sp(1,1)=2
        check_pixel(12, 125, sp(2,1))  -- sp(2,1)=2
        check_pixel(10, 126, sp(0,2))  -- sp(0,2)=1
        check_pixel(12, 126, sp(2,2))  -- sp(2,2)=3
        check_pixel(10, 127, sp(0,3))  -- sp(0,3)=1
        check_pixel(12, 127, sp(2,3))  -- sp(2,3)=3
        -- cols outside sprite must stay background
        check_pixel( 9, 124, 15)
        check_pixel(18, 124, 15)
        -- row above sprite must be untouched
        check_pixel(10, 123, 15)
        check_pixel(17, 123, 15)
    end)
end

------------------------------------------------------------------------
-- partially off-screen with scaling
------------------------------------------------------------------------

function test_partial_scaled()
    test_case("scale_up_off_left", function()
        ssetup()
        sspr(0, 0, 8, 8, -8, 10, 16, 16)
        -- dest: x=-8..7; visible: x=0..7
        -- dst_offset = x+8; src_col = flr((x+8)*8/16) = flr((x+8)/2)
        -- x=0..1: src_col=4; x=2..3: src_col=5; x=4..5: src_col=6; x=6..7: src_col=7
        -- y_offset k -> src_row = flr(k*8/16) = flr(k/2)
        -- y=10..11: src_row=0; y=12..13: src_row=1; y=14..15: src_row=2
        check_pixel(0, 10, sp(4,0))   -- 1
        check_pixel(1, 10, sp(4,0))   -- 1
        check_pixel(2, 10, sp(5,0))   -- 1
        check_pixel(6, 10, sp(7,0))   -- 1
        check_pixel(7, 10, sp(7,0))   -- 1
        check_pixel(0, 12, sp(4,1))   -- 2
        check_pixel(2, 12, sp(5,1))   -- 2
        check_pixel(4, 12, sp(6,1))   -- 2
        check_pixel(6, 12, sp(7,1))   -- 1
        check_pixel(0, 14, sp(4,2))   -- 3
        check_pixel(2, 14, sp(5,2))   -- 3
        check_pixel(4, 14, sp(6,2))   -- 2
        check_pixel(6, 14, sp(7,2))   -- 1
        -- x=8 is past dest; background
        check_pixel(8, 10, 15)
        -- row above and below sprite must be untouched
        check_pixel(0,  9, 15)
        check_pixel(0, 26, 15)
    end)

    test_case("scale_up_off_right", function()
        ssetup()
        sspr(0, 0, 8, 8, 120, 10, 16, 16)
        -- dest: x=120..135; visible: x=120..127
        -- dst_offset = x-120; src_col = flr(offset/2)
        -- x=120..121: src_col=0; x=122..123: src_col=1; x=124..125: src_col=2; x=126..127: src_col=3
        -- y=10..11: src_row=0; y=12..13: src_row=1; y=14..15: src_row=2
        check_pixel(120, 10, sp(0,0))  -- 1
        check_pixel(121, 10, sp(0,0))  -- 1
        check_pixel(122, 10, sp(1,0))  -- 1
        check_pixel(127, 10, sp(3,0))  -- 1
        check_pixel(120, 12, sp(0,1))  -- 1
        check_pixel(122, 12, sp(1,1))  -- 2
        check_pixel(124, 12, sp(2,1))  -- 2
        check_pixel(126, 12, sp(3,1))  -- 2
        check_pixel(127, 12, sp(3,1))  -- 2
        check_pixel(120, 14, sp(0,2))  -- 1
        check_pixel(122, 14, sp(1,2))  -- 2
        check_pixel(124, 14, sp(2,2))  -- 3
        check_pixel(126, 14, sp(3,2))  -- 3
        -- x=119 is before dest; background
        check_pixel(119, 10, 15)
        -- rows above and below
        check_pixel(120,  9, 15)
        check_pixel(120, 26, 15)
        -- overflow-detection: the x=128..135 writes in the fast path (which uses
        -- SPRITE_WIDTH=8 for the on-screen check instead of dw=16) corrupt pixels
        -- at the beginning of the next few screen rows (rows 11..25, pixels 0..7).
        -- These must remain background.
        for row=11,25 do
            check_pixel(0, row, 15)
            check_pixel(7, row, 15)
        end
    end)

    test_case("scale_down_off_left", function()
        ssetup()
        sspr(0, 0, 8, 8, -2, 10, 4, 4)
        -- dest: x=-2..1; visible: x=0..1
        -- dst_offset = x+2; src_col = flr((x+2)*8/4) = (x+2)*2
        -- x=0: off=2; src_col=4   x=1: off=3; src_col=6
        -- y_offset k: src_row = k*2
        -- y=10: src_row=0; y=11: src_row=2; y=12: src_row=4; y=13: src_row=6
        check_pixel(0, 10, sp(4,0))   -- 1
        check_pixel(1, 10, sp(6,0))   -- 1
        check_pixel(0, 11, sp(4,2))   -- 3
        check_pixel(1, 11, sp(6,2))   -- 2
        check_pixel(0, 12, sp(4,4))   -- 3
        check_pixel(1, 12, sp(6,4))   -- 2
        check_pixel(0, 13, sp(4,6))   -- 2
        check_pixel(1, 13, sp(6,6))   -- 2
        -- x=2 is past dest; background
        check_pixel(2, 10, 15)
        -- rows above and below
        check_pixel(0,  9, 15)
        check_pixel(0, 14, 15)
    end)

    test_case("scale_down_off_right", function()
        ssetup()
        sspr(0, 0, 8, 8, 126, 10, 4, 4)
        -- dest: x=126..129; visible: x=126..127
        -- dst_offset = x-126; src_col = offset*2
        -- x=126: off=0; src_col=0   x=127: off=1; src_col=2
        -- y_offset k: src_row = k*2
        -- y=10: src_row=0; y=11: src_row=2; y=12: src_row=4; y=13: src_row=6
        check_pixel(126, 10, sp(0,0))  -- 1
        check_pixel(127, 10, sp(2,0))  -- 1
        check_pixel(126, 11, sp(0,2))  -- 1
        check_pixel(127, 11, sp(2,2))  -- 3
        check_pixel(126, 12, sp(0,4))  -- 1
        check_pixel(127, 12, sp(2,4))  -- 3
        check_pixel(126, 13, sp(0,6))  -- 1
        check_pixel(127, 13, sp(2,6))  -- 2
        -- x=125 before dest; background
        check_pixel(125, 10, 15)
        -- rows above and below
        check_pixel(126,  9, 15)
        check_pixel(126, 14, 15)
    end)

    test_case("scale_up_off_bottom", function()
        ssetup()
        sspr(0, 0, 8, 8, 10, 120, 16, 16)
        -- dest: y=120..135; visible: y=120..127
        -- dst_offset_y = y-120; src_row = flr(offset/2)
        -- y=120..121: src_row=0; y=122..123: src_row=1; y=124..125: src_row=2; y=126..127: src_row=3
        -- dst_offset_x = x-10; src_col = flr(offset/2)
        -- x=10..11: src_col=0; x=12..13: src_col=1; etc.
        check_pixel(10, 120, sp(0,0))   -- 1
        check_pixel(12, 120, sp(1,0))   -- 1
        check_pixel(10, 122, sp(0,1))   -- 1
        check_pixel(12, 122, sp(1,1))   -- 2
        check_pixel(14, 122, sp(2,1))   -- 2
        check_pixel(10, 124, sp(0,2))   -- 1
        check_pixel(12, 124, sp(1,2))   -- 2
        check_pixel(14, 124, sp(2,2))   -- 3
        check_pixel(10, 126, sp(0,3))   -- 1
        check_pixel(12, 126, sp(1,3))   -- 2
        check_pixel(14, 126, sp(2,3))   -- 3
        check_pixel(10, 127, sp(0,3))   -- 1  (src_row=3 repeated for y=126,127)
        -- cols outside dest; background
        check_pixel( 9, 120, 15)
        check_pixel(26, 120, 15)
        -- row above dest; background
        check_pixel(10, 119, 15)
    end)

    test_case("scale_up_off_top", function()
        ssetup()
        sspr(0, 0, 8, 8, 10, -8, 16, 16)
        -- dest: y=-8..7; visible: y=0..7
        -- dst_offset_y = y+8; src_row = flr((y+8)/2)
        -- y=0..1: src_row=4; y=2..3: src_row=5; y=4..5: src_row=6; y=6..7: src_row=7
        -- dst_offset_x = x-10; src_col = flr((x-10)/2)
        -- x=10..11: src_col=0; x=12..13: src_col=1; etc.
        check_pixel(10, 0, sp(0,4))   -- 1
        check_pixel(11, 0, sp(0,4))   -- 1
        check_pixel(12, 0, sp(1,4))   -- 2
        check_pixel(10, 2, sp(0,5))   -- 1
        check_pixel(12, 2, sp(1,5))   -- 2
        check_pixel(10, 4, sp(0,6))   -- 1
        check_pixel(12, 4, sp(1,6))   -- 2
        check_pixel(10, 6, sp(0,7))   -- 1
        check_pixel(12, 6, sp(1,7))   -- 1
        -- y=8 is past dest; background
        check_pixel(10, 8, 15)
        -- cols outside sprite
        check_pixel(9, 0, 15)
        check_pixel(26, 0, 15)
    end)

    test_case("scale_down_off_bottom", function()
        ssetup()
        sspr(0, 0, 8, 8, 10, 126, 4, 4)
        -- dest: y=126..129; visible: y=126..127
        -- dst_offset_y = y-126; src_row = offset*2
        -- y=126: src_row=0; y=127: src_row=2
        -- dst_offset_x = x-10; src_col = offset*2
        -- x=10: src_col=0; x=11: src_col=2; x=12: src_col=4; x=13: src_col=6
        check_pixel(10, 126, sp(0,0))  -- 1
        check_pixel(11, 126, sp(2,0))  -- 1
        check_pixel(12, 126, sp(4,0))  -- 1
        check_pixel(13, 126, sp(6,0))  -- 1
        check_pixel(10, 127, sp(0,2))  -- 1
        check_pixel(11, 127, sp(2,2))  -- 3
        check_pixel(12, 127, sp(4,2))  -- 3
        check_pixel(13, 127, sp(6,2))  -- 2
        -- x=14 past dest; background
        check_pixel(14, 126, 15)
        -- y=125 before dest; background
        check_pixel(10, 125, 15)
    end)
end

------------------------------------------------------------------------
-- fully off-screen: nothing should be drawn anywhere
------------------------------------------------------------------------

function test_fully_off()
    test_case("1to1_off_left", function()
        ssetup()
        sspr(0, 0, 8, 8, -10, 40)
        -- sprite x=-10..-3: entirely off left edge
        for x=0,127 do
            check_pixel(x, 40, 15)
        end
        -- also verify no corruption in row above (off-screen wrap writes)
        for x=0,127 do
            check_pixel(x, 39, 15)
        end
    end)

    test_case("1to1_off_right", function()
        ssetup()
        sspr(0, 0, 8, 8, 130, 40)
        for x=0,127 do
            check_pixel(x, 40, 15)
        end
        for x=0,127 do
            check_pixel(x, 41, 15)
        end
    end)

    test_case("1to1_off_top", function()
        ssetup()
        sspr(0, 0, 8, 8, 40, -10)
        for y=0,127 do
            check_pixel(40, y, 15)
        end
    end)

    test_case("1to1_off_bottom", function()
        ssetup()
        sspr(0, 0, 8, 8, 40, 130)
        for y=0,127 do
            check_pixel(40, y, 15)
        end
    end)

    test_case("scale_up_off_left", function()
        ssetup()
        sspr(0, 0, 8, 8, -20, 40, 16, 16)
        for x=0,127 do
            check_pixel(x, 40, 15)
        end
        for x=0,127 do
            check_pixel(x, 39, 15)
        end
    end)

    test_case("scale_down_off_right", function()
        ssetup()
        sspr(0, 0, 8, 8, 130, 40, 4, 4)
        for x=0,127 do
            check_pixel(x, 40, 15)
        end
    end)
end

------------------------------------------------------------------------

------------------------------------------------------------------------
-- camera tests
-- Bug 1: fast path used SPRITE_WIDTH/SPRITE_HEIGHT (8) instead of dw/dh
--   for the "fully on screen" check, so a 2x sprite at x=120 (screen) passed
--   the check even though x=120+16=136>128, writing off the right edge and
--   corrupting the start of subsequent rows.
-- Bug 2: the fast path subtracted the camera from dx/dy in-place before
--   deciding whether to use the fast or slow path. If the sprite was not
--   fully on screen the slow path was reached with camera already subtracted,
--   and pixel_set subtracted it again → pixels drawn at wrong position.
------------------------------------------------------------------------

function test_camera()
    -- Bug 1 (fast path on-screen check used SPRITE_WIDTH not dw):
    -- 2x scaled sprite placed so its screen rect starts at x=120 and extends
    -- to x=135. The fast path checked dx+8<=128 (passes!) instead of
    -- dx+16<=128 (fails), then wrote x=128..135 without clipping, corrupting
    -- the beginning of the next 8 screen rows.
    test_case("bug1_scale_up_off_right_corrupt", function()
        ssetup()
        sspr(0, 0, 8, 8, 120, 10, 16, 16)
        -- visible: x=120..127
        check_pixel(120, 10, sp(0,0))
        check_pixel(127, 10, sp(3,0))
        -- overflow writes corrupt row-start pixels (x=0..7) of rows 11-25
        for row=11,25 do
            check_pixel(0, row, 15)
            check_pixel(7, row, 15)
        end
    end)

    -- Bug 2 (camera offset double-applied on slow path):
    -- With camera(10,0), sspr world-x=132 → screen x=122. The dest rect
    -- x=122..129 is partially off-right. Before the fix the fast path
    -- modified dx to 122 (in-place camera subtraction), then the slow path
    -- was taken and pixel_set subtracted camera (10) again, drawing at
    -- screen x=112..119 instead of x=122..127.
    test_case("bug2_camera_double_apply_1to1", function()
        ssetup()
        camera(10, 0)
        sspr(0, 0, 8, 8, 132, 10)
        -- world x=132..139, camera cx=10 → screen x=122..127 (clipped)
        -- world offset from 132 → screen offset 0..5 → src cols 0..5
        camera(0, 0)
        check_pixel(122, 10, sp(0,0))  -- 1
        check_pixel(123, 10, sp(1,0))  -- 1
        check_pixel(124, 10, sp(2,0))  -- 1
        check_pixel(125, 10, sp(3,0))  -- 1
        check_pixel(126, 10, sp(4,0))  -- 1
        check_pixel(127, 10, sp(5,0))  -- 1
        -- bug would have drawn at x=112..117 instead; those must be bg
        check_pixel(112, 10, 15)
        check_pixel(117, 10, 15)
        -- x=121 is just before the dest rect; must be bg
        check_pixel(121, 10, 15)
    end)

    -- Bug 2 with scaling: camera(10,0), 2x sprite at world x=130 →
    -- screen x=120..135; partially off right. Before fix the slow path
    -- would draw with camera already subtracted → pixels at screen x=110..127.
    test_case("bug2_camera_double_apply_scaled", function()
        ssetup()
        camera(10, 0)
        sspr(0, 0, 8, 8, 130, 10, 16, 16)
        -- world x=130..145, camera cx=10 → screen x=120..127 (clipped to 127)
        -- world offset 0..7 → src col flr(offset/2): 0,0,1,1,2,2,3,3
        camera(0, 0)
        check_pixel(120, 10, sp(0,0))  -- src col 0 = 1
        check_pixel(121, 10, sp(0,0))  -- src col 0 = 1
        check_pixel(122, 10, sp(1,0))  -- src col 1 = 1
        check_pixel(124, 10, sp(2,0))  -- src col 2 = 1
        check_pixel(126, 10, sp(3,0))  -- src col 3 = 1
        check_pixel(127, 10, sp(3,0))  -- src col 3 = 1
        -- bug would draw at screen x=110..127 (camera subtracted twice);
        -- x=110..119 must be background
        for x=110,119 do
            check_pixel(x, 10, 15)
        end
        -- x=119 is just before the visible dest; must be bg
        check_pixel(119, 10, 15)
    end)

    -- Sanity: sspr with camera, sprite fully on screen uses the fast path
    -- and draws at the correct screen location.
    test_case("camera_on_screen", function()
        ssetup()
        camera(20, 10)
        sspr(0, 0, 8, 8, 30, 20)
        -- world (30,20), camera (20,10) → screen (10,10)
        camera(0, 0)
        check_pixel(10, 10, sp(0,0))
        check_pixel(17, 10, sp(7,0))
        check_pixel(10, 17, sp(0,7))
        check_pixel(17, 17, sp(7,7))
        -- adjacent pixels must be background
        check_pixel( 9, 10, 15)
        check_pixel(18, 10, 15)
        check_pixel(10,  9, 15)
        check_pixel(10, 18, 15)
    end)
end

------------------------------------------------------------------------
-- spr() with fractional w/h
-- PICO-8 allows spr(n, x, y, w, h) where w/h are in sprite units (multiples
-- of 8).  A fractional value like 0.75 means 0.75*8=6 pixels wide/tall.
-- The original femto8 spr() did lua_tointeger for w/h, truncating 0.75 → 0,
-- so nothing was drawn.  The fix uses lua_tonumber and routes fractional
-- values through draw_scaled_sprite at 1:1 scale.
------------------------------------------------------------------------

function test_spr_fractional()
    -- spr(0, 10, 10, 0.75, 0.75): draw top-left 6x6 of sprite 0
    -- source pixels (0..5, 0..5) of the concentric-square sprite
    test_case("frac_0.75x0.75", function()
        ssetup()
        spr(0, 10, 10, 0.75, 0.75)
        -- top row (src y=0): all border = color 1
        check_pixel(10, 10, sp(0,0))   -- src(0,0)=1
        check_pixel(15, 10, sp(5,0))   -- src(5,0)=1
        -- left col (src x=0): border
        check_pixel(10, 11, sp(0,1))   -- src(0,1)=1
        check_pixel(10, 14, sp(0,4))   -- src(0,4)=1
        -- inner ring row 1 (src y=1): color 2 in the middle
        check_pixel(11, 11, sp(1,1))   -- src(1,1)=2
        check_pixel(14, 11, sp(4,1))   -- src(4,1)=2
        -- center (src y=2, x=2): color 3
        check_pixel(12, 12, sp(2,2))   -- src(2,2)=3
        check_pixel(14, 14, sp(4,4))   -- src(4,4)=3
        -- row y=15 is OUTSIDE the 6-pixel-tall dest (dy+6=16); must be bg
        check_pixel(10, 16, 15)
        check_pixel(15, 16, 15)
        -- col x=16 is OUTSIDE the 6-pixel-wide dest (dx+6=16); must be bg
        check_pixel(16, 10, 15)
        check_pixel(16, 15, 15)
        -- adjacent pixels before dest must be bg
        check_pixel( 9, 10, 15)
        check_pixel(10,  9, 15)
    end)

    -- spr(0, 10, 10, 0.5, 1): draw left 4 columns, all 8 rows
    test_case("frac_0.5w_full_h", function()
        ssetup()
        spr(0, 10, 10, 0.5, 1)
        -- src cols 0..3, rows 0..7
        check_pixel(10, 10, sp(0,0))   -- 1
        check_pixel(13, 10, sp(3,0))   -- 1
        check_pixel(10, 17, sp(0,7))   -- 1
        check_pixel(13, 17, sp(3,7))   -- 1
        check_pixel(11, 11, sp(1,1))   -- 2
        check_pixel(12, 12, sp(2,2))   -- 3
        -- col x=14 (dx+4) is outside; must be bg
        check_pixel(14, 10, 15)
        check_pixel(14, 17, 15)
        -- row y=18 is outside; must be bg
        check_pixel(10, 18, 15)
    end)

    -- spr(0, 10, 10, 1, 0.5): draw all 8 cols, top 4 rows
    test_case("frac_full_w_0.5h", function()
        ssetup()
        spr(0, 10, 10, 1, 0.5)
        -- src rows 0..3, cols 0..7
        check_pixel(10, 10, sp(0,0))   -- 1
        check_pixel(17, 10, sp(7,0))   -- 1
        check_pixel(10, 13, sp(0,3))   -- 1
        check_pixel(17, 13, sp(7,3))   -- 1
        check_pixel(11, 11, sp(1,1))   -- 2
        check_pixel(12, 12, sp(2,2))   -- 3
        -- row y=14 (dy+4) is outside; must be bg
        check_pixel(10, 14, 15)
        check_pixel(17, 14, 15)
        -- col x=18 is outside; must be bg
        check_pixel(18, 10, 15)
    end)

    -- spr(0, 10, 10, 1, 1): integer w/h must still use the fast path (no regression)
    test_case("integer_1x1", function()
        ssetup()
        spr(0, 10, 10, 1, 1)
        check_pixel(10, 10, sp(0,0))  -- 1
        check_pixel(17, 17, sp(7,7))  -- 1
        check_pixel(11, 11, sp(1,1))  -- 2
        check_pixel(12, 12, sp(2,2))  -- 3
        check_pixel( 9, 10, 15)
        check_pixel(18, 10, 15)
        check_pixel(10,  9, 15)
        check_pixel(10, 18, 15)
    end)

    -- spr(0, 10, 10) default w=h=1 (omitted args) still works
    test_case("default_args", function()
        ssetup()
        spr(0, 10, 10)
        check_pixel(10, 10, sp(0,0))  -- 1
        check_pixel(17, 17, sp(7,7))  -- 1
        check_pixel(12, 12, sp(2,2))  -- 3
        check_pixel(18, 10, 15)
        check_pixel(10, 18, 15)
    end)
end

test_suite("on_screen",        test_on_screen)
test_suite("partial_1to1",     test_partial_1to1)
test_suite("partial_scaled",   test_partial_scaled)
test_suite("fully_off",        test_fully_off)
test_suite("camera",           test_camera)
test_suite("spr_fractional",   test_spr_fractional)
summary()
__gfx__
11111111000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
12222221000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
12333321000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
12333321000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
12333321000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
12333321000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
12222221000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
11111111000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
