pico-8 cartridge // http://www.pico-8.com
version 43
__lua__

#include test_fwk.lua

function test_all_function()
    test_suite("all()", function()
        -- Basic table iteration
        test_case("iterate table", function()
            local t = {10, 20, 30}
            local result = {}
            for v in all(t) do
                add(result, v)
            end
            check_eq(result, {10, 20, 30})
        end)

        -- Empty table
        test_case("empty table", function()
            local t = {}
            local count = 0
            for v in all(t) do
                count += 1
            end
            check_eq(count, 0)
        end)

        -- String iteration
        test_case("iterate string", function()
            local result = {}
            for c in all("abc") do
                add(result, c)
            end
            check_eq(result, {"a", "b", "c"})
        end)

        -- Empty string
        test_case("empty string", function()
            local count = 0
            for c in all("") do
                count += 1
            end
            check_eq(count, 0)
        end)

        -- Nil input
        test_case("nil input", function()
            local count = 0
            for v in all(nil) do
                count += 1
            end
            check_eq(count, 0)
        end)

        -- Sparse table
        test_case("sparse table", function()
            local t = {10, 20}
            t[5] = 50
            local result = {}
            for v in all(t) do
                add(result, v)
            end
            check_eq(result, {10, 20})
        end)
    end)
end

function test_add_function()
    test_suite("add()", function()
        -- Add to table
        test_case("add to table", function()
            local t = {1, 2}
            local result = add(t, 3)
            check_eq(t, {1, 2, 3})
            check_eq(result, 3)
        end)

        -- Add to empty table
        test_case("add to empty", function()
            local t = {}
            add(t, "first")
            check_eq(t, {"first"})
        end)

        -- Add nil value (PICO-8 adds nil but length stays same)
        test_case("add nil value", function()
            local t = {1}
            add(t, nil)
            check_eq(#t, 1)  -- Length doesn't increase with nil
            check_nil(t[2])
        end)

        -- Add to nil table
        test_case("add to nil", function()
            local result = add(nil, 10)
            check_nil(result)
        end)

        -- Return value
        test_case("return value", function()
            local t = {}
            local v = add(t, 42)
            check_eq(v, 42)
        end)

        -- Add to number
        test_case("add to number", function()
            local result = add(42, 5)
            check_nil(result)
        end)

        -- Add to string
        test_case("add to string", function()
            local result = add("hello", "world")
            check_nil(result)
        end)

        -- Add to boolean
        test_case("add to boolean", function()
            local result = add(true, 1)
            check_nil(result)
        end)
    end)
end

function test_foreach_function()
    test_suite("foreach()", function()
        -- Basic foreach
        test_case("basic foreach", function()
            local t = {1, 2, 3}
            local sum = 0
            foreach(t, function(v) sum += v end)
            check_eq(sum, 6)
        end)

        -- Empty table
        test_case("empty table", function()
            local count = 0
            foreach({}, function(v) count += 1 end)
            check_eq(count, 0)
        end)

        -- Nil table
        test_case("nil table", function()
            local count = 0
            foreach(nil, function(v) count += 1 end)
            check_eq(count, 0)
        end)

        -- String iteration
        test_case("string input", function()
            local result = ""
            foreach("xyz", function(c) result ..= c end)
            check_eq(result, "xyz")
        end)
    end)
end

function test_count_function()
    test_suite("count()", function()
        -- Count all elements (PICO-8 counts up to last index including nils)
        test_case("count all", function()
            local t = {10, 20, 30, nil, 50}
            check_eq(count(t), 5)  -- PICO-8 includes nil positions
        end)

        -- Count specific value
        test_case("count specific", function()
            local t = {1, 2, 1, 3, 1}
            check_eq(count(t, 1), 3)
        end)

        -- Empty table
        test_case("empty table", function()
            check_eq(count({}), 0)
        end)

        -- Nil table
        test_case("nil table", function()
            check_nil(count(nil))  -- PICO-8 returns nil for non-tables
        end)

        -- String count (PICO-8 doesn't support strings in count)
        test_case("string count", function()
            check_nil(count("hello"))  -- PICO-8 returns nil for non-tables
        end)

        -- String count specific char
        test_case("string count char", function()
            check_nil(count("hello", "l"))  -- PICO-8 returns nil for non-tables
        end)

        -- Number input (edge case)
        test_case("number input", function()
            check_nil(count(42))  -- PICO-8 returns nil for non-tables
        end)

        -- Boolean input (edge case)
        test_case("boolean input", function()
            check_nil(count(true))  -- PICO-8 returns nil for non-tables
        end)

        -- Table with nils (PICO-8 counts all positions)
        test_case("table with nils", function()
            local t = {1, nil, 3, nil, 5}
            check_eq(count(t), 5)  -- PICO-8 counts all positions
        end)
    end)
end

function test_del_function()
    test_suite("del()", function()
        -- Delete existing element
        test_case("delete existing", function()
            local t = {10, 20, 30}
            del(t, 20)
            check_eq(t, {10, 30})
        end)

        -- Delete first element
        test_case("delete first", function()
            local t = {"a", "b", "c"}
            del(t, "a")
            check_eq(t, {"b", "c"})
        end)

        -- Delete last element
        test_case("delete last", function()
            local t = {1, 2, 3}
            del(t, 3)
            check_eq(t, {1, 2})
        end)

        -- Delete non-existing
        test_case("delete non-existing", function()
            local t = {1, 2, 3}
            del(t, 99)
            check_eq(t, {1, 2, 3})
        end)

        -- Delete from nil
        test_case("delete from nil", function()
            del(nil, 1)
            -- Should not crash
        end)

        -- Delete duplicate (only first)
        test_case("delete first duplicate", function()
            local t = {1, 2, 1, 3}
            del(t, 1)
            check_eq(t, {2, 1, 3})
        end)

        -- Delete from number
        test_case("delete from number", function()
            del(123, 1)
            -- Should not crash
        end)

        -- Delete from string
        test_case("delete from string", function()
            del("hello", "e")
            -- Should not crash
        end)

        -- Delete nil value
        test_case("delete nil value", function()
            local t = {1, 2, 3}
            del(t, nil)
            check_eq(t, {1, 2, 3})
        end)
    end)
end

function test_deli_function()
    test_suite("deli()", function()
        -- Delete by index
        test_case("delete by index", function()
            local t = {"a", "b", "c"}
            local v = deli(t, 2)
            check_eq(t, {"a", "c"})
            check_eq(v, "b")
        end)

        -- Delete first
        test_case("delete index 1", function()
            local t = {10, 20, 30}
            local v = deli(t, 1)
            check_eq(t, {20, 30})
            check_eq(v, 10)
        end)

        -- Delete last
        test_case("delete last index", function()
            local t = {10, 20, 30}
            local v = deli(t, 3)
            check_eq(t, {10, 20})
            check_eq(v, 30)
        end)

        -- Delete out of bounds
        test_case("out of bounds", function()
            local t = {1, 2}
            local v = deli(t, 10)
            check_eq(t, {1, 2})
            check_nil(v)
        end)

        -- Delete from nil
        test_case("from nil table", function()
            local v = deli(nil, 1)
            check_nil(v)
        end)

        -- Negative index
        test_case("negative index", function()
            local t = {1, 2, 3}
            local v = deli(t, -1)
            check_eq(t, {1, 2, 3})
            check_nil(v)
        end)

        -- Delete from number
        test_case("from number", function()
            local v = deli(42, 1)
            check_nil(v)
        end)

        -- Delete from string
        test_case("from string", function()
            local v = deli("test", 2)
            check_nil(v)
        end)

        -- Zero index
        test_case("zero index", function()
            local t = {1, 2, 3}
            local v = deli(t, 0)
            check_eq(t, {1, 2, 3})
            check_nil(v)
        end)
    end)
end

function test_split_function()
    test_suite("split()", function()
        -- Basic split
        test_case("basic split", function()
            local result = split("a,b,c")
            check_eq(result, {"a", "b", "c"})
        end)

        -- Custom delimiter
        test_case("custom delimiter", function()
            local result = split("x|y|z", "|")
            check_eq(result, {"x", "y", "z"})
        end)

        -- Empty string
        test_case("empty string", function()
            local result = split("")
            check_eq(result, {""})
        end)

        -- No delimiter in string
        test_case("no delimiter", function()
            local result = split("hello")
            check_eq(result, {"hello"})
        end)

        -- Multiple char delimiter (PICO-8 splits on ANY char in delimiter string)
        test_case("multi-char delimiter", function()
            local result = split("a::b::c", "::")
            -- PICO-8 splits on each ':' character
            check_eq(result, {"a", "", "b", "", "c"})
        end)

        -- Consecutive delimiters
        test_case("consecutive delimiters", function()
            local result = split("a,,c")
            check_eq(result, {"a", "", "c"})
        end)

        -- Convert to numbers
        test_case("convert numbers", function()
            local result = split("1,2,3", ",", true)
            check_eq(result, {1, 2, 3})
        end)

        -- Mixed numbers and strings
        test_case("mixed with numbers", function()
            local result = split("1,x,3", ",", true)
            check_eq(result[1], 1)
            check_eq(result[2], "x")
            check_eq(result[3], 3)
        end)

        -- Nil string
        test_case("nil string", function()
            local result = split(nil)
            check_nil(result)  -- PICO-8 returns nil
        end)

        -- Number input
        test_case("number input", function()
            local result = split(123)
            check_eq(result, {123})  -- PICO-8 converts to string then back to number
        end)

        -- Nil delimiter
        test_case("nil delimiter", function()
            local result = split("a,b", nil)
            check_eq(result, {"a", "b"})
        end)
    end)
end

function test_concurrent_modification()
    test_suite("concurrent modification", function()
        -- Add during all() iteration
        test_case("add during all()", function()
            local t = {1, 2, 3}
            local result = {}
            local count = 0
            for v in all(t) do
                add(result, v)
                count += 1
                if count == 2 then
                    add(t, 4)  -- Add element during iteration
                end
                if count > 10 then break end  -- Safety limit
            end
            -- PICO-8 behavior: iteration includes newly added elements
            check_eq(result, {1, 2, 3, 4})
            check_eq(t, {1, 2, 3, 4})
        end)

        -- Delete during all() iteration
        test_case("del during all()", function()
            local t = {1, 2, 3, 4}
            local result = {}
            for v in all(t) do
                add(result, v)
                if v == 2 then
                    del(t, 3)  -- Delete upcoming element
                end
            end
            -- PICO-8 behavior: deletion affects remaining iteration
            check_eq(result, {1, 2, 4})
        end)

        -- Delete current element during all()
        test_case("del current in all()", function()
            local t = {1, 2, 3}
            local result = {}
            for v in all(t) do
                add(result, v)
                del(t, v)  -- Delete current element
            end
            -- PICO-8 processes all elements despite deletion
            check_eq(result, {1, 2, 3})
            check_eq(t, {})
        end)

        -- Add during foreach() iteration
        test_case("add during foreach()", function()
            local t = {10, 20}
            local result = {}
            local count = 0
            foreach(t, function(v)
                add(result, v)
                count += 1
                if count == 1 then
                    add(t, 30)
                end
                if count > 10 then return end  -- Safety limit
            end)
            -- PICO-8 behavior: foreach includes newly added element
            check_eq(result, {10, 20, 30})
            check_eq(t, {10, 20, 30})
        end)

        -- Delete during foreach() iteration
        test_case("del during foreach()", function()
            local t = {5, 10, 15, 20}
            local result = {}
            foreach(t, function(v)
                add(result, v)
                if v == 10 then
                    del(t, 15)
                end
            end)
            check_eq(result, {5, 10, 20})
        end)

        -- Deli during all() iteration
        test_case("deli during all()", function()
            local t = {"a", "b", "c", "d"}
            local result = {}
            local count = 0
            for v in all(t) do
                add(result, v)
                count += 1
                if count == 2 then
                    deli(t, 3)  -- Remove index 3 ("c")
                end
                if count > 10 then break end  -- Safety limit
            end
            check_eq(result, {"a", "b", "d"})
        end)
        -- Multiple modifications during iteration
        test_case("multiple mods in all()", function()
            local t = {1, 2, 3, 4, 5}
            local result = {}
            for v in all(t) do
                add(result, v)
                if v == 2 then
                    del(t, 4)
                    add(t, 6)
                end
            end
            -- PICO-8: del removes 4, shifts 5, adds 6 at end, iteration continues
            check_eq(result, {1, 2, 3, 5, 6})
            check_eq(#t, 5)
        end)

        -- Clear table during iteration
        test_case("clear during all()", function()
            local t = {1, 2, 3}
            local result = {}
            local count = 0
            for v in all(t) do
                add(result, v)
                count += 1
                if count == 1 then
                    -- Remove all remaining elements
                    deli(t, 2)
                    deli(t, 2)
                end
            end
            -- Iteration sees shifted elements
            check_eq(result, {1})
            check_eq(t, {1})
        end)

        -- Modify with count() during iteration
        test_case("count during mods", function()
            local t = {1, 2, 3}
            local counts = {}
            local count_loop = 0
            for v in all(t) do
                add(counts, count(t))
                if v == 1 then
                    add(t, 4)
                end
                count_loop += 1
                if count_loop > 10 then break end  -- Safety limit
            end
            -- PICO-8: iteration includes new element, count reflects current state
            check_eq(counts, {3, 4, 4, 4})
        end)
    end)
end

-- Run all test suites
function _init()
    printh("==============================================")
    printh("LUA_API.H TEST SUITE")
    printh("==============================================")
    printh("")

    test_all_function()
    test_add_function()
    test_foreach_function()
    test_count_function()
    test_del_function()
    test_deli_function()
    test_split_function()
    test_concurrent_modification()

    summary()
end
