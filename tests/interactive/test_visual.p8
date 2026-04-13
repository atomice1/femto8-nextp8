pico-8 cartridge // http://www.pico-8.com
version 43
__lua__

function _draw()
    cls()
    color(7)
    
    -- Set up custom font
    poke(0x5600, 8)
    poke(0x5602, 8)
    poke(0x5605, 1)
    
    -- Define 'T' (84)
    local addr = 0x5600 + 128 + (84 * 8)
    poke(addr + 0, 0xff)
    poke(addr + 1, 0xff)
    poke(addr + 2, 0x18)
    poke(addr + 3, 0x18)
    poke(addr + 4, 0x18)
    poke(addr + 5, 0x18)
    poke(addr + 6, 0x18)
    poke(addr + 7, 0x18)
    
    -- Draw with default font
    cursor(0, 0)
    print("T")
    
    -- Draw with custom font  
    cursor(0, 10)
    print("\014T")
    
    print("\n\nif both 'T's look same,")
    print("custom fonts not working")
end
