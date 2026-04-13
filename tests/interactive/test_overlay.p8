pico-8 cartridge // http://www.pico-8.com
version 41
__lua__
-- overlay test
-- note: overlay control is not exposed in pico-8 memory
-- this test cart won't work as-is
-- overlay must be controlled via C API

function _init()
  -- draw something on main screen
  cls(1)
  for i=0,127 do
    line(i,0,127-i,127,7)
  end
  print("main screen",40,60,7)
end

function _draw()
  -- don't clear - keep main screen
  
  -- overlay control no longer accessible via poke
  
  -- draw on overlay (when enabled, drawing goes to overlay)
  rectfill(20,20,107,107,0) -- transparent background
  rectfill(30,30,97,97,8) -- red box
  print("overlay text",40,50,14) -- yellow text
  circfill(64,64,15,11) -- green circle
  
  -- disable overlay to show we can toggle
  if btnp(4) then
    local ctrl = peek(0x5f5e)
    if band(ctrl, 0x40) != 0 then
      poke(0x5f5e, 0) -- disable
    else
      poke(0x5f5e, 0x40) -- enable with transparent color 0
    end
  end
  
  -- change transparent color with up/down
  if btnp(2) then
    local ctrl = peek(0x5f5e)
    local trans = band(ctrl, 0x0f)
    trans = (trans + 1) % 16
    poke(0x5f5e, bor(0x40, trans))
  end
  if btnp(3) then
    local ctrl = peek(0x5f5e)
    local trans = band(ctrl, 0x0f)
    trans = (trans - 1) % 16
    poke(0x5f5e, bor(0x40, trans))
  end
  
  -- show status on overlay
  poke(0x5f5e, 0x40) -- ensure overlay enabled for status
  local ctrl = peek(0x5f5e)
  local enabled = band(ctrl, 0x40) != 0
  local trans = band(ctrl, 0x0f)
  print("z: toggle overlay",2,2,7)
  print("up/dn: trans color",2,8,7)
  print("trans: "..trans,2,14,7)
end
