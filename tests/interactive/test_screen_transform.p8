pico-8 cartridge // http://www.pico-8.com
version 43
__lua__

-- screen transform test cart
-- exercises each poke(0x5f2c) mode
-- press any key to advance

modes = {
  {0,   "normal\nmode",             128, 128},
  {1,   "horizontal\nstretch",       64, 128},
  {2,   "vertical\nstretch",        128,  64},
  {3,   "both\nstretch",             64,  64},
  {5,   "horizontal\nmirroring",     64, 128},
  {6,   "vertical\nmirroring",      128,  64},
  {7,   "both\nmirroring",           64,  64},
  {129, "horizontal\nflip",         128, 128},
  {130, "vertical\nflip",           128, 128},
  {131, "both\nflip",               128, 128},
  {133, "clockwise\n90 rotation",   128, 128},
  {134, "180\nrotation",            128, 128},
  {135, "counter-cw\n90 rotation",  128, 128},
}

current = 1

function draw_test(m)
  local mode = m[1]
  local label = m[2]
  local w = m[3]
  local h = m[4]

  -- reset to normal before drawing
  poke(0x5f2c, 0)
  cls(0)

  -- draw nested rectangles in the active region
  local layers = min(15, min(w, h) \ 2)
  for i = 0, layers - 1 do
    local col = i + 1
    if col > 15 then col = 15 end
    rectfill(i, i, w - 1 - i, h - 1 - i, col)
  end

  -- print the label centered in the active area
  local lines = {}
  local s = label
  while #s > 0 do
    local nl = nil
    for i = 1, #s do
      if sub(s, i, i) == "\n" then
        nl = i
        break
      end
    end
    if nl then
      add(lines, sub(s, 1, nl - 1))
      s = sub(s, nl + 1)
    else
      add(lines, s)
      s = ""
    end
  end

  local th = #lines * 6
  local start_y = (h - th) \ 2
  for i = 1, #lines do
    local tw = #lines[i] * 4
    local tx = (w - tw) \ 2
    local ty = start_y + (i - 1) * 6
    print(lines[i], tx, ty, 0)
  end

  -- set the transform mode for display
  poke(0x5f2c, mode)
end

function _init()
  draw_test(modes[current])
end

function _update()
  if btnp(4) or btnp(5) then
    current += 1
    if current > #modes then
      current = 1
    end
    draw_test(modes[current])
  end
end

function _draw()
  -- drawing is done in _update / _init
end
