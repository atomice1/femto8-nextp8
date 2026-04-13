pico-8 cartridge // http://www.pico-8.com
version 41
__lua__
function _init()
  print("breadcrumb: "..stat(100))
  print("bbs cart id: "..stat(101))
end

function _draw()
  cls()
  print("breadcrumb: "..stat(100), 0, 0, 7)
  print("bbs cart id: "..stat(101), 0, 8, 7)
end
