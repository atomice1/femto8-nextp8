pico-8 cartridge // http://www.pico-8.com
version 43
__lua__

#include test_fwk.lua

function test_eo()
    it=0
    test_case("eo1", function()
        function eo1(e)local t,o,l,n,d,f,u,i=1,0,0,1,1,8,8,{}for e=1,f do local n={}for e=1,u do n[e]=0end i[e]=n end t=3printh("e=" .. e)while t<#e do printh("eo1: t="..t)it+=1l=10o=10i[n][d]=l n+=1if(n>f)n=1d+=1
        while(o>0)i[n][d]=l o-=1n+=1if(n>f)n=1d+=1
        t+=2end return i,f,u end
        eo1("))(0Q((6c(d((,b(9)d((+Y/87")
        check_eq(it, 12)
    end)

    test_case("eo2", function()
        it=0
        function eo2(e)
            local t, o, l, n, d, f, u, i = 1, 0, 0, 1, 1, 8, 8, {}
            for e = 1, f do
                local n = {}
                for e = 1, u do
                    n[e] = 0
                end
                i[e] = n
            end
            t = 3
            printh("e=" .. e)
            while t < #e do
                printh("eo2:t=" .. t)
                it += 1
                l = 10
                o = 10
                i[n][d] = l
                n += 1
                if n > f then
                    n = 1
                    d += 1
                end
                while o > 0 do
                    i[n][d] = l
                    o -= 1
                    n += 1
                    if n > f then
                        n = 1
                        d += 1
                    end
                end
                t += 2
            end
            return i, f, u
        end
        eo2("))(0Q((6c(d((,b(9)d((+Y/87")
        check_eq(it, 12)
    end)
end

function _init()
    test_suite("eo", test_eo)
    summary()
end
