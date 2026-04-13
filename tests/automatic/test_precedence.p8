pico-8 cartridge // http://www.pico-8.com
version 16
__lua__
-- test operator precedence: & vs comparison operators
-- in pico-8, & has higher precedence than > < == ~= >= <=
-- so: h&0x80>0 should parse as (h&0x80)>0

#include test_fwk.lua

function test_bitwise_precedence()
    local h

    -- h=0x82: (0x82&0x80)=0x80>0 => true; 0x82&1=0 => false
    h=0x82
    test_case("0x82 & 0x80 > 0", function() check_eq(h&0x80>0, true) end)
    test_case("0x82 explicit (h&0x80)>0", function() check_eq((h&0x80)>0, true) end)

    -- h=0x01: (0x01&0x80)=0>0 => false; 0x01&1=1 => true
    h=0x01
    test_case("0x01 & 0x80 > 0 should be false", function() check_eq(h&0x80>0, false) end)

    -- h=0x80: (0x80&0x80)=0x80>0 => true
    h=0x80
    test_case("0x80 & 0x80 > 0 should be true", function() check_eq(h&0x80>0, true) end)

    -- also test & vs < and ==
    h=0xff
    test_case("0xff & 0xf0 == 0xf0", function() check_eq(h&0xf0==0xf0, true) end)
    test_case("0x0f & 0xf0 == 0", function() check_eq(0x0f&0xf0==0, true) end)

    -- unpack_variant style: should only enter if h>=128
    test_case("loop count h&0x80>0", function()
        local entered=0
        for _,h in pairs{0x7f,0x80,0xff,0x00,0x40} do
            if h&0x80>0 then entered+=1 end
        end
        check_eq(entered, 2)
    end)
end

function _init()
    test_suite("operator_precedence", test_bitwise_precedence)
    summary()
end
__gfx__
00000000000000000000000000000000
