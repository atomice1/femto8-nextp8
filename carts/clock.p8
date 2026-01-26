pico-8 cartridge // http://www.pico-8.com
version 33
__lua__
-- digital clock
-- displays current date and time

function _init()
 last_sec=-1
end

function _update()
 -- only redraw when second changes
 sec=stat(95)
 if sec!=last_sec then
  last_sec=sec
  draw_clock()
 end
end

function _draw()
 -- drawing happens in update
 -- when second changes
end

function draw_clock()
 cls()
 
 -- get date/time
 local year=stat(90)
 local month=stat(91)
 local day=stat(92)
 local hour=stat(93)
 local min=stat(94)
 local sec=stat(95)
 
 -- draw title
 print("digital clock",32,10,7)
 
 -- draw date
 local date_str=year.."-"..
  pad(month).."-"..pad(day)
 print(date_str,24,30,12)
 
 -- draw time (big)
 local time_str=pad(hour)..":"..
  pad(min)..":"..pad(sec)
 print(time_str,22,50,11)
end

function pad(n)
 -- pad single digit with 0
 if n<10 then
  return "0"..n
 end
 return ""..n
end
