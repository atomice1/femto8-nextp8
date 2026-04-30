pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
#include test_fwk.lua

function test_loop()
    test_case("1 to 10", function()
        seen_1=false
        seen_7=false
        seen_10=false
        seen_11=false
        for i=1,10 do
            if i==1 then
                seen_1=true
            end
            if i==7 then
                seen_7=true
            end
            if i==10 then
                seen_10=true
            end
            if i==11 then
                seen_11=true
            end
        end
        check_true(seen_1)
        check_true(seen_7)
        check_true(seen_10)
        check_false(seen_11)
    end)
    test_case("boundary_32766_32767", function()
        seen_32766=false
        seen_32767=false
        for i=32766,32767 do
            if i==32766 then
                seen_32766=true
            end
            if i==32767 then
                seen_32767=true
            end
        end
        check_true(seen_32766)
        check_true(seen_32767)
    end)
    test_case("boundary_0x8000_0xffff", function()
        seen_0x8000=false
        seen_0xffff=false
        for i=0x8000,0xffff do
            if i==0x8000 then
                seen_0x8000=true
            end
            if i==0xffff then
                seen_0xffff=true
            end
        end
        check_true(seen_0x8000)
        check_true(seen_0xffff)
    end)
end

function _init()
    test_suite("loop", test_loop)
    summary()
end
