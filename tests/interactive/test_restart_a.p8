pico-8 cartridge // http://www.pico-8.com
version 42
__lua__
counter = 0
function _init()
    counter = counter + 1
    print("restart test: counter = " .. counter)
    if counter < 3 then
        run()
    end
end
