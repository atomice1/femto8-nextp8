pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
state=0
initlen=20

cartdata("snake")
hiscore=dget(0)

function move_food()
  local rx=2+rnd(123)
  local ry=10+rnd(115)
  for x=rx,128 do
    for y=ry,128 do
      if pget(x,y)==0 and pget(x+1,y)==0 and pget(x,y+1)==0 and pget(x+1,y+1)==0 then
        foodx=x
        foody=y
        return
      end
    end
    for y=0,ry do
      if pget(x,y)==0 and pget(x+1,y)==0 and pget(x,y+1)==0 and pget(x+1,y+1)==0 then
        foodx=x
        foody=y
        return
      end
    end
  end
  for x=0,rx do
    for y=ry,128 do
      if pget(x,y)==0 and pget(x+1,y)==0 and pget(x,y+1)==0 and pget(x+1,y+1)==0 then
        foodx=x
        foody=y
        return
      end
    end
    for y=0,ry do
      if pget(x,y)==0 and pget(x+1,y)==0 and pget(x,y+1)==0 and pget(x+1,y+1)==0 then
        foodx=x
        foody=y
        return
      end
    end
  end
  --full!
  state=0
end

function reset_game()
  snakex=64
  snakey=64
  snakedx=1
  snakedy=0
  snakelen=initlen
  lastx=64
  lasty=64
  lastfoodx=0
  lastfoody=0
  score=0
  needcls=true
  dead=false
  tailx={}
  taily={}
  for i=1,initlen do
    add(tailx, snakex)
    add(taily, snakey)
    snakex+=snakedx
    snakey+=snakedy
  end
  tail=1
  lasttail=tail
  lasthead=#tailx
  drawall=true
  beathi=false
  move_food()
end

function _update()
  if state==0 then
    if btnp(🅾️) then
      reset_game()
      state=1
    end
  elseif state==1 then
    local col1=pget(snakex+snakedx*2, snakey+snakedy*2)
    local col2=pget(snakex+snakedx*2+snakedy, snakey+snakedy*2+snakedx)
    local col3=pget(snakex+snakedx*2-snakedy, snakey+snakedy*2-snakedx)
    if (col1!=0 and col1!=15) or (col2!=0 and col2!=15) or (col3!=0 and col3!=15) then
      sfx(2)
      state=0
      drawall=true
      dead=true
      if beathi then
        dset(0, score)
      end
    end
    if col1==15 or col2==15 or col3==15 then
      sfx(0)
      move_food()
      score+=snakelen
      snakelen+=10
      if score>hiscore then
        hiscore=score
        if not beathi then
          sfx(1)
          beathi=true
        end
      end
    end
    add(tailx, snakex)
    add(taily, snakey)
    snakex+=snakedx
    snakey+=snakedy
    tail=max(#tailx-snakelen, tail)
    if btn(⬆️) and snakedy!=1 then
      snakedx=0
      snakedy=-1
    end
    if btn(⬇️) and snakedy!=-1 then
      snakedx=0
      snakedy=1
    end
    if btn(⬅️) and snakedx!=1 then
      snakedx=-1
      snakedy=0
    end
    if btn(➡️) and snakedx!=-1 then
      snakedx=1
      snakedy=0
    end
  end
end

function draw_segments(s, e)
  local col1, col2
  if dead then
    col1=5
    col2=6
  else
    col1=3
    col2=11
  end
  for i=s,e do
    local x=tailx[i]
    local y=taily[i]
    if i<tail then
      c=0
    else
      c=col1
    end
    pset(x-1, y, c)
    pset(x+1, y, c)
    pset(x, y-1, c)
    pset(x, y+1, c)
  end
  if s>tail then
    s=s-1
  end
  if s<tail then
    s=tail
  end
  if e<#tailx then
    e=e+1
  end
  for i=s,e do
    pset(tailx[i], taily[i], col2)
  end
end

function _draw()
  if needcls then
    cls()
    rect(0, 8, 127, 127, 6)
    needcls=false
  end
  if drawall then
    draw_segments(lasttail, #tailx)
		  lasttail=tail
		  lasthead=#tailx
		  drawall=false
 	end
  if state==0 then
    print("press 🅾️ to start", 32, 100, 7)
  elseif state==1 then
    if foodx != lastfoodx then
      rectfill(lastfoodx, lastfoody, lastfoodx+1, lastfoody+1, 0)
    end
    draw_segments(lasttail, tail)
    draw_segments(lasthead, #tailx)
    if foodx != lastfoodx then
    	 rectfill(foodx, foody, foodx+1, foody+1, 15)
    	 lastfoodx=foodx
    	 lastfoody=foody
    end
    lasttail=tail
    lasthead=#tailx
  end
  rectfill(0, 0, 128, 6, 0)
  if score>0 then
    s=tostr(score).."00"
  else
    s="0"
  end
  print(s, 0, 0, 7)
  local col
  if beathi then
    col=10
  else
    col=7
  end
  if hiscore>0 then
    s=tostr(hiscore).."00"
  else
    s="0"
  end
  print(s, 128-#s*4, 0, col)
end

reset_game()

__gfx__
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00700700000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00077000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00077000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00700700000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
__sfx__
010100002405000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
01100000180501a050230502805000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
011000001805015050110500c0500c050000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
