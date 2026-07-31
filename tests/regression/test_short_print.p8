pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- Test suite for ? (short print) syntax
-- ? can be used as both a statement and an expression
-- As an expression, it returns the right-most x position after printing

#include test_fwk.lua

--------------------------------------------------------------------------------
-- Statement context tests
--------------------------------------------------------------------------------

function test_print_statement_basic()
    -- Standalone ? as statement
    test_case("print_single_arg", function()
        local px = peek(0x5f26)
        local py = peek(0x5f27)
        ?"test"
        -- After printing "test", cursor should have moved
        check_true(peek(0x5f27) > py)
    end)

    test_case("print_multiple_args", function()
        ?"a", "b", "c"
        -- Should print "a b c" (with tabs)
    end)

    test_case("print_with_position", function()
        ?"test", 10, 20
        check_eq(peek(0x5f26), 10)
        check_eq(peek(0x5f27), 26)
    end)

    test_case("print_with_color", function()
        ?"test", 10, 20, 5
        -- Color should be set to 5
    end)
end

--------------------------------------------------------------------------------
-- Expression context tests
--------------------------------------------------------------------------------

function test_print_expression_return_value()
    -- ? as expression returns right-most x position
    test_case("print_expr_single_char", function()
        local r = ?"x"
        -- "x" is 8 pixels wide
        check_eq(r, 4)
    end)

    test_case("print_expr_two_chars", function()
        local r = ?"xy"
        -- "xy" is 8 pixels wide
        check_eq(r, 8)
    end)

    test_case("print_expr_with_position", function()
        local r = ?"test", 10, 20
        -- Should print at (10,20) and return right-most x
        check_eq(r, 26)  -- "test" is 16 pixels, 10+16=26
        check_eq(peek(0x5f26), 10)
        check_eq(peek(0x5f27), 26)
    end)

    test_case("print_expr_multiple_args", function()
        -- ?s, x, y, c prints and returns right-most x
        local r = ?"hello", 5, 10, 3
        -- "hello" is 20 pixels, starts at x=5, ends at x=25
        check_eq(r, 25)
    end)
end

--------------------------------------------------------------------------------
-- Return statement with ? tests
--------------------------------------------------------------------------------

function test_return_with_print()
    test_case("return_print_expr", function()
        function foo()
            local s = "hello"
            local x = 10
            local y = 20
            local c = 3
            return ?s, x, y, c  -- prints and returns right-most x
        end
        local r = foo()
        check_eq(r, 30)  -- "hello" is 30 pixels
    end)

    test_case("return_print_in_function", function()
        function bar()
            local a, b = 5, 7
            return ?a, b  -- prints "5 7" and returns width
        end
        local r = bar()
        check_eq(r, 4)  -- "5 7" is 4 pixels
    end)
end

--------------------------------------------------------------------------------
-- Short-if tests (statement context)
--------------------------------------------------------------------------------

function test_short_if_with_print()
    test_case("short_if_print_statement", function()
        local x = 0
        if (true) x = 1 ?"in if", 10, 10
        -- x should be 1, meaning the if block executed
        check_eq(x, 1)
    end)

    test_case("short_if_print_expression", function()
        local r = 0
        if (true) r = ?"test"
        -- r should be the width of "test" (16 pixels)
        check_eq(r, 16)
    end)

    test_case("short_if_print_no_next_line", function()
        local got_here = false
        local printed = false
        if (false) ?"first"
        got_here = true
        -- The above line should NOT be part of the if block
        -- If short-if was incorrectly extended to next line, this would be in if block
        check_true(got_here)
    end)
end

--------------------------------------------------------------------------------
-- Short-while tests (statement context)
--------------------------------------------------------------------------------

function test_short_while_with_print()
    test_case("short_while_print_statement", function()
        local count = 0
        while (count < 1) count = count + 1 ?"loop", 10, 10
        check_eq(count, 1)
    end)

    test_case("short_while_print_expression", function()
        local count = 0
        local r = 0
        while (count < 1) count = count + 1 r = ?"w"
        check_eq(r, 4)  -- "w" is 4 pixels
    end)

    test_case("short_while_print_no_next_line", function()
        local count = 0
        local got_here = false
        while (count > 1) ?"while"
        got_here = true
        -- The above line should NOT be in the while loop
        check_true(got_here)
    end)
end

--------------------------------------------------------------------------------
-- Mixed context tests
--------------------------------------------------------------------------------

function test_mixed_contexts()
    test_case("print_in_if_return", function()
        function foo()
            if (true) return ?"test"
            return 0
        end
        local r = foo()
        check_eq(r, 16)  -- "test" is 16 pixels
    end)

    test_case("print_in_while_return", function()
        function bar()
            local x = 0
            while x < 1 do
                x = x + 1
                if (x == 1) return ?"done"
            end
            return 0
        end
        local r = bar()
        check_eq(r, 16)  -- "done" is 16 pixels
    end)
end

--------------------------------------------------------------------------------
-- Run all tests
--------------------------------------------------------------------------------

test_suite("print_statement", test_print_statement_basic)
test_suite("print_expression", test_print_expression_return_value)
test_suite("return_with_print", test_return_with_print)
test_suite("short_if_with_print", test_short_if_with_print)
test_suite("short_while_with_print", test_short_while_with_print)
test_suite("mixed_contexts", test_mixed_contexts)

summary()

__gfx__
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
