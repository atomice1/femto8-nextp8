-- test_fwk.lua: common test framework for automatic tests
-- #include this at the top of each test cart's __lua__ section

current_test_suite_name = ""
current_test_case_name = ""
current_test_case_failed = false
current_test_suite_failed = true
current_test_suite_test_case_count = 0
current_test_suite_fail_count = 0
test_suite_count = 0
test_suite_fail_count = 0
total_test_case_count = 0
total_test_case_fail_count = 0

color(7)

function fail(message)
    if not current_test_case_failed then
        current_test_case_failed = true
        current_test_suite_fail_count += 1
        total_test_case_fail_count += 1
    end
    if not current_test_suite_failed then
        current_test_suite_failed = true
        test_suite_fail_count += 1
    end
    printh(current_test_case_name .. ": " .. message)
end

function tostr1(x)
    if x == nil then
        return "<nil>"
    elseif type(x) == "boolean" then
        if x then
            return "true"
        else
            return "false"
        end
    elseif type(x) == "table" then
        ret="{"
        for k,v in pairs(x) do
            ret ..= "["..tostr1(k).."]="..tostr1(v)..","
        end
        ret..="}"
        return ret
    elseif type(x) == "string" then
        return '"'..x..'"'
    else
        return x
    end
end

function eq(a, b)
    if type(a) == "table" then
        for ak,av in pairs(a) do
            found = false
            for bk,bv in pairs(b) do
                if ak==bk then
                    if av != bv then
                        return false
                    end
                    found = true
                end
            end
            if not found then
                return false
            end
        end
        for bk,bv in pairs(b) do
            found = false
            for ak,av in pairs(a) do
                if ak==bk then
                    found=true
                end
            end
            if not found then
                return false
            end
        end
        return true
    else
        return a == b
    end
end

function check_true(a)
    if not a then
        fail("false")
    end
end

function check_false(a)
    if a then
        fail("true")
    end
end

function check_nil(a)
    if a != nil then
        fail("expected nil, got " .. tostr1(a))
    end
end

function check_eq(a, b)
    if not eq(a, b) then
        fail(tostr1(a) .. " != " .. tostr1(b))
    end
end

function check_ne(a, b)
    if eq(a,b) then
        fail(tostr1(a) .. " == " .. tostr1(b))
    end
end

function check_lt(a, b)
    if a >= b then
        fail(tostr1(a) .. " >= " .. tostr1(b))
    end
end

function check_le(a, b)
    if a > b then
        fail(tostr1(a) .. " > " .. tostr1(b))
    end
end

function check_gt(a, b)
    if a <= b then
        fail(tostr1(a) .. " <= " .. tostr1(b))
    end
end

function check_ge(a, b)
    if a < b then
        fail(tostr1(a) .. " < " .. tostr1(b))
    end
end

function check_range(val, min, max)
    if val < min or val > max then
        fail(tostr(val) .. " not in range [" .. tostr(min) .. ", " .. tostr(max) .. "]")
    end
end

function check_type(val, typename)
    if type(val) != typename then
        fail("type(" .. tostr(val) .. ") = " .. type(val) .. ", expected " .. typename)
    end
end

function check_pixel(x, y, expected_color, msg)
    local actual = pget(x, y)
    if actual != expected_color then
        local prefix = msg and (msg .. ": ") or ""
        fail(prefix .. "pixel at (" .. x .. "," .. y .. ") is " .. actual .. ", expected " .. expected_color)
    end
end

function save()
    memcpy(0x8000, 0, 0x5e00)
    memcpy(0xdf00, 0x5f00, 0x80)
    memcpy(0xe000, 0x6000, 0x2000)
    cls()
    reset()
end

function restore()
    poke(0x5f54, 0)
    poke(0x5f55, 0x60)
    poke(0x5f56, 0x20)
    memcpy(0, 0x8000, 0x5e00)
    memcpy(0x5f00, 0xdf00, 0x80)
    memcpy(0x6000, 0xe000, 0x2000)
end

function sleep(seconds)
    start = time()
    while time() - start < seconds and time() >= start do
        flip()
    end
end

function test_case(name, f)
    current_test_case_name = name
    current_test_case_failed = false
    current_test_suite_test_case_count += 1
    total_test_case_count += 1
    save()
    local status, err
    if pcall ~= nil then
        status, err = pcall(f)
    else
        f()
        status = true
    end
    if not status then
        current_test_case_failed = true
        printh(current_test_case_name .. ": " .. err)
    end
    restore()
    if current_test_case_failed then
        printh(current_test_case_name .. ": FAIL")
    else
        printh(current_test_case_name .. ": PASS")
    end
end

function test_suite(name, f)
    current_test_suite = name
    current_test_suite_failed = false
    current_test_suite_fail_count = 0
    current_test_suite_test_case_count = 0
    printh(current_test_suite .. ": START")
    local status, err
    if pcall ~= nil then
        status, err = pcall(f)
    else
        f()
        status = true
    end
    if not status then
        current_test_suite_failed = true
        printh(current_test_suite .. ": " .. err)
    end
    if current_test_suite_failed then
        result = "FAIL"
        print_result = "\f8fail\f7"
    else
        result = "PASS"
        print_result = "\fbpass\f7"
    end
    printh(current_test_suite .. ": " .. result .. " (" ..
        (current_test_suite_test_case_count - current_test_suite_fail_count) .. "/" ..
        current_test_suite_test_case_count .. " passed)")
    print(current_test_suite .. " " .. print_result .. " ")
    flip()
end

function summary()
    printh((total_test_case_count - total_test_case_fail_count) .. "/" ..
        total_test_case_count .. " passed")
    print((total_test_case_count - total_test_case_fail_count) .. "/" ..
        total_test_case_count .. " passed")
end
