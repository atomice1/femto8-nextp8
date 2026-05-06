pico-8 cartridge // http://www.pico-8.com
version 16
__lua__

-- menuitem test cart
-- open the pause menu (enter/p) to exercise menu items

local counter = 0
local last_msg = "open pause menu!"
local value = 5

function _init()
    -- item 1: simple action that closes the menu
    menuitem(1, "say hello", function()
        last_msg = "hello!"
        sfx(-1)
    end)

    -- item 2: stay-open toggle (returns true to keep menu open)
    menuitem(2, "keep-open item", function(b)
        if b & 32 > 0 then
            last_msg = "selected (menu stays)"
        end
        return true  -- keep menu open
    end)

    -- item 3: numeric tuner (left/right adjust value)
    menuitem(3, "value:"..value, function(b)
        if b & 1 > 0 then
            value -= 1
            menuitem(3, "value:"..value)
        elseif b & 2 > 0 then
            value += 1
            menuitem(3, "value:"..value)
        elseif b & 112 > 0 then
            last_msg = "value set to "..value
            return false  -- close on select
        end
        return true  -- stay open for left/right
    end)

    -- item 4: gets removed after first press
    menuitem(4, "remove me", function()
        menuitem(4)  -- remove this item
        last_msg = "item 4 removed!"
    end)

    -- item 5: not set initially; added by pressing z/x in game
end

function _update()
    counter += 1

    -- press z to add item 5 dynamically
    if btnp(4) then
        menuitem(5, "dynamic item", function()
            last_msg = "dynamic item works!"
        end)
        last_msg = "item 5 added"
    end

    -- press x to clear item 5
    if btnp(5) then
        menuitem(5)
        last_msg = "item 5 cleared"
    end
end

function _draw()
    cls(1)
    print("menuitem test", 20, 2, 7)
    print("enter/p = pause menu", 4, 14, 6)
    print("z = add item 5", 4, 22, 6)
    print("x = clear item 5", 4, 30, 6)
    print("", 4, 38, 6)
    print("value: "..value, 4, 46, 10)
    print("msg: "..last_msg, 4, 58, 11)
    print("t="..counter, 4, 118, 5)
end

__gfx__
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
