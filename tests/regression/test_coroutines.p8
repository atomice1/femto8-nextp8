pico-8 cartridge // http://www.pico-8.com
version 43
__lua__

#include test_fwk.lua

function test_coroutines()
    test_case("coroutines", function()
        sum=0
        function count()
            for i=1,3 do
                sum += i
                a, b, c = yield(2, 3, 4)
                check_eq(a, 1)
                check_eq(b, 2)
                check_eq(c, 3)
            end
        end
        local cor = cocreate(count)
        check_ne(costatus(cor), 'dead')
        check_eq(sum, 0)
        ok, a, b, c = coresume(cor, 1, 2, 3)
        check_true(ok)
        check_ne(costatus(cor), 'dead')
        check_eq(sum, 1)
        check_eq(a, 2)
        check_eq(b, 3)
        check_eq(c, 4)
        ok, a, b, c = coresume(cor, 1, 2, 3)
        check_true(ok)
        check_ne(costatus(cor), 'dead')
        check_eq(sum, 3)
        check_eq(a, 2)
        check_eq(b, 3)
        check_eq(c, 4)
        ok, a, b, c = coresume(cor, 1, 2, 3)
        check_true(ok)
        check_ne(costatus(cor), 'dead')
        check_eq(sum, 6)
        check_eq(a, 2)
        check_eq(b, 3)
        check_eq(c, 4)
        ok = coresume(cor, 1, 2, 3)
        check_true(ok)
        check_eq(costatus(cor), 'dead')
        ok = coresume(cor, 1, 2, 3)
        check_false(ok)
        cor = nil
    end)
end

test_suite("coroutines", test_coroutines)

summary()